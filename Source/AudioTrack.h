#pragma once
#include <JuceHeader.h>
#include "Track.h"

class AudioTrack : public Track, public juce::AudioSource {
public:
    AudioTrack(const juce::String& name) : Track(name), backgroundThread("Audio Recorder Thread") {
        backgroundThread.startThread();
        formatManager.registerBasicFormats();
    }

    ~AudioTrack() {
        stopRecording();
        stopPlayback();
        backgroundThread.stopThread(500);
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override {
        currentSampleRate = sampleRate;
        transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
    }

    void releaseResources() override {
        transportSource.releaseResources();
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        if (muted) {
            buffer.clear();
            return;
        }

        // Si hay reproducción activa, inyectamos el audio del archivo grabado
        if (isPlaying && readerSource != nullptr) {
            juce::AudioSourceChannelInfo bufferInfo(buffer);
            transportSource.getNextAudioBlock(bufferInfo);
        }

        // Aplicar Paneo Estéreo y Volumen de la Pista en tiempo real
        if (buffer.getNumChannels() >= 2) {
            float leftGain  = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
            float rightGain = (pan >= 0.0f) ? 1.0f : (1.0f + pan);
            
            buffer.applyGain(0, 0, buffer.getNumSamples(), leftGain * volume);
            buffer.applyGain(1, 0, buffer.getNumSamples(), rightGain * volume);
        } else {
            buffer.applyGain(volume);
        }
    }

    // --- CARGAR Y REPRODUCIR AUDIO (LO QUE GRABASTE) ---
    void loadAudioFile(const juce::File& file) {
        stopPlayback();
        if (auto* reader = formatManager.createReaderFor(file)) {
            readerSource.reset(new juce::AudioFormatReaderSource(reader, true));
            transportSource.setSource(readerSource.get(), 0, &backgroundThread, reader->sampleRate);
        }
    }

    void startPlayback() {
        transportSource.start();
        isPlaying = true;
    }

    void stopPlayback() {
        transportSource.stop();
        transportSource.setPosition(0.0);
        isPlaying = false;
    }

    // --- GRABACIÓN EN DISCO ---
    void processRecording(const float* const* inputChannelData, int numChannels, int numSamples) {
        if (!isRecording || !threadedWriter)
            return;

        threadedWriter->write(inputChannelData, numSamples);
    }

    void startRecording(const juce::File& file, double sampleRate) {
        if (isRecording) return;

        file.deleteFile();
        outputFile = file;

        juce::WavAudioFormat wavFormat;
        
        // Versión moderna y limpia compatible con las exigencias de JUCE
        juce::AudioFormatWriterOptions options;
        options.file = outputFile;
        options.sampleRate = sampleRate;
        options.numChannels = 2; // Estéreo profesional
        options.bitsPerSample = 24; // 24-bit
        options.qualityOptionIndex = 0;

        if (auto* writer = wavFormat.createWriterFor(options)) {
            threadedWriter.reset(new juce::AudioFormatWriter::ThreadedWriter(writer, backgroundThread, 32768));
            isRecording = true;
        }
    }

    void stopRecording() {
        if (!isRecording) return;

        isRecording = false;
        threadedWriter.reset();
        
        // Al terminar de grabar, cargamos el archivo automáticamente para poder escucharlo
        loadAudioFile(outputFile);
    }

    bool getIsRecording() const { return isRecording; }

private:
    bool isRecording = false;
    bool isPlaying = false;
    double currentSampleRate = 44100.0;
    
    juce::File outputFile;
    juce::TimeSliceThread backgroundThread;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;

    // Componentes de Reproducción
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
};
