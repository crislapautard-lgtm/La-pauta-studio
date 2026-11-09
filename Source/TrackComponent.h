#pragma once
#include <JuceHeader.h>
#include "AudioTrack.h"

class TrackComponent : public juce::Component,
                       public juce::Button::Listener
{
public:
    TrackComponent(int trackId, Track* track) : id(trackId), targetTrack(track)
    {
        // Botón para cargar archivo de audio WAV
        loadAudioButton.setButtonText("Cargar Audio");
        loadAudioButton.addListener(this);
        addAndMakeVisible(loadAudioButton);

        // Botón para cargar plugin VST3
        loadPluginButton.setButtonText("Cargar VST3");
        loadPluginButton.addListener(this);
        addAndMakeVisible(loadPluginButton);

        // Botón para abrir la interfaz gráfica (Editor) del VST3
        openEditorButton.setButtonText("Abrir VST");
        openEditorButton.addListener(this);
        addAndMakeVisible(openEditorButton);
    }

    ~TrackComponent() override {
        loadAudioButton.removeListener(this);
        loadPluginButton.removeListener(this);
        openEditorButton.removeListener(this);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff2b2b2b)); // Fondo de la pista en el timeline
        g.setColour(juce::Colour(0xff444444));
        g.drawRect(getLocalBounds(), 1);

        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText("Pista " + juce::String(id), 15, 15, 80, 40, juce::Justification::left, true);
    }

    void resized() override {
        int w = getWidth();
        
        // Posicionar los botones de control dentro de la pista en el timeline
        loadAudioButton.setBounds(w - 330, 20, 100, 30);
        loadPluginButton.setBounds(w - 220, 20, 100, 30);
        openEditorButton.setBounds(w - 110, 20, 95, 30);
    }

    void buttonClicked(juce::Button* button) override {
        auto* audioTrack = dynamic_cast<AudioTrack*>(targetTrack);
        if (audioTrack == nullptr) return;

        if (button == &loadAudioButton) {
            fileChooser = std::make_unique<juce::FileChooser>("Seleccionar archivo de audio WAV",
                                                              juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                                              "*.wav");
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [audioTrack](const juce::FileChooser& fc) {
                    auto file = fc.getResult();
                    if (file != juce::File{}) {
                        audioTrack->loadAudioFile(file);
                    }
                });
        }
        else if (button == &loadPluginButton) {
            fileChooser = std::make_unique<juce::FileChooser>("Seleccionar Plugin VST3",
                                                              juce::File::getSpecialLocation(juce::File::VST3PlugInFolderPath),
                                                              "*.vst3");
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [audioTrack](const juce::FileChooser& fc) {
                    auto file = fc.getResult();
                    if (file != juce::File{}) {
                        audioTrack->loadPlugin(file);
                    }
                });
        }
        else if (button == &openEditorButton) {
            audioTrack->openPluginEditor();
        }
    }

private:
    int id;
    Track* targetTrack = nullptr;

    juce::TextButton loadAudioButton;
    juce::TextButton loadPluginButton;
    juce::TextButton openEditorButton;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackComponent)
};
