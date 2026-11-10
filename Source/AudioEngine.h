
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>

#include "Track.h"
#include "AudioTrack.h"

// ============================================================
// LA PAUTA STUDIO
// AudioEngine.h
// Motor de mezcla de audio
// ============================================================

class AudioEngine
{
public:

    AudioEngine() = default;

    ~AudioEngine()
    {
        releaseResources();
    }

    // ========================================================
    // PREPARACION
    // ========================================================

    void prepareToPlay(
        int samplesPerBlock,
        double sampleRate,
        int numChannels = 2)
    {
        juce::ignoreUnused(sampleRate);

        const int channels = juce::jmax(1, numChannels);
        const int samples = juce::jmax(1, samplesPerBlock);

        trackBuffer.setSize(
            channels,
            samples,
            false,
            true,
            false
        );

        prepared = true;
    }

    // ========================================================
    // LIBERAR RECURSOS
    // ========================================================

    void releaseResources()
    {
        trackBuffer.setSize(0, 0);
        prepared = false;
    }

    // ========================================================
    // PROCESAMIENTO PRINCIPAL
    // ========================================================

    void processBlock(
        juce::AudioBuffer<float>& buffer,
        juce::MidiBuffer& midiMessages,
        const std::vector<std::unique_ptr<Track>>& tracks)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        if (numChannels <= 0 || numSamples <= 0)
            return;

        // Comprobar que el motor fue preparado.
        // No asignamos memoria durante el procesamiento.
        if (!prepared
            || trackBuffer.getNumChannels() < numChannels
            || trackBuffer.getNumSamples() < numSamples)
        {
            buffer.clear();
            midiMessages.clear();
            return;
        }

        // Guardar la entrada del micrófono antes de limpiar
        // la salida principal.
        // AudioTrack recibe los punteros originales.
        const float* const* inputData =
            buffer.getArrayOfReadPointers();

        for (const auto& track : tracks)
        {
            if (auto* audioTrack =
                dynamic_cast<AudioTrack*>(track.get()))
            {
                if (audioTrack->getIsRecording())
                {
                    audioTrack->processRecording(
                        inputData,
                        numChannels,
                        numSamples
                    );
                }
            }
        }

        // Limpiar la salida para construir la mezcla.
        buffer.clear();

        // Procesar cada pista en un buffer independiente.
        for (const auto& track : tracks)
        {
            if (track == nullptr)
                continue;

            trackBuffer.clear(
                0,
                0,
                numSamples
            );

            juce::MidiBuffer trackMidi;

            track->processBlock(
                trackBuffer,
                trackMidi
            );

            // Incorporar los eventos MIDI al bloque principal.
            for (const auto metadata : trackMidi)
            {
                midiMessages.addEvent(
                    metadata.getMessage(),
                    metadata.samplePosition
                );
            }

            // Sumar el audio de la pista a la mezcla.
            const int channelsToMix =
                juce::jmin(
                    numChannels,
                    trackBuffer.getNumChannels()
                );

            for (int channel = 0;
                 channel < channelsToMix;
                 ++channel)
            {
                buffer.addFrom(
                    channel,
                    0,
                    trackBuffer,
                    channel,
                    0,
                    numSamples
                );
            }
        }
    }

private:

    // Buffer independiente para cada pista.
    juce::AudioBuffer<float> trackBuffer;

    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
