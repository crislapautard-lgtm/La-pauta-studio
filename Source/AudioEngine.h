
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>

#include "Track.h"
#include "AudioTrack.h"

// ============================================================
// LA PAUTA STUDIO
// AUDIO ENGINE
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
    // PREPARAR MOTOR
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

        trackBuffer.clear();
        prepared = true;
    }

    // ========================================================
    // LIBERAR RECURSOS
    // ========================================================

    void releaseResources()
    {
        prepared = false;
        trackBuffer.setSize(0, 0);
    }

    // ========================================================
    // PROCESAR AUDIO
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

        if (!prepared
            || trackBuffer.getNumChannels() < numChannels
            || trackBuffer.getNumSamples() < numSamples)
        {
            buffer.clear();
            midiMessages.clear();
            return;
        }

        // Guardar entrada antes de limpiar la mezcla.
        const float* const* inputData =
            buffer.getArrayOfReadPointers();

        // Enviar entrada a las pistas que estén grabando.
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

        // Construir mezcla desde cero.
        buffer.clear();

        // Procesar cada pista individualmente.
        for (const auto& track : tracks)
        {
            if (track == nullptr)
                continue;

            // Limpiar TODOS los canales del buffer temporal.
            trackBuffer.clear();

            // Buffer MIDI temporal de la pista.
            juce::MidiBuffer trackMidi;

            track->processBlock(
                trackBuffer,
                trackMidi
            );

            // Transferir MIDI al bloque principal.
            for (const auto metadata : trackMidi)
            {
                midiMessages.addEvent(
                    metadata.getMessage(),
                    metadata.samplePosition
                );
            }

            // Sumar audio de la pista a la mezcla.
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

    juce::AudioBuffer<float> trackBuffer;

    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
