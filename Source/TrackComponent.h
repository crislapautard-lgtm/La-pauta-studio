#pragma once
#include <JuceHeader.h>
#include "AudioTrack.h"

class TrackComponent : public juce::Component,
                       public juce::Button::Listener,
                       public juce::Slider::Listener
{
public:
    TrackComponent(int trackId, AudioTrack* audioTrack)
        : id(trackId), targetAudioTrack(audioTrack)
    {
        // 1. Nombre de pista / Indicador
        // (Se dibuja directo en paint)

        // 2. Botón MUTE
        muteButton.setButtonText("M");
        muteButton.setClickingTogglesState(true);
        muteButton.addListener(this);
        muteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
        addAndMakeVisible(muteButton);

        // 3. Botón SOLO
        soloButton.setButtonText("S");
        soloButton.setClickingTogglesState(true);
        soloButton.addListener(this);
        soloButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
        addAndMakeVisible(soloButton);

        // 4. Perilla de PANEO (Izquierda / Derecha)
        panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        panSlider.setRange(-1.0, 1.0, 0.01); // -1.0 es Full Izquierda, 0.0 Centro, 1.0 Full Derecha
        panSlider.setValue(0.0);
        panSlider.addListener(this);
        panSlider.setTooltip("Paneo (Izquierda / Derecha)");
        addAndMakeVisible(panSlider);

        // 5. Fader de VOLUMEN
        volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
        volumeSlider.setRange(0.0, 1.0, 0.01); 
        volumeSlider.setValue(0.8);            
        volumeSlider.addListener(this);
        addAndMakeVisible(volumeSlider);

        // 6. Botón de RECORD ARM (REC)
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

        // Texto del nombre de la pista
        g.setColour(juce::Colours::white);
        g.drawText("Pista " + juce::String(id), 10, 22, 60, 20, juce::Justification::left);
    }

    void resized() override
    {
        int yOffset = 15;
        
        // Distribuimos los controles horizontalmente dentro de la pista (70px de alto)
        muteButton.setBounds(75, yOffset, 30, 40);
        soloButton.setBounds(110, yOffset, 30, 40);
        
        panSlider.setBounds(150, yOffset, 40, 40);
        
        // Fader de volumen ocupa el espacio central flexible
        volumeSlider.setBounds(200, yOffset + 5, getWidth() - 280, 30);
        
        // Botón REC a la extrema derecha
        recordButton.setBounds(getWidth() - 70, yOffset, 55, 40);
    }

    void buttonClicked (juce::Button* button) override
    {
        if (button == &recordButton)
        {
            if (recordButton.getToggleState())
            {
                auto desktopDir = juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
                auto wavFile = desktopDir.getChildFile("Pista_" + juce::String(id) + ".wav");

                if (targetAudioTrack)
                    targetAudioTrack->startRecording(wavFile, 44100.0);
            }
            else
            {
                if (targetAudioTrack)
                    targetAudioTrack->stopRecording();
            }
        }
        else if (button == &muteButton)
        {
            bool isMuted = muteButton.getToggleState();
            if (targetAudioTrack)
                targetAudioTrack->setMute(isMuted);
        }
        else if (button == &soloButton)
        {
            bool isSolo = soloButton.getToggleState();
            if (targetAudioTrack)
                targetAudioTrack->setSolo(isSolo);
        }
    }

    void sliderValueChanged (juce::Slider* slider) override
    {
        if (slider == &volumeSlider)
        {
            float gain = (float) volumeSlider.getValue();
            if (targetAudioTrack)
                targetAudioTrack->setVolume(gain);
        }
        else if (slider == &panSlider)
        {
            float pan = (float) panSlider.getValue();
            if (targetAudioTrack)
                targetAudioTrack->setPan(pan);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackComponent)
};
