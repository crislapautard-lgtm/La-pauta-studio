#include "MainComponent.h"

MainComponent::MainComponent(AudioPluginAudioProcessor& p) : audioProcessor(p)
{
    TrackComponent* tracks[4] = { &track1, &track2, &track3, &track4 };

    for (int i = 0; i < 4; ++i)
    {
        tracks[i]->onVolumeChanged = [this, i](float val) { audioProcessor.trackVolume[i].store(val); };
        tracks[i]->onPanChanged    = [this, i](float val) { audioProcessor.trackPan[i].store(val); };
        tracks[i]->onMuteToggled   = [this, i](bool val)  { audioProcessor.trackMute[i].store(val); return val; };
        tracks[i]->onSoloToggled   = [this, i](bool val)  { audioProcessor.trackSolo[i].store(val); return val; };
        tracks[i]->onRecordToggled = [this, i](bool val)  { audioProcessor.trackRec[i].store(val); return val; };

        addAndMakeVisible(tracks[i]);
    }

    // Configuración del Master
    masterTrack.onVolumeChanged = [this](float val) { audioProcessor.masterVolume.store(val); };
    addAndMakeVisible(masterTrack);

    setSize(700, 500);
}

MainComponent::~MainComponent() {}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(15);
    int trackWidth = 120;
    int spacing = 15;

    // Distribuimos las 4 pistas y dejamos un espacio para el Master al final
    TrackComponent* tracks[4] = { &track1, &track2, &track3, &track4 };
    for (int i = 0; i < 4; ++i)
    {
        tracks[i]->setBounds(area.removeFromLeft(trackWidth));
        area.removeFromLeft(spacing);
    }

    // Canal Master separado a la derecha
    area.removeFromLeft(20);
    masterTrack.setBounds(area.removeFromLeft(trackWidth));
}
