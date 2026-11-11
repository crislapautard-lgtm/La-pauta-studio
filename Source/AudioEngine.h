
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>

#include "Track.h"
#include "AudioTrack.h"

// ============================================================
// LA PAUTA STUDIO
// AUDIO ENGINE - FASE 1
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
        currentSampleRate = juce::jmax(1.0, sampleRate);

        const int channels = juce::jmax(1, numChannels);
        const int samples = juce::jmax(1, samplesPerBlock);

        trackBuffer.setSize(
            channels,
            samples,
            false,
            true,
            false);

        trackBuffer.clear();
        trackMidiBuffer.clear();

        prepared.store(true, std::memory_order_release);
    }

    // ========================================================
    // LIBERAR RECURSOS
    // ========================================================

    void releaseResources()
    {
        prepared.store(false, std::memory_order_release);

        trackBuffer.setSize(0, 0);
        trackMidiBuffer.clear();
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

        if (!prepared.load(std::memory_order_acquire))
        {
            buffer.clear();
            midiMessages.clear();
            return;
        }

        if (trackBuffer.getNumChannels() < numChannels ||
            trackBuffer.getNumSamples() < numSamples)
        {
            buffer.clear();
            midiMessages.clear();
            return;
        }

        // ====================================================
        // 1. CAPTURAR ENTRADA ANTES DE LIMPIAR LA SALIDA
        // ====================================================

        const float* const* inputData =
            buffer.getArrayOfReadPointers();

        for (const auto& track : tracks)
        {
            if (track == nullptr)
                continue;

            if (auto* audioTrack =
                dynamic_cast<AudioTrack*>(track.get()))
            {
                if (audioTrack->getIsRecording())
                {
                    audioTrack->processRecording(
                        inputData,
                        numChannels,
                        numSamples);
                }
            }
        }

        // ====================================================
        // 2. DETECTAR SI EXISTE ALGUNA PISTA EN SOLO
        // ====================================================

        bool anySolo = false;

        for (const auto& track : tracks)
        {
            if (track == nullptr)
                continue;

            if (auto* audioTrack =
                dynamic_cast<AudioTrack*>(track.get()))
            {
                if (audioTrack->isSoloed())
                {
                    anySolo = true;
                    break;
                }
            }
        }

        // ====================================================
        // 3. PREPARAR SALIDA
        // ====================================================

        buffer.clear();

        // ====================================================
        // 4. MEZCLAR PISTAS
        // ====================================================

        for (const auto& track : tracks)
        {
            if (track == nullptr)
                continue;

            trackBuffer.clear();
            trackMidiBuffer.clear();

            // MUTE
            if (track->isMuted())
                continue;

            // SOLO
            if (anySolo)
            {
                auto* audioTrack =
                    dynamic_cast<AudioTrack*>(track.get());

                if (audioTrack == nullptr ||
                    !audioTrack->isSoloed())
                {
                    continue;
                }
            }

            // PROCESAR PISTA
            track->processBlock(
                trackBuffer,
                trackMidiBuffer);

            // MEZCLAR MIDI
            for (const auto metadata : trackMidiBuffer)
            {
                midiMessages.addEvent(
                    metadata.getMessage(),
                    metadata.samplePosition);
            }

            // MEZCLAR AUDIO
            const int channelsToMix =
                juce::jmin(
                    numChannels,
                    trackBuffer.getNumChannels());

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
                    numSamples);
            }
        }
    }

    double getSampleRate() const
    {
        return currentSampleRate;
    }

    bool isPrepared() const
    {
        return prepared.load(std::memory_order_acquire);
    }

private:

    juce::AudioBuffer<float> trackBuffer;

    juce::MidiBuffer trackMidiBuffer;

    std::atomic<bool> prepared { false };

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
