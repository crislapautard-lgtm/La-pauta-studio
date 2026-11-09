#pragma once
#include <JuceHeader.h>
#include "AudioTrack.h"

class TrackComponent : public juce::Component,
                       public juce::Button::Listener
{
public:
    TrackComponent(int trackId, AudioTrack* audioTrack)
        : id(trackId), targetAudioTrack(audioTrack)
    {
        // Botón de Record Arm (REC)
        recordButton.setButtonText("REC");
        recordButton.setClickingTogglesState(true);
        recordButton.addListener(this);
        recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        recordButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
        addAndMakeVisible(recordButton);
    }

    ~TrackComponent() override {
        recordButton.removeListener(this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff2d2d2d));
        g.setColour(juce::Colours::white);
        g.drawRect(getLocalBounds(), 1);
        g.drawText("Pista " + juce::String(id), 10, 10, 100, 20, juce::Justification::left);
    }

    void resized() override
    {
        recordButton.setBounds(getWidth() - 60, 15, 50, 40);
    }

    void buttonClicked (juce::Button* button) override
    {
        if (button == &recordButton)
        {
            if (recordButton.getToggleState())
            {
                // Archivo .wav que se guardará en tu Escritorio al presionar REC
                auto desktopDir = juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
                auto wavFile = desktopDir.getChildFile("Pista_" + juce::String(id) + ".wav");

                if (targetAudioTrack)
                {
                    targetAudioTrack->startRecording(wavFile, 44100.0);
                }
            }
            else
            {
                if (targetAudioTrack)
                {
                    targetAudioTrack->stopRecording();
                }
            }
        }
    }

private:
    int id;
    AudioTrack* targetAudioTrack = nullptr;
    juce::TextButton recordButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackComponent)
};
