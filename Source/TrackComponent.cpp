
#include "TrackComponent.h"

TrackComponent::TrackComponent(const juce::String& name)
    : trackName(name)
{
    setSize(110, 260);

    volumeSlider.setSliderStyle(juce::Slider::LinearBarVertical);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 18);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.8);
    volumeSlider.setTextValueSuffix(" Vol");

    volumeSlider.onValueChange = [this]
    {
        if (onVolumeChanged)
            onVolumeChanged(static_cast<float>(volumeSlider.getValue()));
    };

    addAndMakeVisible(volumeSlider);

    panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 18);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0);
    panSlider.setTextValueSuffix(" Pan");

    panSlider.onValueChange = [this]
    {
        if (onPanChanged)
            onPanChanged(static_cast<float>(panSlider.getValue()));
    };

    addAndMakeVisible(panSlider);

    muteButton.setClickingTogglesState(true);
    muteButton.onClick = [this]
    {
        if (onMuteToggled)
            onMuteToggled(muteButton.getToggleState());
    };
    addAndMakeVisible(muteButton);

    soloButton.setClickingTogglesState(true);
    soloButton.onClick = [this]
    {
        if (onSoloToggled)
            onSoloToggled(soloButton.getToggleState());
    };
    addAndMakeVisible(soloButton);

    recButton.setClickingTogglesState(true);
    recButton.onClick = [this]
    {
        if (onRecordToggled)
            onRecordToggled(recButton.getToggleState());
    };
    addAndMakeVisible(recButton);
}

TrackComponent::~TrackComponent() = default;

void TrackComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff202735));

    g.setColour(juce::Colour(0xff263b55));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 8.0f);

    g.setColour(juce::Colour(0xff55c8ff));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 8.0f, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText(trackName, 4, 5, getWidth() - 8, 20,
               juce::Justification::centred, true);
}

void TrackComponent::resized()
{
    auto bounds = getLocalBounds().reduced(6);
    bounds.removeFromTop(25);

    recButton.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(5);

    auto muteSolo = bounds.removeFromTop(24);
    const int halfWidth = muteSolo.getWidth() / 2;

    muteButton.setBounds(muteSolo.removeFromLeft(halfWidth));
    soloButton.setBounds(muteSolo);

    bounds.removeFromTop(5);
    panSlider.setBounds(bounds.removeFromTop(75));

    bounds.removeFromTop(5);
    volumeSlider.setBounds(bounds);
}
