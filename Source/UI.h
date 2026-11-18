#pragma once
#include <JuceHeader.h>
#include <map>
#include "Engine.h"

using namespace juce;

namespace ui
{
    constexpr int headerW = 300;
    constexpr int rowH = 96;
    constexpr int rulerH = 28;
    constexpr int toolbarH = 40;

    enum Tool { toolSelect = 0, toolHand = 1, toolCut = 2 };

    // Rejillas: el id de cada opción es su posición + 1. 'beats' = duración de cada paso en negras (0 = libre).
    struct GridItem { const char* name; double beats; };

    inline const GridItem* gridItems()
    {
        static const GridItem items[] =
        {
            { "Libre (sin imán)", 0.0 },        // id 1
            { "1/1 (compás)",     4.0 },        // id 2
            { "1/2",              2.0 },        // id 3
            { "1/4",              1.0 },        // id 4
            { "1/8",              0.5 },        // id 5
            { "1/16",             0.25 },       // id 6
            { "1/32",             0.125 },      // id 7
            { "1/8 tresillo",     1.0 / 3.0 },  // id 8
            { "1/16 tresillo",    1.0 / 6.0 }   // id 9
        };
        return items;
    }

    constexpr int numGridItems = 9;
}

inline String T (const char* utf8) { return String::fromUTF8 (utf8); }

// Interfaz que las filas usan para hablar con la ventana principal.
struct RowOwner
{
    virtual ~RowOwner() = default;
    virtual double pps() const = 0;
    virtual void showFxMenu (Track&) = 0;
    virtual void removeTrack (Track&) = 0;
    virtual void setCursorFromX (int x) = 0;
    virtual void clipsChanged() = 0;
    virtual void showSendPanel (Track&, Component& source) = 0;

    virtual int getTool() const = 0;
    virtual void setTool (int) = 0;
    virtual int64 snapSample (int64) const = 0;
    virtual double gridStepSeconds() const = 0;      // 0 = libre (sin imán)
    virtual double beatSeconds() const = 0;
    virtual Clip* selectedClip() const = 0;
    virtual void setSelected (Track*, Clip*) = 0;
    virtual void setLoopFromClip (Clip&) = 0;
    virtual void toggleLoop() = 0;
};

// ---------------------------------------------------------------------------
// Ventana de editor de un plugin
// ---------------------------------------------------------------------------
class PluginWindow : public DocumentWindow
{
public:
    PluginWindow (AudioPluginInstance& p, std::function<void (PluginWindow*)> closeCb)
        : DocumentWindow (p.getName(), Colour (0xff202020), DocumentWindow::closeButton),
          plugin (&p), onClose (std::move (closeCb))
    {
        setUsingNativeTitleBar (true);

        AudioProcessorEditor* ed = p.hasEditor() ? p.createEditorIfNeeded() : nullptr;
        if (ed == nullptr)
            ed = new GenericAudioProcessorEditor (p);

        setContentOwned (ed, true);
        setResizable (ed->isResizable(), false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    ~PluginWindow() override { clearContentComponent(); }
    void closeButtonPressed() override { onClose (this); }

    AudioPluginInstance* plugin;

private:
    std::function<void (PluginWindow*)> onClose;
};

// ---------------------------------------------------------------------------
// Una fila = una pista: controles a la izquierda, clips a la derecha.
// ---------------------------------------------------------------------------
class TrackRow : public Component
{
public:
    TrackRow (Engine& e, Track& t, RowOwner& o) : eng (e), track (t), owner (o)
    {
        nameLabel.setText (track.name, dontSendNotification);
        nameLabel.setEditable (false, true);
        nameLabel.setColour (Label::textColourId, Colours::white);
        nameLabel.onTextChange = [this] { track.name = nameLabel.getText(); };
        addAndMakeVisible (nameLabel);

        setupToggle (btnMute, "M", Colour (0xffd9a400));
        setupToggle (btnSolo, "S", Colour (0xff2e9d4f));
        setupToggle (btnArm,  "R", Colour (0xffcc2222));
        btnMute.setToggleState (track.mute, dontSendNotification);
        btnSolo.setToggleState (track.solo, dontSendNotification);
        btnArm.setToggleState (track.armed, dontSendNotification);
        btnMute.onClick = [this] { track.mute = btnMute.getToggleState(); };
        btnSolo.onClick = [this] { track.solo = btnSolo.getToggleState(); };
        btnArm.onClick  = [this] { track.armed = btnArm.getToggleState(); };

        btnFx.setButtonText ("FX");
        btnFx.onClick = [this] { owner.showFxMenu (track); };
        addAndMakeVisible (btnFx);

        btnDel.setButtonText ("X");
        btnDel.onClick = [this] { owner.removeTrack (track); };
        addAndMakeVisible (btnDel);

        for (int i = 1; i <= 8; ++i)
            inputBox.addItem ("In " + String (i), i);
        inputBox.setSelectedId (track.inputChannel.load() + 1, dontSendNotification);
        inputBox.setTooltip (T ("Entrada de la tarjeta que graba esta pista"));
        inputBox.onChange = [this] { track.inputChannel = inputBox.getSelectedId() - 1; };
        inputBox.setScrollWheelEnabled (false);   // la rueda es para el zoom y el desplazamiento
        addAndMakeVisible (inputBox);

        btnSend.setButtonText (T ("Env"));
        btnSend.setTooltip (T ("Salida y envíos a buses"));
        btnSend.onClick = [this] { owner.showSendPanel (track, btnSend); };
        addAndMakeVisible (btnSend);

        if (track.isBus.load())   // un bus no graba ni tiene solo/envíos
        {
            btnSolo.setVisible (false);
            btnArm.setVisible (false);
            inputBox.setVisible (false);
            btnSend.setVisible (false);
        }

        vol.setSliderStyle (Slider::LinearHorizontal);
        vol.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        vol.setRange (-60.0, 6.0, 0.1);
        vol.setSkewFactorFromMidPoint (-12.0);
        vol.setDoubleClickReturnValue (true, 0.0);
        vol.setValue (Decibels::gainToDecibels ((float) track.gain, -60.0f), dontSendNotification);
        vol.setPopupDisplayEnabled (true, true, nullptr);
        vol.onValueChange = [this] { track.gain = Decibels::decibelsToGain ((float) vol.getValue(), -60.0f); };
        vol.setScrollWheelEnabled (false);
        addAndMakeVisible (vol);

        pan.setSliderStyle (Slider::LinearHorizontal);
        pan.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        pan.setRange (-1.0, 1.0, 0.01);
        pan.setDoubleClickReturnValue (true, 0.0);
        pan.setValue (track.pan, dontSendNotification);
        pan.setPopupDisplayEnabled (true, true, nullptr);
        pan.onValueChange = [this] { track.pan = (float) pan.getValue(); };
        pan.setScrollWheelEnabled (false);
        addAndMakeVisible (pan);
    }

    Track& getTrack() { return track; }

    void resized() override
    {
        nameLabel.setBounds (6, 4, track.isBus.load() ? 236 : 180, 22);
        btnSend.setBounds (190, 4, 54, 22);
        btnMute.setBounds (6, 30, 32, 24);
        btnSolo.setBounds (42, 30, 32, 24);
        btnArm.setBounds (78, 30, 32, 24);
        btnFx.setBounds (114, 30, 32, 24);
        btnDel.setBounds (150, 30, 32, 24);
        inputBox.setBounds (188, 30, 56, 24);
        vol.setBounds (6, 58, 238, 18);
        pan.setBounds (6, 76, 238, 18);
    }

    void updateMeters()
    {
        const float l = track.peakL.load(), r = track.peakR.load();
        if (std::abs (l - lastL) > 0.002f || std::abs (r - lastR) > 0.002f)
        {
            lastL = l; lastR = r;
            repaint (248, 0, 52, ui::rowH);
        }
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff1c1c1c));
        g.setColour (track.isBus.load() ? Colour (0xff3a2a4a) : Colour (0xff2a2a2a));   // buses en morado
        g.fillRect (0, 0, ui::headerW, getHeight());
        g.setColour (Colours::black);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        g.drawVerticalLine (ui::headerW - 1, 0.0f, (float) getHeight());

        auto bar = [&] (int x, float v)
        {
            const float db = Decibels::gainToDecibels (v, -60.0f);
            const float f = jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
            const int h = (int) (f * 84.0f);
            g.setColour (Colours::black);
            g.fillRect (x, 6, 14, 84);
            g.setColour (db > -3.0f ? Colours::red : (db > -12.0f ? Colours::yellow : Colours::limegreen));
            g.fillRect (x, 6 + 84 - h, 14, h);
        };
        bar (252, track.peakL.load());
        bar (272, track.peakR.load());

        const double pps = owner.pps();
        const auto visible = g.getClipBounds();

        drawGrid (g, pps, visible);

        for (auto& c : track.clips)
        {
            const double startSec = (double) c->startSample.load() / eng.sampleRate;
            const double lenSec = (double) c->audio.getNumSamples() / eng.sampleRate;
            Rectangle<int> r (ui::headerW + (int) (startSec * pps), 4, jmax (2, (int) (lenSec * pps)), getHeight() - 8);

            g.setColour (c.get() == owner.selectedClip() ? Colour (0xff3d7cb8) : Colour (0xff2b5a85));
            g.fillRect (r);

            auto vis = r.getIntersection (visible);
            if (! vis.isEmpty() && c->thumb != nullptr)
            {
                g.setColour (Colours::white.withAlpha (0.85f));
                const double t0 = (vis.getX() - r.getX()) / pps;
                const double t1 = (vis.getRight() - r.getX()) / pps;
                c->thumb->drawChannels (g, vis.withTrimmedTop (16).withTrimmedBottom (2), t0, t1, 1.0f);
            }

            g.setColour (Colours::white);
            g.setFont (12.0f);
            g.drawText (c->name, r.getX() + 4, r.getY() + 1, jmax (10, r.getWidth() - 8), 14, Justification::left, true);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.drawRect (r);
        }
    }

