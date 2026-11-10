#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class MainComponent  : public juce::Component
{
public:
    // El constructor recibe obligatoriamente la referencia al procesador de audio
    MainComponent (AudioPluginAudioProcessor& p) : audioProcessor (p)
    {
        setSize (1000, 700);
    }

    ~MainComponent() override {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
        
        // Ejemplo de uso correcto accediendo a las variables del procesador:
        g.setColour (juce::Colours::white);
        g.setFont (14.0f);
        g.drawText ("La-Pauta-Studio - Tracks: " + juce::String (audioProcessor.numTracks), 20, 20, 300, 30, juce::Justification::left);
    }

    void resized() override
    {
    }

private:
    // Referencia al procesador para acceder a volúmenes, pistas, BPM, etc.
    AudioPluginAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
