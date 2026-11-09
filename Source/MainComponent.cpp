#include "MainComponent.h"

// ============================================================
// LA PAUTA STUDIO
// MainComponent.cpp - Correccion de compilacion
// JUCE / C++
// ============================================================

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

    // TITULO
    addAndMakeVisible(titleLabel);
    titleLabel.setText("LA PAUTA STUDIO - TODO REAPER",
                       juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white);

    // BOTONES PRINCIPALES
    addAndMakeVisible(playButton);
    playButton.setButtonText("PLAY");

    addAndMakeVisible(stopButton);
    stopButton.setButtonText("STOP");

    addAndMakeVisible(recButton);
    recButton.setButtonText("REC");

    addAndMakeVisible(loadSampleButton);
    loadSampleButton.setButtonText("CARGAR SAMPLE");

    addAndMakeVisible(renderButton);
    renderButton.setButtonText("RENDER WAV");

    addAndMakeVisible(scanButton);
    scanButton.setButtonText("SCAN VST3 + JSFX");

    addAndMakeVisible(vstButton);
    vstButton.setButtonText("ABRIR VST");

    addAndMakeVisible(saveButton);
    saveButton.setButtonText("GUARDAR");

    addAndMakeVisible(loadButton);
    loadButton.setButtonText("ABRIR PROYECTO");

    addAndMakeVisible(addTrackButton);
    addTrackButton.setButtonText("+ PISTA");

    addAndMakeVisible(metronomeButton);
    metronomeButton.setButtonText("METRONOMO");

    addAndMakeVisible(beatButton);
    beatButton.setButtonText("BEAT MAKER");

    addAndMakeVisible(mixerButton);
    mixerButton.setButtonText("MIXER");

    addAndMakeVisible(audioSettingsButton);
    audioSettingsButton.setButtonText("AUDIO / ASIO");

    addAndMakeVisible(pianoButton);
    pianoButton.setButtonText("PIANO ROLL");

    addAndMakeVisible(cutButton);
    cutButton.setButtonText("CORTAR");

    addAndMakeVisible(muteButton);
    muteButton.setButtonText("MUTE");

    addAndMakeVisible(soloButton);
    soloButton.setButtonText("SOLO");

    // ACCIONES
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
                b.setSize(b.getNumChannels(),
                         b.getNumSamples() / 2);

                statusLabel.setText("Cortado",
                                    juce::dontSendNotification);
            }
        }
    };

    muteButton.onClick = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < tracks.size())
        {
            auto& t = tracks.getReference(selectedTrack);
            t.muted = !t.muted;

            statusLabel.setText(
                t.name + (t.muted ? " MUTED" : " SONANDO"),
                juce::dontSendNotification);
        }
    };

    soloButton.onClick = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < tracks.size())
        {
            for (auto& t : tracks)
                t.muted = true;

            tracks.getReference(selectedTrack).muted = false;

            statusLabel.setText(
                "SOLO en " + tracks[selectedTrack].name,
                juce::dontSendNotification);
        }
    };

    // ESTADO
    addAndMakeVisible(statusLabel);
    statusLabel.setText("LA PAUTA STUDIO - Listo",
                        juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId,
                          juce::Colours::white);

    // BPM
    addAndMakeVisible(bpmLabel);
    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId,
                       juce::Colours::white);

    addAndMakeVisible(bpmSlider);
    bpmSlider.setRange(60, 200, 1);
    bpmSlider.setValue(95);

    bpmSlider.onValueChange = [this]
    {
        bpm = (int)bpmSlider.getValue();
    };

    // MASTER
    addAndMakeVisible(masterSlider);
    masterSlider.setRange(0, 1, 0.01);
    masterSlider.setValue(0.8);

    // SELECTOR DE PISTAS
    addAndMakeVisible(trackSelector);

    for (int i = 0; i < tracks.size(); ++i)
        trackSelector.addItem(tracks[i].name, i + 1);

    trackSelector.setSelectedId(1);

    trackSelector.onChange = [this]
    {
        selectedTrack = trackSelector.getSelectedId() - 1;
    };

    // EQ
    addAndMakeVisible(eqLabel);
    eqLabel.setText("EQ", juce::dontSendNotification);
    eqLabel.setColour(juce::Label::textColourId,
                      juce::Colours::white);

    addAndMakeVisible(eqLowSlider);
    eqLowSlider.setRange(-12, 12, 0.1);
    eqLowSlider.setValue(0);
    eqLowSlider.onValueChange = [this]
    {
        eqLow = (float)eqLowSlider.getValue();
    };

    addAndMakeVisible(eqMidSlider);
    eqMidSlider.setRange(-12, 12, 0.1);
    eqMidSlider.setValue(0);
    eqMidSlider.onValueChange = [this]
    {
        eqMid = (float)eqMidSlider.getValue();
    };

    addAndMakeVisible(eqHighSlider);
    eqHighSlider.setRange(-12, 12, 0.1);
    eqHighSlider.setValue(0);
    eqHighSlider.onValueChange = [this]
    {
        eqHigh = (float)eqHighSlider.getValue();
    };

    // EFECTOS
    addAndMakeVisible(fxLabel);
    fxLabel.setText("FX: REV / DLY / COMP",
                    juce::dontSendNotification);
    fxLabel.setColour(juce::Label::textColourId,
                      juce::Colours::white);

    addAndMakeVisible(reverbSlider);
    reverbSlider.setRange(0, 1, 0.01);
    reverbSlider.setValue(0);

    reverbSlider.onValueChange = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < tracks.size())
            tracks.getReference(selectedTrack).reverb =
                (float)reverbSlider.getValue();
    };

    addAndMakeVisible(delaySlider);
    delaySlider.setRange(0, 1, 0.01);
    delaySlider.setValue(0);

    delaySlider.onValueChange = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < tracks.size())
            tracks.getReference(selectedTrack).delay =
                (float)delaySlider.getValue();
    };

    addAndMakeVisible(compSlider);
    compSlider.setRange(0, 1, 0.01);
    compSlider.setValue(0);

    compSlider.onValueChange = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < tracks.size())
            tracks.getReference(selectedTrack).comp =
                (float)compSlider.getValue();
    };

    // AUTOMATIZACION Y VOLUMEN
    addAndMakeVisible(autoLabel);
    autoLabel.setText("AUTO VOL / PAN / VOL PISTA",
                      juce::dontSendNotification);
    autoLabel.setColour(juce::Label::textColourId,
                        juce::Colours::white);

    addAndMakeVisible(autoSlider);
    autoSlider.setRange(0, 1, 0.01);
    autoSlider.setValue(1);

    autoSlider.onValueChange = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < tracks.size())
            tracks.getReference(selectedTrack).autoVol =
                (float)autoSlider.getValue();
    };

    addAndMakeVisible(volSlider);
    volSlider.setRange(0, 1, 0.01);
    volSlider.setValue(0.8);

    volSlider.onValueChange = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < tracks.size())
            tracks.getReference(selectedTrack).volume =
                (float)volSlider.getValue();
    };

    addAndMakeVisible(panSlider);
    panSlider.setRange(-1, 1, 0.01);
    panSlider.setValue(0);

    panSlider.onValueChange = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < tracks.size())
            tracks.getReference(selectedTrack).pan =
                (float)panSlider.getValue();
    };
}

