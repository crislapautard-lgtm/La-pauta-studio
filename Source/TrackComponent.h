
#pragma once

#include <JuceHeader.h>
#include <functional>

// ============================================================
// LA PAUTA STUDIO
// TrackComponent.h
// Controles visuales de cada pista
// ============================================================

class TrackComponent : public juce::Component
{
public:

    explicit TrackComponent(const juce::String& name);

    ~TrackComponent() override;

    void paint(juce::Graphics& g) override;

    void resized() override;

    // Callbacks de los controles
    std::function<void(float)> onVolumeChanged;
    std::function<void(float)> onPanChanged;

    std::function<void(bool)> onMuteToggled;
    std::function<void(bool)> onSoloToggled;
    std::function<void(bool)> onRecordToggled;

private:

    juce::String trackName;

    juce::Slider volumeSlider;
    juce::Slider panSlider;

    juce::ToggleButton muteButton { "M" };
    juce::ToggleButton soloButton { "S" };
    juce::ToggleButton recButton { "REC" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackComponent)
};
