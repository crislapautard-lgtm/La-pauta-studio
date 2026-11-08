#pragma once
#include <JuceHeader.h>

class MainComponent : public juce::AudioAppComponent
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

private:
    void play();
    void stop();
    void toggleRec();
    void renderWav();
    void scanVST3Real();
    void openBeatMaker();
    void addTrack();
    void openFxList();

    // UI
    juce::Label titleLabel, bpmLabel, masterLabel;
    juce::TextButton playButton, stopButton, recButton, renderButton;
    juce::TextButton scanButton, beatButton, addTrackButton, fxButton;
    juce::Slider bpmSlider, masterSlider;

    // Estado
    int bpm = 95;
    bool isPlaying = false;
    bool isRecording = false;
    double sampleRate = 44100.0;
    int playhead = 0;

    // --- ESTO ES LO QUE ESCANEA TUS VST DE VERDAD ---
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    juce::StringArray foundPluginNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
