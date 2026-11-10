
#include "MainComponent.h"

// ============================================================
// LA PAUTA STUDIO
// MAIN COMPONENT
// NAVEGADOR VST3
// ============================================================

MainComponent::MainComponent(AudioPluginAudioProcessor& p)
    : audioProcessor(p)
{
    setSize(1000, 700);

    // ========================================================
    // BPM
    // ========================================================

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

    bpmSlider.onValueChange = [this]
    {
        audioProcessor.currentBPM =
            bpmSlider.getValue();
    };

    bpmLabel.setText(
        "BPM",
        juce::dontSendNotification
    );

    bpmLabel.setJustificationType(
        juce::Justification::centred
    );

    addAndMakeVisible(bpmLabel);

    // ========================================================
    // NAVEGADOR VST3
    // ========================================================

    pluginBrowserLabel.setText(
        "VST3 PLUGIN BROWSER",
        juce::dontSendNotification
    );

    pluginBrowserLabel.setFont(
        juce::Font(18.0f, juce::Font::bold)
    );

    pluginBrowserLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xff55c8ff)
    );

    addAndMakeVisible(pluginBrowserLabel);

    // BOTON DE ESCANEO

    scanPluginsButton.setButtonText(
        "ESCANEAR VST3"
    );

    scanPluginsButton.onClick = [this]
    {
        scanVST3Plugins();
    };

    addAndMakeVisible(scanPluginsButton);

    // LISTA DE PLUGINS

    pluginList.setTextWhenNothingSelected(
        "Plugins encontrados apareceran aqui"
    );

    addAndMakeVisible(pluginList);

    // ESTADO DEL ESCANEO

    scanStatusLabel.setText(
        "Pulsa ESCANEAR VST3 para comenzar",
        juce::dontSendNotification
    );

    scanStatusLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xffaab7c8)
    );

    addAndMakeVisible(scanStatusLabel);
}

// ============================================================
// DESTRUCTOR
// ============================================================

MainComponent::~MainComponent() = default;

// ============================================================
// ESCANEAR PLUGINS VST3
// ============================================================

void MainComponent::scanVST3Plugins()
{
    scanPluginsButton.setEnabled(false);

    scanStatusLabel.setText(
        "Escaneando plugins VST3...",
        juce::dontSendNotification
    );

    pluginList.clear(
        juce::dontSendNotification
    );

    // Administrador de plugins del procesador

    auto& manager = audioProcessor.getPluginManager();

    // Ejecutar escaneo

    manager.scanPlugins();

    // Obtener plugins encontrados

    const auto& plugins =
        manager.getKnownPlugins().getTypes();

    // Llenar lista

    for (int i = 0; i < plugins.size(); ++i)
    {
        const auto& description =
            plugins.getReference(i);

        juce::String itemName =
            description.name;

        if (description.manufacturer.isNotEmpty())
        {
            itemName += "  -  ";
            itemName += description.manufacturer;
        }

        pluginList.addItem(
            itemName,
            i + 1
        );
    }

    // Mostrar resultado

    scanStatusLabel.setText(
        "Escaneo terminado: "
        + juce::String(plugins.size())
        + " plugin(s) encontrado(s).",
        juce::dontSendNotification
    );

    scanPluginsButton.setEnabled(true);
}

// ============================================================
// PAINT
// ============================================================

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(
        juce::Colour(0xff10141c)
    );

    // TITULO

    g.setColour(
        juce::Colours::white
    );

    g.setFont(24.0f);

    g.drawText(
        "LA PAUTA STUDIO",
        20,
        10,
        400,
        35,
        juce::Justification::left
    );

    // SUBTITULO

    g.setColour(
        juce::Colour(0xff36a9ff)
    );

    g.setFont(15.0f);

    g.drawText(
        "16 TRACK DIGITAL AUDIO WORKSTATION",
        20,
        45,
        450,
        25,
        juce::Justification::left
    );

    // LINEA DECORATIVA

    g.setColour(
        juce::Colour(0xff263b55)
    );

    g.drawHorizontalLine(
        75,
        20.0f,
        static_cast<float>(getWidth() - 20)
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
        100,
        25
    );

    bpmSlider.setBounds(
        20,
        110,
        100,
        110
    );

    // NAVEGADOR VST3

    pluginBrowserLabel.setBounds(
        160,
        85,
        350,
        30
    );

    scanPluginsButton.setBounds(
        160,
        125,
        180,
        35
    );

    pluginList.setBounds(
        160,
        175,
        juce::jmax(250, getWidth() - 190),
        35
    );

    scanStatusLabel.setBounds(
        160,
        220,
        juce::jmax(250, getWidth() - 190),
        30
    );

    // AREA DE PISTAS
    // Se conectara en la siguiente etapa.

    juce::ignoreUnused(area);
}
