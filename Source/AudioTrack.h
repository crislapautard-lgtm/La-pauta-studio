#pragma once
#include <JuceHeader.h>
#include "Track.h"

class AudioTrack : public Track {
public:
    AudioTrack(const std::string& trackName) : Track(trackName) {
        formatManager.registerBasicFormats();
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

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        if (muted) return;

        if (isPlaying && readerSource != nullptr) {
            juce::AudioSourceChannelInfo sourceInfo(&buffer, 0, buffer.getNumSamples());
            transportSource.getNextAudioBlock(sourceInfo);
        }

        if (activePlugin != nullptr) {
            juce::MidiBuffer dummyMidi;
            activePlugin->processBlock(buffer, dummyMidi);
        }

        buffer.applyGain(volume);
    }

    void processSendBlock(const juce::AudioBuffer<float>& sourceBuffer, juce::AudioBuffer<float>& auxBusBuffer) {
        if (muted || auxSendLevel <= 0.0f) return;

        for (int ch = 0; ch < juce::jmin(sourceBuffer.getNumChannels(), auxBusBuffer.getNumChannels()); ++ch) {
            auxBusBuffer.addFrom(ch, 0, sourceBuffer, ch, 0, sourceBuffer.getNumSamples(), auxSendLevel);
        }
    }

    void loadAudioFile(const juce::File& file) {
        audioFile = file;
        auto* reader = formatManager.createReaderFor(file);
        if (reader != nullptr) {
            auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            transportSource.setSource(newSource.get(), 0, &readAheadThread, reader->sampleRate);
            readerSource = std::move(newSource);
        }
    }

    void loadPlugin(const juce::File& file) {
#JUCE_BEGIN_IGNORE_WARNINGS_MSVC(4996)
        juce::KnownPluginList pluginList;
        juce::VST3PluginFormat vst3Format;
#JUCE_END_IGNORE_WARNINGS_MSVC

        juce::PluginDescription description;
        description.fileOrIdentifier = file.getFullPathName();
        description.pluginFormatName = "VST3";

        juce::String errorMessage;
        activePlugin = vst3Format.createPluginInstance(description, currentSampleRate, 512, errorMessage);

        if (activePlugin != nullptr) {
            activePlugin->prepareToPlay(currentSampleRate, 512);
        }
    }

    void openPluginEditor() {
        if (activePlugin != nullptr && activePlugin->hasEditor()) {
            if (auto* editor = activePlugin->createEditorCustom()) { // Safe editor creation fallback
                pluginEditorWindow = std::make_unique<PluginWindow>(editor, name);
            } else if (auto* editor = activePlugin->createEditor()) {
                pluginEditorWindow = std::make_unique<PluginWindow>(editor, name);
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

    void setAuxSendLevel(float level) { auxSendLevel = juce::jlimit(0.0f, 1.0f, level); }
    float getAuxSendLevel() const { return auxSendLevel; }

    void setMute(bool shouldMute) { muted = shouldMute; }
    bool isMuted() const { return muted; }

    void setSolo(bool shouldSolo) { soloed = shouldSolo; }
    bool isSoloed() const { return soloed; }

    bool getIsRecording() const { return isRecordingState; }
    void processRecording(const float* const*, int, int) {}

    juce::File getCurrentAudioFilePath() const { return audioFile; }

private:
    class PluginWindow : public juce::DocumentWindow {
    public:
        PluginWindow(juce::AudioProcessorEditor* editor, const juce::String& trackName)
            : DocumentWindow("Plugin: " + trackName,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setContentOwned(editor, true);
            setResizable(true, true);
            centreWithSize(editor->getWidth(), editor->getHeight());
            setVisible(true);
        }

        ~PluginWindow() override = default;

        void closeButtonPressed() override {
            delete this;
        }
    };

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    juce::TimeSliceThread readAheadThread { "Audio ReadAhead Thread" };
    
    std::unique_ptr<juce::AudioPluginInstance> activePlugin;
    std::unique_ptr<PluginWindow> pluginEditorWindow;

    juce::File audioFile;
    double currentSampleRate = 44100.0;
    bool isPlaying = false;
    bool isRecordingState = false;

    float volume = 0.8f;
    float pan = 0.0f;
    float auxSendLevel = 0.0f;
    bool muted = false;
    bool soloed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioTrack)
};
