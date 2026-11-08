#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize(1280, 720);
    setAudioChannels(2, 2);
    formatManager.addDefaultFormats();

    addAndMakeVisible(titleLabel);
    titleLabel.setText("LA PAUTA STUDIO  -  GRABA Y CARGA SAMPLES", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(playButton); playButton.setButtonText("PLAY");
    addAndMakeVisible(stopButton); stopButton.setButtonText("STOP");
    addAndMakeVisible(recButton); recButton.setButtonText("🔴 REC VOZ");
    addAndMakeVisible(loadSampleButton); loadSampleButton.setButtonText("CARGAR SAMPLE");
    addAndMakeVisible(renderButton); renderButton.setButtonText("RENDER WAV");
    addAndMakeVisible(scanButton); scanButton.setButtonText("SCAN VST3");
    addAndMakeVisible(fxButton); fxButton.setButtonText("FX / PLUGINS");

    playButton.onClick = [this]{ play(); };
    stopButton.onClick = [this]{ stop(); };
    recButton.onClick = [this]{ toggleRec(); };
    loadSampleButton.onClick = [this]{ loadSample(); };
    renderButton.onClick = [this]{ renderWav(); };
    scanButton.onClick = [this]{ scanVST3Real(); };
    fxButton.onClick = [this]{ openFxList(); };

    addAndMakeVisible(statusLabel);
    statusLabel.setText("Listo. Dale a REC pa' grabar tu voz o CARGAR SAMPLE pa' meter un kick / 808", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(bpmLabel); bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(bpmSlider); bpmSlider.setRange(60, 200, 1); bpmSlider.setValue(95);
    addAndMakeVisible(masterSlider); masterSlider.setRange(0, 1, 0.01); masterSlider.setValue(0.8);
}

MainComponent::~MainComponent() { shutdownAudio(); if (writer != nullptr) writer.reset(); }

void MainComponent::toggleRec()
{
    if (!isRecording)
    {
        // EMPEZAR A GRABAR DE VERDAD
        auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        juce::File recFile = docs.getChildFile("LaPauta_Grabacion.wav");
        if (recFile.exists()) recFile.deleteFile();
        juce::WavAudioFormat wavFormat;
        if (auto stream = recFile.createOutputStream())
        {
            writer.reset(wavFormat.createWriterFor(stream.release(), sampleRate, 2, 16, {}, 0));
            isRecording = true;
            recButton.setButtonText("⏹ PARAR GRABACION");
            statusLabel.setText("🔴 GRABANDO TU VOZ... Canta ahora! Dale a PARAR cuando termines.", juce::dontSendNotification);
        }
    }
    else
    {
        isRecording = false;
        writer.reset();
        recButton.setButtonText("🔴 REC VOZ");
        // Cargar lo que acabas de grabar como sample pa' que lo escuches
        auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        loadFileIntoBuffer(docs.getChildFile("LaPauta_Grabacion.wav"));
        statusLabel.setText("Grabacion guardada en Documentos / LaPauta_Grabacion.wav - Dale PLAY pa' escucharte", juce::dontSendNotification);
    }
}

void MainComponent::loadSample()
{
    fileChooser = std::make_unique<juce::FileChooser>("Elige un sample - WAV, MP3, Kick, 808, Loop", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.wav;*.mp3;*.aif;*.flac");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.exists())
        {
            loadFileIntoBuffer(file);
            statusLabel.setText("Sample cargado: " + file.getFileName() + " - Dale PLAY", juce::dontSendNotification);
        }
    });
}

void MainComponent::loadFileIntoBuffer(juce::File file)
{
    if (auto* reader = formatManager.createReaderFor(file))
    {
        sampleBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&sampleBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
        sampleLoaded = true;
        samplePos = 0;
        delete reader;
    }
}

void MainComponent::play() { isPlaying = true; samplePos = 0; statusLabel.setText("Sonando...", juce::dontSendNotification); }
void MainComponent::stop() { isPlaying = false; samplePos = 0; }

