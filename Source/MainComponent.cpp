#include "MainComponent.h"
#include "TrackComponent.h"

MainComponent::MainComponent()
{
    for (int i = 1; i <= 3; ++i)
    {
        auto track = std::make_unique<TrackComponent>(i);
        addAndMakeVisible(track.get());
        tracks.push_back(std::move(track));
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

    for (auto& track : tracks)
    {
        track->setBounds(10, startY, getWidth() - 20, trackHeight);
        startY += trackHeight + 5;
    }
}
