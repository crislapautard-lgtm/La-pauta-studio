
#include "MainComponent.h"

// ============================================================
// COLORES
// ============================================================

namespace LaPautaColours
{
    const juce::Colour background  { 0xff101722 };
    const juce::Colour panel       { 0xff182536 };
    const juce::Colour panelLight  { 0xff203249 };
    const juce::Colour blue        { 0xff28a9e0 };
    const juce::Colour blueLight   { 0xff55c8ff };
    const juce::Colour text        { 0xffe8f2fc };
    const juce::Colour muted       { 0xff91a6bb };
    const juce::Colour grid        { 0xff2b4057 };
    const juce::Colour green       { 0xff32bd83 };
    const juce::Colour red         { 0xffe65c68 };
}

// ============================================================
// ARRANGEMENT
// ============================================================

ArrangementComponent::ArrangementComponent()
{
    setSize(1900, 32 + (16 * 30));
}

void ArrangementComponent::paint(juce::Graphics& g)
{
    const int labelWidth = 145;
    const int rulerHeight = 32;
    const int laneHeight = 30;

    g.fillAll(LaPautaColours::background);

    // REGLA SUPERIOR
    g.setColour(LaPautaColours::panelLight);
    g.fillRect(0, 0, getWidth(), rulerHeight);

    g.setColour(LaPautaColours::text);
    g.setFont(13.0f);

    g.drawText(
        "COMPAS",
        8, 0, labelWidth - 12, rulerHeight,
        juce::Justification::centredLeft
    );

    // COMPASES
    const int pixelsPerBar = 100;

    for (int bar = 1; bar <= 18; ++bar)
    {
        const int x = labelWidth + ((bar - 1) * pixelsPerBar);

        g.setColour(LaPautaColours::muted);

        g.drawText(
            juce::String(bar),
            x + 5, 0, 40, rulerHeight,
            juce::Justification::centredLeft
        );

        g.setColour(LaPautaColours::grid);

        g.drawVerticalLine(
            x,
            static_cast<float>(rulerHeight),
            static_cast<float>(getHeight())
        );
    }

    // PISTAS
    for (int i = 0; i < 16; ++i)
    {
        const int y = rulerHeight + (i * laneHeight);

        g.setColour(
            i % 2 == 0
                ? juce::Colour(0xff192636)
                : juce::Colour(0xff152231)
        );

        g.fillRect(
            0, y, getWidth(), laneHeight
        );

        g.setColour(LaPautaColours::grid);

        g.drawHorizontalLine(
            y,
            0.0f,
            static_cast<float>(getWidth())
        );

        g.setColour(LaPautaColours::blueLight);

        g.fillRect(
            0, y + 1, 3, laneHeight - 2
        );

        g.setColour(LaPautaColours::text);
        g.setFont(12.0f);

        g.drawText(
            "PISTA " + juce::String(i + 1),
            10, y, labelWidth - 18, laneHeight,
            juce::Justification::centredLeft
        );

        g.setColour(LaPautaColours::muted);
        g.setFont(10.0f);

        g.drawText(
            "Sin clips",
            labelWidth + 12,
            y,
            100,
            laneHeight,
            juce::Justification::centredLeft
        );
    }

    g.setColour(LaPautaColours::grid);

    g.drawVerticalLine(
        labelWidth,
        0.0f,
        static_cast<float>(getHeight())
    );
}

// ============================================================
// MAIN COMPONENT
// ============================================================

