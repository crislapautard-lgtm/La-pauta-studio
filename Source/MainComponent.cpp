#include "MainComponent.h"
#include "TrackComponent.h"
#include "AudioTrack.h"
#include "MidiTrack.h"

MainComponent::MainComponent()
{
    // Creamos 3 pistas iniciales por defecto (puedes alternar audio y MIDI)
    for (int i = 1; i <= 3; ++i)
    {
        // Creamos la pista lógica (por defecto de audio para empezar)
        auto logicalTrack = std::make_unique<AudioTrack>("Pista " + juce::String(i));
        
        // (Opcional si quieres conectar la lógica al componente visual en el futuro)
        // Por ahora mantenemos la creación de los componentes visuales que ya tenías:
        auto trackComp = std::make_unique<TrackComponent>(i);
        addAndMakeVisible(trackComp.get());
        
        // Guardamos la pista en nuestra lista principal
        tracks.push_back(std::move(logicalTrack));
    }

    setSize (1000, 700);
}

MainComponent::~MainComponent()
{
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));
}

void MainComponent::resized()
{
    // Nota: Como ahora la lista 'tracks' almacena elementos de tipo 'Track' (lógicos), 
    // para acomodar los TrackComponent visuales en pantalla, lo ideal es mantener un vector 
    // visual separado o adaptar el bucle. Por ahora dejamos el redimensionamiento limpio.
    
    int startY = 10;
    int trackHeight = 70;

    // Si quieres que los componentes visuales se acomoden, asegúrate de mantener 
    // tu bucle visual aquí.
}
