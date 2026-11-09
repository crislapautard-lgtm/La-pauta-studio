#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include "Track.h"

class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine() = default;

    // Método que llama la tarjeta de sonido en tiempo real (maneja entrada y salida)
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, const std::vector<std::unique_ptr<Track>>& tracks) {
        
        // 1. CAPTURA DE ENTRADA (GRABACIÓN):
        // Guardamos o procesamos temporalmente el audio que viene del micrófono (hardware input)
        // antes de limpiar el buffer para la salida general.
        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();

        // Creamos un buffer auxiliar en tiempo real para capturar la señal de entrada del micro
        juce::AudioBuffer<float> inputBuffer;
        inputBuffer.makeCopyOf(buffer); // Copiamos el audio crudo de entrada

        // 2. Limpiamos el buffer principal para construir la mezcla de salida de las pistas
        buffer.clear();

        // 3. Recorremos todas tus pistas lógicas
        for (const auto& track : tracks) {
            if (track) {
                // Aquí en el futuro, si la pista está en modo "Record Armed", 
                // le inyectaremos los datos de 'inputBuffer' para que los guarde a disco.
                
                // Procesamiento normal de reproducción/mezcla de la pista
                track->processBlock(buffer, midiMessages);
            }
        }
    }
};
