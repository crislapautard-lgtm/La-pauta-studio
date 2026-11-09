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

    // Métodos de estado
    float getVolume() const { return (float)volumeSlider.getValue(); }
    float getPan() const { return (float)panSlider.getValue(); }
    bool isMuted() const { return muteButton.getToggleState(); }
    bool isSoloed() const { return soloButton.getToggleState(); }
    bool isRecording() const { return recordButton.getToggleState(); }

    // Funciones callback para automatizaciones y eventos en tiempo real
    std::function<void(float)> onVolumeChanged;
    std::function<void(float)> onPanChanged;
    std::function<bool(bool)> onMuteToggled;
    std::function<bool(bool)> onSoloToggled;
    std::function<bool(bool)> onRecordToggled;

private:
    juce::String name;
    
    juce::Label titleLabel;
    juce::Slider volumeSlider;
    juce::Slider panSlider;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::TextButton recordButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackComponent)
};
