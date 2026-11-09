#include "TrackComponent.h"

TrackComponent::TrackComponent(const juce::String& name) : trackName(name)
{
    volumeSlider.setSliderStyle(juce::Slider::LinearBarVertical);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.8);
    volumeSlider.onValueChange = [this] {
        if (onVolumeChanged)
            onVolumeChanged((float)volumeSlider.getValue());
    };
    addAndMakeVisible(volumeSlider);

    panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0);
    panSlider.onValueChange = [this] {
        if (onPanChanged)
            onPanChanged((float)panSlider.getValue());
    };
    addAndMakeVisible(panSlider);

    muteButton.onClick = [this] {
        if (onMuteToggled)
            onMuteToggled(muteButton.getToggleState());
    };
    addAndMakeVisible(muteButton);

    soloButton.onClick = [this] {
        if (onSoloToggled)
            onSoloToggled(soloButton.getToggleState());
    };
    addAndMakeVisible(soloButton);

    recButton.onClick = [this] {
        if (onRecordToggled)
            onRecordToggled(recButton.getToggleState());
    };
    addAndMakeVisible(recButton);
}

TrackComponent::~TrackComponent() {}

void TrackComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    g.drawText(trackName, 0, 5, getWidth(), 20, juce::Justification::centred, true);
}

void TrackComponent::resized()
{
    auto bounds = getLocalBounds().reduced(5);
    bounds.removeFromTop(25);

    recButton.setBounds(bounds.removeFromTop(22));
    bounds.removeFromTop(5);
    
    auto muteSolo = bounds.removeFromTop(22);
    muteButton.setBounds(muteSolo.removeFromLeft(muteSolo.getWidth() / 2));
    soloButton.setBounds(muteSolo);

    bounds.removeFromTop(5);
    panSlider.setBounds(bounds.removeFromTop(50));

    bounds.removeFromTop(5);
    volumeSlider.setBounds(bounds);
}
