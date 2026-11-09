#include "TrackComponent.h"

TrackComponent::TrackComponent(int trackNumber) : trackIndex(trackNumber)
{
    trackLabel.setText("Track " + juce::String(trackIndex), juce::dontSendNotification);
    trackLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(trackLabel);

    muteButton.setButtonText("M");
    muteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    addAndMakeVisible(muteButton);

    soloButton.setButtonText("S");
    soloButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgoldenrod);
    addAndMakeVisible(soloButton);

    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.8);
    addAndMakeVisible(volumeSlider);
}

TrackComponent::~TrackComponent()
{
}

void TrackComponent::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff252525));

    g.setColour(juce::Colour(0xff121212));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());

    g.setColour(juce::Colour(0xff1e1e1e));
    g.fillRect(180, 0, getWidth() - 180, getHeight());

    g.setColour(juce::Colour(0xff333333));
    for (int x = 220; x < getWidth(); x += 80)
        g.drawVerticalLine(x, 0.0f, (float)getHeight());

    g.setColour(juce::Colours::orange);
    g.drawLine(180.0f, (float)(getHeight() / 2), (float)getWidth(), (float)(getHeight() / 3), 2.0f);
}

void TrackComponent::resized()
{
    trackLabel.setBounds(10, 10, 160, 20);
    muteButton.setBounds(10, 35, 30, 25);
    soloButton.setBounds(45, 35, 30, 25);
    volumeSlider.setBounds(80, 37, 90, 20);
}

void TrackComponent::mouseDown (const juce::MouseEvent& event)
{
    if (event.getPosition().x > 180)
    {
        auto clickPos = event.getPosition();
        DBG("Punto de automatización creado en X: " << clickPos.x << " | Y: " << clickPos.y);
    }
}
