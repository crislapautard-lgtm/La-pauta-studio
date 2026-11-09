#include "PluginProcessor.h"

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {}

const juce::String AudioPluginAudioProcessor::getName() const { return JucePlugin_Name; }
bool AudioPluginAudioProcessor::acceptsMidi() const { return false; }
bool AudioPluginAudioProcessor::producesMidi() const { return false; }
double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int AudioPluginAudioProcessor::getNumPrograms() override { return 1; }
int AudioPluginAudioProcessor::getCurrentProgram() override { return 0; }
void AudioPluginAudioProcessor::setCurrentProgram (int index) override {}
const juce::String AudioPluginAudioProcessor::getProgramName (int index) override { return {}; }
void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName) {}

void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {}
void AudioPluginAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Si está en Mute, silenciamos la salida
    if (trackMute.load()) {
        buffer.clear();
        return;
    }

    float vol = trackVolume.load();
    float pan = trackPan.load(); // -1.0 izq, 1.0 der

    float leftGain  = vol * (pan <= 0.0f ? 1.0f : 1.0f - pan);
    float rightGain = vol * (pan >= 0.0f ? 1.0f : 1.0f + pan);

    if (totalNumInputChannels >= 2)
    {
        auto* channelDataLeft  = buffer.getWritePointer (0);
        auto* channelDataRight = buffer.getWritePointer (1);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            channelDataLeft[i]  *= leftGain;
            channelDataRight[i] *= rightGain;
        }
    }
    else if (totalNumInputChannels == 1)
    {
        auto* channelData = buffer.getWritePointer (0);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            channelData[i] *= vol;
        }
    }
}

bool AudioPluginAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor() { return new juce::GenericAudioProcessorEditor (*this); }
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {}
void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
