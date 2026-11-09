#pragma once
#include <JuceHeader.h>
#include "Track.h"

class AudioTrack : public Track, public juce::AudioSource {
public:
    AudioTrack(const juce::String& name) : Track(name), backgroundThread("Audio Recorder Thread") {
        backgroundThread.startThread();
        formatManager.registerBasicFormats();
        pluginFormatManager.addDefaultFormats();
    }

    ~AudioTrack() {
        stopRecording();
        stopPlayback();
        if (loadedPlugin != nullptr) {
            loadedPlugin->releaseResources();
        }
        backgroundThread.stopThread(500);
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override {
        currentSampleRate = sampleRate;
        transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
        
        if (loadedPlugin != nullptr) {
            loadedPlugin->prepareToPlay(sampleRate, samplesPerBlockExpected);
        }
    }

    void releaseResources() override {
        transportSource.releaseResources();
        if (loadedPlugin != nullptr) {
            loadedPlugin->releaseResources();
        }
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

        // Procesamiento VST3 / Plugin en tiempo real
        if (loadedPlugin != nullptr) {
            juce::MidiBuffer emptyMidi;
            loadedPlugin->processBlock(buffer, emptyMidi);
        }

        // Aplicar Paneo Estéreo y Volumen de la Pista
        if (buffer.getNumChannels() >= 2) {
            float leftGain  = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
            float rightGain = (pan >= 0.0f) ? 1.0f : (1.0f + pan);
            
            buffer.applyGain(0, 0, buffer.getNumSamples(), leftGain * volume);
            buffer.applyGain(1, 0, buffer.getNumSamples(), rightGain * volume);
        } else {
            buffer.applyGain(volume);
        }
    }

    // --- CARGAR Y REPRODUCIR AUDIO ---
    void loadAudioFile(const juce::File& file) {
        stopPlayback();
        currentAudioFile = file;
        if (auto* reader = formatManager.createReaderFor(file)) {
            readerSource.reset(new juce::AudioFormatReaderSource(reader, true));
            transportSource.setSource(readerSource.get(), 0, &backgroundThread, reader->sampleRate);
        }
    }

    void startPlayback() {
        if (readerSource != nullptr) {
            transportSource.start();
            isPlaying = true;
        }
    }

    void stopPlayback() {
        transportSource.stop();
        transportSource.setPosition(0.0);
        isPlaying = false;
    }

    juce::File getCurrentAudioFilePath() const {
        return currentAudioFile;
    }

    // --- CARGAR PLUGIN VST3 ---
    void loadPlugin(const juce::File& file, std::function<void(const juce::String&)> callback) {
        pluginFormatFinder.findAllTypesForFile(pluginFormatManager, file.getFullPathName(), [this, callback](const juce::OwnedArray<juce::PluginDescription>& descriptions) {
            if (!descriptions.isEmpty()) {
                pluginFormatManager.createPluginInstance(*descriptions[0], currentSampleRate, 512, [this, callback](std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) {
                    if (instance != nullptr) {
                        loadedPlugin = std::move(instance);
                        loadedPlugin->prepareToPlay(currentSampleRate, 512);
                        pluginName = loadedPlugin->getName();
                        callback(pluginName);
                    } else {
                        callback("Error: " + error);
                    }
                });
            } else {
                callback("Formato no válido");
            }
        });
    }

    void removePlugin() {
        if (loadedPlugin != nullptr) {
            loadedPlugin->releaseResources();
            loadedPlugin.reset();
            pluginName = "";
        }
    }

    juce::String getPluginName() const { return pluginName; }

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
        juce::AudioFormatWriterOptions options;
        options.file = outputFile;
        options.sampleRate = sampleRate;
        options.numChannels = 2;
        options.bitsPerSample = 24;
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
        loadAudioFile(outputFile);
    }

    bool getIsRecording() const { return isRecording; }

private:
    bool isRecording = false;
    bool isPlaying = false;
    double currentSampleRate = 44100.0;
    
    juce::File outputFile;
    juce::File currentAudioFile;
    juce::TimeSliceThread backgroundThread;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;

    // VST3 Hosting
    juce::AudioPluginFormatManager pluginFormatManager;
    juce::PluginDirectoryScanThread pluginFormatFinder{pluginFormatManager};
    std::unique_ptr<juce::AudioPluginInstance> loadedPlugin;
    juce::String pluginName = "";
};
