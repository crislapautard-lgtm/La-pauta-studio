#pragma once
#include <JuceHeader.h>
#include "TrackComponent.h"
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
    TrackComponent vocalTrack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