    void mouseMove (const MouseEvent& e) override
    {
        MouseCursor::StandardCursorType cur = MouseCursor::NormalCursor;
        if (e.x >= ui::headerW)
        {
            if (owner.getTool() == ui::toolHand)     cur = MouseCursor::DraggingHandCursor;
            else if (owner.getTool() == ui::toolCut) cur = MouseCursor::CrosshairCursor;
        }
        setMouseCursor (cur);
    }

    void mouseDown (const MouseEvent& e) override
    {
        dragClip = nullptr;
        if (e.x < ui::headerW) return;

        auto* c = clipAt (e.x);

        if (e.mods.isPopupMenu())
        {
            if (c != nullptr) owner.setSelected (&track, c);
            showContextMenu (c);
            return;
        }

        const int tool = owner.getTool();
        owner.setSelected (c != nullptr ? &track : nullptr, c);

        if (c != nullptr && tool == ui::toolCut)
        {
            // Herramienta Cortar: divide el clip donde haces clic (con imán a la rejilla)
            eng.splitClip (track, c, owner.snapSample (sampleAt (e.x)));
            owner.setSelected (nullptr, nullptr);
            owner.clipsChanged();
            return;
        }

        if (c != nullptr && tool == ui::toolHand)
        {
            dragClip = c;
            dragOrigStart = c->startSample.load();
        }
        else
        {
            owner.setCursorFromX (e.x);
        }
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (dragClip == nullptr) return;
        const auto delta = (int64) (e.getDistanceFromDragStartX() / owner.pps() * eng.sampleRate);
        dragClip->startSample = jmax<int64> (0, owner.snapSample (dragOrigStart + delta));
        repaint();
    }

    void mouseUp (const MouseEvent&) override
    {
        if (dragClip != nullptr) { dragClip = nullptr; owner.clipsChanged(); }
    }

private:
    void setupToggle (TextButton& b, const String& text, Colour onColour)
    {
        b.setButtonText (text);
        b.setClickingTogglesState (true);
        b.setColour (TextButton::buttonOnColourId, onColour);
        addAndMakeVisible (b);
    }

    int64 sampleAt (int x) const
    {
        return jmax<int64> (0, (int64) ((x - ui::headerW) / owner.pps() * eng.sampleRate));
    }

    bool ownsClip (Clip* c) const
    {
        for (auto& x : track.clips)
            if (x.get() == c) return true;
        return false;
    }

    // Líneas verticales de la rejilla (compases más marcados que tiempos y subdivisiones)
    void drawGrid (Graphics& g, double pps, Rectangle<int> visible)
    {
        const double beat = owner.beatSeconds();
        double step = owner.gridStepSeconds();
        if (step <= 0.0) step = beat;          // en modo libre solo se dibujan los tiempos
        if (step <= 0.0 || beat <= 0.0) return;
        while (step * pps < 8.0) step *= 2.0;  // muy alejado: menos líneas

        const int x0 = jmax (ui::headerW, visible.getX());
        const int first = jmax (0, (int) std::floor ((x0 - ui::headerW) / (step * pps)) - 1);

        for (int i = first;; ++i)
        {
            const double t = i * step;
            const int x = ui::headerW + (int) (t * pps);
            if (x > visible.getRight()) break;
            if (x < x0) continue;

            const double inBeats = t / beat;
            const bool isBar  = std::abs (inBeats / 4.0 - std::round (inBeats / 4.0)) < 1.0e-6;
            const bool isBeat = std::abs (inBeats - std::round (inBeats)) < 1.0e-6;

            g.setColour (Colours::white.withAlpha (isBar ? 0.16f : (isBeat ? 0.09f : 0.04f)));
            g.drawVerticalLine (x, 0.0f, (float) getHeight());
        }
    }

