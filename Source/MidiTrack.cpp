
#include "MidiTrack.h"

// ============================================================
// LA PAUTA STUDIO
// MIDI TRACK - IMPLEMENTACION
// ============================================================

juce::MidiBuffer& MidiTrack::getMidiData()
{
    return midiClipContent;
}

const juce::MidiBuffer& MidiTrack::getMidiData() const
{
    return midiClipContent;
}

void MidiTrack::clearNotes()
{
    midiClipContent.clear();
}

void MidiTrack::setMidiEnabled(bool shouldEnable)
{
    midiEnabled.store(shouldEnable);
}

bool MidiTrack::isMidiEnabled() const
{
    return midiEnabled.load();
}
