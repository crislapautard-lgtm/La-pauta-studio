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

class MainComponent : public juce::AudioAppComponent,
                      public juce::Button::Listener
{
public:
    MainComponent() {
        // Pista inicial por defecto
        addAudioTrack();

        // Botón Global de PLAY
        playButton.setButtonText("PLAY");
        playButton.addListener(this);
        playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
        playButton.setClickingTogglesState(true);
        addAndMakeVisible(playButton);

        // Botón para Añadir Pista
        addTrackButton.setButtonText("+ Pista");
        addTrackButton.addListener(this);
        addAndMakeVisible(addTrackButton);

        // Botón Guardar Proyecto (.pauta)
        saveButton.setButtonText("Guardar");
        saveButton.addListener(this);
        addAndMakeVisible(saveButton);

        // Botón Abrir Proyecto (.pauta)
        openButton.setButtonText("Abrir");
        openButton.addListener(this);
        addAndMakeVisible(openButton);

        // Botón Toggle Mixer Console
        toggleMixerButton.setButtonText("Mixer");
        toggleMixerButton.addListener(this);
        toggleMixerButton.setClickingTogglesState(true);
        addAndMakeVisible(toggleMixerButton);

        // Inicializar consola de mezcla (oculta por defecto)
        mixerPanel = std::make_unique<MixerPanelComponent>(tracks);
        addChildComponent(mixerPanel.get());

        setSize(950, 700);
        setAudioChannels(2, 2);
    }

    ~MainComponent() override {
        shutdownAudio();
    }

    void addAudioTrack() {
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

            for (int ch = 0; ch < mixBus.getNumChannels(); ++ch) {
                mixBus.addFrom(ch, 0, trackBuffer, ch, 0, bufferToFill.numSamples);
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
        g.fillAll(juce::Colour(0xff1e1e1e)); // Fondo oscuro DAW
        
        g.setColour(juce::Colours::white);
        g.setFont(18.0f);
        g.drawText("LA PAUTA STUDIO - DAW Pro", 20, 15, 300, 30, juce::Justification::left);
    }

    void resized() override {
        // Ubicación de controles superiores
        toggleMixerButton.setBounds(getWidth() - 450, 15, 75, 30);
        openButton.setBounds(getWidth() - 365, 15, 75, 30);
        saveButton.setBounds(getWidth() - 280, 15, 75, 30);
        addTrackButton.setBounds(getWidth() - 195, 15, 80, 30);
        playButton.setBounds(getWidth() - 105, 15, 90, 30);

        int startY = 65;
        int trackHeight = 70;
        int spacing = 10;
        int availableHeightForTracks = getHeight() - startY - 20;

        if (showMixer) {
            availableHeightForTracks -= 210; // Dejamos espacio abajo para la consola
        }

        // Distribuir las pistas visuales verticalmente
        for (size_t i = 0; i < trackComponents.size(); ++i) {
            trackComponents[i]->setBounds(15, startY + (int)i * (trackHeight + spacing), getWidth() - 30, trackHeight);
        }

        // Mostrar u ocultar la consola de mezcla inferior
        if (showMixer && mixerPanel != nullptr) {
            mixerPanel->setBounds(15, getHeight() - 210, getWidth() - 30, 200);
            mixerPanel->setVisible(true);
        } else if (mixerPanel != nullptr) {
            mixerPanel->setVisible(false);
        }
    }

    void buttonClicked (juce::Button* button) override {
        if (button == &playButton) {
            bool shouldPlay = playButton.getToggleState();
            for (auto& track : tracks) {
                if (auto* audioTrack = dynamic_cast<AudioTrack*>(track.get())) {
                    if (shouldPlay) audioTrack->startPlayback();
                    else audioTrack->stopPlayback();
                }
            }
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
        else if (button == &toggleMixerButton) {
            showMixer = toggleMixerButton.getToggleState();
            resized();
        }
    }

private:
    AudioEngine audioEngine;
    std::vector<std::unique_ptr<Track>> tracks;
    std::vector<std::unique_ptr<TrackComponent>> trackComponents;
    
    juce::TextButton playButton;
    juce::TextButton addTrackButton;
    juce::TextButton saveButton;
    juce::TextButton openButton;
    juce::TextButton toggleMixerButton;

    std::unique_ptr<MixerPanelComponent> mixerPanel;
    bool showMixer = false;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
