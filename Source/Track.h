
#pragma once

#include <JuceHeader.h>

// ============================================================
// LA PAUTA STUDIO
// Track.h
// Clase base para todas las pistas
// ============================================================

class Track
{
public:

    explicit Track(const juce::String& name)
        : trackName(name)
    {
    }

    virtual ~Track() = default;

    // ========================================================
    // NOMBRE
    // ========================================================

    juce::String getTrackName() const
    {
        return trackName;
    }

    void setTrackName(const juce::String& newName)
    {
        trackName = newName;
    }

    // ========================================================
    // MUTE
    // ========================================================

    virtual bool isMuted() const
    {
        return muted;
    }

    virtual void setMuted(bool shouldMute)
    {
        muted = shouldMute;
    }

    // ========================================================
    // PROCESAMIENTO
    // ========================================================

    virtual void processBlock(
        juce::AudioBuffer<float>& buffer,
        juce::MidiBuffer& midiMessages
    ) = 0;

protected:

    juce::String trackName;

    bool muted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};
