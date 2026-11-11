
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "TrackComponent.h"

#include <memory>
#include <vector>

// ============================================================
// LA PAUTA STUDIO
// TIMELINE DE ARREGLOS
// ============================================================

class ArrangementComponent : public juce::Component
{
public:
    ArrangementComponent();

    void paint(juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangementComponent)
};

// ============================================================
// MAIN COMPONENT
// ============================================================

class MainComponent : public juce::Component
{
public:
    explicit MainComponent(AudioPluginAudioProcessor& processor);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    AudioPluginAudioProcessor& audioProcessor;

    // TIMELINE
    ArrangementComponent arrangementContent;
    juce::Viewport arrangementViewport;

    // MIXER
    juce::Component trackContainer;
    juce::Viewport trackViewport;

    std::vector<std::unique_ptr<TrackComponent>> trackComponents;
    std::unique_ptr<TrackComponent> masterChannel;

    // BPM
    juce::Label bpmLabel;
    juce::Slider bpmSlider;

    // TRANSPORTE
    juce::TextButton playButton { "PLAY" };
    juce::TextButton pauseButton { "PAUSE" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton recordButton { "REC" };
    juce::TextButton loopButton { "LOOP" };

    // VST3
    juce::Label pluginBrowserLabel;
    juce::TextButton scanPluginsButton { "ESCANEAR VST3" };
    juce::ComboBox pluginList;
    juce::Label scanStatusLabel;

    // ESTADO
    juce::Label statusLabel;

    // FUNCIONES
    void createTrackComponents();
    void updateTrackLayout();

    void scanVST3Plugins();
    void updatePluginList();

    void showTransportPending(const juce::String& action);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