// ============================================================
// DESTRUCTOR
// ============================================================

MainComponent::~MainComponent()
{
    stopTimer();
    shutdownAudio();
}

// ============================================================
// TIMER
// ============================================================

void MainComponent::timerCallback()
{
    repaint();
}

// ============================================================
// PLAY / STOP
// ============================================================

void MainComponent::play()
{
    for (auto& t : tracks)
        t.pos = 0;

    playhead = 0;
    isPlaying = true;
}

void MainComponent::stop()
{
    isPlaying = false;
}

// ============================================================
// PISTAS
// ============================================================

void MainComponent::addTrack()
{
    Track t;
    t.name = "Pista " + juce::String(tracks.size() + 1);
    t.color = juce::Colours::purple;

    tracks.add(t);
    trackSelector.addItem(t.name, tracks.size());
}

// ============================================================
// METRONOMO
// ============================================================

void MainComponent::toggleMetronome()
{
    metronomeOn = !metronomeOn;
    statusLabel.setText(
        metronomeOn ? "Metronomo activado" : "Metronomo apagado",
        juce::dontSendNotification);
}

// ============================================================
// BEAT MAKER
// ============================================================

void MainComponent::openBeatMaker()
{
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "BEAT MAKER",
        "DEMBOW: Kick-Kick-Snare\n"
        "TRAP: 808 + HiHat\n"
        "DRILL: 808 deslizado + Snare",
        "DALE");
}

