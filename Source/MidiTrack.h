#pragma once
#include "Track.h"

class MidiTrack : public Track {
public:
    MidiTrack(const juce::String& name) : Track(name) {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        if (muted) return;
        // Aquí inyectarás o procesarás los eventos MIDI de esta pista hacia los instrumentos
    }
};
