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
        // Aquí puedes seguir mezclando o procesando el audio de reproducción de la pista
    }

    // Método para capturar y escribir el audio entrante en un archivo WAV real
    void processRecording(const float* const* inputChannelData, int numChannels, int numSamples) {
        if (!isRecording || !threadedWriter)
            return;

        // Enviamos el bloque de audio al escritor en segundo plano de forma segura
        threadedWriter->write(inputChannelData, numSamples);
    }

    // Iniciar grabación en un archivo específico
    void startRecording(const juce::File& file, double sampleRate) {
        if (isRecording) return;

        file.deleteFile();
        outputFile = file;

        // Usamos formato WAV por defecto
        juce::WavAudioFormat wavFormat;
        if (auto* writer = wavFormat.createWriterFor(new juce::FileOutputStream(outputFile), 
                                                     sampleRate, 
                                                     2, // Estéreo
                                                     24, // 24-bit de calidad profesional
                                                     {}, 
                                                     0))
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
};
