#include "MainComponent.h"

MainComponent::MainComponent()
{
    // Configurar título del DAW
    titleLabel.setText ("La Pauta Studio - Professional DAW", juce::dontSendNotification);
    titleLabel.setFont (juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType (juce::Justification::center);
    addAndMakeVisible (titleLabel);

    // Botón de Play
    playButton.setButtonText ("PLAY");
    playButton.addListener (this);
    playButton.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgreen);
    addAndMakeVisible (playButton);

    // Botón de Stop
    stopButton.setButtonText ("STOP");
    stopButton.addListener (this);
    stopButton.setColour (juce::TextButton::buttonColourId, juce::Colours::darkred);
    addAndMakeVisible (stopButton);

    // Perilla / Slider de Volumen Master
    masterVolumeSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterVolumeSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    masterVolumeSlider.setRange (0.0, 1.0, 0.01);
    masterVolumeSlider.setValue (0.8);
    addAndMakeVisible (masterVolumeSlider);

    // Etiqueta de Volumen
    volumeLabel.setText ("Master Volume", juce::dontSendNotification);
    volumeLabel.setJustificationType (juce::Justification::center);
    volumeLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (volumeLabel);

    setSize (900, 600);
}

MainComponent::~MainComponent()
{
}

void MainComponent::paint (juce::Graphics& g)
{
    // Fondo general estilo estudio oscuro profesional
    g.fillAll (juce::Colour (0xff1e1e1e));

    // Panel superior para la barra de transporte
    g.setColour (juce::Colour (0xff2d2d2d));
    g.fillRect (0, 0, getWidth(), 70);

    // Línea divisoria elegante
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine (70, 0.0f, (float)getWidth());
}

void MainComponent::resized()
{
    // Posicionar elementos en la interfaz de manera ordenada
    titleLabel.setBounds (20, 20, 400, 30);

    // Controles de transporte arriba a la derecha
    playButton.setBounds (getWidth() - 200, 15, 80, 40);
    stopButton.setBounds (getWidth() - 110, 15, 80, 40);

    // Master volume al centro o en un panel lateral de mezcla
    masterVolumeSlider.setBounds (getWidth() - 150, 150, 100, 100);
    volumeLabel.setBounds (getWidth() - 160, 255, 120, 20);
}

void MainComponent::buttonClicked (juce::Button* button)
{
    if (button == &playButton)
    {
        // Acción de Play en el DAW
        DBG("DAW Play Activado");
    }
    else if (button == &stopButton)
    {
        // Acción de Stop en el DAW
        DBG("DAW Stop Activado");
    }
}
