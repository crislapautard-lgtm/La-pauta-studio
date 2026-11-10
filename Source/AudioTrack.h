
#pragma once

#include <JuceHeader.h>
#include <string>
#include <memory>

#include "Track.h"

// ============================================================
// LA PAUTA STUDIO
// AudioTrack.h
// Administración de pistas de audio
// ============================================================

class AudioTrack : public Track
{
public:

    explicit AudioTrack(const std::string& name)
        : Track(juce::String(name)),
          trackNameStr(name),
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
        currentSampleRate = sampleRate;

        transportSource.prepareToPlay(
            samplesPerBlockExpected,
            sampleRate
        );
    }

    // ========================================================
    // LIBERAR RECURSOS
    // ========================================================

    void releaseResources()
    {
        transportSource.stop();
        transportSource.releaseResources();
    }

    // ========================================================
    // PROCESAMIENTO DE AUDIO
    // ========================================================

    void processBlock(
        juce::AudioBuffer<float>& buffer,
        juce::MidiBuffer& midiMessages) override
    {
        juce::ignoreUnused(midiMessages);

        if (muted)
        {
            buffer.clear();
            return;
        }

        if (isPlaying && readerSource != nullptr)
        {
            juce::AudioSourceChannelInfo info(
                &buffer,
                0,
                buffer.getNumSamples()
            );

            transportSource.getNextAudioBlock(info);
        }

        applyVolumeAndPan(buffer);
    }

    // ========================================================
    // VOLUMEN Y PANORAMA
    // ========================================================

    void applyVolumeAndPan(
        juce::AudioBuffer<float>& buffer)
    {
        const int channels = buffer.getNumChannels();

        if (channels == 1)
        {
            buffer.applyGain(volume);
            return;
        }

        const float leftPan =
            juce::jlimit(0.0f, 1.0f, 1.0f - pan);

        const float rightPan =
            juce::jlimit(0.0f, 1.0f, 1.0f + pan);

        const float leftGain =
            volume * std::sqrt(leftPan);

        const float rightGain =
            volume * std::sqrt(rightPan);

        buffer.applyGain(0, 0, buffer.getNumSamples(), leftGain);
        buffer.applyGain(1, 0, buffer.getNumSamples(), rightGain);

        for (int ch = 2; ch < channels; ++ch)
            buffer.applyGain(ch, 0, buffer.getNumSamples(), volume);
    }

    // ========================================================
    // ENVIO AUXILIAR
    // ========================================================

    void processSendBlock(
        const juce::AudioBuffer<float>& sourceBuffer,
        juce::AudioBuffer<float>& auxBusBuffer)
    {
        if (muted || auxSendLevel <= 0.0f)
            return;

        const int channels = juce::jmin(
            sourceBuffer.getNumChannels(),
            auxBusBuffer.getNumChannels()
        );

        const int samples = juce::jmin(
            sourceBuffer.getNumSamples(),
            auxBusBuffer.getNumSamples()
        );

        for (int ch = 0; ch < channels; ++ch)
        {
            auxBusBuffer.addFrom(
                ch,
                0,
                sourceBuffer,
                ch,
                0,
                samples,
                auxSendLevel
            );
        }
    }

    // ========================================================
    // CARGAR ARCHIVO DE AUDIO
    // ========================================================

    bool loadAudioFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;

        auto* reader = formatManager.createReaderFor(file);

        if (reader == nullptr)
            return false;

        transportSource.stop();
        transportSource.setSource(nullptr);
        readerSource.reset();

        auto newSource =
            std::make_unique<juce::AudioFormatReaderSource>(
                reader,
                true
            );

        transportSource.setSource(
            newSource.get(),
            32768,
            &readAheadThread,
            reader->sampleRate
        );

        readerSource = std::move(newSource);

        audioFile = file;
        isPlaying = false;

        return true;
    }

    // ========================================================
    // REPRODUCCION
    // ========================================================

    void startPlayback()
    {
        if (readerSource == nullptr)
            return;

        transportSource.setPosition(0.0);
        transportSource.start();

        isPlaying = true;
    }

    void stopPlayback()
    {
        transportSource.stop();
        isPlaying = false;
    }

    bool isCurrentlyPlaying() const
    {
        return isPlaying;
    }

    // ========================================================
    // GRABACION
    // ========================================================

    bool getIsRecording() const
    {
        return isRecordingState;
    }

    void setIsRecording(bool shouldRecord)
    {
        isRecordingState = shouldRecord;
    }

    void stopRecording()
    {
        isRecordingState = false;
    }

    // La escritura WAV se integrará mediante un sistema
    // de grabación independiente del hilo de audio.
    void processRecording(
        const float* const* inputData,
        int numChannels,
        int numSamples)
    {
        juce::ignoreUnused(
            inputData,
            numChannels,
            numSamples
        );
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
    // CONTROLES DE PISTA
    // ========================================================

    void setVolume(float newVolume)
    {
        volume = juce::jlimit(0.0f, 1.5f, newVolume);
    }

    float getVolume() const
    {
        return volume;
    }

    void setPan(float newPan)
    {
        pan = juce::jlimit(-1.0f, 1.0f, newPan);
    }

    float getPan() const
    {
        return pan;
    }

    void setAuxSendLevel(float level)
    {
        auxSendLevel = juce::jlimit(0.0f, 1.0f, level);
    }

    float getAuxSendLevel() const
    {
        return auxSendLevel;
    }

    void setMute(bool shouldMute)
    {
        muted = shouldMute;
    }

    bool isMuted() const
    {
        return muted;
    }

    void setSolo(bool shouldSolo)
    {
        soloed = shouldSolo;
    }

    bool isSoloed() const
    {
        return soloed;
    }

    juce::File getCurrentAudioFilePath() const
    {
        return audioFile;
    }

private:

    juce::String trackNameStr;

    juce::AudioFormatManager formatManager;

    std::unique_ptr<juce::AudioFormatReaderSource>
        readerSource;

    juce::AudioTransportSource transportSource;

    juce::TimeSliceThread readAheadThread;

    juce::File audioFile;

    double currentSampleRate = 44100.0;

    float volume = 0.8f;
    float pan = 0.0f;
    float auxSendLevel = 0.0f;

    bool isPlaying = false;
    bool isRecordingState = false;

    bool muted = false;
    bool soloed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioTrack)
};
