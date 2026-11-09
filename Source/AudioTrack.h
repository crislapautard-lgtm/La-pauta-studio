#pragma once
#include <JuceHeader.h>
#include "Track.h"

class AudioTrack : public Track {
public:
    AudioTrack(const juce::String& name) : Track(name), backgroundThread("Audio Recorder Thread") {
        backgroundThread.startThread();
    }

    ~AudioTrack() {
        stopRecording();
        backgroundThread.stopThread(500);
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        if (muted) {
            buffer.clear();
            return;
        }

        // Aplicamos el control de volumen general de la pista
        buffer.applyGain(volume);

        // Aquí puedes seguir procesando efectos o paneo estéreo
    }

    // Métodos para controlar la mezcla desde la interfaz (TrackComponent)
    void setVolume(float newVolume) { volume = newVolume; }
    void setPan(float newPan)       { pan = newPan; }
    void setMute(bool shouldMute)   { muted = shouldMute; }
    void setSolo(bool shouldSolo)   { soloed = shouldSolo; }

    // Método para capturar y escribir el audio entrante en un archivo WAV real
    void processRecording(const float* const* inputChannelData, int numChannels, int numSamples) {
        if (!isRecording || !threadedWriter)
            return;

        // Enviamos el bloque de audio al escritor en segundo plano de forma segura
        threadedWriter->write(inputChannelData, numSamples);
    }

    // Iniciar grabación en un archivo específico (Actualizado con AudioFormatWriterOptions de JUCE)
    void startRecording(const juce::File& file, double sampleRate) {
        if (isRecording) return;

        file.deleteFile();
        outputFile = file;

        juce::WavAudioFormat wavFormat;
        
        // Forma moderna compatible con las últimas versiones de JUCE
        juce::AudioFormatWriterOptions options;
        options.file = outputFile;
        options.sampleRate = sampleRate;
        options.numChannels = 2; // Estéreo
        options.bitsPerSample = 24; // 24-bit de calidad profesional
        options.qualityOptionIndex = 0;

        if (auto* writer = wavFormat.createWriterFor (options))
        {
            // Creamos el escritor con hilo seguro para evitar cortes de audio
            threadedWriter.reset(new juce::AudioFormatWriter::ThreadedWriter(writer, backgroundThread, 32768));
            isRecording = true;
        }
    }

    // Detener grabación y cerrar el archivo correctamente
    void stopRecording() {
        if (!isRecording) return;

        isRecording = false;
        
        // Al destruir el threadedWriter, se vacía el búfer pendiente y se cierra el archivo .wav en disco
        threadedWriter.reset();
    }

    bool getIsRecording() const { return isRecording; }

private:
    bool isRecording = false;
    juce::File outputFile;
    juce::TimeSliceThread backgroundThread;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;

    // Parámetros de Mezcla (Mixer)
    float volume = 0.8f;
    float pan = 0.0f;      // -1.0 Izquierda, 0.0 Centro, 1.0 Derecha
    bool soloed = false;
};
