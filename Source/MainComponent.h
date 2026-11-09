#pragma once
#include <JuceHeader.h>
class MainComponent : public juce::AudioAppComponent, public juce::Timer
{
public:
    MainComponent(); ~MainComponent() override;
    void prepareToPlay(int, double) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
private:
    struct Track { juce::String name; juce::AudioSampleBuffer buffer; bool muted=false, solo=false; float volume=0.8f, pan=0; int pos=0, clipOffset=0; bool hasAudio=false; juce::Colour color; float reverb=0, delay=0, comp=0, autoVol=1; };
    struct MidiNote { int note=60; int start=0; };
    void play(); void stop(); void toggleRec(); void loadSample(); void loadFileIntoBuffer(juce::File, int);
    void scanVST3(); void openVST(); void saveProject(); void loadProject(); void addTrack(); void renderMix();
    void toggleMetronome(); void openBeatMaker(); void openMixer(); void openAudioSettings(); void openPianoRoll(); void applyFX(juce::AudioBuffer<float>&, Track&);
    juce::Label titleLabel, bpmLabel, statusLabel, eqLabel, fxLabel, autoLabel;
    juce::TextButton playButton, stopButton, recButton, loadSampleButton, renderButton, scanButton, vstButton, saveButton, loadButton, addTrackButton, metronomeButton, beatButton, mixerButton, audioSettingsButton, pianoButton, cutButton, muteButton, soloButton;
    juce::Slider bpmSlider, masterSlider, eqLowSlider, eqMidSlider, eqHighSlider, reverbSlider, delaySlider, compSlider, autoSlider, volSlider, panSlider;
    juce::ComboBox trackSelector;
    juce::AudioFormatManager formatManager; juce::AudioPluginFormatManager pluginManager;
    std::unique_ptr<juce::FileChooser> fileChooser; std::unique_ptr<juce::AudioFormatWriter> writer;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance; std::unique_ptr<juce::AudioProcessorEditor> pluginEditor;
    juce::Array<Track> tracks; juce::Array<MidiNote> midiNotes; juce::StringArray foundPlugins; juce::Array<juce::PluginDescription> pluginDescs; juce::StringArray foundJSFX;
    int selectedTrack=0, playhead=0, bpm=95, dragStartX=0, dragTrack=-1;
    bool isPlaying=false, isRecording=false, metronomeOn=false;
    double sampleRate=44100; float eqLow=0, eqMid=0, eqHigh=0; juce::File currentSampleFile;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
