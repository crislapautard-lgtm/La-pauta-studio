#include "MainComponent.h"
#include "TrackComponent.h"
#include "AudioTrack.h"
#include "MidiTrack.h"

MainComponent::MainComponent()
{
    // Creamos 3 pistas iniciales por defecto para probar
    for (int i = 1; i <= 3; ++i)
    {
        // 1. Creamos la pista lógica (audio)
        auto logicalTrack = std::make_unique<AudioTrack>("Pista " + juce::String(i));
        
        // Guardamos un puntero crudo para que la interfaz visual pueda controlarla
        auto* audioTrackRaw = logicalTrack.get();
        tracks.push_back(std::move(logicalTrack));

        // 2. Creamos su cajita visual pasándole su respectiva pista lógica
        auto trackComp = std::make_unique<TrackComponent>(i, audioTrackRaw);
        addAndMakeVisible(trackComp.get());
        trackComponents.push_back(std::move(trackComp));
    }

    // Solicitamos 2 canales de ENTRADA (micrófono) y 2 canales de SALIDA (estéreo)
    setAudioChannels(2, 2);

    setSize (1000, 700);
}

MainComponent::~MainComponent()
{
    // Apagamos y liberamos el dispositivo de audio de JUCE al cerrar
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // Inicialización del sample rate para el motor o efectos
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::MidiBuffer incomingMidi;
    
    // Procesamos reproducción y grabación en tiempo real
    audioEngine.processBlock (*bufferToFill.buffer, incomingMidi, tracks);
}

void MainComponent::releaseResources()
{
    // Limpieza de recursos de audio cuando se detiene la reproducción
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));
}

void MainComponent::resized()
{
    int startY = 10;
    int trackHeight = 70;

    // Acomodamos las cajitas visuales en la pantalla ordenadamente
    for (auto& trackComp : trackComponents)
    {
        trackComp->setBounds(10, startY, getWidth() - 20, trackHeight);
        startY += trackHeight + 5;
    }
}
