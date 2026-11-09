#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize(1280, 760);
    setAudioChannels(2, 2);

    formatManager.registerBasicFormats();
    pluginManager.addDefaultFormats();

    startTimer(50);

    juce::Colour cols[4] = {
        juce::Colours::red,
        juce::Colours::orange,
        juce::Colours::green,
        juce::Colours::skyblue
    };

    for (int i = 0; i < 4; ++i)
    {
        Track t;
        t.name = "Pista " + juce::String(i + 1);
        t.color = cols[i];
        tracks.add(t);
    }

    addAndMakeVisible(titleLabel);
    titleLabel.setText("LA PAUTA STUDIO", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(playButton); playButton.setButtonText("PLAY");
    addAndMakeVisible(stopButton); stopButton.setButtonText("STOP");
    addAndMakeVisible(recButton); recButton.setButtonText("REC");
    addAndMakeVisible(loadSampleButton); loadSampleButton.setButtonText("CARGAR SAMPLE");
    addAndMakeVisible(renderButton); renderButton.setButtonText("RENDER WAV");

    addAndMakeVisible(scanButton); scanButton.setButtonText("SCAN VST3 + JSFX");
    addAndMakeVisible(vstButton); vstButton.setButtonText("ABRIR VST");
    addAndMakeVisible(saveButton); saveButton.setButtonText("GUARDAR");
    addAndMakeVisible(loadButton); loadButton.setButtonText("ABRIR PROYECTO");
    addAndMakeVisible(addTrackButton); addTrackButton.setButtonText("+ PISTA");

    addAndMakeVisible(metronomeButton); metronomeButton.setButtonText("METRONOMO");
    addAndMakeVisible(beatButton); beatButton.setButtonText("BEAT MAKER");
    addAndMakeVisible(mixerButton); mixerButton.setButtonText("MIXER");
    addAndMakeVisible(audioSettingsButton); audioSettingsButton.setButtonText("AUDIO / ASIO");
    addAndMakeVisible(pianoButton); pianoButton.setButtonText("PIANO ROLL");

    addAndMakeVisible(cutButton); cutButton.setButtonText("CORTAR");
    addAndMakeVisible(muteButton); muteButton.setButtonText("MUTE");
    addAndMakeVisible(soloButton); soloButton.setButtonText("SOLO");

    playButton.onClick = [this] { play(); };
    stopButton.onClick = [this] { stop(); };
    recButton.onClick = [this] { toggleRec(); };
    loadSampleButton.onClick = [this] { loadSample(); };
    renderButton.onClick = [this] { renderMix(); };

    scanButton.onClick = [this] { scanVST3(); };
    vstButton.onClick = [this] { openVST(); };
    saveButton.onClick = [this] { saveProject(); };
    loadButton.onClick = [this] { loadProject(); };
    addTrackButton.onClick = [this] { addTrack(); };
    metronomeButton.onClick = [this] { toggleMetronome(); };

    beatButton.onClick = [this] { openBeatMaker(); };
    mixerButton.onClick = [this] { openMixer(); };
    audioSettingsButton.onClick = [this] { openAudioSettings(); };
    pianoButton.onClick = [this] { openPianoRoll(); };

    cutButton.onClick = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < tracks.size())
        {
            auto& b = tracks.getReference(selectedTrack).buffer;
            if (b.getNumSamples() > 0)
            {
                b.setSize(b.getNumChannels(), b.getNumSamples() / 2);
                statusLabel.setText("Cortado", juce::dontSendNotification);
            }
        }
    };

    muteButton.onClick = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < tracks.size())
        {
            auto& t = tracks.getReference(selectedTrack);
            t.muted = !t.muted;
            statusLabel.setText(t.name + (t.muted ? " MUTED" : " SONANDO"), juce::dontSendNotification);
        }
    };

    soloButton.onClick = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < tracks.size())
        {
            for (auto& t : tracks) t.muted = true;
            tracks.getReference(selectedTrack).muted = false;
            statusLabel.setText("SOLO en " + tracks[selectedTrack].name, juce::dontSendNotification);
        }
    };

    addAndMakeVisible(statusLabel);
    statusLabel.setText("LA PAUTA STUDIO - Listo", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);

    addAndMakeVisible(bpmLabel); bpmLabel.setText("BPM", juce::dontSendNotification); bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(bpmSlider);
    bpmSlider.setRange(60, 200, 1);
    bpmSlider.setValue(95);
    bpmSlider.onValueChange = [this] { bpm = (int)bpmSlider.getValue(); };

    addAndMakeVisible(masterSlider);
    masterSlider.setRange(0, 1, 0.01);
    masterSlider.setValue(0.8);

    addAndMakeVisible(trackSelector);
    for (int i = 0; i < tracks.size(); ++i)
        trackSelector.addItem(tracks[i].name, i + 1);
    trackSelector.setSelectedId(1);
    trackSelector.onChange = [this] { selectedTrack = trackSelector.getSelectedId() - 1; };

    addAndMakeVisible(eqLabel); eqLabel.setText("EQ Master", juce::dontSendNotification); eqLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(eqLowSlider); eqLowSlider.setRange(-12, 12, 0.1);
    addAndMakeVisible(eqMidSlider); eqMidSlider.setRange(-12, 12, 0.1);
    addAndMakeVisible(eqHighSlider); eqHighSlider.setRange(-12, 12, 0.1);

    addAndMakeVisible(fxLabel); fxLabel.setText("FX: REV / DLY / COMP", juce::dontSendNotification); fxLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(reverbSlider); reverbSlider.setRange(0, 1, 0.01);
    addAndMakeVisible(delaySlider); delaySlider.setRange(0, 1, 0.01);
    addAndMakeVisible(compSlider); compSlider.setRange(0, 1, 0.01);

    addAndMakeVisible(autoLabel); autoLabel.setText("Vol / Pan Pista Activa", juce::dontSendNotification); autoLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(volSlider); volSlider.setRange(0, 1, 0.01); volSlider.setValue(0.8);
    addAndMakeVisible(panSlider); panSlider.setRange(-1, 1, 0.01); panSlider.setValue(0);
}

