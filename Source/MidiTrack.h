
#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "Track.h"

// ============================================================
// LA PAUTA STUDIO
// MIDI TRACK - FASE 1
// ============================================================

class MidiTrack : public Track
{
public:

    explicit MidiTrack(const juce::String& name)
        : Track(name)
    {
    }

    ~MidiTrack() override = default;

    // ========================================================
    // PROCESAMIENTO
    // ========================================================

    void processBlock(
        juce::AudioBuffer<float>& buffer,
        juce::MidiBuffer& midiMessages) override
    {
        juce::ignoreUnused(buffer);

        if (isMuted())
            return;

        // No reproducir todo el contenido MIDI en cada bloque.
        // La programación temporal global se conectará
        // posteriormente con el transporte del DAW.
    }

    // ========================================================
    // AGREGAR NOTA
    // ========================================================

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

        const int startSample =
            juce::roundToInt(startBeat * samplesPerBeat);

        const int endSample =
            juce::roundToInt(
                (startBeat + durationBeats) * samplesPerBeat);

        const int midiVelocity =
            juce::jlimit(
                1,
                127,
                juce::roundToInt(velocity * 127.0f));

        const auto noteOn = juce::MidiMessage::noteOn(
            1,
            noteNumber,
            static_cast<juce::uint8>(midiVelocity));

        const auto noteOff = juce::MidiMessage::noteOff(
            1,
            noteNumber);

        midiClipContent.addEvent(noteOn, startSample);
        midiClipContent.addEvent(noteOff, endSample);
    }

    // ========================================================
    // DATOS MIDI
    // ========================================================

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

    // ========================================================
    // ESTADO
    // ========================================================

    void setMidiEnabled(bool enabled)
    {
        midiEnabled.store(
            enabled,
            std::memory_order_relaxed);
    }

    bool isMidiEnabled() const
    {
        return midiEnabled.load(
            std::memory_order_relaxed);
    }

private:

    juce::MidiBuffer midiClipContent;

    std::atomic<bool> midiEnabled { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiTrack)
};
