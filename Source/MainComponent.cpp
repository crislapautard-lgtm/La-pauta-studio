#include "MainComponent.h"

MainComponent::MainComponent(AudioPluginAudioProcessor& p) : audioProcessor(p)
{
    for (int i = 0; i < AudioPluginAudioProcessor::numTracks; ++i)
    {
        auto trackName = "Vocal " + juce::String(i + 1);
        auto trackComp = std::make_unique<TrackComponent>(trackName);

        trackComp->onVolumeChanged = [this, i](float val) { audioProcessor.trackVolume[i].store(val); };
        trackComp->onPanChanged    = [this, i](float val) { audioProcessor.trackPan[i].store(val); };
        trackComp->onMuteToggled   = [this, i](bool val)  { audioProcessor.trackMute[i].store(val); return val; };
        trackComp->onSoloToggled   = [this, i](bool val)  { audioProcessor.trackSolo[i].store(val); return val; };
        trackComp->onRecordToggled = [this, i](bool val)  { audioProcessor.trackRec[i].store(val); return val; };

        trackContainer.addAndMakeVisible(trackComp.get());
        tracks.push_back(std::move(trackComp));
    }

    masterTrack.onVolumeChanged = [this](float val) { audioProcessor.masterVolume.store(val); };
    trackContainer.addAndMakeVisible(masterTrack);

    viewport.setViewedComponent(&trackContainer, false);
    viewport.setScrollBarsShown(true, true);
    addAndMakeVisible(viewport);

    setSize(900, 520);
}

MainComponent::~MainComponent() {}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));
}

void MainComponent::resized()
{
    viewport.setBounds(getLocalBounds());

    int trackWidth = 120;
    int spacing = 10;
    int totalWidth = (AudioPluginAudioProcessor::numTracks * (trackWidth + spacing)) + trackWidth + 40;

    trackContainer.setBounds(0, 0, totalWidth, getHeight() - 20);

    int xPos = 15;
    for (int i = 0; i < AudioPluginAudioProcessor::numTracks; ++i)
    {
        tracks[i]->setBounds(xPos, 15, trackWidth, 450);
        xPos += trackWidth + spacing;
    }

    xPos += 20;
    masterTrack.setBounds(xPos, 15, trackWidth, 450);
}
