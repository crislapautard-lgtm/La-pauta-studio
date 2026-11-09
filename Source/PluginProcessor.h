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

    // Control de Grabación externo para la interfaz
    void setTrackRecording(int trackIndex, bool isRecording);

    // --- ARQUITECTURA DE ESTUDIO CON GRABACIÓN ---
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

    // Parámetros de Subgrupos y Auxiliares
    std::atomic<float> subgroupVolume[numSubgroups];
    std::atomic<float> auxVolume[numAuxes];

    // Master Global
    std::atomic<float> masterVolume { 0.9f };

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
