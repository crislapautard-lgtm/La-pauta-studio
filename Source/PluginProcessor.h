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

    // Controles externos (Grabación y Tempo dinámico)
    void setTrackRecording(int trackIndex, bool isRecording);
    void setBPM(float bpm);

    // --- ARQUITECTURA DE ESTUDIO ---
    static constexpr int numTracks = 16;
    static constexpr int numSubgroups = 4;
    static constexpr int numAuxes = 2;
    
    // Parámetros por Pista
    std::atomic<float> trackVolume[numTracks];
    std::atomic<float> trackPan[numTracks];
    std::atomic<bool>  trackMute[numTracks];
    std::atomic<bool>  trackSolo[numTracks];
    std::atomic<bool>  trackRec[numTracks];
    std::atomic<int>   trackSubgroupAssign[numTracks];
    std::atomic<float> trackSend[numTracks][numAuxes];

    // Subgrupos, Auxiliares, Master y Tempo
    std::atomic<float> subgroupVolume[numSubgroups];
    std::atomic<float> auxVolume[numAuxes];
    std::atomic<float> masterVolume { 0.9f };
    std::atomic<float> currentBPM { 120.0f }; // Ajustable en tiempo real

private:
    double currentSampleRate = 44100.0;

    struct TrackRecorder
    {
        std::unique_ptr<juce::AudioFormatWriter> writer;
        juce::TimeSliceThread backgroundThread { "Audio Recorder Thread" };
        std::atomic<bool> isRecording { false };
    };

    std::unique_ptr<TrackRecorder> recorders[numTracks];

    void startRecordingTrack(int trackIndex);
    void stopRecordingTrack(int trackIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
