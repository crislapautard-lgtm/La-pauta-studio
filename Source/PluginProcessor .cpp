
#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================
// LA PAUTA STUDIO
// PluginProcessor.cpp
// JUCE / C++
// ============================================================

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : juce::AudioProcessor (
        BusesProperties()
            .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
      )
{
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

// ============================================================
// NOMBRE
// ============================================================

const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

// ============================================================
// MIDI
// ============================================================

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

// ============================================================
// COLA DE AUDIO
// ============================================================

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

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (
    int index,
    const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

// ============================================================
// PREPARACION DE AUDIO
// ============================================================

void AudioPluginAudioProcessor::prepareToPlay (
    double sampleRate,
    int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void AudioPluginAudioProcessor::releaseResources()
{
}

// ============================================================
// CONFIGURACION DE CANALES
// ============================================================

bool AudioPluginAudioProcessor::isBusesLayoutSupported (
    const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect

    juce::ignoreUnused (layouts);
    return true;

#else

    const auto outputChannels = layouts.getMainOutputChannelSet();

    if (outputChannels != juce::AudioChannelSet::mono()
        && outputChannels != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth

    if (outputChannels != layouts.getMainInputChannelSet())
        return false;

#endif

    return true;

#endif
}

// ============================================================
// PROCESAMIENTO DE AUDIO
// Entrada -> Procesador -> Salida
// Actualmente mantiene la señal sin alterarla.
// ============================================================

void AudioPluginAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    // Limpiar canales de salida que no tienen entrada.
    for (int channel = totalNumInputChannels;
         channel < totalNumOutputChannels;
         ++channel)
    {
        buffer.clear (channel, 0, buffer.getNumSamples());
    }

    // La señal de entrada pasa directamente a la salida.
    // Aquí se incorporarán posteriormente los efectos y el mezclador.
    juce::ignoreUnused (midiMessages);
}

// ============================================================
// INTERFAZ
// ============================================================

bool AudioPluginAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor*
AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

// ============================================================
// GUARDAR Y CARGAR ESTADO
// ============================================================

void AudioPluginAudioProcessor::getStateInformation (
    juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void AudioPluginAudioProcessor::setStateInformation (
    const void* data,
    int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

// ============================================================
// CONTROLES DE LA PAUTA STUDIO
// ============================================================

void AudioPluginAudioProcessor::setTrackRecording (
    int trackIndex,
    bool isRecording)
{
    if (trackIndex >= 0 && trackIndex < numTracks)
        trackRecording[trackIndex] = isRecording;
}

void AudioPluginAudioProcessor::setBPM (float newBPM)
{
    currentBPM = juce::jlimit (40.0f, 240.0f, newBPM);
}

// ============================================================
// PUNTO DE ENTRADA OBLIGATORIO DE JUCE
// ============================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
