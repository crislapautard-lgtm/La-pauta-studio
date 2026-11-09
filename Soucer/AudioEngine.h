#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include "Track.h"

class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine() = default;

    // Método que llama la tarjeta de sonido en tiempo real
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, const std::vector<std::unique_ptr<Track>>& tracks) {
        // Limpiamos el buffer principal antes de sumar las pistas
        buffer.clear();

        // Recorremos todas tus pistas lógicas y ejecutamos su procesamiento
        for (const auto& track : tracks) {
            if (track) {
                track->processBlock(buffer, midiMessages);
            }
        }
    }
};
