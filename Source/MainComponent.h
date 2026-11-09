#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include "TrackComponent.h"
#include "Track.h"
#include "AudioTrack.h"
#include "MidiTrack.h"
#include "AudioEngine.h"

class MainComponent : public juce::AudioAppComponent,
                      public juce::Button::Listener
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

    // Listener de botones globales (Play / Añadir Pista)
    void buttonClicked (juce::Button* button) override;

    // Función unificada para crear y desplegar una nueva pista de audio con su GUI
    void addAudioTrack() {
        int trackId = (int)tracks.size() + 1;
        auto newTrack = std::make_unique<AudioTrack>("Pista de Audio " + std::to_string(trackId));
        
        // Creamos su componente visual enlazándolo directamente a su pista lógica
        auto newTrackComp = std::make_unique<TrackComponent>(trackId, newTrack.get());

        tracks.push_back(std::move(newTrack));
        trackComponents.push_back(std::move(newTrackComp));

        addAndMakeVisible(trackComponents.back().get());
        resized(); // Reacomodamos la pantalla para que aparezca la nueva pista
    }

    // Función para crear una nueva pista MIDI
    void addMidiTrack() {
        int trackId = (int)tracks.size() + 1;
        auto newTrack = std::make_unique<MidiTrack>("Pista MIDI " + std::to_string(trackId));
        
        tracks.push_back(std::move(newTrack));
        // Nota: Si creas un MidiTrackComponent, lo agregarías aquí igual que el de audio.
        
        resized();
    }

private:
    // Motor encargado de procesar las pistas lógicas en tiempo real
    AudioEngine audioEngine;

    // Lista principal que almacena las pistas lógicas (sonido/datos)
    std::vector<std::unique_ptr<Track>> tracks;

    // Lista para almacenar los componentes visuales en la interfaz
    std::vector<std::unique_ptr<TrackComponent>> trackComponents;

    // Botones de control global del DAW
    juce::TextButton playButton;
    juce::TextButton addTrackButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
