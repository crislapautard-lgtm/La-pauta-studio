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

        // Botón FX (Cargar Plugins VST3)
        pluginButton.setButtonText("FX");
        pluginButton.addListener(this);
        pluginButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
        pluginButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff007acc));
        addAndMakeVisible(pluginButton);

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
        pluginButton.removeListener(this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff2d2d2d));
        g.setColour(juce::Colours::darkgrey);
        g.drawRect(getLocalBounds(), 1);

        g.setColour(juce::Colours::white);
        g.drawText("Pista " + juce::String(id), 10, 22, 60, 20, juce::Justification::left);
    }

    void resized() override
    {
        int yOffset = 15;
        
        muteButton.setBounds(75, yOffset, 30, 40);
        soloButton.setBounds(110, yOffset, 30, 40);
        panSlider.setBounds(145, yOffset, 40, 40);
        pluginButton.setBounds(190, yOffset, 35, 40);
        
        // Forma de onda en el centro
        waveformDisplay->setBounds(235, yOffset, getWidth() - 410, 40);

        // Fader de volumen
        volumeSlider.setBounds(getWidth() - 170, yOffset + 5, 90, 30);
        
        // Botón REC
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
        else if (button == &pluginButton)
        {
            pluginChooser = std::make_unique<juce::FileChooser>("Selecciona un plugin VST3",
                                                             juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                             "*.vst3");
            pluginChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& fc) {
                    auto file = fc.getResult();
                    if (file != juce::File{} && targetAudioTrack) {
                        targetAudioTrack->loadPlugin(file, [this](const juce::String& name) {
                            juce::MessageManager::callAsync([this, name]() {
                                pluginButton.setButtonText(name.substring(0, 4));
                                pluginButton.setTooltip(name);
                            });
                        });
                    }
                });
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
    juce::TextButton pluginButton;
    juce::Slider panSlider;
    juce::Slider volumeSlider;

    juce::AudioFormatManager formatManager;
    std::unique_ptr<WaveformDisplay> waveformDisplay;
    std::unique_ptr<juce::FileChooser> pluginChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackComponent)
};
