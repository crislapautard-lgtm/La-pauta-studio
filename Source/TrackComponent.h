#pragma once
#include <JuceHeader.h>
#include "AudioTrack.h"
#include "WaveformDisplay.h"

class TrackComponent : public juce::Component,
                       public juce::Button::Listener,
                       public juce::Slider::Listener
{
public:
    TrackComponent(int trackId, AudioTrack* audioTrack)
        : id(trackId), targetAudioTrack(audioTrack)
    {
        // Inicializar el gestor de formatos y el visor de ondas
        formatManager.registerBasicFormats();
        waveformDisplay = std::make_unique<WaveformDisplay>(formatManager);
        addAndMakeVisible(waveformDisplay.get());

        // Botón MUTE
        muteButton.setButtonText("M");
        muteButton.setClickingTogglesState(true);
        muteButton.addListener(this);
        muteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
        addAndMakeVisible(muteButton);

        // Botón SOLO
        soloButton.setButtonText("S");
        soloButton.setClickingTogglesState(true);
        soloButton.addListener(this);
        soloButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
        addAndMakeVisible(soloButton);

        // Perilla de PANEO
        panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        panSlider.setRange(-1.0, 1.0, 0.01);
        panSlider.setValue(0.0);
        panSlider.addListener(this);
        panSlider.setTooltip("Paneo (L / R)");
        addAndMakeVisible(panSlider);

        // Fader de VOLUMEN
        volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
        volumeSlider.setRange(0.0, 1.0, 0.01); 
        volumeSlider.setValue(0.8);            
        volumeSlider.addListener(this);
        addAndMakeVisible(volumeSlider);

        // Botón de RECORD ARM (REC)
        recordButton.setButtonText("REC");
        recordButton.setClickingTogglesState(true);
        recordButton.addListener(this);
        recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        recordButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
        addAndMakeVisible(recordButton);
    }

    ~TrackComponent() override {
        recordButton.removeListener(this);
        muteButton.removeListener(this);
        soloButton.removeListener(this);
        volumeSlider.removeListener(this);
        panSlider.removeListener(this);
    }

    void paint (juce::Graphics& g) override
    {
        // Fondo y borde de la pista
        g.fillAll(juce::Colour(0xff2d2d2d));
        g.setColour(juce::Colours::darkgrey);
        g.drawRect(getLocalBounds(), 1);

        // Nombre de la pista
        g.setColour(juce::Colours::white);
        g.drawText("Pista " + juce::String(id), 10, 22, 60, 20, juce::Justification::left);
    }

    void resized() override
    {
        int yOffset = 15;
        
        // Distribución exacta de controles en la pista (70px de alto)
        muteButton.setBounds(75, yOffset, 30, 40);
        soloButton.setBounds(110, yOffset, 30, 40);
        panSlider.setBounds(145, yOffset, 40, 40);
        
        // La forma de onda ocupa el espacio central principal
        waveformDisplay->setBounds(190, yOffset, getWidth() - 365, 40);

        // Fader de volumen a la derecha de la onda
        volumeSlider.setBounds(getWidth() - 170, yOffset + 5, 90, 30);
        
        // Botón REC en la esquina extrema derecha
        recordButton.setBounds(getWidth() - 75, yOffset, 60, 40);
    }

    void buttonClicked (juce::Button* button) override
    {
        if (button == &recordButton)
        {
            auto desktopDir = juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
            auto wavFile = desktopDir.getChildFile("Pista_" + juce::String(id) + ".wav");

            if (recordButton.getToggleState())
            {
                if (targetAudioTrack)
                    targetAudioTrack->startRecording(wavFile, 44100.0);
            }
            else
            {
                if (targetAudioTrack)
                {
                    targetAudioTrack->stopRecording();
                    // Al detener la grabación, cargamos automáticamente la forma de onda
                    waveformDisplay->setFile(wavFile);
                }
            }
        }
        else if (button == &muteButton)
        {
            if (targetAudioTrack)
                targetAudioTrack->setMute(muteButton.getToggleState());
        }
        else if (button == &soloButton)
        {
            if (targetAudioTrack)
                targetAudioTrack->setSolo(soloButton.getToggleState());
        }
    }

    void sliderValueChanged (juce::Slider* slider) override
    {
        if (slider == &volumeSlider)
        {
            if (targetAudioTrack)
                targetAudioTrack->setVolume((float) volumeSlider.getValue());
        }
        else if (slider == &panSlider)
        {
            if (targetAudioTrack)
                targetAudioTrack->setPan((float) panSlider.getValue());
        }
    }

private:
    int id;
    AudioTrack* targetAudioTrack = nullptr;
    
    juce::TextButton recordButton;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::Slider panSlider;
    juce::Slider volumeSlider;

    juce::AudioFormatManager formatManager;
    std::unique_ptr<WaveformDisplay> waveformDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackComponent)
};
