#pragma once
#include "Track.h"

class AudioTrack : public Track {
public:
    AudioTrack(const juce::String& name) : Track(name) {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        if (muted) {
            buffer.clear();
            return;
        }
        // Aquí procesarás o mezclarás el audio de esta pista
    }
};
