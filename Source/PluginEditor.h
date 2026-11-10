
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MainComponent.h"

class AudioPluginAudioProcessorEditor
    : public juce::AudioProcessorEditor
{
public:

    explicit AudioPluginAudioProcessorEditor(
        AudioPluginAudioProcessor& processor)
        : juce::AudioProcessorEditor(&processor),
          audioProcessor(processor),
          mainComponent(processor)
    {
        addAndMakeVisible(mainComponent);

        setSize(1000, 700);
        setResizable(true, true);
        setResizeLimits(800, 600, 1600, 1200);
    }

    ~AudioPluginAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff10141c));
    }

    void resized() override
    {
        mainComponent.setBounds(getLocalBounds());
    }

private:

    AudioPluginAudioProcessor& audioProcessor;
    MainComponent mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        AudioPluginAudioProcessorEditor)
};
