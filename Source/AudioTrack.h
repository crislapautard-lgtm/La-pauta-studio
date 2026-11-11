
#pragma once

#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <cmath>
#include <string>

#include "Track.h"

// ============================================================
// LA PAUTA STUDIO
// AUDIOTRACK - FASE 1
// ============================================================

class AudioTrack : public Track
{
public:

    explicit AudioTrack(const std::string& name)
        : Track(juce::String(name)),
          readAheadThread("La Pauta Read Ahead")
    {
        formatManager.registerBasicFormats();
        readAheadThread.startThread();
    }

    ~AudioTrack() override
    {
        stopRecording();
        stopPlayback();

        transportSource.setSource(nullptr);
        hasAudioSource.store(false);

        readerSource.reset();

        readAheadThread.stopThread(2000);
    }

    // ========================================================
    // PREPARAR AUDIO
    // ========================================================

    void prepareToPlay(
        int samplesPerBlockExpected,
        double sampleRate)
    {
        currentSampleRate.store(
            juce::jmax(1.0, sampleRate));

        transportSource.prepareToPlay(
            samplesPerBlockExpected,
            sampleRate);
    }

    void releaseResources()
    {
        stopPlayback();
        transportSource.releaseResources();
    }

    // ========================================================
    // PROCESAMIENTO
    // ========================================================

    void processBlock(
        juce::AudioBuffer<float>& buffer,
        juce::MidiBuffer& midiMessages) override
    {
        juce::ignoreUnused(midiMessages);

        if (isMuted() ||
            !isPlaying.load(std::memory_order_acquire))
        {
            buffer.clear();
            return;
        }

        juce::AudioSourceChannelInfo info(
            &buffer,
            0,
            buffer.getNumSamples());

        transportSource.getNextAudioBlock(info);

        applyVolumeAndPan(buffer);
    }

    // ========================================================
    // VOLUMEN Y PAN
    // ========================================================

    void applyVolumeAndPan(
        juce::AudioBuffer<float>& buffer)
    {
        const int channels = buffer.getNumChannels();

        if (channels <= 0)
            return;

        const float currentVolume =
            volume.load(std::memory_order_relaxed);

        const float currentPan =
            pan.load(std::memory_order_relaxed);

        if (channels == 1)
        {
            buffer.applyGain(currentVolume);
            return;
        }

        const float leftPan =
            juce::jlimit(0.0f, 1.0f, 1.0f - currentPan);

        const float rightPan =
            juce::jlimit(0.0f, 1.0f, 1.0f + currentPan);

        const float leftGain =
            currentVolume * std::sqrt(leftPan);

        const float rightGain =
            currentVolume * std::sqrt(rightPan);

        buffer.applyGain(
            0, 0, buffer.getNumSamples(), leftGain);

        buffer.applyGain(
            1, 0, buffer.getNumSamples(), rightGain);

        for (int ch = 2; ch < channels; ++ch)
        {
            buffer.applyGain(
                ch, 0, buffer.getNumSamples(), currentVolume);
        }
    }

    // ========================================================
    // ENVIO AUXILIAR
    // ========================================================

    void processSendBlock(
        const juce::AudioBuffer<float>& sourceBuffer,
        juce::AudioBuffer<float>& auxBusBuffer)
    {
        if (isMuted())
            return;

        const float sendLevel =
            auxSendLevel.load(std::memory_order_relaxed);

        if (sendLevel <= 0.0f)
            return;

        const int channels = juce::jmin(
            sourceBuffer.getNumChannels(),
            auxBusBuffer.getNumChannels());

        const int samples = juce::jmin(
            sourceBuffer.getNumSamples(),
            auxBusBuffer.getNumSamples());

        for (int ch = 0; ch < channels; ++ch)
        {
            auxBusBuffer.addFrom(
                ch, 0, sourceBuffer, ch, 0, samples,
                sendLevel);
        }
    }

    // ========================================================
    // CARGAR ARCHIVO
    // ========================================================

    bool loadAudioFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;

        std::unique_ptr<juce::AudioFormatReader> reader(
            formatManager.createReaderFor(file));

        if (reader == nullptr)
            return false;

        const double fileSampleRate = reader->sampleRate;