MainComponent::MainComponent(AudioPluginAudioProcessor& p)
    : audioProcessor(p)
{
    setSize(1200, 800);

    // --------------------------------------------------------
    // BPM
    // --------------------------------------------------------

    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setColour(
        juce::Label::textColourId,
        LaPautaColours::blueLight
    );
    bpmLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(bpmLabel);
    addAndMakeVisible(bpmSlider);

    bpmSlider.setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag
    );

    bpmSlider.setTextBoxStyle(
        juce::Slider::TextBoxBelow,
        false,
        55,
        18
    );

    bpmSlider.setRange(40.0, 240.0, 1.0);
    bpmSlider.setValue(audioProcessor.currentBPM);

    bpmSlider.onValueChange = [this]
    {
        audioProcessor.currentBPM =
            static_cast<float>(bpmSlider.getValue());
    };

    // --------------------------------------------------------
    // TRANSPORTE VISUAL
    // --------------------------------------------------------

    auto configureButton = [](juce::TextButton& button,
                              juce::Colour colour)
    {
        button.setColour(
            juce::TextButton::buttonColourId,
            colour
        );

        button.setColour(
            juce::TextButton::textColourOffId,
            LaPautaColours::text
        );
    };

    configureButton(playButton, LaPautaColours::green);
    configureButton(pauseButton, LaPautaColours::panelLight);
    configureButton(stopButton, LaPautaColours::panelLight);
    configureButton(recordButton, LaPautaColours::red);
    configureButton(loopButton, LaPautaColours::blue);

    addAndMakeVisible(playButton);
    addAndMakeVisible(pauseButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(recordButton);
    addAndMakeVisible(loopButton);

    playButton.onClick = [this]
    {
        showTransportPending("PLAY");
    };

    pauseButton.onClick = [this]
    {
        showTransportPending("PAUSE");
    };

    stopButton.onClick = [this]
    {
        showTransportPending("STOP");
    };

    recordButton.onClick = [this]
    {
        showTransportPending("RECORD");
    };

    loopButton.onClick = [this]
    {
        showTransportPending("LOOP");
    };

    // --------------------------------------------------------
    // NAVEGADOR VST3
    // --------------------------------------------------------

    pluginBrowserLabel.setText(
        "VST3",
        juce::dontSendNotification
    );

    pluginBrowserLabel.setColour(
        juce::Label::textColourId,
        LaPautaColours::blueLight
    );

    pluginBrowserLabel.setFont(
        juce::Font(
            juce::FontOptions(14.0f).withStyle("Bold")
        )
    );

    addAndMakeVisible(pluginBrowserLabel);

    scanPluginsButton.setButtonText("ESCANEAR");

    scanPluginsButton.setColour(
        juce::TextButton::buttonColourId,
        LaPautaColours::blue
    );

    scanPluginsButton.onClick = [this]
    {
        scanVST3Plugins();
    };

    addAndMakeVisible(scanPluginsButton);

    pluginList.setTextWhenNothingSelected(
        "Seleccionar plugin VST3"
    );

    addAndMakeVisible(pluginList);

    scanStatusLabel.setText(
        "Escaneo VST3 pendiente",
        juce::dontSendNotification
    );

    scanStatusLabel.setColour(
        juce::Label::textColourId,
        LaPautaColours::muted
    );

    addAndMakeVisible(scanStatusLabel);

    // --------------------------------------------------------
    // ESTADO
    // --------------------------------------------------------

    statusLabel.setText(
        "LA PAUTA STUDIO - Proyecto preparado",
        juce::dontSendNotification
    );

    statusLabel.setColour(
        juce::Label::textColourId,
        LaPautaColours::muted
    );

    addAndMakeVisible(statusLabel);

    // --------------------------------------------------------
    // TIMELINE
    // --------------------------------------------------------

    arrangementViewport.setViewedComponent(
        &arrangementContent,
        false
    );

    arrangementViewport.setScrollBarsShown(
        true,
        true
    );

    addAndMakeVisible(arrangementViewport);

    // --------------------------------------------------------
    // MIXER
    // --------------------------------------------------------

    trackViewport.setViewedComponent(
        &trackContainer,
        false
    );

    trackViewport.setScrollBarsShown(
        false,
        true
    );

    addAndMakeVisible(trackViewport);

    createTrackComponents();
}

// ============================================================
// DESTRUCTOR
// ============================================================

MainComponent::~MainComponent()
{
    arrangementViewport.setViewedComponent(nullptr, false);
    trackViewport.setViewedComponent(nullptr, false);
}

// ============================================================
// CREAR 16 CANALES DE MIXER
// ============================================================

void MainComponent::createTrackComponents()
{
    trackComponents.clear();

    for (int i = 0; i < 16; ++i)
    {
        auto channel = std::make_unique<TrackComponent>(
            "TRACK " + juce::String(i + 1)
        );

        trackContainer.addAndMakeVisible(*channel);

        trackComponents.push_back(std::move(channel));
    }

    masterChannel = std::make_unique<TrackComponent>("MASTER");

    trackContainer.addAndMakeVisible(*masterChannel);

    updateTrackLayout();
}

// ============================================================
// DISTRIBUCION DEL MIXER
// ============================================================

void MainComponent::updateTrackLayout()
{
    const int channelWidth = 118;
    const int channelHeight = 225;
    const int spacing = 5;

    const int totalChannels = 17;

    trackContainer.setSize(
        totalChannels * (channelWidth + spacing) + 10,
        channelHeight
    );

    for (int i = 0; i < static_cast<int>(trackComponents.size()); ++i)
    {
        trackComponents[static_cast<size_t>(i)]->setBounds(
            5 + i * (channelWidth + spacing),
            0,
            channelWidth,
            channelHeight
        );
    }

    if (masterChannel)
    {
        masterChannel->setBounds(
            5 + 16 * (channelWidth + spacing),
            0,
            channelWidth,
            channelHeight
        );
    }
}

// ============================================================
// ESTADO DE TRANSPORTE
// ============================================================

void MainComponent::showTransportPending(
    const juce::String& action)
{
    statusLabel.setText(
        action + " solicitado - transporte de audio pendiente de conexion",
        juce::dontSendNotification
    );
}

