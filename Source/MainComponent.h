#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include "TrackComponent.h"
#include "Track.h"
#include "AudioTrack.h"
#include "MidiTrack.h"

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Función para crear una nueva pista de audio
    void addAudioTrack() {
        tracks.push_back(std::make_unique<AudioTrack>("Pista de Audio " + std::to_string(tracks.size() + 1)));
    }

    // Función para crear una nueva pista MIDI
    void addMidiTrack() {
        tracks.push_back(std::make_unique<MidiTrack>("Pista MIDI " + std::to_string(tracks.size() + 1)));
    }

private:
    // Lista principal que ahora almacena las pistas lógicas
    std::vector<std::unique_ptr<Track>> tracks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
