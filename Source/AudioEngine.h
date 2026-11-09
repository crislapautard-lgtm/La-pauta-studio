#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include "Track.h"
#include "AudioTrack.h" // Importante para reconocer métodos específicos de grabación

class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine() = default;

    // Método que llama la tarjeta de sonido en tiempo real
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, const std::vector<std::unique_ptr<Track>>& tracks) {
        
        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();

        // 1. Copiamos el audio crudo que entra por el micrófono (hardware input)
        juce::AudioBuffer<float> inputBuffer;
        inputBuffer.makeCopyOf(buffer);

        // 2. Limpiamos el buffer principal para construir la mezcla de salida de las pistas
        buffer.clear();

        // 3. Recorremos todas las pistas lógicas
        for (const auto& track : tracks) {
            if (track) {
                // Verificamos si la pista es de audio y si está grabando
                if (auto* audioTrack = dynamic_cast<AudioTrack*>(track.get())) {
                    if (audioTrack->getIsRecording()) {
                        // Enviamos los datos del micrófono directamente al escritor de la pista
                        audioTrack->processRecording(inputBuffer.getArrayOfReadPointers(), numChannels, numSamples);
                    }
                }

                // Procesamiento normal de reproducción/mezcla de la pista
                track->processBlock(buffer, midiMessages);
            }
        }
    }
};