void MainComponent::scanVST3Real()
{
    juce::StringArray folders; folders.add("C:\\Program Files\\Common Files\\VST3"); folders.add("C:\\Program Files\\VSTPlugins"); folders.add("C:\\Program Files (x86)\\Common Files\\VST3");
    foundPluginNames.clear();
    for (auto &f : folders) { juce::File folder(f); if (folder.exists()) { juce::Array<juce::File> files; folder.findChildFiles(files, juce::File::findFiles, true, "*.vst3"); for (auto &file : files) foundPluginNames.add(file.getFileNameWithoutExtension()); } }
    juce::String r = "ENCONTRE " + juce::String(foundPluginNames.size()) + " PLUGINS:\n\n"; for (auto &n : foundPluginNames) r += "✅ " + n + "\n";
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "SCAN", r, "OK");
}
void MainComponent::openFxList() { juce::String m; for (auto &n : foundPluginNames) m += n + "\n"; if (m.isEmpty()) m = "Dale SCAN primero"; juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "FX", m, "OK"); }
void MainComponent::renderWav() { juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "RENDER", "Tu voz ya esta guardada en Documentos como LaPauta_Grabacion.wav", "OK"); }

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::white); g.setColour(juce::Colour(0, 180, 180)); g.fillRect(0, 0, getWidth(), 60);
    g.setColour(juce::Colours::lightgrey); g.fillRect(0, 60, getWidth(), 50);
    g.setColour(juce::Colours::darkgrey); g.fillRect(0, getHeight() - 60, getWidth(), 60);
    g.setColour(juce::Colours::black); g.setFont(16.0f);
    g.drawText(isRecording ? "🔴 GRABANDO" : (sampleLoaded ? "Sample cargado - Listo pa' sonar" : "Sin sample"), 15, 120, 500, 30, juce::Justification::left);
}

void MainComponent::resized()
{
    titleLabel.setBounds(15, 10, 500, 40);
    playButton.setBounds(500, 15, 80, 30); stopButton.setBounds(585, 15, 80, 30);
    recButton.setBounds(670, 15, 140, 30); loadSampleButton.setBounds(815, 15, 160, 30);
    scanButton.setBounds(15, 70, 150, 30); fxButton.setBounds(170, 70, 140, 30);
    renderButton.setBounds(315, 70, 130, 30);
    bpmLabel.setBounds(980, 15, 40, 30); bpmSlider.setBounds(1020, 15, 120, 30);
    masterSlider.setBounds(1150, 15, 100, 30);
    statusLabel.setBounds(15, getHeight() - 50, getWidth() - 30, 30);
}

void MainComponent::prepareToPlay(int s, double sr) { sampleRate = sr; }
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (info.buffer == nullptr) return;
    auto* out = info.buffer;
    // Si esta grabando, guardar lo que entra del microfono
    if (isRecording && writer != nullptr && info.buffer->getNumChannels() > 0)
    {
        writer->writeFromAudioSampleBuffer(*out, 0, info.numSamples);
    }
    // Limpiar y luego sonar el sample si esta en play
    out->clear();
    if (isPlaying && sampleLoaded)
    {
        int numToCopy = juce::jmin(info.numSamples, sampleBuffer.getNumSamples() - samplePos);
        if (numToCopy > 0)
        {
            for (int ch = 0; ch < out->getNumChannels(); ++ch)
            {
                int srcCh = juce::jmin(ch, sampleBuffer.getNumChannels() - 1);
                out->copyFrom(ch, 0, sampleBuffer, srcCh, samplePos, numToCopy);
            }
            samplePos += numToCopy;
            if (samplePos >= sampleBuffer.getNumSamples()) { isPlaying = false; samplePos = 0; }
        }
    }
    out->applyGain(masterSlider.getValue());
}
void MainComponent::releaseResources() {}