MainComponent::~MainComponent()
{
    stopTimer();
    shutdownAudio();
}

void MainComponent::paint(juce::Graphics& g)
{
    // Pintamos un fondo gris oscuro profesional tipo DAW
    g.fillAll(juce::Colour(30, 30, 30));

    // Dibujar las pistas visualmente en el centro
    int y = 140;
    for (int i = 0; i < tracks.size(); ++i)
    {
        auto& t = tracks[i];
        
        // Fondo de la pista
        g.setColour(i == selectedTrack ? juce::Colour(55, 55, 65) : juce::Colour(40, 40, 40));
        g.fillRect(15, y, 900, 55);

        // Barra de color de la pista
        g.setColour(t.color);
        g.fillRect(15, y, 10, 55);

        // Texto del nombre de la pista
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText(t.name + (t.muted ? " [MUTED]" : ""), 35, y + 15, 120, 25, juce::Justification::left);

        // Simulación visual de la forma de onda si tiene audio
        g.setColour(t.color.contrasting(0.3f));
        if (t.hasAudio)
        {
            g.drawRect(160, y + 15, 740, 25);
            g.fillRect(160, y + 25, 300, 5);
        }
        else
        {
            g.setColour(juce::Colours::grey);
            g.drawText("(Sin audio - Carga un sample o graba)", 160, y + 15, 300, 25, juce::Justification::left);
        }

        y += 60;
    }
}

