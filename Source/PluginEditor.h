#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "MainComponent.h"

class LaPautaStudioAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    LaPautaStudioAudioProcessorEditor (LaPautaStudioAudioProcessor& p)
        : AudioProcessorEditor (&p), audioProcessor (p)
    {
        addAndMakeVisible (mainComponent);
        setSize (1000, 700);
    }

    ~LaPautaStudioAudioProcessorEditor() override {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        mainComponent.setBounds (getLocalBounds());
    }

private:
    LaPautaStudioAudioProcessor& audioProcessor;
    MainComponent mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaPautaStudioAudioProcessorEditor)
};
