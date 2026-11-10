
#pragma once

#include <JuceHeader.h>

// ============================================================
// LA PAUTA STUDIO
// AUDIO SETTINGS WINDOW
// ============================================================

class AudioSettingsWindow : public juce::DocumentWindow
{
public:

    explicit AudioSettingsWindow(
        juce::AudioDeviceManager& deviceManager)

        : juce::DocumentWindow(
            "La Pauta Studio - Configuracion de Audio",
            juce::Desktop::getInstance()
                .getDefaultLookAndFeel()
                .findColour(
                    juce::ResizableWindow::backgroundColourId),
            juce::DocumentWindow::allButtons)
    {
        auto* settingsComponent =
            new juce::AudioDeviceSelectorComponent(
                deviceManager,
                0,
                256,
                0,
                256,
                true,
                true,
                true,
                false);

        setContentOwned(settingsComponent, true);

        setResizable(true, true);
        centreWithSize(600, 500);
        setVisible(true);
    }

    ~AudioSettingsWindow() override = default;

    void closeButtonPressed() override
    {
        // Ocultar la ventana sin destruir un objeto
        // que podría estar administrado por otra clase.
        setVisible(false);
    }

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        AudioSettingsWindow)
};
