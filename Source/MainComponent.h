#pragma once
#include <JuceHeader.h>
class MainComponent : public juce::AudioAppComponent
{
public:
    MainComponent(); ~MainComponent() override;
    void prepareToPlay(int, double) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void play(); void stop(); void toggleRec(); void loadSample(); void loadFileIntoBuffer(juce::File); void scanVST3Real(); void openFxList(); void renderWav();
    juce::Label titleLabel, bpmLabel, statusLabel;
    juce::TextButton playButton, stopButton, recButton, loadSampleButton, renderButton, scanButton, fxButton;
    juce::Slider bpmSlider, masterSlider;
    juce::AudioFormatManager formatManager;
    juce::StringArray foundPluginNames;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::AudioSampleBuffer sampleBuffer;
    bool isPlaying = false, isRecording = false, sampleLoaded = false;
    int samplePos = 0, bpm = 95; double sampleRate = 44100;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
