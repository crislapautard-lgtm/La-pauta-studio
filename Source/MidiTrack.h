#pragma once
#include <JuceHeader.h>
#include "Track.h"

class MidiTrack : public Track {
public:
    MidiTrack(const juce::String& name) : Track(name) {}
    ~MidiTrack() override = default;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        if (muted) return;

        // Inyectamos los mensajes MIDI de las notas dibujadas hacia el buffer de procesamiento
        for (const auto metadata : midiClipContent) {
            midiMessages.addEvent(metadata.getMessage(), metadata.samplePosition);
        }
    }

    void addMidiNote(int noteNumber, float velocity, double startBeat, double durationBeats) {
        // Creamos una nota MIDI estándar (Nota encendida y nota apagada)
        juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, noteNumber, (uint8)(velocity * 127.0f));
        juce::MidiMessage noteOff = juce::MidiMessage::noteOff(1, noteNumber);

        // Calculamos posiciones basadas en un tempo estándar de 120 BPM (muestras por beat a 44.1kHz)
        int startSample = (int)(startBeat * 0.5 * 44100.0); 
        int endSample = (int)((startBeat + durationBeats) * 0.5 * 44100.0);

        midiClipContent.addEvent(noteOn, startSample);
        midiClipContent.addEvent(noteOff, endSample);
        midiClipContent.sort();
    }

    juce::MidiBuffer& getMidiData() { return midiClipContent; }

    void clearNotes() { midiClipContent.clear(); }

private:
    juce::MidiBuffer midiClipContent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiTrack)
};
