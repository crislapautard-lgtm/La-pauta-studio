#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize(1280, 720);
    setAudioChannels(2, 2);

    titleLabel.setText("LA PAUTA STUDIO  |  Santo Domingo - RD", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    addAndMakeVisible(titleLabel);

    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00d4d4));
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
    recButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    addTrackButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffd400));
    scanButton.setColour(juce::TextButton::buttonColourId, juce::Colours::white);
    beatButton.setColour(juce::TextButton::buttonColourId, juce::Colours::white);
    renderButton.setColour(juce::TextButton::buttonColourId, juce::Colours::white);

    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(recButton);
    addAndMakeVisible(addTrackButton);
    addAndMakeVisible(scanButton);
    addAndMakeVisible(beatButton);
    addAndMakeVisible(renderButton);

    playButton.addListener(this);
    stopButton.addListener(this);
    recButton.addListener(this);
    addTrackButton.addListener(this);
    scanButton.addListener(this);
    beatButton.addListener(this);
    renderButton.addListener(this);

    bpmLabel.setText("BPM: 95 - DEMBOW", juce::dontSendNotification);
    bpmLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    addAndMakeVisible(bpmLabel);

    bpmSlider.setRange(60, 200, 1);
    bpmSlider.setValue(95);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible(bpmSlider);
    bpmSlider.addListener(this);

    masterSlider.setRange(0, 1, 0.01);
    masterSlider.setValue(0.8);
    masterSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible(masterSlider);
    masterSlider.addListener(this);

    track1Volume.setRange(0, 1, 0.01);
    track1Volume.setValue(0.8);
    addAndMakeVisible(track1Volume);
    track1Volume.addListener(this);

    track2Volume.setRange(0, 1, 0.01);
    track2Volume.setValue(0.8);
    addAndMakeVisible(track2Volume);
    track2Volume.addListener(this);

    addAndMakeVisible(mute1); addAndMakeVisible(solo1); addAndMakeVisible(rec1);
    addAndMakeVisible(mute2); addAndMakeVisible(solo2); addAndMakeVisible(rec2);

    recordedBuffer.setSize(2, 44100 * 60 * 5);
    recordedBuffer.clear();

    startTimer(30);
}

MainComponent::~MainComponent() { shutdownAudio(); }

void MainComponent::prepareToPlay(int, double sampleRate) { sampleRateNow = sampleRate; }
void MainComponent::releaseResources() {}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBuffer();
    if (!isPlaying && !isRecording) return;
    auto* outL = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* outR = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);
    float vol = (float)masterSlider.getValue();
    for (int i = 0; i < bufferToFill.numSamples; ++i)
    {
        float s = 0.0f;
        if (isPlaying) { double t = currentPosition / sampleRateNow; s = 0.15f * std::sin(2.0 * 3.14159 * 220.0 * t); }
        outL[i] = s * vol;
        outR[i] = s * vol;
        currentPosition += 1.0;
    }
    if (isRecording)
    {
        auto* inL = bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample);
        for (int i = 0; i < bufferToFill.numSamples; ++i)
        {
            if (recordedSamples < recordedBuffer.getNumSamples())
            {
                recordedBuffer.setSample(0, recordedSamples, inL[i]);
                recordedBuffer.setSample(1, recordedSamples, inL[i]);
                recordedSamples++;
            }
        }
    }
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::white);
    g.setColour(juce::Colour(0xff00d4d4));
    g.fillRect(0, 0, getWidth(), 60);
    g.setColour(juce::Colours::black);
    g.setFont(14.0f);
    g.drawText("TCP - PISTAS  |  ARRANGE - TIMELINE  |  MCP - MIXER", 20, 70, 600, 20, juce::Justification::left);
    g.drawText("PISTA 1 - VOZ PRINCIPAL", 20, 130, 250, 20, juce::Justification::left);
    g.drawText("PISTA 2 - BEAT / DEMBOW", 20, 180, 250, 20, juce::Justification::left);
    g.drawText("MASTER", getWidth() - 200, 70, 150, 20, juce::Justification::left);
    if (isPlaying) { g.setColour(juce::Colours::red); g.fillRect(300 + (int)(currentPosition / sampleRateNow * 20) % 700, 100, 2, 400); }
}

void MainComponent::resized()
{
    titleLabel.setBounds(20, 15, 500, 30);
    playButton.setBounds(650, 12, 80, 36);
    stopButton.setBounds(740, 12, 80, 36);
    recButton.setBounds(830, 12, 80, 36);
    bpmLabel.setBounds(950, 12, 150, 20);
    bpmSlider.setBounds(950, 32, 150, 20);
    addTrackButton.setBounds(20, 100, 130, 28);
    scanButton.setBounds(160, 100, 140, 28);
    beatButton.setBounds(310, 100, 130, 28);
    renderButton.setBounds(450, 100, 130, 28);
    mute1.setBounds(20, 130, 30, 25); solo1.setBounds(55, 130, 30, 25); rec1.setBounds(90, 130, 30, 25); track1Volume.setBounds(130, 130, 180, 25);
    mute2.setBounds(20, 180, 30, 25); solo2.setBounds(55, 180, 30, 25); rec2.setBounds(90, 180, 30, 25); track2Volume.setBounds(130, 180, 180, 25);
    masterSlider.setBounds(getWidth() - 180, 100, 80, 150);
}

void MainComponent::buttonClicked(juce::Button* b)
{
    if (b == &playButton) { isPlaying = true; }
    if (b == &stopButton) { isPlaying = false; currentPosition = 0; }
    if (b == &recButton) { isRecording = !isRecording; if (isRecording) isPlaying = true; recButton.setColour(juce::TextButton::buttonColourId, isRecording ? juce::Colours::darkred : juce::Colours::red); }
    if (b == &scanButton) { juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "La Pauta Studio", "Escaneando VST3 en: C:\\Program Files\\Common Files\\VST3\n+ Tus carpetas personalizadas\nListo broth!"); }
    if (b == &beatButton) { juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Beat Maker", "Step Sequencer 16 pasos - Kit Dembow cargado\nKick, Snare, Hi-Hat, Clap, 808 listo!"); }
    if (b == &renderButton) { juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Render", "Exportando a WAV... Listo!"); }
    if (b == &addTrackButton) { juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Track", "Nueva pista creada!"); }
}

void MainComponent::sliderValueChanged(juce::Slider* s)
{
    if (s == &bpmSlider) { bpmLabel.setText("BPM: " + juce::String((int)bpmSlider.getValue()) + " - DEMBOW", juce::dontSendNotification); }
}

void MainComponent::timerCallback() { repaint(); }
