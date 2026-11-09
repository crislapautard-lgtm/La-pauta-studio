#pragma once
#include <JuceHeader.h>

class TrackComponent : public juce::Component
{
public:
    TrackComponent(const juce::String& name);
    ~TrackComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void(float)> onVolumeChanged;
    std::function<void(float)> onPanChanged;
    std::function<bool(bool)>  onMuteToggled;
    std::function<bool(bool)>  onSoloToggled;
    std::function<bool(bool)>  onRecordToggled;

private:
    juce::String trackName;
    juce::Slider volumeSlider;
    juce::Slider panSlider;
    juce::ToggleButton muteButton { "M" };
    juce::ToggleButton soloButton { "S" };
    juce::ToggleButton recButton  { "REC" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackComponent)
};
