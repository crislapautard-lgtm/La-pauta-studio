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
        trackSubgroupAssign[i].store(-1);

        for (int a = 0; a < numAuxes; ++a)
            trackSend[i][a].store(0.0f);

        recorders[i] = std::make_unique<TrackRecorder>();
    }

    for (int s = 0; s < numSubgroups; ++s)
        subgroupVolume[s].store(0.9f);

    for (int a = 0; a < numAuxes; ++a)
        auxVolume[a].store(0.7f);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    for (int i = 0; i < numTracks; ++i) {
        stopRecordingTrack(i);
    }
}

const juce::String AudioPluginAudioProcessor::getName() const { return JucePlugin_Name; }
bool AudioPluginAudioProcessor::acceptsMidi() const { return false; }
bool AudioPluginAudioProcessor::producesMidi() const { return false; }
double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int AudioPluginAudioProcessor::getNumPrograms() override { return 1; }
int AudioPluginAudioProcessor::getCurrentProgram() override { return 0; }
void AudioPluginAudioProcessor::setCurrentProgram (int index) override {}
const juce::String AudioPluginAudioProcessor::getProgramName (int index) override { return {}; }
void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName) {}

void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
}

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

void AudioPluginAudioProcessor::setTrackRecording(int trackIndex, bool shouldRecord)
{
    if (trackIndex < 0 || trackIndex >= numTracks) return;

    bool currentlyRec = trackRec[trackIndex].load();
    if (shouldRecord != currentlyRec) {
        trackRec[trackIndex].store(shouldRecord);
        if (shouldRecord) {
            startRecordingTrack(trackIndex);
        } else {
            stopRecordingTrack(trackIndex);
        }
    }
}

void AudioPluginAudioProcessor::startRecordingTrack(int trackIndex)
{
    auto& rec = recorders[trackIndex];
    if (rec->isRecording.load()) return;

    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("LaPautaStudio_Recordings");
    dir.createDirectory();

    auto file = dir.getChildFile("Vocal_" + juce::String(trackIndex + 1) + "_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".wav");
    file.deleteFile();

    rec->backgroundThread.startThread();

    auto* outputStream = file.createOutputStream();
    if (outputStream != nullptr)
    {
        juce::WavAudioFormat wavFormat; // CORREGIDO AQUÍ
        auto* writer = wavFormat.createWriterFor(outputStream, currentSampleRate, 2, 16, {}, 0);
        if (writer != nullptr)
        {
            rec->writer.reset(writer);
            rec->isRecording.store(true);
        }
    }
}

void AudioPluginAudioProcessor::stopRecordingTrack(int trackIndex)
{
    auto& rec = recorders[trackIndex];
    if (!rec->isRecording.load()) return;

    rec->isRecording.store(false);
    rec->backgroundThread.stopThread(2000);

    rec->writer.reset();
}

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

        std::vector<float> subL(numSubgroups * numSamples, 0.0f);
        std::vector<float> subR(numSubgroups * numSamples, 0.0f);
        std::vector<float> auxL(numAuxes * numSamples, 0.0f);
        std::vector<float> auxR(numAuxes * numSamples, 0.0f);

        std::vector<float> masterAccumL(numSamples, 0.0f);
        std::vector<float> masterAccumR(numSamples, 0.0f);

        for (int t = 0; t < numTracks; ++t)
        {
            if (recorders[t]->isRecording.load() && recorders[t]->writer != nullptr)
            {
                juce::AudioBuffer<float> tempBuf(2, numSamples);
                tempBuf.copyFrom(0, 0, buffer, 0, 0, numSamples);
                tempBuf.copyFrom(1, 0, buffer, 1, 0, numSamples);
                recorders[t]->writer->writeFromAudioSampleBuffer(tempBuf, 0, numSamples);
            }

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

                if (sendA > 0.0f) {
                    auxL[0 * numSamples + i] += processedL * sendA;
                    auxR[0 * numSamples + i] += processedR * sendA;
                }
                if (sendB > 0.0f) {
                    auxL[1 * numSamples + i] += processedL * sendB;
                    auxR[1 * numSamples + i] += processedR * sendB;
                }

                if (subGroup >= 0 && subGroup < numSubgroups) {
                    subL[subGroup * numSamples + i] += processedL;
                    subR[subGroup * numSamples + i] += processedR;
                } else {
                    masterAccumL[i] += processedL;
                    masterAccumR[i] += processedR;
                }
            }
        }

        for (int s = 0; s < numSubgroups; ++s) {
            float sVol = subgroupVolume[s].load();
            for (int i = 0; i < numSamples; ++i) {
                masterAccumL[i] += subL[s * numSamples + i] * sVol;
                masterAccumR[i] += subR[s * numSamples + i] * sVol;
            }
        }

        for (int a = 0; a < numAuxes; ++a) {
            float aVol = auxVolume[a].load();
            for (int i = 0; i < numSamples; ++i) {
                masterAccumL[i] += auxL[a * numSamples + i] * aVol;
                masterAccumR[i] += auxR[a * numSamples + i] * aVol;
            }
        }

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
