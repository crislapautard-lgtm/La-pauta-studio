#pragma once
#include <JuceHeader.h>
#include "Track.h"

class AudioTrack : public Track {
public:
    AudioTrack(const std::string& name) : Track(name) {
        formatManager.addBasicFormats();
    }

    ~AudioTrack() override {
        if (activePlugin != nullptr && pluginEditorWindow != nullptr) {
            pluginEditorWindow.reset();
        }
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
        currentSampleRate = sampleRate;
        transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
        
        if (activePlugin != nullptr) {
            activePlugin->prepareToPlay(sampleRate, samplesPerBlockExpected);
        }
    }

    void releaseResources() {
        transportSource.releaseResources();
        if (activePlugin != nullptr) {
            activePlugin->releaseResources();
        }
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        if (muted) return;

        // 1. Procesar audio desde el archivo de audio cargado si hay reproducción activa
        if (isPlaying && readerSource != nullptr) {
            juce::AudioSourceChannelInfo sourceInfo(&buffer, 0, buffer.getNumSamples());
            transportSource.getNextAudioBlock(sourceInfo);
        }

        // 2. Si hay un plugin VST3 cargado, procesamos el audio a través de él
        if (activePlugin != nullptr) {
            juce::MidiBuffer dummyMidi;
            activePlugin->processBlock(buffer, dummyMidi);
        }

        // 3. Aplicar Ganancia de Panal (Volumen y Paneo estéreo básico)
        buffer.applyGain(volume);
    }

    // --- CARGAR ARCHIVO DE AUDIO ---
    void loadAudioFile(const juce::File& file) {
        audioFile = file;
        auto* reader = formatManager.createReaderFor(file);
        if (reader != nullptr) {
            auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            transportSource.setSource(newSource.get(), 0, &readAheadThread, reader->sampleRate);
            readerSource = std::move(newSource);
        }
    }

    // --- CARGAR PLUGIN VST3 ---
    void loadPlugin(const juce::File& file) {
        juce::VST3PluginFormat vst3Format;
        juce::PluginDescription description;
        description.fileOrIdentifier = file.getFullPathName();
        description.pluginFormatName = "VST3";

        juce::String errorMessage;
        activePlugin = vst3Format.createPluginInstance(description, currentSampleRate, 512, errorMessage);

        if (activePlugin != nullptr) {
            activePlugin->prepareToPlay(currentSampleRate, 512);
        }
    }

    // --- ABRIR LA INTERFAZ GRÁFICA (EDITOR) DEL VST3 EN VENTANA FLOTANTE ---
    void openPluginEditor() {
        if (activePlugin != nullptr && activePlugin->hasEditor()) {
            auto* editor = activePlugin->createEditor();
            if (editor != nullptr) {
                pluginEditorWindow = std::make_unique<PluginWindow>(editor, getName());
            }
        }
    }

    void startPlayback() {
        if (readerSource != nullptr) {
            transportSource.setPosition(0.0);
            transportSource.start();
        }
        isPlaying = true;
    }

    void stopPlayback() {
        transportSource.stop();
        isPlaying = false;
    }

    void setVolume(float newVolume) { volume = newVolume; }
    float getVolume() const { return volume; }

    void setPan(float newPan) { pan = newPan; }
    float getPan() const { return pan; }

    void setMute(bool shouldMute) { muted = shouldMute; }
    bool isMuted() const { return muted; }

    void setSolo(bool shouldSolo) { soloed = shouldSolo; }
    bool isSoloed() const { return soloed; }

    juce::File getCurrentAudioFilePath() const { return audioFile; }

private:
    // Ventana flotante independiente para la interfaz gráfica del VST3
    class PluginWindow : public juce::DocumentWindow {
    public:
        PluginWindow(juce::AudioProcessorEditor* editor, const juce::String& trackName)
            : DocumentWindow("Plugin: " + trackName,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)