    // Menú del clic derecho: herramientas + acciones sobre el clip
    void showContextMenu (Clip* c)
    {
        const int tool = owner.getTool();

        PopupMenu m;
        m.addItem (10, T ("Herramienta Selección (V)"), true, tool == ui::toolSelect);
        m.addItem (11, T ("Herramienta Mano: mover audio (H)"), true, tool == ui::toolHand);
        m.addItem (12, T ("Herramienta Cortar: dividir al hacer clic (C)"), true, tool == ui::toolCut);
        m.addSeparator();

        if (c != nullptr)
        {
            m.addItem (1, T ("Dividir en el cursor"));
            m.addItem (2, T ("Eliminar clip (Supr)"));
            m.addItem (3, T ("Bucle sobre este clip"));
            m.addSeparator();
        }
        m.addItem (20, T ("Activar / desactivar bucle"));

        SafePointer<TrackRow> sp (this);
        m.showMenuAsync (PopupMenu::Options(), [sp, c] (int r)
        {
            auto* self = sp.getComponent();
            if (self == nullptr || r == 0) return;

            if (r >= 10 && r <= 12) { self->owner.setTool (r - 10); return; }
            if (r == 20)            { self->owner.toggleLoop(); return; }

            if (! self->ownsClip (c)) return;   // el clip pudo borrarse con el menú abierto

            if (r == 3) { self->owner.setLoopFromClip (*c); return; }
            if (r == 1) self->eng.splitClip (self->track, c, self->eng.position.load());
            if (r == 2) self->eng.removeClip (self->track, c);

            self->owner.setSelected (nullptr, nullptr);
            self->owner.clipsChanged();
        });
    }

    Clip* clipAt (int x) const
    {
        const double pps = owner.pps();
        for (auto it = track.clips.rbegin(); it != track.clips.rend(); ++it)
        {
            const int x0 = ui::headerW + (int) ((double) (*it)->startSample.load() / eng.sampleRate * pps);
            const int w = (int) ((double) (*it)->audio.getNumSamples() / eng.sampleRate * pps);
            if (x >= x0 && x <= x0 + jmax (2, w))
                return it->get();
        }
        return nullptr;
    }

    Engine& eng;
    Track& track;
    RowOwner& owner;

    Label nameLabel;
    TextButton btnMute, btnSolo, btnArm, btnFx, btnDel, btnSend;
    Slider vol, pan;
    ComboBox inputBox;

    Clip* dragClip = nullptr;
    int64 dragOrigStart = 0;
    float lastL = 0.0f, lastR = 0.0f;
};

// ---------------------------------------------------------------------------
// Contenido del timeline: regla de tiempo + filas
// ---------------------------------------------------------------------------
class TimelineContent : public Component
{
public:
    std::function<double()> getPps;
    std::function<double()> getBeatSec;
    std::function<void (int)> onRulerClick;
    std::function<void (int, int)> onRulerDrag;           // arrastrar en la regla = elegir región de bucle
    std::function<void (float, int)> onZoomWheel;         // Ctrl + rueda

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff161616));
        const auto clip = g.getClipBounds();
        if (clip.getY() >= ui::rulerH || ! getPps || ! getBeatSec) return;

        g.setColour (Colour (0xff262626));
        g.fillRect (0, 0, getWidth(), ui::rulerH);

        // La regla marca compases (números) y tiempos (rayitas) según el tempo del proyecto.
        const double pps = getPps();
        const double beatPx = getBeatSec() * pps;
        const double barPx = beatPx * 4.0;

        int barStep = 1;
        while (barPx * barStep < 44.0) barStep *= 2;      // muy alejado: números cada 2, 4, 8... compases

        g.setFont (11.0f);

        if (beatPx >= 10.0)
        {
            const int firstBeat = jmax (0, (int) ((clip.getX() - ui::headerW) / beatPx) - 1);
            for (int b = firstBeat;; ++b)
            {
                const int x = ui::headerW + (int) (b * beatPx);
                if (x > clip.getRight()) break;
                if (b % 4 == 0) continue;

                g.setColour (Colour (0xff6a6a6a));
                g.drawVerticalLine (x, (float) ui::rulerH - 6.0f, (float) ui::rulerH);
            }
        }

        const int firstBar = jmax (0, (int) ((clip.getX() - ui::headerW) / barPx) - 1);
        for (int bar = firstBar;; ++bar)
        {
            const int x = ui::headerW + (int) (bar * barPx);
            if (x > clip.getRight()) break;

            g.setColour (Colours::grey);
            g.drawVerticalLine (x, (float) ui::rulerH - 14.0f, (float) ui::rulerH);

            if (bar % barStep == 0)
            {
                g.setColour (Colours::lightgrey);
                g.drawText (String (bar + 1), x + 3, 2, 50, 14, Justification::left);
            }
        }
    }

    void mouseDown (const MouseEvent& e) override
    {
        rulerDownX = -1;
        if (e.y < ui::rulerH && e.x >= ui::headerW && onRulerClick)
        {
            rulerDownX = e.x;
            onRulerClick (e.x);
        }
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (rulerDownX >= 0 && std::abs (e.x - rulerDownX) > 4 && onRulerDrag)
            onRulerDrag (rulerDownX, e.x);
    }

    void mouseUp (const MouseEvent&) override { rulerDownX = -1; }

    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (e.mods.isCtrlDown() && onZoomWheel)
            onZoomWheel (w.deltaY, e.x);
        else
            Component::mouseWheelMove (e, w);     // sin Ctrl: desplaza la vista como siempre
    }

private:
    int rulerDownX = -1;
};

class Playhead : public Component
{
public:
    Playhead() { setInterceptsMouseClicks (false, false); }
    void paint (Graphics& g) override
    {
        g.setColour (Colours::orangered);
        g.fillRect (1, 0, 1, getHeight());
    }
};

// Región de bucle: banda dorada sobre todas las pistas (gris si el bucle está apagado)
class LoopOverlay : public Component
{
public:
    LoopOverlay (Engine& e, std::function<double()> p) : eng (e), pps (std::move (p))
    {
        setInterceptsMouseClicks (false, false);
    }

