#pragma once
#include <JuceHeader.h>

class MainComponent : public juce::AudioAppComponent,
                      public juce::Button::Listener,
                      public juce::Slider::Listener,
                      public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void timerCallback() override;

private:
    juce::TextButton playButton{"PLAY"};
    juce::TextButton stopButton{"STOP"};
    juce::TextButton recButton{"REC"};
    juce::TextButton addTrackButton{"+ ADD TRACK"};
    juce::TextButton scanButton{"ESCANEAR VST3"};
    juce::TextButton beatButton{"BEAT MAKER"};
    juce::TextButton renderButton{"RENDER WAV"};

    juce::Label titleLabel;
    juce::Label bpmLabel;
    juce::Slider bpmSlider;
    juce::Slider masterSlider;
    juce::Slider track1Volume;
    juce::Slider track2Volume;

    juce::ToggleButton mute1{"M"}, solo1{"S"}, rec1{"R"};
    juce::ToggleButton mute2{"M"}, solo2{"S"}, rec2{"R"};

    bool isPlaying = false;
    bool isRecording = false;
    double currentPosition = 0.0;
    double sampleRateNow = 44100.0;
    juce::AudioBuffer<float> recordedBuffer;
    int recordedSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
