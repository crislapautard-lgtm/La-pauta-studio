#pragma once
#include <JuceHeader.h>

class Track {
public:
    Track(const juce::String& name) : trackName(name) {}
    virtual ~Track() = default;

    juce::String getTrackName() const { return trackName; }
    void setTrackName(const juce::String& newName) { trackName = newName; }

    bool isMuted() const { return muted; }
    void setMuted(bool shouldMute) { muted = shouldMute; }

    // Método virtual para procesar audio o eventos en el bucle en tiempo real
    virtual void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) = 0;

protected:
    juce::String trackName;
    bool muted = false;
};
