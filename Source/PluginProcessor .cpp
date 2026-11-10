
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <string>

// ============================================================
// LA PAUTA STUDIO
// PluginProcessor.cpp
// Motor principal del proyecto
// ============================================================

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : juce::AudioProcessor(
        BusesProperties()
            .withInput(
                "Input",
                juce::AudioChannelSet::stereo(),
                true
            )
            .withOutput(
                "Output",
                juce::AudioChannelSet::stereo(),
                true
            )
    )
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
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    for (auto& track : tracks)
    {
        if (auto* audioTrack = dynamic_cast<AudioTrack*>(track.get()))
            audioTrack->prepareToPlay(samplesPerBlock, sampleRate);
    }
}

void AudioPluginAudioProcessor::releaseResources()
{
    for (auto& track : tracks)
    {
        if (auto* audioTrack = dynamic_cast<AudioTrack*>(track.get()))
            audioTrack->releaseResources();
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

    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono()
        && output != juce::AudioChannelSet::stereo())
        return false;

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

    const int inputChannels = getTotalNumInputChannels();
    const int outputChannels = getTotalNumOutputChannels();

    for (int channel = inputChannels;
         channel < outputChannels;
         ++channel)
    {
        buffer.clear(channel, 0, buffer.getNumSamples());
    }

    // Motor de pistas existente.
    // La grabación real y la mezcla independiente requieren
    // completar AudioEngine y AudioTrack.
    audioEngine.processBlock(buffer, midiMessages, tracks);

    // Volumen general de salida.
    buffer.applyGain(
        juce::jlimit(0.0f, 1.5f, masterVolume)
    );
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
    juce::MemoryOutputStream stream(destData, false);

    stream.writeFloat(currentBPM);
    stream.writeFloat(masterVolume);

    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        stream.writeFloat(trackVolume[i]);
        stream.writeFloat(trackPan[i]);
        stream.writeBool(trackMute[i]);
        stream.writeBool(trackSolo[i]);
        stream.writeBool(trackRecording[i]);
    }
}

void AudioPluginAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    juce::MemoryInputStream stream(data, (size_t)sizeInBytes, false);

    if (stream.getNumBytesRemaining() < 8)
        return;

    currentBPM = juce::jlimit(40.0f, 240.0f, stream.readFloat());
    masterVolume = juce::jlimit(0.0f, 1.5f, stream.readFloat());

    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        if (stream.getNumBytesRemaining() < 9)
            break;

        trackVolume[i] = juce::jlimit(0.0f, 1.5f, stream.readFloat());
        trackPan[i] = juce::jlimit(-1.0f, 1.0f, stream.readFloat());
        trackMute[i] = stream.readBool();
        trackSolo[i] = stream.readBool();
        trackRecording[i] = stream.readBool();
    }
}

// ============================================================
// ADMINISTRACION DE PISTAS
// ============================================================

Track* AudioPluginAudioProcessor::getTrack(int index)
{
    if (index < 0 || index >= (int)tracks.size())
        return nullptr;

    return tracks[(size_t)index].get();
}

AudioTrack* AudioPluginAudioProcessor::getAudioTrack(int index)
{
    return dynamic_cast<AudioTrack*>(getTrack(index));
}

MidiTrack* AudioPluginAudioProcessor::getMidiTrack(int index)
{
    return dynamic_cast<MidiTrack*>(getTrack(index));
}

bool AudioPluginAudioProcessor::addAudioTrack(
    const juce::String& name)
{
    if ((int)tracks.size() >= MAX_TRACKS)
        return false;

    auto newTrack = std::make_unique<AudioTrack>(
        name.toStdString()
    );

    newTrack->prepareToPlay(
        currentBlockSize,
        currentSampleRate
    );

    tracks.push_back(std::move(newTrack));

    return true;
}

bool AudioPluginAudioProcessor::addMidiTrack(
    const juce::String& name)
{
    if ((int)tracks.size() >= MAX_TRACKS)
        return false;

    tracks.push_back(
        std::make_unique<MidiTrack>(name)
    );

    return true;
}

void AudioPluginAudioProcessor::removeTrack(int index)
{
    if (index < 0 || index >= (int)tracks.size())
        return;

    tracks.erase(tracks.begin() + index);
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

    trackRecording[(size_t)trackIndex] = shouldRecord;
}

bool AudioPluginAudioProcessor::isTrackRecording(
    int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return false;

    return trackRecording[(size_t)trackIndex];
}

// ============================================================
// MEZCLADOR
// ============================================================

void AudioPluginAudioProcessor::setTrackVolume(
    int trackIndex,
    float volume)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    volume = juce::jlimit(0.0f, 1.5f, volume);

    trackVolume[(size_t)trackIndex] = volume;

    if (auto* track = getAudioTrack(trackIndex))
        track->setVolume(volume);
}

void AudioPluginAudioProcessor::setTrackPan(
    int trackIndex,
    float pan)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    pan = juce::jlimit(-1.0f, 1.0f, pan);

    trackPan[(size_t)trackIndex] = pan;

    if (auto* track = getAudioTrack(trackIndex))
        track->setPan(pan);
}

void AudioPluginAudioProcessor::setTrackMute(
    int trackIndex,
    bool shouldMute)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    trackMute[(size_t)trackIndex] = shouldMute;

    if (auto* track = getAudioTrack(trackIndex))
        track->setMute(shouldMute);
}

void AudioPluginAudioProcessor::setTrackSolo(
    int trackIndex,
    bool shouldSolo)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    trackSolo[(size_t)trackIndex] = shouldSolo;

    if (auto* track = getAudioTrack(trackIndex))
        track->setSolo(shouldSolo);
}

void AudioPluginAudioProcessor::setMasterVolume(float volume)
{
    masterVolume = juce::jlimit(0.0f, 1.5f, volume);
}

void AudioPluginAudioProcessor::setBPM(float newBPM)
{
    currentBPM = juce::jlimit(40.0f, 240.0f, newBPM);
}

// ============================================================
// PUNTO DE ENTRADA JUCE
// ============================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