    void paint (Graphics& g) override
    {
        const int64 s = eng.loopStart.load(), en = eng.loopEnd.load();
        if (en <= s) return;

        const double p = pps();
        const int x0 = ui::headerW + (int) ((double) s / eng.sampleRate * p);
        const int x1 = ui::headerW + (int) ((double) en / eng.sampleRate * p);
        const Colour col = eng.loopOn.load() ? Colours::gold : Colours::grey;

        g.setColour (col.withAlpha (0.12f));
        g.fillRect (x0, 0, x1 - x0, getHeight());
        g.setColour (col.withAlpha (0.85f));
        g.fillRect (x0, ui::rulerH - 6, x1 - x0, 6);
        g.drawVerticalLine (x0, 0.0f, (float) getHeight());
        g.drawVerticalLine (x1, 0.0f, (float) getHeight());
    }

private:
    Engine& eng;
    std::function<double()> pps;
};

// ---------------------------------------------------------------------------
// Panel de salida y envíos de una pista (se abre con el botón "Env")
// ---------------------------------------------------------------------------
class SendPanel : public Component
{
public:
    SendPanel (Engine& e, Track& t) : eng (e), track (t)
    {
        buses = eng.busList();
        setSize (260, 176);

        lblOut.setText (T ("Salida de la pista"), dontSendNotification);
        addAndMakeVisible (lblOut);

        outBox.addItem ("Master", 1);
        for (size_t i = 0; i < buses.size(); ++i)
            outBox.addItem (buses[i]->name, (int) i + 2);
        outBox.setSelectedId (idOf (track.outputBus.load()), dontSendNotification);
        outBox.onChange = [this]
        {
            const int id = outBox.getSelectedId();
            track.outputBus = id >= 2 ? buses[(size_t) id - 2] : nullptr;
        };
        addAndMakeVisible (outBox);

        for (int s = 0; s < 2; ++s)
        {
            lblSend[s].setText (T ("Envío ") + String (s + 1), dontSendNotification);
            addAndMakeVisible (lblSend[s]);

            sendBox[s].addItem (T ("(ninguno)"), 1);
            for (size_t i = 0; i < buses.size(); ++i)
                sendBox[s].addItem (buses[i]->name, (int) i + 2);
            sendBox[s].setSelectedId (idOf (track.sendTarget[s].load()), dontSendNotification);
            sendBox[s].onChange = [this, s]
            {
                const int id = sendBox[s].getSelectedId();
                track.sendTarget[s] = id >= 2 ? buses[(size_t) id - 2] : nullptr;
            };
            addAndMakeVisible (sendBox[s]);

            lvl[s].setSliderStyle (Slider::LinearHorizontal);
            lvl[s].setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
            lvl[s].setRange (-60.0, 6.0, 0.1);
            lvl[s].setDoubleClickReturnValue (true, -12.0);
            lvl[s].setValue (Decibels::gainToDecibels (track.sendLevel[s].load(), -60.0f), dontSendNotification);
            lvl[s].setPopupDisplayEnabled (true, true, nullptr);
            lvl[s].onValueChange = [this, s]
            {
                track.sendLevel[s] = Decibels::decibelsToGain ((float) lvl[s].getValue(), -60.0f);
            };
            addAndMakeVisible (lvl[s]);
        }

        if (buses.empty())
        {
            hint.setText (T ("Crea un bus con '+ Bus' en la barra superior."), dontSendNotification);
            hint.setFont (Font (FontOptions (12.0f)));
            addAndMakeVisible (hint);
        }
    }

    void resized() override
    {
        lblOut.setBounds (10, 8, 240, 18);
        outBox.setBounds (10, 28, 240, 24);
        lblSend[0].setBounds (10, 58, 240, 18);
        sendBox[0].setBounds (10, 78, 120, 24);
        lvl[0].setBounds (134, 78, 116, 24);
        lblSend[1].setBounds (10, 106, 240, 18);
        sendBox[1].setBounds (10, 126, 120, 24);
        lvl[1].setBounds (134, 126, 116, 24);
        hint.setBounds (10, 152, 240, 18);
    }

private:
    int idOf (Track* t) const
    {
        for (size_t i = 0; i < buses.size(); ++i)
            if (buses[i] == t) return (int) i + 2;
        return 1;
    }

    Engine& eng;
    Track& track;
    std::vector<Track*> buses;
    Label lblOut, lblSend[2], hint;
    ComboBox outBox, sendBox[2];
    Slider lvl[2];
};

// ---------------------------------------------------------------------------
// Ventana principal
// ---------------------------------------------------------------------------
class MainComponent : public Component, public RowOwner, private Timer
{
public:
    MainComponent()
    {
        setSize (1280, 720);

        auto err = engine.start();
        if (err.isNotEmpty())
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, T ("Audio"),
                T ("No se pudo abrir el dispositivo de audio por defecto.\nAbre 'Audio' y elige otro driver o tarjeta.\n\n") + err);

        mk (bHome, "|<", [this] { engine.position = 0; });
        mk (bPlay, "Play", [this] { if (engine.playing) engine.pause(); else engine.play(); });
        mk (bStop, "Stop", [this] { doStop(); });
        mk (bRec,  "Rec",  [this] { doRecord(); });
        mk (bAdd,  T ("+ Pista"), [this] { addTrackRow (engine.addTrack ("Pista " + String (rows.size() + 1))); layoutContent(); });
        mk (bAddBus, "+ Bus", [this]
        {
            int n = 1;
            for (auto& t : engine.tracks) if (t->isBus.load()) ++n;
            addTrackRow (engine.addTrack ("Bus " + String (n), true));
            layoutContent();
        });
        mk (bImport, "Importar", [this] { importAudio(); });
        mk (bZoomOut, "-", [this] { setZoom (pxPerSec / 1.25); });
        mk (bZoomIn, "+", [this] { setZoom (pxPerSec * 1.25); });
        mk (bAudio, "Audio", [this] { showAudioSettings(); });
        mk (bScan, "Escanear plugins", [this] { scanPlugins(); });
        mk (bReport, "Ver plugins", [this] { showReport(); });
        mk (bOpen, "Abrir", [this] { openProject(); });
        mk (bSave, "Guardar", [this] { saveProjectQuick(); });

        // Herramientas: Selección / Mano / Cortar
        auto mkTool = [this] (TextButton& b, const String& text, int toolId)
        {
            b.setButtonText (text);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (1001);
            b.setColour (TextButton::buttonOnColourId, Colour (0xff3d7cb8));
            b.setWantsKeyboardFocus (false);
            b.onClick = [this, toolId] { setTool (toolId); };
            addAndMakeVisible (b);
        };
        mkTool (bSelect, T ("Selección"), ui::toolSelect);
        mkTool (bHand, "Mano", ui::toolHand);
        mkTool (bCut, "Cortar", ui::toolCut);
        bSelect.setToggleState (true, dontSendNotification);

