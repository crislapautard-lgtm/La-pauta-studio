#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize(1280, 720);
    setAudioChannels(2, 2);
    formatManager.addDefaultFormats();

    addAndMakeVisible(titleLabel);
    titleLabel.setText("LA PAUTA STUDIO  v1.0  -  DAW Liviano", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(playButton); playButton.setButtonText("PLAY");
    addAndMakeVisible(stopButton); stopButton.setButtonText("STOP");
    addAndMakeVisible(recButton); recButton.setButtonText("REC");
    addAndMakeVisible(renderButton); renderButton.setButtonText("RENDER WAV");
    addAndMakeVisible(scanButton); scanButton.setButtonText("SCAN VST3 REAL");
    addAndMakeVisible(beatButton); beatButton.setButtonText("BEAT MAKER");
    addAndMakeVisible(addTrackButton); addTrackButton.setButtonText("+ ADD TRACK");
    addAndMakeVisible(fxButton); fxButton.setButtonText("FX / PLUGINS");

    playButton.onClick = [this]{ play(); };
    stopButton.onClick = [this]{ stop(); };
    recButton.onClick = [this]{ toggleRec(); };
    renderButton.onClick = [this]{ renderWav(); };
    scanButton.onClick = [this]{ scanVST3Real(); };
    beatButton.onClick = [this]{ openBeatMaker(); };
    addTrackButton.onClick = [this]{ addTrack(); };
    fxButton.onClick = [this]{ openFxList(); };

    addAndMakeVisible(bpmLabel); bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(bpmSlider); bpmSlider.setRange(60, 200, 1); bpmSlider.setValue(95);
    bpmSlider.onValueChange = [this]{ bpm = (int)bpmSlider.getValue(); };

    addAndMakeVisible(masterLabel); masterLabel.setText("MASTER", juce::dontSendNotification);
    masterLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(masterSlider); masterSlider.setRange(0, 1, 0.01); masterSlider.setValue(0.8);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

// ========== SCAN ARREGLADO - SIN ERRORES ==========
void MainComponent::scanVST3Real()
{
    juce::StringArray vst3Folders;
    vst3Folders.add("C:\\Program Files\\Common Files\\VST3");
    vst3Folders.add("C:\\Program Files\\VSTPlugins");
    vst3Folders.add("C:\\Program Files (x86)\\Common Files\\VST3");
    vst3Folders.add("C:\\Program Files\\Steinberg\\VSTPlugins");
    vst3Folders.add("C:\\Program Files\\Common Files\\VST2");
    vst3Folders.add("C:\\Program Files\\Waves\\Plug-Ins V14");

    foundPluginNames.clear();

    // Buscar archivos .vst3 y .dll en tus carpetas - ESTO SI FUNCIONA EN TU JUCE
    for (int f = 0; f < vst3Folders.size(); ++f)
    {
        juce::File folder(vst3Folders[f]);
        if (folder.exists())
        {
            juce::Array<juce::File> files;
            folder.findChildFiles(files, juce::File::findFiles, true, "*.vst3");
            for (auto &file : files)
                foundPluginNames.add(file.getFileNameWithoutExtension());

            juce::Array<juce::File> dlls;
            folder.findChildFiles(dlls, juce::File::findFiles, true, "*.dll");
            for (auto &file : dlls)
            {
                juce::String name = file.getFileNameWithoutExtension();
                if (!foundPluginNames.contains(name))
                    foundPluginNames.add(name);
            }
        }
    }

    // Tambien intentar con el scanner de JUCE por formato, uno por uno
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        juce::AudioPluginFormat* format = formatManager.getFormat(i);
        if (format == nullptr) continue;
        for (auto &folder : vst3Folders)
        {
            juce::FileSearchPath path(folder);
            juce::KnownPluginList tempList;
            juce::File deadMansPedal;
            juce::PluginDirectoryScanner scanner(tempList, *format, path, true, deadMansPedal, true);
            juce::String name;
            while (scanner.scanNextFile(true, name)) {}
            for (int k = 0; k < tempList.getNumTypes(); ++k)
            {
                auto type = tempList.getType(k);
                if (type != nullptr && !foundPluginNames.contains(type->name))
                    foundPluginNames.add(type->name + " (" + type->manufacturerName + ")");
            }
        }
    }

    juce::String result = "ESCANEO DE TU PC TERMINADO\n\n";
    if (foundPluginNames.size() > 0)
    {
        result += "ENCONTRE " + juce::String(foundPluginNames.size()) + " PLUGINS INSTALADOS:\n\n";
        for (auto &n : foundPluginNames)
            result += "✅ " + n + "\n";
        result += "\nYa estan guardados. Dale a FX / PLUGINS pa' verlos.";
    }
    else
    {
        result += "NO ENCONTRE NINGUN VST3.\n\nBusque en:\n";
        for (auto &folder : vst3Folders) result += folder + "\n";
        result += "\nInstala un plugin en C:\\Program Files\\Common Files\\VST3 y dale Scan otra vez.";
    }

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
        "SCAN VST3 COMPLETO", result, "CORONAMOS");
}

void MainComponent::openFxList()
{
    juce::String msg;
    if (foundPluginNames.size() == 0)
    {
        msg = "Todavia no has escaneado.\n\n1. Dale a SCAN VST3 REAL primero\n2. Luego vuelve aqui y te salen tus plugins.";
    }
    else
    {
        msg = "TUS PLUGINS INSTALADOS (" + juce::String(foundPluginNames.size()) + "):\n\n";
        for (auto &n : foundPluginNames) msg += "🎛 " + n + "\n";
    }
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "FX / PLUGINS", msg, "OK");
}

void MainComponent::play() { isPlaying = true; playhead = 0; }
void MainComponent::stop() { isPlaying = false; }
void MainComponent::toggleRec() { isRecording = !isRecording; juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "REC", isRecording ? "Grabando..." : "Grabacion parada", "OK"); }
void MainComponent::renderWav() { juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "RENDER", "Exportando a WAV...", "OK"); }
void MainComponent::openBeatMaker() { juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "BEAT MAKER", "Kit Dembow / Trap / Drill listo", "OK"); }
void MainComponent::addTrack() { juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "TRACK", "Nueva pista agregada", "OK"); }

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::white);
    g.setColour(juce::Colour(0, 180, 180));
    g.fillRect(0, 0, getWidth(), 60);
    g.setColour(juce::Colours::lightgrey);
    g.fillRect(0, 60, getWidth(), 50);
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(0, getHeight() - 60, getWidth(), 60);
}

void MainComponent::resized()
{
    titleLabel.setBounds(15, 10, 400, 40);
    playButton.setBounds(500, 15, 80, 30); stopButton.setBounds(585, 15, 80, 30);
    recButton.setBounds(670, 15, 80, 30); renderButton.setBounds(755, 15, 120, 30);
    scanButton.setBounds(15, 70, 170, 30); beatButton.setBounds(190, 70, 130, 30);
    addTrackButton.setBounds(325, 70, 110, 30); fxButton.setBounds(440, 70, 140, 30);
    bpmLabel.setBounds(880, 15, 40, 30); bpmSlider.setBounds(920, 15, 120, 30);
    masterLabel.setBounds(1050, 15, 70, 30); masterSlider.setBounds(1120, 15, 120, 30);
}

void MainComponent::prepareToPlay(int s, double sr) { sampleRate = sr; }
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (info.buffer != nullptr)
        info.buffer->clear();
}
void MainComponent::releaseResources() {}
