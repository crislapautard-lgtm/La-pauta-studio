#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include "TrackComponent.h"
#include "Track.h"
#include "AudioTrack.h"
#include "MidiTrack.h"
#include "AudioEngine.h"
#include "ProjectManager.h"
#include "MixerPanelComponent.h"
#include "AudioSettingsWindow.h"
#include "MidiEditorComponent.h"

class MainComponent : public juce::AudioAppComponent,
                      public juce::Button::Listener,
                      public juce::Slider::Listener,
                      public juce::KeyListener
{
public:
    MainComponent() {
        addAudioTrack();

        playButton.setButtonText("PLAY");
        playButton.addListener(this);
        playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
        playButton.setClickingTogglesState(true);
        addAndMakeVisible(playButton);

        metronomeButton.setButtonText("Click");
        metronomeButton.addListener(this);
        metronomeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        metronomeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
        metronomeButton.setClickingTogglesState(true);
        addAndMakeVisible(metronomeButton);

        bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        bpmSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
        bpmSlider.setRange(60.0, 200.0, 1.0);
        bpmSlider.setValue(120.0);
        bpmSlider.addListener(this);
        addAndMakeVisible(bpmSlider);

        addTrackButton.setButtonText("+ Pista");
        addTrackButton.addListener(this);
        addAndMakeVisible(addTrackButton);

        saveButton.setButtonText("Guardar");
        saveButton.addListener(this);
        addAndMakeVisible(saveButton);

        openButton.setButtonText("Abrir");
        openButton.addListener(this);
        addAndMakeVisible(openButton);

        configButton.setButtonText("Config");
        configButton.addListener(this);
        addAndMakeVisible(configButton);

        toggleMixerButton.setButtonText("Mixer");
        toggleMixerButton.addListener(this);
        toggleMixerButton.setClickingTogglesState(true);
        addAndMakeVisible(toggleMixerButton);

        midiEditorButton.setButtonText("Piano Roll");
        midiEditorButton.addListener(this);
        addAndMakeVisible(midiEditorButton);

        exportButton.setButtonText("Exportar");
        exportButton.addListener(this);
        addAndMakeVisible(exportButton);

        mixerPanel = std::make_unique<MixerPanelComponent>(tracks);
        addChildComponent(mixerPanel.get());

        setSize(1100, 700);
        setAudioChannels(2, 2);
        
        setWantsKeyboardFocus(true);
        addKeyListener(this);
    }

    ~MainComponent() override {
        shutdownAudio();
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override {
        if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)) {
            if (undoManager.canUndo()) {
                undoManager.undo();
                repaint();
                return true;
            }
        }
        else if (key == juce::KeyPress('y', juce::ModifierKeys::commandModifier, 0) ||
                 key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) {
            if (undoManager.canRedo()) {
                undoManager.redo();
                repaint();
                return true;
            }
        }
        else if (key == juce::KeyPress('s', juce::ModifierKeys::commandModifier, 0)) {
            fileChooser = std::make_unique<juce::FileChooser>("Guardar Sesión de La Pauta Studio",
                                                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                              "*.pauta");
            fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& fc) {
                    auto file = fc.getResult();
                    if (file != juce::File{}) {
                        ProjectManager::saveProject(file, tracks);
                    }
                });
            return true;
        }
        return false;
    }

    bool keyStateChanged(bool, juce::Component*) override { return false; }

    void addAudioTrack() {
        undoManager.beginNewTransaction("Añadir Pista de Audio");
        
        int trackId = (int)tracks.size() + 1;
        auto newTrack = std::make_unique<AudioTrack>("Pista " + juce::String(trackId));
        auto newTrackComp = std::make_unique<TrackComponent>(trackId, newTrack.get());

        tracks.push_back(std::move(newTrack));
        trackComponents.push_back(std::move(newTrackComp));

        addAndMakeVisible(trackComponents.back().get());
        
        if (mixerPanel != nullptr)
            mixerPanel->updateStripping();

        resized();
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override {
        currentSampleRate = sampleRate;
        for (auto& track : tracks) {
            if (auto* audioTrack = dynamic_cast<AudioTrack*>(track.get())) {
                audioTrack->prepareToPlay(samplesPerBlockExpected, sampleRate);
            }
        }
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override {
        bufferToFill.buffer->clear();

        juce::MidiBuffer midiMessages;
        juce::AudioBuffer<float> mixBus(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples);
        mixBus.clear();

        juce::AudioBuffer<float> auxBus(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples);
        auxBus.clear();

        for (size_t i = 0; i < tracks.size(); ++i) {
            juce::AudioBuffer<float> trackBuffer(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples);
            trackBuffer.clear();

            if (auto* audioTrack = dynamic_cast<AudioTrack*>(tracks[i].get())) {
                if (audioTrack->getIsRecording()) {
                    audioTrack->processRecording(bufferToFill.buffer->getArrayOfReadPointers(), 
                                                   bufferToFill.buffer->getNumChannels(), 
                                                   bufferToFill.numSamples);
                }
            }

            tracks[i]->processBlock(trackBuffer, midiMessages);

            if (auto* audioTrack = dynamic_cast<AudioTrack*>(tracks[i].get())) {
                audioTrack->processSendBlock(trackBuffer, auxBus);
            }

            for (int ch = 0; ch < mixBus.getNumChannels(); ++ch) {
                mixBus.addFrom(ch, 0, trackBuffer, ch, 0, bufferToFill.numSamples);
            }
        }

        auxBus.applyGain(0.7f); 
        for (int ch = 0; ch < mixBus.getNumChannels(); ++ch) {
            mixBus.addFrom(ch, 0, auxBus, ch, 0, bufferToFill.numSamples);
        }

        if (metronomeActive && isPlayingState) {
            double bpm = bpmSlider.getValue();
            double secondsPerBeat = 60.0 / bpm;
            double samplesPerBeat = secondsPerBeat * currentSampleRate;

            for (int sample = 0; sample < bufferToFill.numSamples; ++sample) {
                sampleCounter++;
                if (sampleCounter >= samplesPerBeat) {
                    sampleCounter = 0;
                    playClickSample = true;
                }

                if (playClickSample) {
                    float clickValue = (sampleCounter < 500) ? 0.3f : 0.0f; 
                    for (int ch = 0; ch < mixBus.getNumChannels(); ++ch) {
                        mixBus.addSample(ch, sample, clickValue);
                    }
                    if (sampleCounter >= 500) playClickSample = false;
                }
            }
        }

        mixBus.applyGain(0.9f);

        for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch) {
            bufferToFill.buffer->copyFrom(ch, 0, mixBus, ch, 0, bufferToFill.numSamples);
        }
    }

    void releaseResources() override {
        for (auto& track : tracks) {
            if (auto* audioTrack = dynamic_cast<AudioTrack*>(track.get())) {
                audioTrack->releaseResources();
            }
        }
    }

    void paint (juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff1e1e1e));
        
        g.setColour(juce::Colours::white);
        g.setFont(18.0f);
        g.drawText("LA PAUTA STUDIO - DAW Pro", 20, 15, 270, 30, juce::Justification::left);

        g.setFont(12.0f);
        g.drawText("BPM:", 310, 20, 35, 20, juce::Justification::right);
    }

    void resized() override {
        playButton.setBounds(getWidth() - 95, 15, 80, 30);
        metronomeButton.setBounds(getWidth() - 175, 15, 70, 30);
        bpmSlider.setBounds(350, 15, 130, 30);

        addTrackButton.setBounds(getWidth() - 265, 15, 80, 30);
        saveButton.setBounds(getWidth() - 350, 15, 75, 30);
        openButton.setBounds(getWidth() - 435, 15, 75, 30);
        configButton.setBounds(getWidth() - 520, 15, 75, 30);
        toggleMixerButton.setBounds(getWidth() - 605, 15, 75, 30);
        midiEditorButton.setBounds(getWidth() - 700, 15, 90, 30);
        exportButton.setBounds(getWidth() - 785, 15, 80, 30);

        int startY = 65;
        int trackHeight = 70;
        int spacing = 10;
        int availableHeightForTracks = getHeight() - startY - 20;

        if (showMixer) {
            availableHeightForTracks -= 210;
        }

        for (size_t i = 0; i < trackComponents.size(); ++i) {
            trackComponents[i]->setBounds(15, startY + (int)i * (trackHeight + spacing), getWidth() - 30, trackHeight);
        }

        if (showMixer && mixerPanel != nullptr) {
            mixerPanel->setBounds(15, getHeight() - 210, getWidth() - 30, 200);
            mixerPanel->setVisible(true);
        } else if (mixerPanel != nullptr) {
            mixerPanel->setVisible(false);
        }
    }

    void buttonClicked (juce::Button* button) override {
        if (button == &playButton) {
            isPlayingState = playButton.getToggleState();
            for (auto& track : tracks) {
                if (auto* audioTrack = dynamic_cast<AudioTrack*>(track.get())) {
                    if (isPlayingState) audioTrack->startPlayback();
                    else audioTrack->stopPlayback();
                }
            }
        }
        else if (button == &metronomeButton) {
            metronomeActive = metronomeButton.getToggleState();
        }
        else if (button == &addTrackButton) {
            addAudioTrack();
        }
        else if (button == &saveButton) {
            fileChooser = std::make_unique<juce::FileChooser>("Guardar Sesión de La Pauta Studio",
                                                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                              "*.pauta");
            fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& fc) {
                    auto file = fc.getResult();
                    if (file != juce::File{}) {
                        ProjectManager::saveProject(file, tracks);
                    }
                });
        }
        else if (button == &openButton) {
            fileChooser = std::make_unique<juce::FileChooser>("Abrir Sesión de La Pauta Studio",
                                                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                              "*.pauta");
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& fc) {
                    auto file = fc.getResult();
                    if (file != juce::File{}) {
                        ProjectManager::loadProject(file, tracks, trackComponents, *this);
                        if (mixerPanel != nullptr) mixerPanel->updateStripping();
                    }
                });
        }
        else if (button == &configButton) {
            if (settingsWindow == nullptr) {
                settingsWindow = std::make_unique<AudioSettingsWindow>(deviceManager);
            }
            settingsWindow->toFront(true);
        }
        else if (button == &toggleMixerButton) {
            showMixer = toggleMixerButton.getToggleState();
            resized();
        }
        else if (button == &midiEditorButton) {
            midiEditorWindow = std::make_unique<MidiEditorWindow>("Pista MIDI Principal");
        }
        else if (button == &exportButton) {
            fileChooser = std::make_unique<juce::FileChooser>("Exportar Mezcla a WAV",
                                                              juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                                              "*.wav");
            fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& fc) {
                    auto file = fc.getResult();
                    if (file != juce::File{}) {
                        if (file.existsAsFile()) file.deleteFile();

                        juce::WavAudioFormat wavFormat;
                        juce::AudioFormatWriterOptions options;
                        options.bitsPerSample = 24;
                        options.sampleRate = currentSampleRate;
                        options.numChannels = 2;

                        auto writer = std::unique_ptr<juce::AudioFormatWriter>(
                            wavFormat.createWriterFor(std::make_unique<juce::FileOutputStream>(file), options)
                        );

                        if (writer != nullptr) {
                            for (auto& track : tracks) {
                                if (auto* at = dynamic_cast<AudioTrack*>(track.get())) {
                                    at->startPlayback();
                                }
                            }

                            const int numSamplesPerBlock = 512;
                            double totalSeconds = 20.0;
                            int totalBlocks = (int)((totalSeconds * currentSampleRate) / numSamplesPerBlock);

                            for (int b = 0; b < totalBlocks; ++b) {
                                juce::MidiBuffer dummyMidi;
                                juce::AudioBuffer<float> mixBus(2, numSamplesPerBlock);
                                mixBus.clear();
                                juce::AudioBuffer<float> auxBus(2, numSamplesPerBlock);
                                auxBus.clear();

                                for (size_t i = 0; i < tracks.size(); ++i) {
                                    juce::AudioBuffer<float> trackBuffer(2, numSamplesPerBlock);
                                    trackBuffer.clear();
                                    tracks[i]->processBlock(trackBuffer, dummyMidi);
                                    
                                    if (auto* audioTrack = dynamic_cast<AudioTrack*>(tracks[i].get())) {
                                        audioTrack->processSendBlock(trackBuffer, auxBus);
                                    }

                                    for (int ch = 0; ch < 2; ++ch) {
                                        mixBus.addFrom(ch, 0, trackBuffer, ch, 0, numSamplesPerBlock);
                                    }
                                }

                                auxBus.applyGain(0.7f);
                                for (int ch = 0; ch < 2; ++ch) {
                                    mixBus.addFrom(ch, 0, auxBus, ch, 0, numSamplesPerBlock);
                                }

                                mixBus.applyGain(0.9f);
                                writer->writeFromAudioSampleBuffer(mixBus, 0, numSamplesPerBlock);
                            }

                            for (auto& track : tracks) {
                                if (auto* at = dynamic_cast<AudioTrack*>(track.get())) {
                                    at->stopPlayback();
                                }
                            }
                        }
                    }
                });
        }
    }

    void sliderValueChanged(juce::Slider*) override {}

private:
    AudioEngine audioEngine;
    std::vector<std::unique_ptr<Track>> tracks;
    std::vector<std::unique_ptr<TrackComponent>> trackComponents;
    
    juce::TextButton playButton;
    juce::TextButton metronomeButton;
    juce::Slider bpmSlider;
    juce::TextButton addTrackButton;
    juce::TextButton saveButton;
    juce::TextButton openButton;
    juce::TextButton configButton;
    juce::TextButton toggleMixerButton;
    juce::TextButton midiEditorButton;
    juce::TextButton exportButton;

    std::unique_ptr<MixerPanelComponent> mixerPanel;
    bool showMixer = false;

    bool isPlayingState = false;
    bool metronomeActive = false;
    double currentSampleRate = 44100.0;
    double sampleCounter = 0.0;
    bool playClickSample = false;

    std::unique_ptr<AudioSettingsWindow> settingsWindow;
    std::unique_ptr<MidiEditorWindow> midiEditorWindow;
    juce::UndoManager undoManager;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
