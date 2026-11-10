
#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

#include "Track.h"
#include "AudioTrack.h"
#include "MidiTrack.h"
#include "AudioEngine.h"
#include "PluginManager.h"

// ============================================================
// LA PAUTA STUDIO
// PLUGIN PROCESSOR
// MOTOR PRINCIPAL DEL DAW
// ============================================================

class AudioPluginAudioProcessor : public juce::AudioProcessor
{
public:

    static constexpr int MAX_TRACKS = 16;
    static constexpr int MAX_SUBGROUPS = 4;
    static constexpr int MAX_AUXES = 4;

    AudioPluginAudioProcessor();

    ~AudioPluginAudioProcessor() override;

    // ========================================================
    // AUDIO
    // ========================================================

    void prepareToPlay(
        double sampleRate,
        int samplesPerBlock) override;

    void releaseResources() override;

    bool isBusesLayoutSupported(
        const BusesLayout& layouts) const override;

    void processBlock(
        juce::AudioBuffer<float>& buffer,
        juce::MidiBuffer& midiMessages) override;

    // ========================================================
    // EDITOR
    // ========================================================

    juce::AudioProcessorEditor* createEditor() override;

    bool hasEditor() const override;

    // ========================================================
    // INFORMACION
    // ========================================================

    const juce::String getName() const override;

    bool isMidiEffect() const override;

    bool acceptsMidi() const override;

    bool producesMidi() const override;

    double getTailLengthSeconds() const override;

    // ========================================================
    // PROGRAMAS
    // ========================================================

    int getNumPrograms() override;

    int getCurrentProgram() override;

    void setCurrentProgram(int index) override;

    const juce::String getProgramName(int index) override;

    void changeProgramName(
        int index,
        const juce::String& newName) override;

    // ========================================================
    // ESTADO
    // ========================================================

    void getStateInformation(
        juce::MemoryBlock& destData) override;

    void setStateInformation(
        const void* data,
        int sizeInBytes) override;

    // ========================================================
    // PISTAS
    // ========================================================

    int numTracks = MAX_TRACKS;

    int numSubgroups = MAX_SUBGROUPS;

    int numAuxes = MAX_AUXES;

    std::array<float, MAX_TRACKS> trackVolume {};

    std::array<float, MAX_TRACKS> trackPan {};

    std::array<bool, MAX_TRACKS> trackMute {};

    std::array<bool, MAX_TRACKS> trackSolo {};

    std::array<bool, MAX_TRACKS> trackRecording {};

    std::array<float, MAX_SUBGROUPS> subgroupVolume {};

    std::array<float, MAX_AUXES> auxVolume {};

    float masterVolume = 0.9f;

    float currentBPM = 120.0f;

    std::vector<std::unique_ptr<Track>> tracks;

    AudioEngine audioEngine;

    // ========================================================
    // ADMINISTRADOR VST3
    // ========================================================

    PluginManager& getPluginManager()
    {
        return pluginManager;
    }

    const PluginManager& getPluginManager() const
    {
        return pluginManager;
    }

    // ========================================================
    // ACCESO A PISTAS
    // ========================================================

    AudioTrack* getAudioTrack(int index);

    MidiTrack* getMidiTrack(int index);

    Track* getTrack(int index);

    bool addAudioTrack(const juce::String& name);

    bool addMidiTrack(const juce::String& name);

    void removeTrack(int index);

    // ========================================================
    // GRABACION
    // ========================================================

    void setTrackRecording(
        int trackIndex,
        bool shouldRecord);

    bool isTrackRecording(int trackIndex) const;

    // ========================================================
    // MEZCLADOR
    // ========================================================

    void setTrackVolume(
        int trackIndex,
        float volume);

    void setTrackPan(
        int trackIndex,
        float pan);

    void setTrackMute(
        int trackIndex,
        bool shouldMute);

    void setTrackSolo(
        int trackIndex,
        bool shouldSolo);

    void setMasterVolume(float volume);

    void setBPM(float newBPM);

private:

    // ========================================================
    // ADMINISTRADOR DE PLUGINS
    // ========================================================

    PluginManager pluginManager;

    // ========================================================
    // CONFIGURACION INTERNA
    // ========================================================

    double currentSampleRate = 44100.0;

    int currentBlockSize = 512;

    juce::CriticalSection trackLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        AudioPluginAudioProcessor)
};