        auto newSource =
            std::make_unique<juce::AudioFormatReaderSource>(
                reader.release(), true);

        stopPlayback();

        hasAudioSource.store(false);

        transportSource.setSource(nullptr);
        readerSource.reset();

        transportSource.setSource(
            newSource.get(),
            32768,
            &readAheadThread,
            fileSampleRate);

        readerSource = std::move(newSource);

        audioFile = file;

        hasAudioSource.store(true, std::memory_order_release);

        return true;
    }

    // ========================================================
    // REPRODUCCION
    // ========================================================

    void startPlayback()
    {
        if (!hasAudioSource.load(std::memory_order_acquire))
            return;

        transportSource.setPosition(0.0);
        transportSource.start();

        isPlaying.store(true, std::memory_order_release);
    }

    void stopPlayback()
    {
        isPlaying.store(false, std::memory_order_release);
        transportSource.stop();
    }

    bool isCurrentlyPlaying() const
    {
        return isPlaying.load(std::memory_order_acquire);
    }

    // ========================================================
    // GRABACION
    // ========================================================

    bool getIsRecording() const
    {
        return isRecordingState.load(std::memory_order_acquire);
    }

    void setIsRecording(bool shouldRecord)
    {
        isRecordingState.store(shouldRecord, std::memory_order_release);
    }

    void stopRecording()
    {
        isRecordingState.store(false, std::memory_order_release);
    }

    void processRecording(
        const float* const* inputData,
        int numChannels,
        int numSamples)
    {
        // Grabación WAV pendiente de implementar mediante
        // un escritor en segundo plano.
        juce::ignoreUnused(inputData, numChannels, numSamples);
    }

    // ========================================================
    // PLUGINS
    // ========================================================

    void loadPlugin(const juce::File& file)
    {
        juce::ignoreUnused(file);
    }

    void openPluginEditor()
    {
    }

    // ========================================================
    // CONTROLES
    // ========================================================

    void setVolume(float newVolume)
    {
        volume.store(
            juce::jlimit(0.0f, 1.5f, newVolume),
            std::memory_order_relaxed);
    }

    float getVolume() const
    {
        return volume.load(std::memory_order_relaxed);
    }

    void setPan(float newPan)
    {
        pan.store(
            juce::jlimit(-1.0f, 1.0f, newPan),
            std::memory_order_relaxed);
    }

    float getPan() const
    {
        return pan.load(std::memory_order_relaxed);
    }

    void setAuxSendLevel(float level)
    {
        auxSendLevel.store(
            juce::jlimit(0.0f, 1.0f, level),
            std::memory_order_relaxed);
    }

    float getAuxSendLevel() const
    {
        return auxSendLevel.load(std::memory_order_relaxed);
    }

    // ========================================================
    // MUTE
    // ========================================================

    void setMute(bool shouldMute)
    {
        mutedState.store(shouldMute, std::memory_order_relaxed);
    }

    void setMuted(bool shouldMute) override
    {
        setMute(shouldMute);
    }

    bool isMuted() const override
    {
        return mutedState.load(std::memory_order_relaxed);
    }

    // ========================================================
    // SOLO
    // ========================================================

    void setSolo(bool shouldSolo)
    {
        soloed.store(shouldSolo, std::memory_order_relaxed);
    }

    bool isSoloed() const
    {
        return soloed.load(std::memory_order_relaxed);
    }

    // ========================================================
    // ARCHIVO
    // ========================================================

    juce::File getCurrentAudioFilePath() const
    {
        return audioFile;
    }

    double getCurrentSampleRate() const
    {
        return currentSampleRate.load(std::memory_order_relaxed);
    }

private:

    juce::AudioFormatManager formatManager;

    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    juce::AudioTransportSource transportSource;

    juce::TimeSliceThread readAheadThread;

    juce::File audioFile;

    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<float> volume { 0.8f };
    std::atomic<float> pan { 0.0f };
    std::atomic<float> auxSendLevel { 0.0f };

    std::atomic<bool> isPlaying { false };
    std::atomic<bool> isRecordingState { false };
    std::atomic<bool> mutedState { false };
    std::atomic<bool> soloed { false };
    std::atomic<bool> hasAudioSource { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioTrack)
};
