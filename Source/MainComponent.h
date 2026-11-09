#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include "TrackComponent.h"
#include "Track.h"
#include "AudioTrack.h"
#include "MidiTrack.h"
#include "AudioEngine.h"

class MainComponent : public juce::AudioAppComponent
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Métodos obligatorios del ciclo de vida de audio en tiempo real de JUCE
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // Función para crear una nueva pista de audio
    void addAudioTrack() {
        tracks.push_back(std::make_unique<AudioTrack>("Pista de Audio " + std::to_string(tracks.size() + 1)));
    }

    // Función para crear una nueva pista MIDI
    void addMidiTrack() {
        tracks.push_back(std::make_unique<MidiTrack>("Pista MIDI " + std::to_string(tracks.size() + 1)));
    }

private:
    // Motor encargado de procesar las pistas lógicas en tiempo real
    AudioEngine audioEngine;

    // Lista principal que almacena las pistas lógicas (sonido/datos)
    std::vector<std::unique_ptr<Track>> tracks;

    // Lista para almacenar los componentes visuales en la interfaz
    std::vector<std::unique_ptr<TrackComponent>> trackComponents;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