// ============================================================
// MIXER
// ============================================================

void MainComponent::openMixer()
{
    juce::String m;

    for (auto& t : tracks)
    {
        m += t.name
            + " Vol " + juce::String((int)(t.volume * 100)) + "%"
            + " Pan " + juce::String((int)(t.pan * 100))
            + " REV " + juce::String((int)(t.reverb * 100)) + "%"
            + " "
            + (t.muted ? "MUTE" : "ON")
            + "\n";
    }

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "MIXER",
        m,
        "OK");
}

// ============================================================
// AUDIO SETTINGS
// ============================================================

void MainComponent::openAudioSettings()
{
    auto* s = new juce::AudioDeviceSelectorComponent(
        deviceManager, 0, 2, 0, 2,
        false, false, true, false);

    s->setSize(600, 500);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(s);
    o.dialogTitle = "AUDIO / ASIO";
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

// ============================================================
// PIANO ROLL
// ============================================================

void MainComponent::openPianoRoll()
{
    MidiNote n;
    n.note = 60 + (int)(midiNotes.size() % 12);
    n.start = playhead;

    midiNotes.add(n);

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "PIANO ROLL",
        "Notas MIDI: " + juce::String(midiNotes.size()),
        "OK");
}

// ============================================================
// EDICION DE CLIPS
// ============================================================

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    int y = 170;

    for (int i = 0; i < tracks.size(); ++i)
    {
        if (e.y >= y && e.y < y + 60)
        {
            selectedTrack = i;
            trackSelector.setSelectedId(i + 1);

            dragStartX = e.x;
            dragTrack = i;
            break;
        }

        y += 65;
    }
}

void MainComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragTrack >= 0 && dragTrack < tracks.size())
    {
        tracks.getReference(dragTrack).clipOffset +=
            (e.x - dragStartX) / 10;

        dragStartX = e.x;
        repaint();
    }
}

// ============================================================
// GRABACION
// ============================================================

void MainComponent::toggleRec()
{
    if (selectedTrack < 0 || selectedTrack >= tracks.size())
        return;

    if (!isRecording)
    {
        auto d = juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory);

        juce::File f = d.getChildFile("LaPauta_Voz.wav");

        if (f.exists())
            f.deleteFile();

        juce::WavAudioFormat wav;

        if (auto s = f.createOutputStream())
        {
            writer.reset(wav.createWriterFor(
                s.release(),
                sampleRate,
                2,
                16,
                {},
                0));

            if (writer != nullptr)
            {
                isRecording = true;
                isPlaying = false;

                recButton.setButtonText("PARAR");

                statusLabel.setText(
                    "GRABANDO: " + tracks[selectedTrack].name,
                    juce::dontSendNotification);
            }
        }
    }
    else
    {
        isRecording = false;
        writer.reset();

        recButton.setButtonText("REC");

        loadFileIntoBuffer(
            juce::File::getSpecialLocation(
                juce::File::userDocumentsDirectory)
                .getChildFile("LaPauta_Voz.wav"),
            selectedTrack);

        statusLabel.setText(
            "Archivo guardado",
            juce::dontSendNotification);
    }
}

