#pragma once
#include <JuceHeader.h>
struct PautaPreset { juce::String name; float eqLow, eqMid, eqHigh; float compThresh; float reverbMix; float delayMix; float autoTune; };
class PresetManager
{
public:
    static juce::Array<PautaPreset> getPresets()
    {
        juce::Array<PautaPreset> p;
        p.add({"VOZ DEMBOW LIMPIA", 2.0f, 1.0f, 3.0f, 0.5f, 0.2f, 0.0f, 0.6f});
        p.add({"VOZ TRAP OSCURA", -1.0f, -2.0f, 1.0f, 0.4f, 0.35f, 0.2f, 0.9f});
        p.add({"VOZ DRILL AGRESIVA", 3.0f, 2.0f, 4.0f, 0.3f, 0.15f, 0.1f, 0.4f});
        p.add({"VOZ R&B SUAVE", 1.0f, 0.0f, 2.0f, 0.6f, 0.45f, 0.15f, 0.3f});
        p.add({"CORO IGLESIA", 1.0f, 1.0f, 2.0f, 0.6f, 0.6f, 0.3f, 0.0f});
        return p;
    }
};
