#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    for (int i = 0; i < numTracks; ++i) {
        trackVolume[i].store(0.8f);
        trackPan[i].store(0.0f);
        trackMute[i].store(false);
        trackSolo[i].store(false);
        trackRec[i].store(false);
        trackSubgroupAssign[i].store(-1); // Por defecto van directo al Master

        for (int a = 0; a < numAuxes; ++a)
            trackSend[i][a].store(0.0f);
    }

    for (int s = 0; s < numSubgroups; ++s)
        subgroupVolume[s].store(0.9f);

    for (int a = 0; a < numAuxes; ++a)
        auxVolume[a].store(0.7f);
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

    if (totalNumInputChannels >= 2)
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        int numSamples = buffer.getNumSamples();

        // Buffers temporales para Subgrupos y Auxiliares en el DSP
        std::vector<float> subL(numSubgroups * numSamples, 0.0f);
        std::vector<float> subR(numSubgroups * numSamples, 0.0f);
        std::vector<float> auxL(numAuxes * numSamples, 0.0f);
        std::vector<float> auxR(numAuxes * numSamples, 0.0f);

        std::vector<float> masterAccumL(numSamples, 0.0f);
        std::vector<float> masterAccumR(numSamples, 0.0f);

        // 1. Procesar cada pista individual
        for (int t = 0; t < numTracks; ++t)
        {
            if (trackMute[t].load()) continue;

            float vol = trackVolume[t].load();
            float pan = trackPan[t].load();
            float lGain = vol * (pan <= 0.0f ? 1.0f : 1.0f - pan);
            float rGain = vol * (pan >= 0.0f ? 1.0f : 1.0f + pan);

            int subGroup = trackSubgroupAssign[t].load();
            float sendA = trackSend[t][0].load();
            float sendB = trackSend[t][1].load();

            for (int i = 0; i < numSamples; ++i)
            {
                float processedL = left[i] * lGain;
                float processedR = right[i] * rGain;

                // Envíos a Auxiliares (Sends)
                if (sendA > 0.0f) {
                    auxL[0 * numSamples + i] += processedL * sendA;
                    auxR[0 * numSamples + i] += processedR * sendA;
                }
                if (sendB > 0.0f) {
                    auxL[1 * numSamples + i] += processedL * sendB;
                    auxR[1 * numSamples + i] += processedR * sendB;
                }

                // Ruteo a Subgrupo o Master directo
                if (subGroup >= 0 && subGroup < numSubgroups) {
                    subL[subGroup * numSamples + i] += processedL;
                    subR[subGroup * numSamples + i] += processedR;
                } else {
                    masterAccumL[i] += processedL;
                    masterAccumR[i] += processedR;
                }
            }
        }

        // 2. Sumar Subgrupos al Master aplicando su volumen
        for (int s = 0; s < numSubgroups; ++s) {
            float sVol = subgroupVolume[s].load();
            for (int i = 0; i < numSamples; ++i) {
                masterAccumL[i] += subL[s * numSamples + i] * sVol;
                masterAccumR[i] += subR[s * numSamples + i] * sVol;
            }
        }

        // 3. Sumar Auxiliares al Master aplicando su volumen (Efecto en paralelo)
        for (int a = 0; a < numAuxes; ++a) {
            float aVol = auxVolume[a].load();
            for (int i = 0; i < numSamples; ++i) {
                masterAccumL[i] += auxL[a * numSamples + i] * aVol;
                masterAccumR[i] += auxR[a * numSamples + i] * aVol;
            }
        }

        // 4. Aplicar Volumen Master Final
        float masterVol = masterVolume.load();
        for (int i = 0; i < numSamples; ++i) {
            left[i]  = masterAccumL[i] * masterVol;
            right[i] = masterAccumR[i] * masterVol;
        }
    }
}

bool AudioPluginAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor() { return new AudioPluginAudioProcessorEditor (*this); }
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {}
void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