        // Tempo y rejilla
        bpmLabel.setText ("BPM", dontSendNotification);
        bpmLabel.setColour (Label::textColourId, Colours::white);
        addAndMakeVisible (bpmLabel);

        bpmSlider.setSliderStyle (Slider::IncDecButtons);
        bpmSlider.setIncDecButtonsMode (Slider::incDecButtonsDraggable_AutoDirection);
        bpmSlider.setTextBoxStyle (Slider::TextBoxLeft, false, 60, 24);
        bpmSlider.setRange (20.0, 400.0, 0.1);
        bpmSlider.setValue (engine.bpm.load(), dontSendNotification);
        bpmSlider.setScrollWheelEnabled (false);
        bpmSlider.onValueChange = [this]
        {
            engine.bpm = bpmSlider.getValue();
            content.repaint();
            repaintRows();
        };
        addAndMakeVisible (bpmSlider);

        gridLabel.setText (T ("Rejilla"), dontSendNotification);
        gridLabel.setColour (Label::textColourId, Colours::white);
        addAndMakeVisible (gridLabel);

        for (int i = 0; i < ui::numGridItems; ++i)
            gridBox.addItem (T (ui::gridItems()[i].name), i + 1);
        gridBox.setSelectedId (engine.gridId, dontSendNotification);
        gridBox.setWantsKeyboardFocus (false);
        gridBox.setScrollWheelEnabled (false);
        gridBox.onChange = [this] { engine.gridId = gridBox.getSelectedId(); repaintRows(); };
        addAndMakeVisible (gridBox);

        // Bucle
        bLoop.setButtonText ("Bucle");
        bLoop.setClickingTogglesState (true);
        bLoop.setColour (TextButton::buttonOnColourId, Colour (0xffb8891a));
        bLoop.setWantsKeyboardFocus (false);
        bLoop.onClick = [this] { toggleLoop(); };
        addAndMakeVisible (bLoop);

        statusLabel.setColour (Label::textColourId, Colours::lightgreen);
        statusLabel.setJustificationType (Justification::centredLeft);
        addAndMakeVisible (statusLabel);

        timeLabel.setFont (Font (FontOptions (22.0f, Font::bold)));
        timeLabel.setColour (Label::textColourId, Colours::orange);
        timeLabel.setText ("00:00.000", dontSendNotification);
        addAndMakeVisible (timeLabel);

        master.setSliderStyle (Slider::LinearHorizontal);
        master.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        master.setRange (-60.0, 6.0, 0.1);
        master.setSkewFactorFromMidPoint (-12.0);
        master.setValue (0.0, dontSendNotification);
        master.setDoubleClickReturnValue (true, 0.0);
        master.setPopupDisplayEnabled (true, true, nullptr);
        master.onValueChange = [this] { engine.masterGain = Decibels::decibelsToGain ((float) master.getValue(), -60.0f); };
        master.setWantsKeyboardFocus (false);
        addAndMakeVisible (master);

