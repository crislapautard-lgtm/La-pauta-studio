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
    
    juce::Viewport viewport;
    juce::Component trackContainer;

    std::vector<std::unique_ptr<TrackComponent>> tracks;
    
    // Controles visuales para Subgrupos y Auxiliares
    std::vector<std::unique_ptr<TrackComponent>> subgroups;
    std::vector<std::unique_ptr<TrackComponent>> auxes;
    TrackComponent masterTrack { "MASTER" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
