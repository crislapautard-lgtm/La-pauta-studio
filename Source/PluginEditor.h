#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "MainComponent.h"

class AudioPluginAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
        : AudioProcessorEditor (&p), audioProcessor (p)
    {
        // Inicializar el componente principal de manera segura con puntero único
        mainComponent = std::make_unique<MainComponent>();
        addAndMakeVisible (mainComponent.get());
        setSize (1000, 700);
    }

    ~AudioPluginAudioProcessorEditor() override {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        if (mainComponent)
            mainComponent->setBounds (getLocalBounds());
    }

private:
    AudioPluginAudioProcessor& audioProcessor;
    std::unique_ptr<MainComponent> mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
