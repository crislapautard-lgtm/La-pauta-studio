#pragma once

#include <JuceHeader.h>

class MainComponent : public juce::Component,
                      public juce::Button::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked (juce::Button* button) override;

private:
    // Componentes principales de la interfaz del DAW
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::Slider masterVolumeSlider;
    juce::Label titleLabel;
    juce::Label volumeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
