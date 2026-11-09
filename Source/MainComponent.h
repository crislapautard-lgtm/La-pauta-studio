#pragma once
#include <JuceHeader.h>

class MainComponent : public juce::AudioAppComponent, public juce::Timer
{
public:
    MainComponent(); ~MainComponent() override;
    void prepareToPlay(int, double) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
private:
    struct Track { juce::String name; juce::AudioSampleBuffer buffer; bool muted=false; float volume=0.8f; int pos=0; bool hasAudio=false; juce::Colour color; };
    void play(); void stop(); void toggleRec(); void loadSample(); void loadFileIntoBuffer(juce::File, int);
    void scanVST3(); void saveProject(); void loadProject(); void addTrack(); void renderMix();
    void toggleMetronome(); void openBeatMaker(); void openMixer(); void openAudioSettings();

    juce::Label titleLabel, bpmLabel, statusLabel, eqLabel;
    juce::TextButton playButton, stopButton, recButton, loadSampleButton, renderButton, scanButton, saveButton, loadButton, addTrackButton, metronomeButton, beatButton, mixerButton, audioSettingsButton;
    juce::Slider bpmSlider, masterSlider, eqLowSlider, eqMidSlider, eqHighSlider;
    juce::ComboBox trackSelector;
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::Array<Track> tracks;
    juce::StringArray foundPlugins;
    int selectedTrack=0, playhead=0, bpm=95;
    bool isPlaying=false, isRecording=false, metronomeOn=false;
    double sampleRate=44100;
    float eqLow=0, eqMid=0, eqHigh=0;
    juce::File currentSampleFile;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
