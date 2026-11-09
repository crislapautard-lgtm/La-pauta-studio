#include "MainComponent.h"
#include "TrackComponent.h"
#include "AudioTrack.h"
#include "MidiTrack.h"

MainComponent::MainComponent()
{
    // Creamos 3 pistas iniciales por defecto para probar
    for (int i = 1; i <= 3; ++i)
    {
        // 1. Creamos la pista lógica (audio)
        auto logicalTrack = std::make_unique<AudioTrack>("Pista " + juce::String(i));
        tracks.push_back(std::move(logicalTrack));

        // 2. Creamos su cajita visual correspondiente en la pantalla
        auto trackComp = std::make_unique<TrackComponent>(i);
        addAndMakeVisible(trackComp.get());
        trackComponents.push_back(std::move(trackComp));
    }

    setSize (1000, 700);
}

MainComponent::~MainComponent()
{
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));
}

void MainComponent::resized()
{
    int startY = 10;
    int trackHeight = 70;

    // Acomodamos las cajitas visuales en la pantalla ordenadamente
    for (auto& trackComp : trackComponents)
    {
        trackComp->setBounds(10, startY, getWidth() - 20, trackHeight);
        startY += trackHeight + 5;
    }
}
