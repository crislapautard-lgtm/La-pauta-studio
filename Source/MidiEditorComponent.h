#pragma once
#include <JuceHeader.h>
#include "MidiTrack.h"

class MidiEditorComponent : public juce::Component
{
public:
    MidiEditorComponent(MidiTrack& track) : midiTrack(track)
    {
        setSize(800, 400);
    }

    ~MidiEditorComponent() override = default;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff181818)); // Fondo oscuro del piano roll

        // Dibujar líneas de la cuadrícula (compases / tiempos)
        g.setColour(juce::Colour(0xff333333));
        float beatWidth = 40.0f;
        for (float x = 0; x < (float)getWidth(); x += beatWidth)
        {
            g.drawVerticalLine((int)x, 0.0f, (float)getHeight());
        }

        // Dibujar filas de notas (teclas del piano)
        float rowHeight = 16.0f;
        for (float y = 0; y < (float)getHeight(); y += rowHeight)
        {
            g.drawHorizontalLine((int)y, 0.0f, (float)getWidth());
        }

        // Dibujar las notas MIDI que el usuario ha pintado
        g.setColour(juce::Colours::orange);
        for (const auto& note : paintedNotes)
        {
            float x = note.beat * beatWidth;
            float y = (127 - note.pitch) * rowHeight;
            g.fillRect(x, y, beatWidth, rowHeight - 1.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        float beatWidth = 40.0f;
        float rowHeight = 16.0f;

        // Calcular en qué nota y tiempo hizo clic el usuario
        double beat = event.x / beatWidth;
        int pitch = 127 - (int)(event.y / rowHeight);

        pitch = juce::jlimit(0, 127, pitch);

        // Añadir la nota a nuestra lista visual y al buffer MIDI de la pista
        paintedNotes.push_back({ beat, pitch, 100, 1.0 }); // beat, pitch, velocity, duration
        midiTrack.addNote(pitch, 100, (double)paintedNotes.size(), 1.0); // Conexión con la pista MIDI

        repaint();
    }

private:
    MidiTrack& midiTrack;

    struct MidiNoteVisual {
        double beat;
        int pitch;
        int velocity;
        double duration;
    };

    std::vector<MidiNoteVisual> paintedNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditorComponent)
};