// ============================================================
// ESCANEAR VST3
// ============================================================

void MainComponent::scanVST3Plugins()
{
    auto& manager = audioProcessor.getPluginManager();

    if (manager.isScanning())
        return;

    scanPluginsButton.setEnabled(false);

    scanStatusLabel.setText(
        "Escaneando plugins VST3...",
        juce::dontSendNotification
    );

    pluginList.clear(juce::dontSendNotification);

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
// ACTUALIZAR LISTA VST3
// ============================================================

void MainComponent::updatePluginList()
{
    auto& manager = audioProcessor.getPluginManager();

    const auto& plugins =
        manager.getKnownPlugins().getTypes();

    pluginList.clear(juce::dontSendNotification);

    for (int i = 0; i < plugins.size(); ++i)
    {
        const auto& description = plugins.getReference(i);

        juce::String itemName = description.name;

        if (description.manufacturerName.isNotEmpty())
        {
            itemName += " - ";
            itemName += description.manufacturerName;
        }

        pluginList.addItem(itemName, i + 1);
    }

    scanStatusLabel.setText(
        "Encontrados: " + juce::String(plugins.size()) + " VST3",
        juce::dontSendNotification
    );

    scanPluginsButton.setEnabled(true);
}

// ============================================================
// PAINT
// ============================================================

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(LaPautaColours::background);

    // CABECERA
    g.setColour(LaPautaColours::text);
    g.setFont(
        juce::Font(
            juce::FontOptions(23.0f).withStyle("Bold")
        )
    );

    g.drawText(
        "LA PAUTA STUDIO",
        15, 5, 350, 30,
        juce::Justification::centredLeft
    );

    g.setColour(LaPautaColours::blueLight);
    g.setFont(11.0f);

    g.drawText(
        "DIGITAL AUDIO WORKSTATION",
        17, 32, 350, 18,
        juce::Justification::centredLeft
    );

    // PANEL TRANSPORTE
    g.setColour(LaPautaColours::panel);
    g.fillRoundedRectangle(
        10.0f, 52.0f,
        static_cast<float>(getWidth() - 20),
        65.0f,
        8.0f
    );

    // PANEL TIMELINE
    g.setColour(LaPautaColours::panel);
    g.fillRoundedRectangle(
        10.0f, 122.0f,
        static_cast<float>(getWidth() - 20),
        static_cast<float>(trackViewport.getY() - 130),
        8.0f
    );

    // TITULO ARRANGEMENT
    g.setColour(LaPautaColours::blueLight);
    g.setFont(
        juce::Font(
            juce::FontOptions(14.0f).withStyle("Bold")
        )
    );

    g.drawText(
        "ARRANGEMENT",
        20, 125, 200, 22,
        juce::Justification::centredLeft
    );

    // PANEL MIXER
    g.setColour(LaPautaColours::panel);
    g.fillRoundedRectangle(
        10.0f,
        static_cast<float>(trackViewport.getY() - 8),
        static_cast<float>(getWidth() - 20),
        static_cast<float>(trackViewport.getHeight() + 8),
        8.0f
    );

    g.setColour(LaPautaColours::blueLight);
    g.setFont(
        juce::Font(
            juce::FontOptions(14.0f).withStyle("Bold")
        )
    );

    g.drawText(
        "MIXER",
        20,
        trackViewport.getY() - 7,
        150,
        20,
        juce::Justification::centredLeft
    );
}

// ============================================================
// RESIZED
// ============================================================

void MainComponent::resized()
{
    const int width = getWidth();
    const int height = getHeight();

    // BPM
    bpmLabel.setBounds(15, 57, 45, 18);
    bpmSlider.setBounds(55, 53, 80, 60);

    // TRANSPORTE
    playButton.setBounds(150, 67, 65, 30);
    pauseButton.setBounds(220, 67, 65, 30);
    stopButton.setBounds(290, 67, 65, 30);
    recordButton.setBounds(360, 67, 65, 30);
    loopButton.setBounds(430, 67, 65, 30);

    // VST3
    pluginBrowserLabel.setBounds(520, 58, 45, 22);
    scanPluginsButton.setBounds(565, 64, 95, 28);
    pluginList.setBounds(670, 64, 230, 28);
    scanStatusLabel.setBounds(910, 64, width - 925, 28);

    // ESTADO
    statusLabel.setBounds(15, height - 18, width - 30, 16);

    // TIMELINE
    const int mixerHeight = 245;
    const int timelineTop = 153;
    const int mixerTop = height - mixerHeight;

    arrangementViewport.setBounds(
        18,
        timelineTop,
        width - 36,
        juce::jmax(100, mixerTop - timelineTop - 12)
    );

    // MIXER
    trackViewport.setBounds(
        18,
        mixerTop + 22,
        width - 36,
        mixerHeight - 27
    );

    updateTrackLayout();
}
