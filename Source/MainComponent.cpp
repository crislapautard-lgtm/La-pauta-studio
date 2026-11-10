#include "MainComponent.h"

MainComponent::MainComponent (AudioPluginAudioProcessor& p) : audioProcessor (p)
{
    setSize (1000, 700);

    addAndMakeVisible (bpmSlider);
    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (audioProcessor.currentBPM);
    
    bpmLabel.setText ("BPM", juce::dontSendNotification);
    addAndMakeVisible (bpmLabel);
}

MainComponent::~MainComponent()
{
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawText ("La-Pauta-Studio (16-Track DAW)", 20, 10, 400, 30, juce::Justification::left);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    
    bpmLabel.setBounds (area.removeFromTop (40).removeFromLeft (60).reduced (5));
    bpmSlider.setBounds (area.removeFromTop (40).removeFromLeft (120).reduced (5));

    // ACCESO CORRECTO CON audioProcessor. (Asegúrate de mantenerlo así en todo el archivo)
    for (int i = 0; i < audioProcessor.numTracks; ++i)
    {
        // Lógica de pistas
    }
    
    for (int i = 0; i < audioProcessor.numSubgroups; ++i)
    {
        // Lógica de subgrupos
    }

    for (int i = 0; i < audioProcessor.numAuxes; ++i)
    {
        // Lógica de canales aux
    }
}
