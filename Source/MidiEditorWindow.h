#pragma once
#include <JuceHeader.h>
#include "MidiEditorComponent.h"

class MidiEditorWindow : public juce::DocumentWindow
{
public:
    MidiEditorWindow(MidiTrack& track, const juce::String& trackName)
        : DocumentWindow("Piano Roll - " + trackName,
                         juce::Desktop::getInstance().getDefaultLookAndFeel()
                             .findColour(juce::ResizableWindow::backgroundColourId),
                         DocumentWindow::allButtons)
    {
        editorComponent = std::make_unique<MidiEditorComponent>(track);
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
