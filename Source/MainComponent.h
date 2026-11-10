
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "TrackComponent.h"

#include <memory>
#include <vector>

class MainComponent : public juce::Component
{
public:
    explicit MainComponent(AudioPluginAudioProcessor& processor);

    ~MainComponent() override;

    void paint(juce::Graphics& g) override;

    void resized() override;

private:
    AudioPluginAudioProcessor& audioProcessor;

    // PISTAS
    juce::Viewport trackViewport;
    juce::Component trackContainer;

    std::vector<std::unique_ptr<TrackComponent>> trackComponents;

    juce::Component masterTrack;

    // BPM
    juce::Label bpmLabel;
    juce::Slider bpmSlider;

    // MASTER
    juce::Label masterLabel;
    juce::Slider masterVolumeSlider;

    // NAVEGADOR VST3
    juce::Label pluginBrowserLabel;
    juce::TextButton scanPluginsButton;
    juce::ComboBox pluginList;
    juce::Label scanStatusLabel;

    // FUNCIONES
    void createTrackComponents();
    void updateTrackLayout();

    void scanVST3Plugins();
    void updatePluginList();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