        content.getPps = [this] { return pxPerSec; };
        content.getBeatSec = [this] { return beatSeconds(); };
        content.onRulerClick = [this] (int x) { setCursorFromX (x); };
        content.onRulerDrag = [this] (int a, int b) { setLoopFromRuler (a, b); };
        content.onZoomWheel = [this] (float dy, int x)
        {
            zoomAt (std::pow (1.2, (double) jlimit (-1.0f, 1.0f, dy * 4.0f)), x);
        };
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, true);
        viewport.setWantsKeyboardFocus (false);   // así Supr, Inicio, Ctrl+S llegan a esta ventana
        addAndMakeVisible (viewport);
        content.addAndMakeVisible (loopOverlay);
        content.addAndMakeVisible (playhead);

        setWantsKeyboardFocus (true);
        layoutContent();
        startTimerHz (30);
    }

    ~MainComponent() override
    {
        stopTimer();
        windows.clear();      // cerrar editores antes de destruir los plugins
        rows.clear();
    }

    // ------------------------- RowOwner ------------------------------------
    double pps() const override { return pxPerSec; }
    void setCursorFromX (int x) override
    {
        const auto s = (int64) (jmax (0.0, (x - ui::headerW) / pxPerSec) * engine.sampleRate);
        engine.position = snapSample (s);
    }
    void clipsChanged() override { validateSelection(); layoutContent(); repaintRows(); }

    // --- herramientas, rejilla y selección ---
    int getTool() const override { return tool; }

    void setTool (int t) override
    {
        tool = t;
        bSelect.setToggleState (t == ui::toolSelect, dontSendNotification);
        bHand.setToggleState (t == ui::toolHand, dontSendNotification);
        bCut.setToggleState (t == ui::toolCut, dontSendNotification);
    }

    double beatSeconds() const override { return 60.0 / jmax (20.0, engine.bpm.load()); }

    double gridStepSeconds() const override
    {
        const int idx = jlimit (0, ui::numGridItems - 1, engine.gridId - 1);
        return ui::gridItems()[idx].beats * beatSeconds();
    }

    int64 snapSample (int64 s) const override
    {
        const double step = gridStepSeconds() * engine.sampleRate;
        if (step < 1.0) return s;                         // modo libre
        return jmax<int64> (0, (int64) (std::round ((double) s / step) * step));
    }

    Clip* selectedClip() const override { return selClip; }

    void setSelected (Track* t, Clip* c) override
    {
        selTrack = t;
        selClip = c;
        repaintRows();
    }

    void setLoopFromClip (Clip& c) override
    {
        engine.loopStart = c.startSample.load();
        engine.loopEnd = c.startSample.load() + c.audio.getNumSamples();
        engine.loopOn = true;
        syncLoopButton();
    }

    void toggleLoop() override
    {
        if (! engine.loopOn.load() && engine.loopEnd.load() <= engine.loopStart.load())
        {
            // todavía no hay región: se crea de 4 compases desde el cursor
            const int64 s = engine.position.load();
            engine.loopStart = s;
            engine.loopEnd = s + (int64) (beatSeconds() * 16.0 * engine.sampleRate);
        }
        engine.loopOn = ! engine.loopOn.load();
        syncLoopButton();
    }

    void showSendPanel (Track& t, Component& source) override
    {
        CallOutBox::launchAsynchronously (std::make_unique<SendPanel> (engine, t), source.getScreenBounds(), nullptr);
    }

    void removeTrack (Track& t) override
    {
        closeWindowsFor (t);
        for (size_t i = 0; i < rows.size(); ++i)
            if (&rows[i]->getTrack() == &t)
            {
                // se destruye la fila primero (guarda una referencia a la pista)
                auto* tp = &t;
                SafePointer<MainComponent> sp (this);
                MessageManager::callAsync ([sp, tp, i]
                {
                    if (auto* self = sp.getComponent())
                    {
                        for (size_t k = 0; k < self->rows.size(); ++k)
                            if (&self->rows[k]->getTrack() == tp)
                            {
                                self->rows.erase (self->rows.begin() + (long) k);
                                break;
                            }
                        self->engine.removeTrack (tp);
                        self->layoutContent();
                    }
                    juce::ignoreUnused (i);
                });
                return;
            }
    }

    void showFxMenu (Track& t) override
    {
        PopupMenu m, add;
        auto types = engine.host.list.getTypes();

        std::map<String, PopupMenu> byVendor;
        for (int i = 0; i < types.size(); ++i)
            if (! types[i].isInstrument)
                byVendor[types[i].manufacturerName].addItem (1000 + i, types[i].name);

        for (auto& kv : byVendor)
            add.addSubMenu (kv.first.isEmpty() ? String ("Otros") : kv.first, kv.second);
        if (byVendor.empty())
            add.addItem (1, T ("(primero pulsa 'Escanear plugins')"), false);

        m.addSubMenu (T ("Añadir plugin"), add);
        m.addSeparator();
        for (int i = 0; i < (int) t.plugins.size(); ++i)
            m.addItem (100 + i, T ("Abrir: ") + t.plugins[(size_t) i]->getName());
        for (int i = 0; i < (int) t.plugins.size(); ++i)
            m.addItem (200 + i, T ("Quitar: ") + t.plugins[(size_t) i]->getName());

        SafePointer<MainComponent> sp (this);
        Track* tp = &t;
        m.showMenuAsync (PopupMenu::Options(), [sp, tp, types] (int r)
        {
            auto* self = sp.getComponent();
            if (self == nullptr || r == 0 || ! self->engine.hasTrack (tp)) return;

            if (r >= 1000)
            {
                String err;
                auto inst = self->engine.host.create (types[r - 1000], self->engine.sampleRate, self->engine.maxBlock, err);
                if (inst == nullptr)
                {
                    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Plugin", "No se pudo cargar:\n" + err);
                    return;
                }
                self->engine.addPlugin (*tp, std::move (inst));
                self->openEditor (*tp->plugins.back());
            }
            else if (r >= 200 && r < 300)
            {
                const int idx = r - 200;
                if (idx < (int) tp->plugins.size())
                {
                    self->closeWindowFor (tp->plugins[(size_t) idx].get());
                    self->engine.removePlugin (*tp, idx);
                }
            }
            else if (r >= 100 && r < 200)
            {
                const int idx = r - 100;
                if (idx < (int) tp->plugins.size())
                    self->openEditor (*tp->plugins[(size_t) idx]);
            }
        });
    }

    // ------------------------- Componente ----------------------------------
    void resized() override
    {
        auto r = getLocalBounds();
        auto bar1 = r.removeFromTop (ui::toolbarH).reduced (6, 5);   // fila 1: transporte y proyecto
        auto bar2 = r.removeFromTop (ui::toolbarH).reduced (6, 5);   // fila 2: herramientas, tempo, rejilla
        masterMeterBounds = {};

        for (auto* b : { &bHome, &bPlay, &bStop, &bRec })
        {
            b->setBounds (bar1.removeFromLeft (56));
            bar1.removeFromLeft (3);
        }
        bar1.removeFromLeft (8);
        timeLabel.setBounds (bar1.removeFromLeft (130));
        bar1.removeFromLeft (8);

        for (auto* b : { &bOpen, &bSave, &bAudio, &bScan, &bReport })
        {
            b->setBounds (bar1.removeFromLeft (b == &bScan ? 112 : 70));
            bar1.removeFromLeft (3);
        }

        masterMeterBounds = bar1.removeFromRight (40).reduced (0, 6);
        master.setBounds (bar1.removeFromRight (130));
        statusLabel.setBounds (bar1.reduced (8, 0));

        for (auto* b : { &bSelect, &bHand, &bCut })
        {
            b->setBounds (bar2.removeFromLeft (74));
            bar2.removeFromLeft (3);
        }
        bar2.removeFromLeft (10);
        bpmLabel.setBounds (bar2.removeFromLeft (36));
        bpmSlider.setBounds (bar2.removeFromLeft (130));
        bar2.removeFromLeft (10);
        gridLabel.setBounds (bar2.removeFromLeft (52));
        gridBox.setBounds (bar2.removeFromLeft (130));
        bar2.removeFromLeft (6);
        bLoop.setBounds (bar2.removeFromLeft (60));
        bar2.removeFromLeft (10);

        for (auto* b : { &bAdd, &bAddBus, &bImport, &bZoomOut, &bZoomIn })
        {
            b->setBounds (bar2.removeFromLeft ((b == &bZoomOut || b == &bZoomIn) ? 34 : 76));
            bar2.removeFromLeft (3);
        }

        viewport.setBounds (r);
        layoutContent();
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff111111));

        if (! masterMeterBounds.isEmpty())
        {
            auto drawBar = [&] (Rectangle<int> b, float v)
            {
                const float db = Decibels::gainToDecibels (v, -60.0f);
                const float f = jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
                g.setColour (Colours::black);
                g.fillRect (b);
                g.setColour (db > -3.0f ? Colours::red : (db > -12.0f ? Colours::yellow : Colours::limegreen));
                g.fillRect (b.withTrimmedRight ((int) ((1.0f - f) * (float) b.getWidth())));
            };
            auto area = masterMeterBounds;
            auto top = area.removeFromTop (area.getHeight() / 2 - 1);
            drawBar (top, engine.masterPeakL.load());
            area.removeFromTop (2);
            drawBar (area, engine.masterPeakR.load());
        }
    }

    bool keyPressed (const KeyPress& k) override
    {
        // Si se está escribiendo en una caja de texto (nombre de pista, BPM), no se roban las teclas.
        if (dynamic_cast<TextEditor*> (Component::getCurrentlyFocusedComponent()) != nullptr)
            return false;

        const auto mods = k.getModifiers();

        // Ctrl+S = guardar; Ctrl+Mayús+S = guardar como
        if ((mods.isCtrlDown() || mods.isCommandDown()) && (k.getKeyCode() == 'S' || k.getKeyCode() == 's'))
        {
            if (mods.isShiftDown()) saveProject(); else saveProjectQuick();
            return true;
        }

        if (k == KeyPress::spaceKey)
        {
            if (engine.playing) doStop(); else engine.play();
            return true;
        }
        if (k == KeyPress::homeKey) { engine.position = 0; return true; }

        // Supr (o Retroceso) = borrar el audio seleccionado
        if (k == KeyPress::deleteKey || k == KeyPress::backspaceKey)
        {
            deleteSelectedClip();
            return true;
        }

        // V / H / C = cambiar de herramienta
        if (! mods.isAnyModifierKeyDown())
        {
            const auto ch = CharacterFunctions::toLowerCase (k.getTextCharacter());
            if (ch == 'v') { setTool (ui::toolSelect); return true; }
            if (ch == 'h') { setTool (ui::toolHand);   return true; }
            if (ch == 'c') { setTool (ui::toolCut);    return true; }
        }
        return false;
    }

