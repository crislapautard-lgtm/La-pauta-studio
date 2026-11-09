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

class MainComponent : public juce::AudioAppComponent,
                      public juce::Button::Listener,
                      public juce::Slider::Listener
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

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override {
        // Ctrl + Z (Deshacer)
        if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)) {
            if (undoManager.canUndo()) {
                undoManager.undo();
                repaint();
                return true;
            }
        }
        // Ctrl + Y o Ctrl + Shift + Z (Rehacer)
        else if (key == juce::KeyPress('y', juce::ModifierKeys::commandModifier, 0) ||
                 key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) {
            if (undoManager.canRedo()) {
                undoManager.redo();
                repaint();
                return true;
            }
        }
        // Ctrl + S (Guardar Proyecto Rápido)
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
        bufferToFill.clearBuffer();

        juce::MidiBuffer midiMessages;
        juce::AudioBuffer<float> mixBus(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples);
        mixBus.clear();

        // Buffer dedicado al Bus Auxiliar de Efectos (Enrutamiento de Envíos)
        juce::AudioBuffer<float> auxBus(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples);
        auxBus.clear();

        // 1. Procesar cada pista de manera individual
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

            // Procesar audio de la pista (archivo + plugin de insert)
            tracks[i]->processBlock(trackBuffer, midiMessages);

            // Enrutar envío (Send) hacia el Bus Auxiliar
            if (auto* audioTrack = dynamic_cast<AudioTrack*>(tracks[i].get())) {
                audioTrack->processSendBlock(trackBuffer, auxBus);
            }

            // Sumar la señal directa de la pista al Master Mix Bus
            for (int ch = 0; ch < mixBus.getNumChannels(); ++ch) {
                mixBus.addFrom(ch, 0, trackBuffer, ch, 0, bufferToFill.numSamples);
            }
        }

        // Procesar y sumar el Bus Auxiliar al Master Mix Bus principal
        auxBus.applyGain(0.7f); 
        for (int ch = 0; ch < mixBus.getNumChannels(); ++ch) {
            mixBus.addFrom(ch, 0, auxBus, ch, 0, bufferToFill.numSamples);
        }

        // --- MOTOR DE METRÓNOMO (CLICK) ---
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

        mixBus.applyGain(0.9f); // Master Gain protector anti-clipping

        for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch) {
            bufferToFill.buffer->copyFrom(ch, bufferToFill.buffer->getStartSample(), mixBus, ch, 0, bufferToFill.numSamples);
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
    }

    void sliderValueChanged(juce::Slider* slider) override {}

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

    std::unique_ptr<MixerPanelComponent> mixerPanel;
    bool showMixer = false;

    bool isPlayingState = false;
    bool metronomeActive = false;
    double currentSampleRate = 44100.0;
    double sampleCounter = 0.0;
    bool playClickSample = false;

    std::unique_ptr<AudioSettingsWindow> settingsWindow;
    juce::UndoManager undoManager;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
