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
    
    // 4 Pistas en la consola
    TrackComponent track1 { "Vocal" };
    TrackComponent track2 { "Guitarra" };
    TrackComponent track3 { "Bajo" };
    TrackComponent track4 { "Bateria" };

    // Fader Master visual
    TrackComponent masterTrack { "MASTER" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
