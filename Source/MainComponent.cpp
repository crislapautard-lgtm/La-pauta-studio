
#include "MainComponent.h"

// ============================================================
// LA PAUTA STUDIO
// MAIN COMPONENT
// NAVEGADOR VST3 ASINCRONO
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
            static_cast<float>(bpmSlider.getValue());
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
    // TITULO DEL NAVEGADOR
    // ========================================================

    pluginBrowserLabel.setText(
        "VST3 PLUGIN BROWSER - 64 BIT",
        juce::dontSendNotification
    );

    pluginBrowserLabel.setFont(
        juce::Font(
            juce::FontOptions(18.0f).withStyle("Bold")
        )
    );

    pluginBrowserLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xff55c8ff)
    );

    addAndMakeVisible(pluginBrowserLabel);

    // ========================================================
    // BOTON DE ESCANEO
    // ========================================================

    scanPluginsButton.setButtonText(
        "ESCANEAR VST3"
    );

    scanPluginsButton.onClick = [this]
    {
        scanVST3Plugins();
    };

    addAndMakeVisible(scanPluginsButton);

    // ========================================================
    // LISTA DE PLUGINS
    // ========================================================

    pluginList.setTextWhenNothingSelected(
        "Plugins encontrados apareceran aqui"
    );

    addAndMakeVisible(pluginList);

    // ========================================================
    // ESTADO
    // ========================================================

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
// ESCANEAR VST3 EN SEGUNDO PLANO
// ============================================================

void MainComponent::scanVST3Plugins()
{
    auto& manager =
        audioProcessor.getPluginManager();

    if (manager.isScanning())
        return;

    scanPluginsButton.setEnabled(false);

    scanStatusLabel.setText(
        "Escaneando plugins VST3...",
        juce::dontSendNotification
    );

    pluginList.clear(
        juce::dontSendNotification
    );

    juce::Component::SafePointer<MainComponent> safeThis(this);

    manager.scanPluginsAsync(
        [safeThis]()
        {
            if (safeThis == nullptr)
                return;

            safeThis->updatePluginList();
        }
    );
}

// ============================================================
// ACTUALIZAR LISTA AL TERMINAR EL ESCANEO
// ============================================================

void MainComponent::updatePluginList()
{
    auto& manager =
        audioProcessor.getPluginManager();

    const auto& plugins =
        manager.getKnownPlugins().getTypes();

    pluginList.clear(
        juce::dontSendNotification
    );

    for (int i = 0; i < plugins.size(); ++i)
    {
        const auto& description =
            plugins.getReference(i);

        juce::String itemName =
            description.name;

        if (description.manufacturerName.isNotEmpty())
        {
            itemName += " - ";
            itemName += description.manufacturerName;
        }

        pluginList.addItem(
            itemName,
            i + 1
        );
    }

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
    bpmLabel.setBounds(
        20, 85, 100, 25
    );

    bpmSlider.setBounds(
        20, 110, 100, 110
    );

    pluginBrowserLabel.setBounds(
        160, 85, 350, 30
    );

    scanPluginsButton.setBounds(
        160, 125, 180, 35
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
}
