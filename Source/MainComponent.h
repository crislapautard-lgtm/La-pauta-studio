#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class MainComponent  : public juce::Component
{
public:
    MainComponent (AudioPluginAudioProcessor& p) : audioProcessor (p)
    {
        setSize (1000, 700);

        // Inicialización básica de componentes de la interfaz del DAW
        addAndMakeVisible (bpmSlider);
        bpmSlider.setRange (40.0, 240.0, 1.0);
        bpmSlider.setValue (audioProcessor.currentBPM);
        
        bpmLabel.setText ("BPM", juce::dontSendNotification);
        addAndMakeVisible (bpmLabel);
    }

    ~MainComponent() override {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
        
        g.setColour (juce::Colours::white);
        g.setFont (16.0f);
        g.drawText ("La-Pauta-Studio (16-Track DAW)", 20, 10, 400, 30, juce::Justification::left);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        
        // Posicionamiento de ejemplo para los controles de transporte / BPM
        bpmLabel.setBounds (area.removeFromTop (40).removeFromLeft (60).reduced (5));
        bpmSlider.setBounds (area.removeFromTop (40).removeFromLeft (120).reduced (5));

        // Acceso correcto usando audioProcessor.numTracks, audioProcessor.numSubgroups, etc.
        for (int i = 0; i < audioProcessor.numTracks; ++i)
        {
            // Lógica de diseño para las pistas del DAW
        }
        
        for (int i = 0; i < audioProcessor.numSubgroups; ++i)
        {
            // Lógica de diseño para los subgrupos
        }

        for (int i = 0; i < audioProcessor.numAuxes; ++i)
        {
            // Lógica de diseño para los canales Aux
        }
    }

private:
    // Referencia al procesador de audio
    AudioPluginAudioProcessor& audioProcessor;

    // Componentes de la interfaz que causaban error por no estar declarados:
    juce::Component trackContainer;
    juce::OwnedArray<juce::Component> tracks;
    juce::OwnedArray<juce::Component> subgroups;
    juce::OwnedArray<juce::Component> auxes;
    juce::Component masterTrack;
    
    juce::Label bpmLabel;
    juce::Slider bpmSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