// ============================================================
// CARGAR SAMPLE
// ============================================================

void MainComponent::loadSample()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Elige sample",
        juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory),
        "*.wav;*.mp3;*.aif;*.flac");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();

            if (file.exists())
            {
                currentSampleFile = file;
                loadFileIntoBuffer(file, selectedTrack);

                statusLabel.setText(
                    "Sample cargado",
                    juce::dontSendNotification);
            }
        });
}

// ============================================================
// CARGAR ARCHIVO EN PISTA
// ============================================================

void MainComponent::loadFileIntoBuffer(juce::File file, int idx)
{
    if (idx < 0 || idx >= tracks.size() || !file.exists())
        return;

    if (auto* r = formatManager.createReaderFor(file))
    {
        auto& buffer = tracks.getReference(idx).buffer;

        buffer.setSize(
            (int)r->numChannels,
            (int)r->lengthInSamples);

        r->read(
            &buffer,
            0,
            (int)r->lengthInSamples,
            0,
            true,
            true);

        tracks.getReference(idx).hasAudio = true;
        tracks.getReference(idx).pos = 0;

        delete r;
    }
}

// ============================================================
// BUSCAR VST3 Y JSFX
// ============================================================

void MainComponent::scanVST3()
{
    foundPlugins.clear();
    pluginDescs.clear();
    foundJSFX.clear();

    juce::KnownPluginList list;

    juce::FileSearchPath vst3Path(
        "C:\\Program Files\\Common Files\\VST3;"
        "C:\\Program Files\\VSTPlugins");

    for (auto* fmt : pluginManager.getFormats())
    {
        juce::PluginDirectoryScanner scanner(
            list,
            *fmt,
            vst3Path,
            true,
            juce::File());

        juce::String pluginBeingScanned;

        while (scanner.scanNextFile(true, pluginBeingScanned))
        {
        }
    }

    for (int i = 0; i < list.getNumTypes(); ++i)
    {
        auto* desc = list.getType(i);

        if (desc != nullptr)
        {
            pluginDescs.add(*desc);
            foundPlugins.add(desc->name);
        }
    }

    juce::StringArray jsfxFolders;

    jsfxFolders.add("C:\\Program Files\\REAPER\\Effects");

    jsfxFolders.add(
        juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory)
            .getChildFile("REAPER\\Effects")
            .getFullPathName());

    jsfxFolders.add(
        juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory)
            .getChildFile("REAPER\\Effects")
            .getFullPathName());

    for (auto& f : jsfxFolders)
    {
        juce::File folder(f);

        if (folder.exists())
        {
            auto files = folder.findChildFiles(
                juce::File::findFiles,
                true,
                "*.jsfx");

            for (auto& file : files)
                foundJSFX.add(file.getFileNameWithoutExtension());
        }
    }

    juce::String r =
        "VST3: " + juce::String(foundPlugins.size()) + " encontrados\n";

    for (auto& n : foundPlugins)
        r += " - " + n + "\n";

    r += "\nJSFX: " + juce::String(foundJSFX.size()) + " encontrados\n";

    for (auto& n : foundJSFX)
        r += " - " + n + "\n";

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "SCAN VST3 + JSFX",
        r,
        "OK");
}

// ============================================================
// ABRIR VST3
// ============================================================

void MainComponent::openVST()
{
    if (pluginDescs.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "VST",
            "Ejecuta SCAN VST3 primero",
            "OK");
        return;
    }

    juce::String err;

    for (auto& desc : pluginDescs)
    {
        auto inst = pluginManager.createPluginInstance(
            desc,
            sampleRate,
            512,
            err);

        if (inst != nullptr)
        {
            pluginInstance
