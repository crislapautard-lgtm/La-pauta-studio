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

    // --- ARQUITECTURA DE ESTUDIO AVANZADA ---
    static constexpr int numTracks = 16;
    static constexpr int numSubgroups = 4;
    static constexpr int numAuxes = 2;
    
    // Parámetros por Pista
    std::atomic<float> trackVolume[numTracks];
    std::atomic<float> trackPan[numTracks];
    std::atomic<bool>  trackMute[numTracks];
    std::atomic<bool>  trackSolo[numTracks];
    std::atomic<bool>  trackRec[numTracks];
    std::atomic<int>   trackSubgroupAssign[numTracks]; // A qué subgrupo va (0 a 3, o -1 para directo al Master)
    std::atomic<float> trackSend[numTracks][numAuxes]; // Envíos a Auxiliares (Aux A / Aux B)

    // Parámetros de Subgrupos y Auxiliares
    std::atomic<float> subgroupVolume[numSubgroups];
    std::atomic<float> auxVolume[numAuxes];

    // Master Global
    std::atomic<float> masterVolume { 0.9f };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
