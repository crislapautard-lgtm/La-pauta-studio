#pragma once

#include <JuceHeader.h>

class TrackComponent : public juce::Component
{
public:
    TrackComponent(int trackNumber);
    ~TrackComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    int trackIndex;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::Slider volumeSlider;
    juce::Label trackLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackComponent)
};
