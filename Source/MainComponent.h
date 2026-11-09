#pragma once
#include <JuceHeader.h>
#include "TrackComponent.h"

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Nuestra primera pista de prueba en la mezcladora
    TrackComponent vocalTrack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
