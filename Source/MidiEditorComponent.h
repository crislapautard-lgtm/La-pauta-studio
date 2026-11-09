#pragma once
#include <JuceHeader.h>

class MidiEditorComponent : public juce::Component
{
public:
    MidiEditorComponent()
    {
        setSize(800, 400);
    }

    ~MidiEditorComponent() override = default;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff181818)); // Fondo oscuro del piano roll

        // Dibujar líneas verticales de la cuadrícula (tiempos / beats)
        g.setColour(juce::Colour(0xff333333));
        float beatWidth = 40.0f;
        for (float x = 0; x < (float)getWidth(); x += beatWidth)
        {
            g.drawVerticalLine((int)x, 0.0f, (float)getHeight());
        }

        // Dibujar líneas horizontales (teclas del piano)
        float rowHeight = 16.0f;
        for (float y = 0; y < (float)getHeight(); y += rowHeight)
        {
            g.drawHorizontalLine((int)y, 0.0f, (float)getWidth());
        }

        // Dibujar las notas MIDI pintadas por el usuario
        g.setColour(juce::Colours::orange);
        for (const auto& note : paintedNotes)
        {
            float x = (float)note.beat * beatWidth;
            float y = (127.0f - (float)note.pitch) * rowHeight;
            g.fillRect(x, y, beatWidth, rowHeight - 1.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        float beatWidth = 40.0f;
        float rowHeight = 16.0f;

        // Calcular nota y tiempo según dónde se hizo clic
        double beat = (double)event.x / (double)beatWidth;
        int pitch = 127 - (int)((double)event.y / (double)rowHeight);
        pitch = juce::jlimit(0, 127, pitch);

        paintedNotes.push_back({ beat, pitch, 100, 1.0 });
        repaint();
    }

private:
    struct MidiNoteVisual {
        double beat;
        int pitch;
        int velocity;
        double duration;
    };

    std::vector<MidiNoteVisual> paintedNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditorComponent)
};

// Ventana flotante para el Piano Roll
class MidiEditorWindow : public juce::DocumentWindow
{
public:
    MidiEditorWindow(const juce::String& trackName)
        : DocumentWindow("Piano Roll - " + trackName,
                         juce::Desktop::getInstance().getDefaultLookAndFeel()
                             .findColour(juce::ResizableWindow::backgroundColourId),
                         DocumentWindow::allButtons)
    {
        editorComponent = std::make_unique<MidiEditorComponent>();
        setContentOwned(editorComponent.get(), true);
        setResizable(true, true);
        centreWithSize(800, 400);
        setVisible(true);
    }

    ~MidiEditorWindow() override = default;

    void closeButtonPressed() override
    {
        delete this;
    }

private:
    std::unique_ptr<MidiEditorComponent> editorComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditorWindow)
};
