#include "TrackComponent.h"

TrackComponent::TrackComponent(const juce::String& trackName) : name(trackName)
{
    // Configuración de la etiqueta con el nombre de la pista
    titleLabel.setText(name, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    // Fader de Volumen
    volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.8);
    volumeSlider.addListener(this);
    addAndMakeVisible(volumeSlider);

    // Perilla de Paneo
    panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange(-1.0, 1.0, 0.01); // -1 Izquierda, 1 Derecha
    panSlider.setValue(0.0);
    panSlider.addListener(this);
    addAndMakeVisible(panSlider);

    // Botón Mute
    muteButton.setButtonText("M");
    muteButton.setClickingTogglesState(true);
    muteButton.addListener(this);
    muteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    addAndMakeVisible(muteButton);

    // Botón Solo
    soloButton.setButtonText("S");
    soloButton.setClickingTogglesState(true);
    soloButton.addListener(this);
    soloButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgoldenrod);
    addAndMakeVisible(soloButton);

    // Botón Record
    recordButton.setButtonText("REC");
    recordButton.setClickingTogglesState(true);
    recordButton.addListener(this);
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::maroon);
    addAndMakeVisible(recordButton);

    setSize(120, 400); // Tamaño base de una tira de canal en la mezcladora
}

TrackComponent::~TrackComponent() {}

void TrackComponent::paint(juce::Graphics& g)
{
    // Fondo de la pista estilo consola de mezcla profesional
    g.fillAll(juce::Colour(0xff2d2d2d));
    
    // Borde elegante alrededor de la pista
    g.setColour(juce::Colour(0xff404040));
    g.drawRect(getLocalBounds(), 1);
}

void TrackComponent::resized()
{
    auto area = getLocalBounds().reduced(10);

    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    // Botones de Mute, Solo y Record arriba en fila
    auto topButtons = area.removeFromTop(25);
    int buttonWidth = topButtons.getWidth() / 3;
    muteButton.setBounds(topButtons.removeFromLeft(buttonWidth).reduced(2));
    soloButton.setBounds(topButtons.removeFromLeft(buttonWidth).reduced(2));
    recordButton.setBounds(topButtons.reduced(2));

    area.removeFromTop(15);

    // Perilla de paneo
    panSlider.setBounds(area.removeFromTop(60).reduced(20, 0));

    area.removeFromTop(10);

    // El fader de volumen ocupa el resto del espacio hacia abajo
    volumeSlider.setBounds(area);
}

void TrackComponent::buttonClicked(juce::Button* button)
{
    if (button == &muteButton) {
        // Aquí programaremos la lógica de silencio
    }
    else if (button == &soloButton) {
        // Aquí programaremos la lógica de solo
    }
    else if (button == &recordButton) {
        // Aquí activaremos el estado de grabación de la pista
    }
}

void TrackComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &volumeSlider) {
        float val = (float)volumeSlider.getValue();
        // Aquí conectaremos el volumen con el motor de audio
    }
    else if (slider == &panSlider) {
        float panVal = (float)panSlider.getValue();
        // Aquí conectaremos el paneo estéreo
    }
}
