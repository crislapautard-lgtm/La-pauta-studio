#include "TrackComponent.h"

TrackComponent::TrackComponent(const juce::String& trackName) : name(trackName)
{
    titleLabel.setText(name, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.8);
    volumeSlider.addListener(this);
    addAndMakeVisible(volumeSlider);

    panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0);
    panSlider.addListener(this);
    addAndMakeVisible(panSlider);

    muteButton.setButtonText("M");
    muteButton.setClickingTogglesState(true);
    muteButton.addListener(this);
    muteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    addAndMakeVisible(muteButton);

    soloButton.setButtonText("S");
    soloButton.setClickingTogglesState(true);
    soloButton.addListener(this);
    soloButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgoldenrod);
    addAndMakeVisible(soloButton);

    recordButton.setButtonText("REC");
    recordButton.setClickingTogglesState(true);
    recordButton.addListener(this);
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::maroon);
    addAndMakeVisible(recordButton);

    setSize(120, 400);
}

TrackComponent::~TrackComponent() {}

void TrackComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2d2d2d));
    g.setColour(juce::Colour(0xff404040));
    g.drawRect(getLocalBounds(), 1);
}

void TrackComponent::resized()
{
    auto area = getLocalBounds().reduced(10);

    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    auto topButtons = area.removeFromTop(25);
    int buttonWidth = topButtons.getWidth() / 3;
    muteButton.setBounds(topButtons.removeFromLeft(buttonWidth).reduced(2));
    soloButton.setBounds(topButtons.removeFromLeft(buttonWidth).reduced(2));
    recordButton.setBounds(topButtons.reduced(2));

    area.removeFromTop(15);
    panSlider.setBounds(area.removeFromTop(60).reduced(20, 0));
    area.removeFromTop(10);
    volumeSlider.setBounds(area);
}

void TrackComponent::buttonClicked(juce::Button* button)
{
    if (button == &muteButton) {
        bool state = muteButton.getToggleState();
        muteButton.setColour(juce::TextButton::buttonColourId, state ? juce::Colours::red : juce::Colours::darkred);
        if (onMuteToggled) onMuteToggled(state);
    }
    else if (button == &soloButton) {
        bool state = soloButton.getToggleState();
        soloButton.setColour(juce::TextButton::buttonColourId, state ? juce::Colours::yellow : juce::Colours::darkgoldenrod);
        if (onSoloToggled) onSoloToggled(state);
    }
    else if (button == &recordButton) {
        bool state = recordButton.getToggleState();
        // Corregido: Usamos recordButton y un color válido de JUCE (green)
        recordButton.setColour(juce::TextButton::buttonColourId, state ? juce::Colours::green : juce::Colours::maroon);
        if (onRecordToggled) onRecordToggled(state);
    }
}

void TrackComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &volumeSlider) {
        float val = (float)volumeSlider.getValue();
        if (onVolumeChanged) onVolumeChanged(val);
    }
    else if (slider == &panSlider) {
        float panVal = (float)panSlider.getValue();
        if (onPanChanged) onPanChanged(panVal);
    }
}
