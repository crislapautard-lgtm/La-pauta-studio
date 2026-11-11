
#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================
// LA PAUTA STUDIO
// PLUGIN PROCESSOR
// ============================================================

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : juce::AudioProcessor(
        BusesProperties()
            .withInput(
                "Input",
                juce::AudioChannelSet::stereo(),
                true)
            .withOutput(
                "Output",
                juce::AudioChannelSet::stereo(),
                true))
{
    trackVolume.fill(0.8f);
    trackPan.fill(0.0f);
    trackMute.fill(false);
    trackSolo.fill(false);
    trackRecording.fill(false);

    subgroupVolume.fill(0.8f);
    auxVolume.fill(0.8f);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    releaseResources();
}

// ============================================================
// PREPARAR AUDIO
// ============================================================

void AudioPluginAudioProcessor::prepareToPlay(
    double sampleRate,
    int samplesPerBlock)
{
    const juce::ScopedLock lock(trackLock);

    currentSampleRate = juce::jmax(1.0, sampleRate);
    currentBlockSize = juce::jmax(1, samplesPerBlock);

    const int outputChannels =
        juce::jmax(1, getTotalNumOutputChannels());

    audioEngine.prepareToPlay(
        currentBlockSize,
        currentSampleRate,
        outputChannels);

    for (auto& track : tracks)
    {
        if (auto* audioTrack =
                dynamic_cast<AudioTrack*>(track.get()))
        {
            audioTrack->prepareToPlay(
                currentBlockSize,
                currentSampleRate);
        }
    }
}

// ============================================================
// LIBERAR AUDIO
// ============================================================

void AudioPluginAudioProcessor::releaseResources()
{
    const juce::ScopedLock lock(trackLock);

    audioEngine.releaseResources();

    for (auto& track : tracks)
    {
        if (auto* audioTrack =
                dynamic_cast<AudioTrack*>(track.get()))
        {
            audioTrack->releaseResources();
        }
    }
}

// ============================================================
// CONFIGURACION DE CANALES
// ============================================================

bool AudioPluginAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect

    juce::ignoreUnused(layouts);
    return true;

#else

    const auto output =
        layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono()
        && output != juce::AudioChannelSet::stereo())
    {
        return false;
    }

#if ! JucePlugin_IsSynth

    if (output != layouts.getMainInputChannelSet())
        return false;

#endif

    return true;

#endif
}

// ============================================================
// PROCESAMIENTO PRINCIPAL
// ============================================================

void AudioPluginAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int inputChannels =
        getTotalNumInputChannels();

    const int outputChannels =
        getTotalNumOutputChannels();

    const int numSamples =
        buffer.getNumSamples();

    // Limpiar canales de salida adicionales.

    for (int channel = inputChannels;
         channel < outputChannels;
         ++channel)
    {
        buffer.clear(channel, 0, numSamples);
    }

    // Proteger temporalmente la colección de pistas.
    // Esta protección es provisional para evitar modificaciones
    // simultáneas mientras se procesa el bloque.

    {
        const juce::ScopedLock lock(trackLock);

        audioEngine.processBlock(
            buffer,
            midiMessages,
            tracks);
    }

    // Volumen maestro.

    const float safeMasterVolume =
        juce::jlimit(0.0f, 1.5f, masterVolume);

    buffer.applyGain(safeMasterVolume);
}

// ============================================================
// INFORMACION DEL PLUGIN
// ============================================================

const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

// ============================================================
// PROGRAMAS
// ============================================================

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String AudioPluginAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName(
    int index,
    const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

// ============================================================
// EDITOR
// ============================================================

bool AudioPluginAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor*
AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor(*this);
}

// ============================================================
// GUARDAR ESTADO
// ============================================================

void AudioPluginAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData)
{
    const juce::ScopedLock lock(trackLock);

    juce::MemoryOutputStream stream(destData, false);

    stream.writeFloat(currentBPM);
    stream.writeFloat(masterVolume);

    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        stream.writeFloat(trackVolume[i]);
        stream.writeFloat(trackPan[i]);

        stream.writeBool(trackMute[i]);
        stream.writeBool(trackSolo[i]);

        // Estado de grabacion reservado.
        stream.writeBool(trackRecording[i]);
    }
}

// ============================================================
// RESTAURAR ESTADO
// ============================================================

void AudioPluginAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes)
{
    if (data == nullptr || sizeInBytes < 8)
        return;

    const juce::ScopedLock lock(trackLock);

    juce::MemoryInputStream stream(
        data,
        static_cast<size_t>(sizeInBytes),
        false);

    currentBPM =
        juce::jlimit(
            40.0f,
            240.0f,
            stream.readFloat());

    masterVolume =
        juce::jlimit(
            0.0f,
            1.5f,
            stream.readFloat());

    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        // Dos floats y tres booleanos = 11 bytes.

        if (stream.getNumBytesRemaining() < 11)
            break;

        trackVolume[i] =
            juce::jlimit(
                0.0f,
                1.5f,
                stream.readFloat());

        trackPan[i] =
            juce::jlimit(
                -1.0f,
                1.0f,
                stream.readFloat());

        trackMute[i] = stream.readBool();
        trackSolo[i] = stream.readBool();
        trackRecording[i] = stream.readBool();

        if (auto* audioTrack = getAudioTrack(i))
        {
            audioTrack->setVolume(trackVolume[i]);
            audioTrack->setPan(trackPan[i]);
            audioTrack->setMute(trackMute[i]);
            audioTrack->setSolo(trackSolo[i]);
            audioTrack->setIsRecording(false);
        }
    }

    // No se restaura una grabacion activa al abrir el proyecto.
    trackRecording.fill(false);
}

