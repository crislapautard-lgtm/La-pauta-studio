#pragma once

#include <JuceHeader.h>

struct Track
{
    juce::String name;
    juce::Colour color;
    juce::AudioBuffer<float> buffer;
    float volume = 0.8f;
    float pan = 0.0f;
    float reverb = 0.0f;
    float delay = 0.0f;
    float comp = 0.0f;
    float autoVol = 1.0f;
    bool muted = false;
    bool hasAudio = false;
    int pos = 0;
    int clipOffset = 0;
};

struct MidiNote
{
    int note = 60;
    int start = 0;
};

class MainComponent  : public juce::AudioAppComponent,
                       public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override {}
    void resized() override {}

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    void timerCallback() override;

    void play();
    void stop();
    void addTrack();
    void toggleMetronome();
    void openBeatMaker();
    void openMixer();
    void openAudioSettings();
    void openPianoRoll();
    void renderMix() {}

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    void toggleRec();
    void loadSample();
    void loadFileIntoBuffer(juce::File file, int idx);

    void scanVST3();
    void openVST();

    void saveProject() {}
    void loadProject() {}

private:
    juce::AudioFormatManager formatManager;
    juce::AudioPluginFormatManager pluginManager;
    std::unique_ptr<juce::AudioFormatWriter> writer;

    juce::Array<Track> tracks;
    juce::Array<MidiNote> midiNotes;

    int selectedTrack = 0;
    int bpm = 95;
    int playhead = 0;
    double currentSampleRate = 44100.0; // Almacena la frecuencia de muestreo actual de forma segura
    bool isPlaying = false;
    bool isRecording = false;
    bool metronomeOn = false;

    int dragTrack = -1;
    int dragStartX = 0;

    float eqLow = 0.0f;
    float eqMid = 0.0f;
    float eqHigh = 0.0f;

    juce::File currentSampleFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::StringArray foundPlugins;
    juce::Array<juce::PluginDescription> pluginDescs;
    juce::StringArray foundJSFX;

    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    std::unique_ptr<juce::AudioProcessorEditor> pluginEditor;

    juce::Label titleLabel, statusLabel, bpmLabel, eqLabel, fxLabel, autoLabel;
    juce::TextButton playButton, stopButton, recButton, loadSampleButton, renderButton;
    juce::TextButton scanButton, vstButton, saveButton, loadButton, addTrackButton;
    juce::TextButton metronomeButton, beatButton, mixerButton, audioSettingsButton, pianoButton;
    juce::TextButton cutButton, muteButton, soloButton;

    juce::Slider bpmSlider, masterSlider, eqLowSlider, eqMidSlider, eqHighSlider;
    juce::Slider reverbSlider, delaySlider, compSlider, autoSlider, volSlider, panSlider;
    juce::ComboBox trackSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
