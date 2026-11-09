#pragma once
#include <JuceHeader.h>
#include "Track.h"
#include "AudioTrack.h"
#include <vector>
#include <memory>

class MixerStripComponent : public juce::Component,
                            public juce::Button::Listener,
                            public juce::Slider::Listener
{
public:
    MixerStripComponent(int trackId, Track* track) : id(trackId), targetTrack(track)
    {
        // Botón Mute
        muteButton.setButtonText("M");
        muteButton.setClickingTogglesState(true);
        muteButton.addListener(this);
        muteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
        addAndMakeVisible(muteButton);

        // Botón Solo
        soloButton.setButtonText("S");
        soloButton.setClickingTogglesState(true);
        soloButton.addListener(this);
        soloButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
        addAndMakeVisible(soloButton);

        // Perilla de Paneo
        panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        panSlider.setRange(-1.0, 1.0, 0.01);
        panSlider.setValue(0.0);
        panSlider.addListener(this);
        addAndMakeVisible(panSlider);

        // Fader de Volumen vertical estilo consola
        volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
        volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        volumeSlider.setRange(0.0, 1.0, 0.01);
        volumeSlider.setValue(0.8);
        volumeSlider.addListener(this);
        addAndMakeVisible(volumeSlider);
    }

    ~MixerStripComponent() override {
        muteButton.removeListener(this);
        soloButton.removeListener(this);
        panSlider.removeListener(this);
        volumeSlider.removeListener(this);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff222222)); // Fondo gris oscuro de canal de mezcla
        g.setColour(juce::Colour(0xff333333));
        g.drawRect(getLocalBounds(), 1);

        // Nombre de la pista en la cabecera del canal
        g.setColour(juce::Colours::white);
        g.setFont(12.0f);
        g.drawText("Pista " + juce::String(id), 5, 5, getWidth() - 10, 20, juce::Justification::centred, true);
    }

    void resized() override {
        int w = getWidth();
        int h = getHeight();

        muteButton.setBounds(10, 30, w - 20, 22);
        soloButton.setBounds(10, 57, w - 20, 22);
        panSlider.setBounds((w - 40) / 2, 85, 40, 40);
        
        // El fader de volumen ocupa el espacio vertical central/inferior
        volumeSlider.setBounds(15, 135, w - 30, h - 150);
    }

    void buttonClicked(juce::Button* button) override {
        if (auto* audioTrack = dynamic_cast<AudioTrack*>(targetTrack)) {
            if (button == &muteButton) {
                audioTrack->setMute(muteButton.getToggleState());
            } else if (button == &soloButton) {
                audioTrack->setSolo(soloButton.getToggleState());
            }
        }
    }

    void sliderValueChanged(juce::Slider* slider) override {
        if (auto* audioTrack = dynamic_cast<AudioTrack*>(targetTrack)) {
            if (slider == &volumeSlider) {
                audioTrack->setVolume((float)volumeSlider.getValue());
            } else if (slider == &panSlider) {
                audioTrack->setPan((float)panSlider.getValue());
            }
        }
    }

private:
    int id;
    Track* targetTrack = nullptr;

    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::Slider panSlider;
    juce::Slider volumeSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerStripComponent)
};

class MixerPanelComponent : public juce::Component
{
public:
    MixerPanelComponent(const std::vector<std::unique_ptr<Track>>& tracksList) : tracks(tracksList)
    {
        updateStripping();
    }

    ~MixerPanelComponent() override = default;

    void updateStripping() {
        strips.clear();
        for (size_t i = 0; i < tracks.size(); ++i) {
            auto strip = std::make_unique<MixerStripComponent>((int)i + 1, tracks[i].get());
            strips.push_back(std::move(strip));
            addAndMakeVisible(strips.back().get());
        }
        resized();
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff181818)); // Fondo general de la consola
        g.setColour(juce::Colour(0xff2d2d2d));
        g.drawRect(getLocalBounds(), 1);

        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText("CONSOLA DE MEZCLA (Mixer Master)", 15, 10, 300, 20, juce::Justification::left);
    }

    void resized() override {
        int startX = 15;
        int startY = 35;
        int stripWidth = 80;
        int stripHeight = getHeight() - 50;
        int spacing = 6;

        for (size_t i = 0; i < strips.size(); ++i) {
            strips[i]->setBounds(startX + (int)i * (stripWidth + spacing), startY, stripWidth, stripHeight);
        }
    }

private:
    const std::vector<std::unique_ptr<Track>>& tracks;
    std::vector<std::unique_ptr<MixerStripComponent>> strips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEak_DETECTOR(MixerPanelComponent)
};