void MainComponent::resized()
{
    // Distribución ordenada de todos los botones y controles en la ventana
    titleLabel.setBounds(20, 15, 250, 30);
    statusLabel.setBounds(280, 15, 400, 30);

    // Barra de transporte superior
    playButton.setBounds(20, 55, 70, 30);
    stopButton.setBounds(95, 55, 70, 30);
    recButton.setBounds(170, 55, 70, 30);
    loadSampleButton.setBounds(245, 55, 120, 30);
    renderButton.setBounds(370, 55, 100, 30);

    // Herramientas y plugins
    scanButton.setBounds(480, 55, 130, 30);
    vstButton.setBounds(615, 55, 90, 30);
    addTrackButton.setBounds(710, 55, 80, 30);

    // Botones de funciones extra
    metronomeButton.setBounds(20, 95, 100, 28);
    beatButton.setBounds(125, 95, 100, 28);
    mixerButton.setBounds(230, 95, 75, 28);
    audioSettingsButton.setBounds(310, 95, 110, 28);
    pianoButton.setBounds(425, 95, 100, 28);

    cutButton.setBounds(535, 95, 75, 28);
    muteButton.setBounds(615, 95, 70, 28);
    soloButton.setBounds(690, 95, 70, 28);

    // Panel derecho de mezcla y efectos
    int rx = 930;
    trackSelector.setBounds(rx, 140, 325, 30);

    autoLabel.setBounds(rx, 180, 325, 20);
    volSlider.setBounds(rx, 205, 325, 25);
    panSlider.setBounds(rx, 235, 325, 25);

    fxLabel.setBounds(rx, 280, 325, 20);
    reverbSlider.setBounds(rx, 305, 325, 25);
    delaySlider.setBounds(rx, 335, 325, 25);
    compSlider.setBounds(rx, 365, 325, 25);

    eqLabel.setBounds(rx, 410, 325, 20);
    eqLowSlider.setBounds(rx, 435, 325, 25);
    eqMidSlider.setBounds(rx, 465, 325, 25);
    eqHighSlider.setBounds(rx, 495, 325, 25);

    saveButton.setBounds(rx, 560, 155, 35);
    loadButton.setBounds(rx + 165, 560, 160, 35);
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
}

void MainComponent::releaseResources() {}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
}

void MainComponent::timerCallback()
{
    repaint();
}

void MainComponent::play()
{
    for (auto& t : tracks) t.pos = 0;
    playhead = 0;
    isPlaying = true;
    statusLabel.setText("Reproduciendo...", juce::dontSendNotification);
}

void MainComponent::stop()
{
    isPlaying = false;
    statusLabel.setText("Detenido", juce::dontSendNotification);
}

void MainComponent::addTrack()
{
    Track t;
    t.name = "Pista " + juce::String(tracks.size() + 1);
    t.color = juce::Colours::purple;
    tracks.add(t);
    trackSelector.addItem(t.name, tracks.size());
}

void MainComponent::toggleMetronome()
{
    metronomeOn = !metronomeOn;
    statusLabel.setText(metronomeOn ? "Metrónomo activado" : "Metrónomo apagado", juce::dontSendNotification);
}

void MainComponent::openBeatMaker()
{
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "BEAT MAKER", "DEMBOW: Kick-Kick-Snare\nTRAP: 808 + HiHat", "DALE");
}

void MainComponent::openMixer()
{
    juce::String m;
    for (auto& t : tracks)
    {
        m += t.name + " - Vol: " + juce::String((int)(t.volume * 100)) + "%\n";
    }
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "MIXER", m, "OK");
}

void MainComponent::openAudioSettings()
{
    auto* s = new juce::AudioDeviceSelectorComponent(deviceManager, 0, 2, 0, 2, false, false, true, false);
    s->setSize(600, 500);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(s);
    o.dialogTitle = "AUDIO / ASIO";
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

void MainComponent::openPianoRoll()
{
    MidiNote n;
    n.note = 60 + (int)(midiNotes.size() % 12);
    n.start = playhead;
    midiNotes.add(n);
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "PIANO ROLL", "Nota MIDI agregada. Total: " + juce::String(midiNotes.size()), "OK");
}

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    int y = 140;
    for (int i = 0; i < tracks.size(); ++i)
    {
        if (e.y >= y && e.y < y + 55)
        {
            selectedTrack = i;
            trackSelector.setSelectedId(i + 1);
            dragStartX = e.x;
            dragTrack = i;
            break;
        }
        y += 60;
    }
}

void MainComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragTrack >= 0 && dragTrack < tracks.size())
    {
        tracks.getReference(dragTrack).clipOffset += (e.x - dragStartX) / 10;
        dragStartX = e.x;
        repaint();
    }
}

void MainComponent::toggleRec()
{
    if (selectedTrack < 0 || selectedTrack >= tracks.size()) return;
    if (!isRecording)
    {
        auto d = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        juce::File f = d.getChildFile("LaPauta_Voz.wav");
        if (f.exists()) f.deleteFile();
        juce::WavAudioFormat wav;
        if (auto s = f.createOutputStream())
        {
            writer.reset(wav.createWriterFor(s.release(), currentSampleRate, 2, 16, {}, 0));
            if (writer != nullptr)
            {
                isRecording = true;
                isPlaying = false;
                recButton.setButtonText("PARAR");
                statusLabel.setText("GRABANDO...", juce::dontSendNotification);
            }
        }
    }
    else
    {
        isRecording = false;
        writer.reset();
        recButton.setButtonText("REC");
        loadFileIntoBuffer(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LaPauta_Voz.wav"), selectedTrack);
        statusLabel.setText("Grabación guardada", juce::dontSendNotification);
    }
}

void MainComponent::loadSample()
{
    fileChooser = std::make_unique<juce::FileChooser>("Elige sample", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.wav;*.mp3;*.aif;*.flac");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.exists())
        {
            currentSampleFile = file;
            loadFileIntoBuffer(file, selectedTrack);
            statusLabel.setText("Sample cargado correctamente", juce::dontSendNotification);
        }
    });
}

void MainComponent::loadFileIntoBuffer(juce::File file, int idx)
{
    if (idx < 0 || idx >= tracks.size() || !file.exists()) return;
    if (auto* r = formatManager.createReaderFor(file))
    {
        auto& buffer = tracks.getReference(idx).buffer;
        buffer.setSize((int)r->numChannels, (int)r->lengthInSamples);
        r->read(&buffer, 0, (int)r->lengthInSamples, 0, true, true);
        tracks.getReference(idx).hasAudio = true;
        tracks.getReference(idx).pos = 0;
        delete r;
    }
}

void MainComponent::scanVST3()
{
    foundPlugins.clear();
    pluginDescs.clear();
    foundJSFX.clear();
    juce::KnownPluginList list;
    juce::FileSearchPath vst3Path("C:\\Program Files\\Common Files\\VST3;C:\\Program Files\\VSTPlugins");
    for (auto* fmt : pluginManager.getFormats())
    {
        juce::PluginDirectoryScanner scanner(list, *fmt, vst3Path, true, juce::File());
        juce::String name;
        while (scanner.scanNextFile(true, name)) {}
    }
    for (int i = 0; i < list.getNumTypes(); ++i)
    {
        auto* desc = list.getType(i);
        if (desc != nullptr) { pluginDescs.add(*desc); foundPlugins.add(desc->name); }
    }
    juce::String r = "VST3 encontrados: " + juce::String(foundPlugins.size()) + "\n";
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "SCAN", r, "OK");
}

void MainComponent::openVST()
{
    if (pluginDescs.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "VST", "Escanea plugins primero", "OK");
        return;
    }
    auto& desc = pluginDescs.getReference(0);
    juce::String err;
    auto inst = pluginManager.createPluginInstance(desc, currentSampleRate, 512, err);
    if (inst != nullptr)
    {
        pluginInstance.reset(inst.release());
        pluginInstance->prepareToPlay(currentSampleRate, 512);
        if (pluginInstance->hasEditor())
        {
            pluginEditor.reset(pluginInstance->createEditor());
            if (pluginEditor != nullptr)
            {
                juce::DialogWindow::LaunchOptions o;
                o.content.setOwned(pluginEditor.release());
                o.dialogTitle = "VST: " + desc.name;
                o.escapeKeyTriggersCloseButton = true;
                o.useNativeTitleBar = true;
                o.resizable = true;
                o.launchAsync();
            }
        }
    }
}