// ============================================================
// ADMINISTRACION DE PISTAS
// ============================================================

Track* AudioPluginAudioProcessor::getTrack(int index)
{
    const juce::ScopedLock lock(trackLock);

    if (index < 0
        || index >= static_cast<int>(tracks.size()))
    {
        return nullptr;
    }

    return tracks[static_cast<size_t>(index)].get();
}

AudioTrack* AudioPluginAudioProcessor::getAudioTrack(int index)
{
    return dynamic_cast<AudioTrack*>(getTrack(index));
}

MidiTrack* AudioPluginAudioProcessor::getMidiTrack(int index)
{
    return dynamic_cast<MidiTrack*>(getTrack(index));
}

// ============================================================
// CREAR PISTA DE AUDIO
// ============================================================

bool AudioPluginAudioProcessor::addAudioTrack(
    const juce::String& name)
{
    const juce::ScopedLock lock(trackLock);

    if (static_cast<int>(tracks.size()) >= MAX_TRACKS)
        return false;

    const int index =
        static_cast<int>(tracks.size());

    auto newTrack =
        std::make_unique<AudioTrack>(name.toStdString());

    newTrack->prepareToPlay(
        currentBlockSize,
        currentSampleRate);

    newTrack->setVolume(trackVolume[index]);
    newTrack->setPan(trackPan[index]);
    newTrack->setMute(trackMute[index]);
    newTrack->setSolo(trackSolo[index]);

    tracks.push_back(std::move(newTrack));

    return true;
}

// ============================================================
// CREAR PISTA MIDI
// ============================================================

bool AudioPluginAudioProcessor::addMidiTrack(
    const juce::String& name)
{
    const juce::ScopedLock lock(trackLock);

    if (static_cast<int>(tracks.size()) >= MAX_TRACKS)
        return false;

    const int index =
        static_cast<int>(tracks.size());

    auto newTrack =
        std::make_unique<MidiTrack>(name);

    newTrack->setMuted(trackMute[index]);

    tracks.push_back(std::move(newTrack));

    return true;
}

// ============================================================
// ELIMINAR PISTA
// ============================================================

void AudioPluginAudioProcessor::removeTrack(int index)
{
    const juce::ScopedLock lock(trackLock);

    const int totalTracks =
        static_cast<int>(tracks.size());

    if (index < 0 || index >= totalTracks)
        return;

    // Eliminar pista.

    tracks.erase(tracks.begin() + index);

    // Desplazar los datos del mezclador para mantener
    // los indices sincronizados con las pistas.

    for (int i = index; i < totalTracks - 1; ++i)
    {
        trackVolume[i] = trackVolume[i + 1];
        trackPan[i] = trackPan[i + 1];

        trackMute[i] = trackMute[i + 1];
        trackSolo[i] = trackSolo[i + 1];
        trackRecording[i] = trackRecording[i + 1];
    }

    const int lastIndex = totalTracks - 1;

    trackVolume[lastIndex] = 0.8f;
    trackPan[lastIndex] = 0.0f;

    trackMute[lastIndex] = false;
    trackSolo[lastIndex] = false;
    trackRecording[lastIndex] = false;
}

// ============================================================
// GRABACION
// ============================================================

void AudioPluginAudioProcessor::setTrackRecording(
    int trackIndex,
    bool shouldRecord)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    trackRecording[trackIndex] = shouldRecord;

    if (auto* audioTrack = getAudioTrack(trackIndex))
    {
        audioTrack->setIsRecording(shouldRecord);
    }
}

bool AudioPluginAudioProcessor::isTrackRecording(
    int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return false;

    return trackRecording[trackIndex];
}

// ============================================================
// MEZCLADOR - VOLUMEN
// ============================================================

void AudioPluginAudioProcessor::setTrackVolume(
    int trackIndex,
    float volume)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    volume = juce::jlimit(0.0f, 1.5f, volume);

    trackVolume[trackIndex] = volume;

    if (auto* track = getAudioTrack(trackIndex))
    {
        track->setVolume(volume);
    }
}

// ============================================================
// MEZCLADOR - PANORAMA
// ============================================================

void AudioPluginAudioProcessor::setTrackPan(
    int trackIndex,
    float pan)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    pan = juce::jlimit(-1.0f, 1.0f, pan);

    trackPan[trackIndex] = pan;

    if (auto* track = getAudioTrack(trackIndex))
    {
        track->setPan(pan);
    }
}

// ============================================================
// MEZCLADOR - MUTE
// ============================================================

void AudioPluginAudioProcessor::setTrackMute(
    int trackIndex,
    bool shouldMute)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    trackMute[trackIndex] = shouldMute;

    if (auto* track = getTrack(trackIndex))
    {
        track->setMuted(shouldMute);
    }
}

// ============================================================
// MEZCLADOR - SOLO
// ============================================================

void AudioPluginAudioProcessor::setTrackSolo(
    int trackIndex,
    bool shouldSolo)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    trackSolo[trackIndex] = shouldSolo;

    if (auto* track = getAudioTrack(trackIndex))
    {
        track->setSolo(shouldSolo);
    }
}

// ============================================================
// MASTER
// ============================================================

void AudioPluginAudioProcessor::setMasterVolume(float volume)
{
    masterVolume =
        juce::jlimit(0.0f, 1.5f, volume);
}

// ============================================================
// BPM
// ============================================================

void AudioPluginAudioProcessor::setBPM(float newBPM)
{
    currentBPM =
        juce::jlimit(40.0f, 240.0f, newBPM);
}

// ============================================================
// PUNTO DE ENTRADA JUCE
// ============================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
