#pragma once
#include <JuceHeader.h>
#include "MidiTrack.h"

class PianoRollComponent : public juce::Component,
                           public juce::Button::Listener
{
public:
    PianoRollComponent(MidiTrack* track) : targetMidiTrack(track) {
        clearButton.setButtonText("Limpiar Notas");
        clearButton.addListener(this);
        addAndMakeVisible(clearButton);

        setSize(800, 400);
    }

    ~PianoRollComponent() override {
        clearButton.removeListener(this);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff1e1e1e)); // Fondo oscuro de la rejilla

        // Dibujar líneas horizontales de la cuadrícula (para las 12 notas de una octava)
        g.setColour(juce::Colour(0xff333333));
        float rowHeight = getHeight() / 12.0f;
        for (int i = 0; i < 12; ++i) {
            g.drawHorizontalLine((int)(i * rowHeight), 60.0f, (float)getWidth());
        }

        // Dibujar las teclas del piano a la izquierda
        juscDrawPianoKeys(g, rowHeight);
    }

    void juscDrawPianoKeys(juce::Graphics& g, float rowHeight) {
        juce::StringArray noteNames = { "B", "A#", "A", "G#", "G", "F#", "F", "E", "D#", "D", "C#", "C" };
        
        for (int i = 0; i < 12; ++i) {
            float y = i * rowHeight;
            bool isBlackKey = noteNames[i].contains("#");

            g.setColour(isBlackKey ? juce::Colours::black : juce::Colours::white);
            g.fillRect(0.0f, y, 60.0f, rowHeight - 1.0f);

            g.setColour(isBlackKey ? juce::Colours::white : juce::Colours::black);
            g.setFont(10.0f);
            g.drawText(noteNames[i], 5, (int)y, 50, (int)rowHeight, juce::Justification::centredLeft, false);
        }
    }

    void resized() override {
        clearButton.setBounds(getWidth() - 110, 10, 100, 30);
    }

    void mouseDown(const juce::MouseEvent& event) override {
        if (event.x > 60 && targetMidiTrack != nullptr) {
            // Al hacer clic en la rejilla, calculamos la nota MIDI según la altura del clic
            float rowHeight = getHeight() / 12.0f;
            int clickedRow = (int)(event.y / rowHeight);
            
            // Mapeo básico de filas a notas MIDI (Octava 4: C4 = 60)
            int midiNote = 72 - clickedRow; 
            double beatPosition = (event.x - 60.0) / 40.0; // Posición horizontal en la rejilla

            // Añadir nota de 1 beat de duración
            targetMidiTrack->addMidiNote(midiNote, 0.8f, beatPosition, 1.0f);
            repaint();
        }
    }

    void buttonClicked(juce::Button* button) override {
        if (button == &clearButton && targetMidiTrack != nullptr) {
            targetMidiTrack->clearNotes();
            repaint();
        }
    }

private:
    MidiTrack* targetMidiTrack = nullptr;
    juce::TextButton clearButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollComponent)
};
