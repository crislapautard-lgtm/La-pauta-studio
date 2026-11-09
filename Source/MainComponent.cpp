#include "MainComponent.h"

MainComponent::MainComponent() 
    : vocalTrack("Vocal Principal")
{
    // Hacemos visible la pista en la ventana principal
    addAndMakeVisible(vocalTrack);

    setSize(800, 600);
}

MainComponent::~MainComponent() {}

void MainComponent::paint(juce::Graphics& g)
{
    // Fondo gris oscuro profesional de estudio
    g.fillAll(juce::Colour(0xff1e1e1e));
}

void MainComponent::resized()
{
    // Posicionamos la pista en la esquina superior izquierda de la mezcladora
    vocalTrack.setBounds(20, 20, 120, 450);
}
