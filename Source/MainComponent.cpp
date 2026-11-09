#include "MainComponent.h"

MainComponent::MainComponent(AudioPluginAudioProcessor& p) 
    : audioProcessor(p), vocalTrack("Vocal Principal")
{
    // Conectamos los faders y botones de la UI directamente al procesador de audio en tiempo real
    vocalTrack.onVolumeChanged = [this](float newVolume) {
        audioProcessor.trackVolume.store(newVolume);
    };

    vocalTrack.onPanChanged = [this](float newPan) {
        audioProcessor.trackPan.store(newPan);
    };

    vocalTrack.onMuteToggled = [this](bool isMuted) {
        audioProcessor.trackMute.store(isMuted);
        return isMuted;
    };

    vocalTrack.onSoloToggled = [this](bool isSoloed) {
        audioProcessor.trackSolo.store(isSoloed);
        return isSoloed;
    };

    vocalTrack.onRecordToggled = [this](bool isRec) {
        audioProcessor.trackRec.store(isRec);
        return isRec;
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
