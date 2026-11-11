#include "PluginEditor.h"

LaPautaEditor::LaPautaEditor (LaPautaProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      engine (p.getEngine()),
      ruler (engine, timeline),
      arrange (engine, timeline)
{
    setWantsKeyboardFocus (true);

    auto setupButton = [this] (juce::TextButton& b, std::function<void()> fn)
    {
        b.setWantsKeyboardFocus (false);        // así Espacio y Enter llegan al editor
        b.onClick = std::move (fn);
        addAndMakeVisible (b);
    };

    setupButton (playButton,     [this] { engine.play(); });
    setupButton (pauseButton,    [this] { engine.pause(); });
    setupButton (stopButton,     [this] { engine.stop(); arrange.repaint(); });
    setupButton (addTrackButton, [this] { addTrack(); });
    setupButton (importButton,   [this] { chooseAndImportFile(); });
    setupButton (snapButton,     [this] { engine.snapEnabled = snapButton.getToggleState(); });

    playButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff34a853));
    snapButton.setClickingTogglesState (true);
    snapButton.setToggleState (engine.snapEnabled, juce::dontSendNotification);

    timeLabel.setFont (timeLabel.getFont().withHeight (22.0f).boldened());
    timeLabel.setColour (juce::Label::textColourId, Theme::text);
    addAndMakeVisible (timeLabel);

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (bpmLabel);

    bpmSlider.setSliderStyle (juce::Slider::IncDecButtons);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 24);
    bpmSlider.setRange (20.0, 300.0, 1.0);
    bpmSlider.setValue (engine.bpm.load(), juce::dontSendNotification);
    bpmSlider.setWantsKeyboardFocus (false);
    bpmSlider.onValueChange = [this]
    {
        engine.bpm.store (bpmSlider.getValue());
        arrange.repaint();
        ruler.repaint();
        grabKeyboardFocus();                    // devuelve el teclado al editor tras escribir el tempo
    };
    addAndMakeVisible (bpmSlider);

    viewport.setViewedComponent (&arrange, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setWantsKeyboardFocus (false);     // el Viewport se comería Inicio/Fin/AvPág
    addAndMakeVisible (viewport);
    addAndMakeVisible (ruler);

    arrange.onViewChanged = [this] { ruler.repaint(); };
    arrange.onTracksChanged = [this] { updateArrangeSize(); };
    arrange.rebuildTracks();

    setResizable (true, true);
    setResizeLimits (900, 500, 3000, 2000);
    setSize (1280, 720);

    startTimerHz (30);
}

LaPautaEditor::~LaPautaEditor()
{
    stopTimer();
}

void LaPautaEditor::parentHierarchyChanged()
{
    grabKeyboardFocus();
}

//==============================================================================
void LaPautaEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);
    g.setColour (Theme::header);
    g.fillRect (0, 0, getWidth(), 48);
}

void LaPautaEditor::resized()
{
    auto r = getLocalBounds();

    auto bar = r.removeFromTop (48).reduced (8, 8);
    playButton.setBounds  (bar.removeFromLeft (64));  bar.removeFromLeft (6);
    pauseButton.setBounds (bar.removeFromLeft (64));  bar.removeFromLeft (6);
    stopButton.setBounds  (bar.removeFromLeft (64));  bar.removeFromLeft (16);
    timeLabel.setBounds   (bar.removeFromLeft (260)); bar.removeFromLeft (8);
    bpmLabel.setBounds    (bar.removeFromLeft (36));
    bpmSlider.setBounds   (bar.removeFromLeft (120)); bar.removeFromLeft (12);
    snapButton.setBounds  (bar.removeFromLeft (60));  bar.removeFromLeft (12);
    addTrackButton.setBounds (bar.removeFromLeft (80)); bar.removeFromLeft (6);
    importButton.setBounds   (bar.removeFromLeft (100));

    ruler.setBounds (r.removeFromTop (TimelineState::rulerHeight));
    viewport.setBounds (r);
    updateArrangeSize();
}

void LaPautaEditor::updateArrangeSize()
{
    arrange.setSize (viewport.getMaximumVisibleWidth(),
                     juce::jmax (arrange.getContentHeight(), viewport.getHeight()));
    ruler.repaint();
}

