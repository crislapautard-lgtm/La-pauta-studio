#pragma once
#include <JuceHeader.h>

class AudioSettingsWindow : public juce::DocumentWindow
{
public:
    AudioSettingsWindow(juce::AudioDeviceManager& deviceManager)
        : DocumentWindow("Configuracion de Audio y Hardware",
                         juce::Desktop::getInstance().getDefaultLookAndFeel()
                             .findColour(juce::ResizableWindow::backgroundColourId),
                         DocumentWindow::allButtons)
    {
        // Creamos el panel de configuración nativo de JUCE conectado al gestor de audio global
        audioSettingsComp = std::make_unique<juce::AudioDeviceSelectorComponent>(
            deviceManager,
            0,     // min input channels
            256,   // max input channels
            0,     // min output channels
            256,   // max output channels
            true,  // include MIDI inputs
            true,  // include MIDI outputs
            true,  // use stereo pairs
            false  // hide advanced options
        );

        setContentOwned(audioSettingsComp.release(), true);
        setResizable(true, true);
        centreWithSize(500, 450);
        setVisible(true);
    }

    ~AudioSettingsWindow() override = default;

    void closeButtonPressed() override {
        // Al cerrar la ventana, se destruye y guarda la preferencia automáticamente
        delete this;
    }

private:
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSettingsComp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsWindow)
};
