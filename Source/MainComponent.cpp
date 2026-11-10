
#include "MainComponent.h"

// ============================================================
// LA PAUTA STUDIO
// MainComponent.cpp
// JUCE / C++
// ============================================================

MainComponent::MainComponent(AudioPluginAudioProcessor& p)
    : audioProcessor(p)
{
    setSize(1000, 700);

    // BPM SLIDER
    addAndMakeVisible(bpmSlider);

    bpmSlider.setRange(40.0, 240.0, 1.0);
    bpmSlider.setValue(audioProcessor.currentBPM);

    bpmSlider.setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag
    );

    bpmSlider.setTextBoxStyle(
        juce::Slider::TextBoxBelow,
        false,
        70,
        20
    );

    // BPM LABEL
    bpmLabel.setText(
        "BPM",
        juce::dontSendNotification
    );

    bpmLabel.setJustificationType(
        juce::Justification::centred
    );

    addAndMakeVisible(bpmLabel);
}

// ============================================================
// DESTRUCTOR
// ============================================================

MainComponent::~MainComponent()
{
}

// ============================================================
// PAINT
// ============================================================

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10141c));

    g.setColour(juce::Colours::white);
    g.setFont(24.0f);

    g.drawText(
        "LA PAUTA STUDIO",
        20,
        10,
        400,
        35,
        juce::Justification::left
    );

    g.setColour(juce::Colour(0xff36a9ff));

    g.setFont(15.0f);

    g.drawText(
        "16 TRACK DIGITAL AUDIO WORKSTATION",
        20,
        45,
        450,
        25,
        juce::Justification::left
    );
}

// ============================================================
// RESIZED
// ============================================================

void MainComponent::resized()
{
    auto area = getLocalBounds();

    // BPM
    bpmLabel.setBounds(
        20,
        85,
        70,
        25
    );

    bpmSlider.setBounds(
        20,
        110,
        100,
        100
    );

    // TRACKS
    for (int i = 0; i < audioProcessor.numTracks; ++i)
    {
        // Aquí se organizarán las pistas de audio.
    }

    // SUBGROUPS
    for (int i = 0; i < audioProcessor.numSubgroups; ++i)
    {
        // Aquí se organizarán los subgrupos.
    }

    // AUX CHANNELS
    for (int i = 0; i < audioProcessor.numAuxes; ++i)
    {
        // Aquí se organizarán los canales auxiliares.
    }
}
