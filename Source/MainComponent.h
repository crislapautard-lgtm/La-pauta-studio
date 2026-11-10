
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "TrackComponent.h"

#include <memory>
#include <vector>

class MainComponent : public juce::Component
{
public:

    explicit MainComponent(AudioPluginAudioProcessor& processor);

    ~MainComponent() override;

    void paint(juce::Graphics& g) override;

    void resized() override;

private:

    AudioPluginAudioProcessor& audioProcessor;

    juce::Viewport trackViewport;
    juce::Component trackContainer;

    std::vector<std::unique_ptr<TrackComponent>> trackComponents;

    juce::Component masterTrack;

    juce::Label bpmLabel;
    juce::Slider bpmSlider;

    juce::Label masterLabel;
    juce::Slider masterVolumeSlider;

    void createTrackComponents();
    void updateTrackLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
