
#pragma once

#include <JuceHeader.h>
#include "Track.h"

class MidiTrack : public Track
{
public:
    explicit MidiTrack(const juce::String& name)
        : Track(name)
    {
    }

    ~MidiTrack() override = default;

    void processBlock(
        juce::AudioBuffer<float>& buffer,
        juce::MidiBuffer& midiMessages) override
    {
        juce::ignoreUnused(buffer);

        if (isMuted())
            return;

        for (const auto metadata : midiClipContent)
        {
            midiMessages.addEvent(
                metadata.getMessage(),
                metadata.samplePosition);
        }
    }

    void addMidiNote(
        int noteNumber,
        float velocity,
        double startBeat,
        double durationBeats,
        double bpm = 120.0,
        double sampleRate = 44100.0)
    {
        noteNumber = juce::jlimit(0, 127, noteNumber);
        velocity = juce::jlimit(0.0f, 1.0f, velocity);
        bpm = juce::jlimit(20.0, 300.0, bpm);
        sampleRate = juce::jmax(1.0, sampleRate);

        startBeat = juce::jmax(0.0, startBeat);
        durationBeats = juce::jmax(0.01, durationBeats);

        const double samplesPerBeat =
            sampleRate * 60.0 / bpm;

        const int startSample = static_cast<int>(
            startBeat * samplesPerBeat);

        const int endSample = static_cast<int>(
            (startBeat + durationBeats) * samplesPerBeat);

        const int midiVelocity = juce::jlimit(
            1, 127,
            static_cast<int>(std::round(velocity * 127.0f)));

        const juce::MidiMessage noteOn =
            juce::MidiMessage::noteOn(
                1, noteNumber,
                static_cast<juce::uint8>(midiVelocity));

        const juce::MidiMessage noteOff =
            juce::MidiMessage::noteOff(1, noteNumber);

        midiClipContent.addEvent(noteOn, startSample);
        midiClipContent.addEvent(noteOff, endSample);
        midiClipContent.sort();
    }

    juce::MidiBuffer& getMidiData()
    {
        return midiClipContent;
    }

    const juce::MidiBuffer& getMidiData() const
    {
        return midiClipContent;
    }

    void clearNotes()
    {
        midiClipContent.clear();
    }

private:
    juce::MidiBuffer midiClipContent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiTrack)
};