void LaPautaEditor::timerCallback()
{
    const bool active = engine.isPlaying() || engine.isPaused();
    timeLabel.setText (formatPosition (active ? engine.getPlayPosition() : engine.getEditCursor()),
                       juce::dontSendNotification);

    playButton.setToggleState (engine.isPlaying(), juce::dontSendNotification);
    ruler.repaint();
}

// "compás.tiempo.centésimas  |  m:ss.mmm", igual que la barra de transporte de Reaper
juce::String LaPautaEditor::formatPosition (double seconds) const
{
    seconds = juce::jmax (0.0, seconds);

    const double beats = seconds * engine.bpm.load() / 60.0;
    const int bar = (int) (beats / 4.0) + 1;
    const int beat = (int) std::fmod (beats, 4.0) + 1;
    const int hundredths = (int) ((beats - std::floor (beats)) * 100.0);

    const int minutes = (int) (seconds / 60.0);
    const double secs = seconds - minutes * 60.0;

    return juce::String::formatted ("%d.%d.%02d  |  %d:%06.3f", bar, beat, hundredths, minutes, secs);
}

//==============================================================================
void LaPautaEditor::addTrack()
{
    engine.addTrack();
    arrange.rebuildTracks();
    updateArrangeSize();
}

void LaPautaEditor::chooseAndImportFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Insertar archivo de audio", juce::File(),
                                                   engine.formatManager.getWildcardForAllFormats());

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::canSelectMultipleItems;

    chooser->launchAsync (flags, [safe = juce::Component::SafePointer<LaPautaEditor> (this)] (const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;

        auto& eng = safe->engine;
        if (eng.getNumTracks() == 0)
            safe->addTrack();

        auto* track = safe->arrange.getSelectedTrack();
        if (track == nullptr)
            return;

        double t = eng.getEditCursor();
        for (auto& file : fc.getResults())
            if (auto item = eng.importAudioFile (file, *track, t))
                t = item->endTime();

        safe->arrange.repaint();
    });
}

void LaPautaEditor::ensureCursorVisible()
{
    const int x = timeline.timeToX (engine.getEditCursor());
    if (x < TimelineState::headerWidth || x > arrange.getWidth() - 20)
        timeline.viewStart = juce::jmax (0.0, engine.getEditCursor() - 1.0);

    arrange.repaint();
    ruler.repaint();
}

//==============================================================================
// Atajos de teclado tomados de Reaper
bool LaPautaEditor::keyPressed (const juce::KeyPress& key)
{
    using KP = juce::KeyPress;
    const auto cmd = juce::ModifierKeys::commandModifier;   // Ctrl en Windows/Linux, Cmd en macOS

    if (key == KP::spaceKey)   { engine.togglePlayStop();  return true; }   // Espacio: play / stop
    if (key == KP::returnKey)  { engine.togglePlayPause(); return true; }   // Enter: play / pausa

    if (key == KP::homeKey)    { engine.goToStart(); ensureCursorVisible(); return true; }
    if (key == KP::endKey)     { engine.goToEnd();   ensureCursorVisible(); return true; }

    if (key == KP::leftKey || key == KP::rightKey)                          // mueve el cursor un tiempo
    {
        const double beat = 60.0 / engine.bpm.load();
        engine.setEditCursor (engine.getEditCursor() + (key == KP::rightKey ? beat : -beat));
        ensureCursorVisible();
        return true;
    }

    if (key == KP ('s'))       { engine.splitAtEditCursor();   arrange.repaint(); return true; }   // S: dividir
    if (key == KP::deleteKey || key == KP::backspaceKey)
                               { engine.deleteSelectedItems(); arrange.repaint(); return true; }   // Supr
    if (key == KP ('a', cmd, 0)) { engine.selectAllItems();    arrange.repaint(); return true; }   // Ctrl+A
    if (key == KP ('t', cmd, 0)) { addTrack();                 return true; }                      // Ctrl+T
    if (key == KP::insertKey || key == KP ('i', cmd, 0))
                               { chooseAndImportFile();        return true; }                      // Insert

    const auto c = key.getTextCharacter();
    if (c == '+' || c == '=')  { arrange.zoomAt (arrange.getWidth() / 2, 1.25);       return true; }
    if (c == '-')              { arrange.zoomAt (arrange.getWidth() / 2, 1.0 / 1.25); return true; }

    return false;
}