private:
    // ---------------------- Utilidades de interfaz -------------------------
    void mk (TextButton& b, const String& text, std::function<void()> fn)
    {
        b.setButtonText (text);
        b.onClick = std::move (fn);
        b.setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    void addTrackRow (Track* t)
    {
        auto row = std::make_unique<TrackRow> (engine, *t, *this);
        content.addAndMakeVisible (*row);
        rows.push_back (std::move (row));
    }

    void layoutContent()
    {
        int64 maxEnd = 0;
        for (auto& t : engine.tracks)
            for (auto& c : t->clips)
                maxEnd = jmax (maxEnd, c->startSample.load() + c->audio.getNumSamples());

        const double secs = jmax ((double) maxEnd / engine.sampleRate + 30.0,
                                  (viewport.getWidth() - ui::headerW) / pxPerSec);
        const int w = ui::headerW + (int) (secs * pxPerSec);
        const int h = ui::rulerH + (int) rows.size() * ui::rowH;

        content.setSize (jmax (w, viewport.getMaximumVisibleWidth()), jmax (h, viewport.getMaximumVisibleHeight()));
        for (size_t i = 0; i < rows.size(); ++i)
            rows[i]->setBounds (0, ui::rulerH + (int) i * ui::rowH, content.getWidth(), ui::rowH);

        loopOverlay.setBounds (content.getLocalBounds());
        loopOverlay.toFront (false);
        playhead.toFront (false);
        content.repaint();
    }

    void repaintRows() { for (auto& r : rows) r->repaint(); }

    void setZoom (double v)
    {
        pxPerSec = jlimit (5.0, 2000.0, v);
        layoutContent();
        repaintRows();
    }

    // Zoom que mantiene quieto el punto bajo el ratón (Ctrl + rueda)
    void zoomAt (double factor, int contentX)
    {
        const double anchor = jmax (0.0, (contentX - ui::headerW) / pxPerSec);
        const int screenOffset = contentX - viewport.getViewPositionX();

        setZoom (pxPerSec * factor);

        const int newX = ui::headerW + (int) (anchor * pxPerSec);
        viewport.setViewPosition (jmax (0, newX - screenOffset), viewport.getViewPositionY());
    }

    // Arrastrar en la regla de tiempo define la región del bucle (con imán a la rejilla)
    void setLoopFromRuler (int x0, int x1)
    {
        auto toSample = [this] (int x)
        {
            return snapSample ((int64) (jmax (0.0, (x - ui::headerW) / pxPerSec) * engine.sampleRate));
        };

        const int64 a = toSample (jmin (x0, x1)), b = toSample (jmax (x0, x1));
        if (b - a < (int64) (engine.sampleRate * 0.05)) return;

        engine.loopStart = a;
        engine.loopEnd = b;
        engine.loopOn = true;
        syncLoopButton();
    }

    void syncLoopButton()
    {
        bLoop.setToggleState (engine.loopOn.load(), dontSendNotification);
        loopOverlay.repaint();
    }

    // Si el clip seleccionado ya no existe (borrado, dividido...), se quita la selección.
    void validateSelection()
    {
        if (selClip == nullptr) return;
        for (auto& t : engine.tracks)
            for (auto& c : t->clips)
                if (c.get() == selClip) return;
        selTrack = nullptr;
        selClip = nullptr;
    }

    void deleteSelectedClip()
    {
        if (selClip == nullptr || selTrack == nullptr || ! engine.hasTrack (selTrack)) return;

        for (auto& c : selTrack->clips)
            if (c.get() == selClip)
            {
                engine.removeClip (*selTrack, selClip);
                selTrack = nullptr;
                selClip = nullptr;
                clipsChanged();
                return;
            }
    }

    void setStatus (const String& s)
    {
        statusLabel.setText (s, dontSendNotification);
        statusTicks = 90;     // ~3 segundos
    }

    // Refresca los controles de la barra con los datos del proyecto (tempo, rejilla, bucle)
    void syncControls()
    {
        bpmSlider.setValue (engine.bpm.load(), dontSendNotification);
        gridBox.setSelectedId (jlimit (1, ui::numGridItems, engine.gridId), dontSendNotification);
        syncLoopButton();
        content.repaint();
        repaintRows();
    }

    void saveProjectQuick()
    {
        if (currentProject == File())
            saveProject();            // primera vez: pide el nombre
        else
            doSave (currentProject);
    }

    void doSave (const File& f)
    {
        if (engine.saveProject (f))
        {
            currentProject = f;
            setStatus (T ("Guardado: ") + f.getFileName());
        }
        else
        {
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Guardar", "No se pudo guardar el proyecto.");
        }
    }

    void rebuildRows()
    {
        selTrack = nullptr;
        selClip = nullptr;
        windows.clear();
        rows.clear();
        for (auto& t : engine.tracks)
            addTrackRow (t.get());
        layoutContent();
    }

    void timerCallback() override
    {
        const double t = (double) engine.position.load() / engine.sampleRate;
        timeLabel.setText (String::formatted ("%02d:%02d.%03d", (int) t / 60, (int) t % 60, (int) ((t - std::floor (t)) * 1000.0)),
                           dontSendNotification);

        const int x = ui::headerW + (int) (t * pxPerSec);
        playhead.setBounds (x - 1, 0, 3, content.getHeight());

        if (engine.playing)
        {
            auto v = viewport.getViewArea();
            if (x > v.getRight() - 40 || x < v.getX())
                viewport.setViewPosition (jmax (0, x - 80), v.getY());
        }

        for (auto& r : rows) r->updateMeters();

        const int64 ls = engine.loopStart.load(), le = engine.loopEnd.load();
        const bool lo = engine.loopOn.load();
        if (ls != lastLoopS || le != lastLoopE || lo != lastLoopOn)
        {
            lastLoopS = ls; lastLoopE = le; lastLoopOn = lo;
            syncLoopButton();
        }

        if (statusTicks > 0 && --statusTicks == 0)
            statusLabel.setText ({}, dontSendNotification);

        bPlay.setColour (TextButton::buttonColourId, engine.playing ? Colour (0xff2e9d4f) : Colour (0xff3a3a3a));
        bRec.setColour (TextButton::buttonColourId, engine.recording ? Colour (0xffcc2222) : Colour (0xff3a3a3a));
        bPlay.repaint();
        bRec.repaint();
        repaint (getWidth() - 190, 0, 190, ui::toolbarH);
    }

    // ------------------------- Transporte ----------------------------------
    void doStop()
    {
        if (engine.stop())
        {
            layoutContent();
            repaintRows();
        }
    }

    void doRecord()
    {
        if (engine.recording) { doStop(); return; }
        if (! engine.startRecording())
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, T ("Grabar"),
                T ("Primero arma una pista con el botón R."));
    }

    // ------------------------- Archivos ------------------------------------
    void importAudio()
    {
        chooser = std::make_unique<FileChooser> (T ("Importar audio"), File(), engine.formats.getWildcardForAllFormats());
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles
                                  | FileBrowserComponent::canSelectMultipleItems,
                              [this] (const FileChooser& fc)
        {
            for (auto& f : fc.getResults())
            {
                auto clip = engine.loadClip (f);
                if (clip == nullptr)
                {
                    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Audio",
                        T ("No se pudo cargar: ") + f.getFileName() + T ("\n(formato no soportado o archivo demasiado grande)"));
                    continue;
                }
                auto* t = engine.addTrack (f.getFileNameWithoutExtension());
                clip->startSample = engine.position.load();
                engine.addClip (*t, std::move (clip));
                addTrackRow (t);
            }
            layoutContent();
            repaintRows();
        });
    }

    void saveProject()
    {
        chooser = std::make_unique<FileChooser> (T ("Guardar proyecto"), currentProject, "*.lapauta");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                                  | FileBrowserComponent::warnAboutOverwriting,
                              [this] (const FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == File()) return;
            doSave (f.withFileExtension ("lapauta"));
        });
    }

    void openProject()
    {
        chooser = std::make_unique<FileChooser> (T ("Abrir proyecto"), File(), "*.lapauta");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == File()) return;

            windows.clear();
            rows.clear();
            String err;
            const bool ok = engine.loadProject (f, err);
            rebuildRows();
            if (ok)
            {
                currentProject = f;
                syncControls();
                setStatus (T ("Abierto: ") + f.getFileName());
            }
            if (err.isNotEmpty())
                AlertWindow::showMessageBoxAsync (ok ? MessageBoxIconType::InfoIcon : MessageBoxIconType::WarningIcon,
                                                  "Proyecto", err);
        });
    }

    // --------------------------- Audio -------------------------------------
    void showAudioSettings()
    {
        auto* sel = new AudioDeviceSelectorComponent (engine.deviceManager, 0, 8, 0, 2, false, false, false, false);
        DialogWindow::LaunchOptions o;
        o.content.setOwned (sel);
        o.content->setSize (520, 460);
        o.dialogTitle = T ("Configuración de audio (driver y tarjeta)");
        o.dialogBackgroundColour = Colour (0xff202020);
        o.componentToCentreAround = this;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }

    // --------------------------- Plugins -----------------------------------
    class ScanJob : public ThreadWithProgressWindow
    {
    public:
        ScanJob (PluginHost& h, std::function<void()> done)
            : ThreadWithProgressWindow (T ("Analizando plugins..."), true, true), host (h), onDone (std::move (done)) {}

        void run() override
        {
            host.scanAll (*this, [this] (const String& n, float p)
            {
                setStatusMessage (T ("Analizando: ") + n);
                setProgress (p);
            });
        }

        void threadComplete (bool) override { onDone(); }

    private:
        PluginHost& host;
        std::function<void()> onDone;
    };

    void scanPlugins()
    {
        SafePointer<MainComponent> sp (this);
        scanJob = std::make_unique<ScanJob> (engine.host, [this, sp]
        {
            engine.host.saveList();
            MessageManager::callAsync ([sp]
            {
                if (auto* self = sp.getComponent()) self->showReport();
            });
        });
        scanJob->launchThread();
    }

    void showReport()
    {
        auto* te = new TextEditor();
        te->setMultiLine (true);
        te->setReadOnly (true);
        te->setText (engine.host.buildReport());
        te->setSize (760, 460);

        DialogWindow::LaunchOptions o;
        o.content.setOwned (te);
        o.dialogTitle = T ("Análisis de plugins");
        o.componentToCentreAround = this;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = true;
        o.launchAsync();
    }

    void openEditor (AudioPluginInstance& p)
    {
        for (auto& w : windows)
            if (w->plugin == &p) { w->toFront (true); return; }

        SafePointer<MainComponent> sp (this);
        windows.push_back (std::make_unique<PluginWindow> (p, [sp] (PluginWindow* w)
        {
            MessageManager::callAsync ([sp, w]
            {
                if (auto* self = sp.getComponent()) self->closeWindow (w);
            });
        }));
    }

    void closeWindow (PluginWindow* w)
    {
        for (size_t i = 0; i < windows.size(); ++i)
            if (windows[i].get() == w) { windows.erase (windows.begin() + (long) i); return; }
    }

    void closeWindowFor (AudioPluginInstance* p)
    {
        for (size_t i = 0; i < windows.size(); ++i)
            if (windows[i]->plugin == p) { windows.erase (windows.begin() + (long) i); return; }
    }

    void closeWindowsFor (Track& t)
    {
        for (auto& p : t.plugins)
            closeWindowFor (p.get());
    }

    // ----------------------------- Datos -----------------------------------
    Engine engine;                                       // se destruye al final
    std::vector<std::unique_ptr<PluginWindow>> windows;
    std::vector<std::unique_ptr<TrackRow>> rows;

    TimelineContent content;
    LoopOverlay loopOverlay { engine, [this] { return pxPerSec; } };
    Playhead playhead;
    Viewport viewport;

    TextButton bHome, bPlay, bStop, bRec, bAdd, bAddBus, bImport, bZoomOut, bZoomIn, bAudio, bScan, bReport, bOpen, bSave;
    TextButton bSelect, bHand, bCut, bLoop;
    Label timeLabel, bpmLabel, gridLabel, statusLabel;
    Slider master, bpmSlider;
    ComboBox gridBox;

    int tool = ui::toolSelect;
    Track* selTrack = nullptr;
    Clip* selClip = nullptr;
    File currentProject;
    int statusTicks = 0;
    int64 lastLoopS = -1, lastLoopE = -1;
    bool lastLoopOn = false;
    Rectangle<int> masterMeterBounds;

    std::unique_ptr<FileChooser> chooser;
    std::unique_ptr<ScanJob> scanJob;
    double pxPerSec = 80.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
