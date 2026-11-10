
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class MainComponent : public juce::Component
{
public:

    MainComponent(AudioPluginAudioProcessor& p);

    ~MainComponent() override;

    void paint(juce::Graphics& g) override;

    void resized() override;

private:

    AudioPluginAudioProcessor& audioProcessor;

    juce::Component trackContainer;

    juce::OwnedArray<juce::Component> tracks;
    juce::OwnedArray<juce::Component> subgroups;
    juce::OwnedArray<juce::Component> auxes;

    juce::Component masterTrack;

    juce::Label bpmLabel;
    juce::Slider bpmSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
