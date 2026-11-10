
#pragma once

#include <JuceHeader.h>
#include <vector>

// ============================================================
// LA PAUTA STUDIO
// MIDI EDITOR - PIANO ROLL
// ============================================================

class MidiEditorComponent : public juce::Component
{
public:

    MidiEditorComponent()
    {
        setSize(1000, 600);
    }

    ~MidiEditorComponent() override = default;

    // ========================================================
    // DIBUJAR PIANO ROLL
    // ========================================================

    void paint(juce::Graphics& g) override
    {
        const float beatWidth = 40.0f;
        const float rowHeight = 16.0f;

        g.fillAll(juce::Colour(0xff151a22));

        // Fondo de teclas
        for (int row = 0; row < 128; ++row)
        {
            const int pitch = 127 - row;

            if (isBlackKey(pitch))
                g.setColour(juce::Colour(0xff202631));
            else
                g.setColour(juce::Colour(0xff29313d));

            g.fillRect(
                0.0f,
                row * rowHeight,
                static_cast<float>(getWidth()),
                rowHeight);
        }

        // Líneas horizontales
        g.setColour(juce::Colour(0xff46505e));

        for (float y = 0; y < getHeight(); y += rowHeight)
            g.drawHorizontalLine(
                static_cast<int>(y),
                0.0f,
                static_cast<float>(getWidth()));

        // Líneas verticales
        for (int beat = 0; beat * beatWidth < getWidth(); ++beat)
        {
            if (beat % 4 == 0)
                g.setColour(juce::Colour(0xff657487));
            else
                g.setColour(juce::Colour(0xff394453));

            g.drawVerticalLine(
                static_cast<int>(beat * beatWidth),
                0.0f,
                static_cast<float>(getHeight()));
        }

        // Dibujar notas MIDI
        for (const auto& note : paintedNotes)
        {
            const float x =
                static_cast<float>(note.beat * beatWidth);

            const float y =
                (127.0f - static_cast<float>(note.pitch))
                * rowHeight;

            const float width =
                static_cast<float>(note.duration * beatWidth);

            g.setColour(juce::Colour(0xff36d6c5));

            g.fillRoundedRectangle(
                x,
                y + 1.0f,
                width - 2.0f,
                rowHeight - 2.0f,
                3.0f);

            g.setColour(juce::Colour(0xffb9fff6));

            g.drawRoundedRectangle(
                x,
                y + 1.0f,
                width - 2.0f,
                rowHeight - 2.0f,
                3.0f,
                1.0f);
        }
    }

    // ========================================================
    // AGREGAR NOTAS
    // ========================================================

    void mouseDown(const juce::MouseEvent& event) override
    {
        const float beatWidth = 40.0f;
        const float rowHeight = 16.0f;

        const double beat =
            static_cast<double>(event.x) / beatWidth;

        int pitch =
            127 - static_cast<int>(event.y / rowHeight);

        pitch = juce::jlimit(0, 127, pitch);

        paintedNotes.push_back(
            { beat, pitch, 100, 1.0 });

        repaint();
    }

    // ========================================================
    // BORRAR NOTAS
    // ========================================================

    void clearNotes()
    {
        paintedNotes.clear();
        repaint();
    }

    // ========================================================
    // CANTIDAD DE NOTAS
    // ========================================================

    int getNumNotes() const
    {
        return static_cast<int>(paintedNotes.size());
    }

private:

    struct MidiNoteVisual
    {
        double beat;
        int pitch;
        int velocity;
        double duration;
    };

    std::vector<MidiNoteVisual> paintedNotes;

    bool isBlackKey(int pitch) const
    {
        const int note = pitch % 12;

        return note == 1 ||
               note == 3 ||
               note == 6 ||
               note == 8 ||
               note == 10;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        MidiEditorComponent)
};

// ============================================================
// VENTANA FLOTANTE MIDI
// ============================================================

class MidiEditorWindow : public juce::DocumentWindow
{
public:

    explicit MidiEditorWindow(
        const juce::String& trackName)

        : juce::DocumentWindow(
            "Piano Roll - " + trackName,
            juce::Colour(0xff151a22),
            juce::DocumentWindow::allButtons)
    {
        setContentOwned(
            new MidiEditorComponent(),
            true);

        setResizable(true, true);
        centreWithSize(1000, 600);
        setVisible(true);
    }

    ~MidiEditorWindow() override = default;

    void closeButtonPressed() override
    {
        setVisible(false);
    }

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        MidiEditorWindow)
};
