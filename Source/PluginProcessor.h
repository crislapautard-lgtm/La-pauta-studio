#pragma once
#include <JuceHeader.h>

class AudioPluginAudioProcessor : public juce::AudioProcessor
{
public:
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- ARREGLO MULTITRACK (4 Pistas + Master) ---
    static constexpr int numTracks = 4;
    
    std::atomic<float> trackVolume[numTracks] { {0.8f}, {0.8f}, {0.8f}, {0.8f} };
    std::atomic<float> trackPan[numTracks]    { {0.0f}, {0.0f}, {0.0f}, {0.0f} };
    std::atomic<bool>  trackMute[numTracks]   { {false}, {false}, {false}, {false} };
    std::atomic<bool>  trackSolo[numTracks]   { {false}, {false}, {false}, {false} };
    std::atomic<bool>  trackRec[numTracks]    { {false}, {false}, {false}, {false} };

    std::atomic<float> masterVolume { 0.9f };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
