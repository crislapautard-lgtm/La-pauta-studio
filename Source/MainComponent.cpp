#include "MainComponent.h"
#include "PluginProcessor.h"

MainComponent::MainComponent() 
    : vocalTrack("Vocal Principal")
{
    // Esto hace que al mover el fader en la pantalla, el motor de audio baje o suba el volumen real
    vocalTrack.onVolumeChanged = [this](float newVolume) {
        // Aquí conectamos con el procesador
    };

    addAndMakeVisible(vocalTrack);
    setSize(800, 600);
}

MainComponent::~MainComponent() {}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));
}

void MainComponent::resized()
{
    vocalTrack.setBounds(20, 20, 120, 450);
}
