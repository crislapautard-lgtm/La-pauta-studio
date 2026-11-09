#pragma once
#include <JuceHeader.h>

class TrackComponent : public juce::Component,
                       public juce::Button::Listener,
                       public juce::Slider::Listener
{
public:
    TrackComponent(const juce::String& trackName);
    ~TrackComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;

private:
    juce::String name;
    
    // Controles visuales de la pista
    juce::Label titleLabel;
    juce::Slider volumeSlider;
    juce::Slider panSlider;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::TextButton recordButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackComponent)
};
