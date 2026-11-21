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
    constexpr int laneH = 72;       // alto de un carril de automatización
    constexpr int masterH = 64;     // alto de la cabecera de la fila Master
    constexpr int addH = 70;        // alto del botón "+" al final de las pistas
    constexpr int bottomH = 46;     // alto de la barra inferior (metrónomo y medidor RMS)

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
    virtual void showInputFxMenu (Track&) = 0;          // efectos de entrada (se graban con la voz)
    virtual void timeSelectionChanged() = 0;            // la selección de tiempo cambió (repintar)
    virtual void showRangeMenu (Track&) = 0;            // menú de la selección: exportar / traer audio de otro DAW

    virtual int getTool() const = 0;
    virtual void setTool (int) = 0;
    virtual int64 snapSample (int64) const = 0;
    virtual double gridStepSeconds() const = 0;      // 0 = libre (sin imán)
    virtual double beatSeconds() const = 0;
    virtual Clip* selectedClip() const = 0;
    virtual void setSelected (Track*, Clip*) = 0;
    virtual void setLoopFromClip (Clip&) = 0;
    virtual void toggleLoop() = 0;

    virtual void showEnvelopePanel (Track*) = 0;      // panel de automatización; nullptr = Master
    virtual void showMasterMenu() = 0;
    virtual Track* trackAtScreenY (int screenY) = 0;  // pista (no bus) bajo esa altura de pantalla, o nullptr
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
// Rejilla de fondo (la usan las filas de pistas y la fila Master)
// ---------------------------------------------------------------------------
inline void drawTimeGrid (Graphics& g, RowOwner& owner, double pps, Rectangle<int> visible, int height)
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
        g.drawVerticalLine (x, 0.0f, (float) height);
    }
}

// Texto legible del valor de una envolvente (dB, pan, estado, o el texto propio del parámetro del plugin)
inline String envValueText (const Envelope& e, float v)
{
    switch (e.target)
    {
        case envVolume:
        {
            const double db = volumeRange().convertFrom0to1 ((double) v);
            return db <= -59.9 ? String (T ("-inf dB")) : String (db, 1) + " dB";
        }
        case envPan:
        {
            const int p = (int) std::round (std::abs (v * 2.0f - 1.0f) * 100.0f);
            if (p == 0) return T ("Centro");
            return (v < 0.5f ? T ("Izq ") : T ("Der ")) + String (p) + "%";
        }
        case envMute:   return v >= 0.5f ? T ("Silenciado") : T ("Sonando");
        case envBypass: return v >= 0.5f ? T ("Efecto apagado") : T ("Efecto encendido");
        default:
            if (e.param != nullptr)
                return e.param->getText (v, 14) + " " + e.param->getLabel();
            return String (v, 2);
    }
}

// ---------------------------------------------------------------------------
// Carriles de automatización bajo una pista (o bajo el Master): dibujan y editan las curvas.
// 'target' = la pista, o nullptr para el Master. Clic en vacío = punto nuevo; arrastrar = mover;
// clic derecho = menú. En mute y bypass el valor es de tipo escalón (encendido/apagado).
// ---------------------------------------------------------------------------
class EnvLanes
{
public:
    EnvLanes (Engine& e, RowOwner& o, Component& c, Track* t) : eng (e), owner (o), comp (c), target (t) {}

    std::vector<Envelope*> visibleLanes() const
    {
        std::vector<Envelope*> v;
        for (auto& e : eng.envsOf (target))
            if (e->visible) v.push_back (e.get());
        return v;
    }

    int height() const { return (int) visibleLanes().size() * ui::laneH; }

    // curves = false: fondo y cabeceras (antes de la rejilla); curves = true: líneas y puntos (encima)
    void paint (Graphics& g, int top0, int width, double pps, bool curves)
    {
        auto lanes = visibleLanes();

        for (size_t k = 0; k < lanes.size(); ++k)
        {
            const Envelope& e = *lanes[k];
            const int top = top0 + (int) k * ui::laneH;
            const bool on = e.active.load();

            if (! curves)
            {
                g.setColour (Colour (0xff23272c));
                g.fillRect (0, top, ui::headerW, ui::laneH);
                g.setColour (Colour (0xff12171c));
                g.fillRect (ui::headerW, top, jmax (0, width - ui::headerW), ui::laneH);
                g.setColour (Colours::black);
                g.drawHorizontalLine (top, 0.0f, (float) width);
                g.drawVerticalLine (ui::headerW - 1, (float) top, (float) (top + ui::laneH));

                g.setFont (12.0f);
                g.setColour (on ? Colours::white : Colours::grey);
                g.drawText (e.name, 8, top + 5, 190, 16, Justification::left, true);

                g.setColour (on ? Colour (0xff2e9d4f) : Colour (0xff555555));
                g.fillRoundedRectangle (202.0f, (float) top + 5.0f, 42.0f, 18.0f, 3.0f);
                g.setColour (Colours::white);
                g.setFont (11.0f);
                g.drawText (on ? "ON" : "OFF", 202, top + 5, 42, 18, Justification::centred);

                g.setColour (Colour (0xff444a52));
                g.fillRoundedRectangle (248.0f, (float) top + 5.0f, 46.0f, 18.0f, 3.0f);
                g.setColour (Colours::white);
                g.setFont (10.0f);
                g.drawText (T ("Ocultar"), 248, top + 5, 46, 18, Justification::centred);

                String line;
                if (dragEnv == &e)           line = envValueText (e, dragValue);
                else if (e.points.empty())   line = T ("Sin puntos: haz clic en la curva para añadir");
                else                         line = T ("En el cursor: ") + envValueText (e, e.valueAt (eng.position.load()));

                g.setColour (Colours::orange);
                g.setFont (11.0f);
                g.drawText (line, 8, top + 28, ui::headerW - 16, 14, Justification::left, true);

                g.setColour (Colours::grey);
                g.setFont (10.0f);
                g.drawText (T ("Clic: punto · Arrastrar: mover · Clic dcho: opciones"), 8, top + 46, ui::headerW - 12, 14,
                            Justification::left, true);
            }
            else
            {
                g.setColour (Colours::white.withAlpha (0.07f));
                g.drawHorizontalLine (top + ui::laneH / 2, (float) ui::headerW, (float) width);

                if (e.points.empty()) continue;

                auto yOf = [top] (float v) { return (float) top + 4.0f + (1.0f - v) * (float) (ui::laneH - 8); };
                auto xOf = [&] (int64 s) { return (float) ui::headerW + (float) ((double) s / eng.sampleRate * pps); };

                Path p;
                p.startNewSubPath ((float) ui::headerW, yOf (e.points.front().value));
                for (size_t i = 0; i < e.points.size(); ++i)
                {
                    const float x = xOf (e.points[i].sample);
                    if (e.isStep() && i > 0) p.lineTo (x, yOf (e.points[i - 1].value));   // escalón
                    p.lineTo (x, yOf (e.points[i].value));
                }
                p.lineTo ((float) width, yOf (e.points.back().value));

                const Colour line = on ? Colour (0xffff9a2e) : Colours::grey;
                g.setColour (line);
                g.strokePath (p, PathStrokeType (1.6f));

                for (size_t i = 0; i < e.points.size(); ++i)
                {
                    const bool held = (dragEnv == &e && dragIdx == (int) i);
                    g.setColour (held ? Colours::white : line);
                    g.fillEllipse (xOf (e.points[i].sample) - 4.0f, yOf (e.points[i].value) - 4.0f, 8.0f, 8.0f);
                }
            }
        }
    }

    // 'e' viene de la fila (coordenadas de la fila); top0 = y donde empiezan los carriles.
    bool mouseDown (const MouseEvent& e, int top0)
    {
        dragEnv = nullptr;
        dragIdx = -1;

        auto lanes = visibleLanes();
        if (e.y < top0) return false;
        const int k = (e.y - top0) / ui::laneH;
        if (! isPositiveAndBelow (k, (int) lanes.size())) return false;

        auto* env = lanes[(size_t) k];
        const int top = top0 + k * ui::laneH;

        if (e.x < ui::headerW)                                  // cabecera del carril
        {
            if (e.mods.isPopupMenu())  showMenu (env, -1);
            else if (e.x >= 248)       { env->visible = false; owner.clipsChanged(); }
            else if (e.x >= 202)
            {
                eng.undoBegin();
                env->active = ! env->active.load();
                eng.undoEnd();
                comp.repaint();
            }
            return true;
        }

        int idx = pointAt (*env, top, e.x, e.y);
        if (e.mods.isPopupMenu()) { showMenu (env, idx); return true; }

        eng.undoBegin();                                        // Ctrl+Z deshace el punto nuevo / el movimiento
        undoOpen = true;

        if (idx < 0)                                            // clic en vacío = punto nuevo
        {
            const int64 s = sampleAt (e.x);
            eng.envAddPoint (*env, s, valueFromY (*env, e.y - top));
            env->active = true;                                 // al editar la curva se enciende sola
            for (size_t i = 0; i < env->points.size(); ++i)
                if (env->points[i].sample == s) { idx = (int) i; break; }
        }

        dragEnv = env;
        dragIdx = idx;
        dragTop = top;
        dragValue = idx >= 0 ? env->points[(size_t) idx].value : 0.0f;
        comp.repaint();
        return true;
    }

    bool mouseDrag (const MouseEvent& e, int)
    {
        if (dragEnv == nullptr || dragIdx < 0) return false;

        const float v = valueFromY (*dragEnv, e.y - dragTop);
        eng.envSetPoint (*dragEnv, dragIdx, sampleAt (e.x), v);
        dragValue = v;
        comp.repaint();
        return true;
    }

    void mouseUp()
    {
        if (undoOpen)
        {
            undoOpen = false;
            eng.undoEnd();               // un clic que no cambió nada no deja paso
        }
        if (dragEnv == nullptr) return;
        dragEnv = nullptr;
        dragIdx = -1;
        comp.repaint();
    }

private:
    int64 sampleAt (int x) const
    {
        const double secs = jmax (0.0, (x - ui::headerW) / owner.pps());
        return owner.snapSample ((int64) (secs * eng.sampleRate));
    }

    static float valueFromY (const Envelope& env, int yInLane)
    {
        const float v = jlimit (0.0f, 1.0f, 1.0f - (float) (yInLane - 4) / (float) (ui::laneH - 8));
        return env.isStep() ? (v >= 0.5f ? 1.0f : 0.0f) : v;
    }

    int pointAt (const Envelope& env, int top, int x, int y) const
    {
        for (size_t i = 0; i < env.points.size(); ++i)
        {
            const double px = ui::headerW + (double) env.points[i].sample / eng.sampleRate * owner.pps();
            const double py = top + 4.0 + (1.0 - env.points[i].value) * (ui::laneH - 8);
            if (std::abs (x - px) <= 6.0 && std::abs (y - py) <= 6.0)
                return (int) i;
        }
        return -1;
    }

    bool exists (Envelope* env) const
    {
        for (auto& e : eng.envsOf (target))
            if (e.get() == env) return true;
        return false;
    }

    void showMenu (Envelope* env, int pointIdx)
    {
        PopupMenu m;
        if (pointIdx >= 0) m.addItem (1, T ("Borrar este punto"));
        m.addItem (2, T ("Borrar todos los puntos"));
        m.addItem (3, env->active.load() ? T ("Apagar esta automatización") : T ("Encender esta automatización"));
        m.addItem (4, T ("Ocultar carril"));

        Component::SafePointer<Component> sp (&comp);
        m.showMenuAsync (PopupMenu::Options(), [this, sp, env, pointIdx] (int r)
        {
            if (sp.getComponent() == nullptr || r == 0 || ! exists (env)) return;

            if (r != 4) eng.undoBegin();
            if (r == 1)      eng.envRemovePoint (*env, pointIdx);
            else if (r == 2) eng.envClear (*env);
            else if (r == 3) env->active = ! env->active.load();
            else if (r == 4) { env->visible = false; owner.clipsChanged(); return; }

            comp.repaint();
        });
    }

    Engine& eng;
    RowOwner& owner;
    Component& comp;
    Track* target;

    bool undoOpen = false;
    Envelope* dragEnv = nullptr;
    int dragIdx = -1;
    int dragTop = 0;
    float dragValue = 0.0f;
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

        btnAuto.setButtonText ("Auto");
        btnAuto.setTooltip (T ("Automatización: volumen, pan, mute y parámetros de los plugins"));
        btnAuto.onClick = [this] { owner.showEnvelopePanel (&track); };
        addAndMakeVisible (btnAuto);

        btnInFx.setButtonText ("Input FX");
        btnInFx.setTooltip (T ("Efectos de entrada: tu voz entra al DAW ya con estos efectos (se graban con ellos)"));
        btnInFx.onClick = [this] { owner.showInputFxMenu (track); };
        addAndMakeVisible (btnInFx);

        setupToggle (btnMon, "Monitor", Colour (0xff2a7fd4));
        btnMon.setToggleState (track.monitor.load(), dontSendNotification);
        btnMon.setTooltip (T ("Escuchar tu voz con los Input FX mientras la pista está armada (usa audífonos para evitar acople)"));
        btnMon.onClick = [this] { track.monitor = btnMon.getToggleState(); };

        if (track.isBus.load())   // un bus no graba ni tiene solo/envíos
        {
            btnInFx.setVisible (false);
            btnMon.setVisible (false);
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

    // alto de la fila: la pista + un carril por cada automatización visible
    int desiredHeight() const { return ui::rowH + lanes.height(); }

    void resized() override
    {
        nameLabel.setBounds (6, 4, track.isBus.load() ? 182 : 128, 22);
        btnSend.setBounds (138, 4, 50, 22);
        btnAuto.setBounds (192, 4, 52, 22);
        btnMute.setBounds (6, 30, 32, 24);
        btnSolo.setBounds (42, 30, 32, 24);
        btnArm.setBounds (78, 30, 32, 24);
        btnFx.setBounds (114, 30, 32, 24);
        btnDel.setBounds (150, 30, 32, 24);
        inputBox.setBounds (188, 30, 56, 24);
        vol.setBounds (6, 58, track.isBus.load() ? 238 : 150, 18);
        pan.setBounds (6, 76, track.isBus.load() ? 238 : 150, 18);
        btnInFx.setBounds (160, 58, 84, 18);
        btnMon.setBounds (160, 76, 84, 18);
    }

    void updateMeters()
    {
        const float l = track.peakL.load(), r = track.peakR.load();
        if (std::abs (l - lastL) > 0.002f || std::abs (r - lastR) > 0.002f)
        {
            lastL = l; lastR = r;
            repaint (248, 0, 52, ui::rowH);
        }

        // "Input FX" se ilumina si la pista tiene efectos de entrada
        const bool hasIn = ! track.inputPlugins.empty();
        if (hasIn != lastHasInFx)
        {
            lastHasInFx = hasIn;
            btnInFx.setColour (TextButton::buttonColourId, hasIn ? Colour (0xff2e9d4f) : getLookAndFeel().findColour (TextButton::buttonColourId));
            btnInFx.setButtonText (hasIn ? "Input FX (" + String ((int) track.inputPlugins.size()) + ")" : String ("Input FX"));
        }

        // el texto de valor de los carriles sigue al cursor
        if (lanes.height() > 0)
        {
            const int64 p = eng.position.load();
            if (p != lastPos)
            {
                lastPos = p;
                repaint (0, ui::rowH, ui::headerW, lanes.height());
            }
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

        lanes.paint (g, ui::rowH, getWidth(), pps, false);   // fondo y cabeceras de los carriles
        drawGrid (g, pps, visible);

        for (auto& c : track.clips)
        {
            const double startSec = (double) c->startSample.load() / eng.sampleRate;
            const double lenSec = (double) c->audio.getNumSamples() / eng.sampleRate;
            Rectangle<int> r (ui::headerW + (int) (startSec * pps), 4, jmax (2, (int) (lenSec * pps)), ui::rowH - 8);

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

            // fades: zona oscurecida + línea de la rampa + tirador para arrastrar
            const int total = c->audio.getNumSamples();
            const int fiW = (int) ((double) jlimit (0, total, c->fadeInSamples.load()) / eng.sampleRate * pps);
            const int foW = (int) ((double) jlimit (0, total, c->fadeOutSamples.load()) / eng.sampleRate * pps);
            g.setColour (Colours::black.withAlpha (0.45f));
            if (fiW > 0)
            {
                Path p;
                p.addTriangle ((float) r.getX(), (float) r.getY(), (float) (r.getX() + fiW), (float) r.getY(), (float) r.getX(), (float) r.getBottom());
                g.fillPath (p);
            }
            if (foW > 0)
            {
                Path p;
                p.addTriangle ((float) r.getRight(), (float) r.getY(), (float) (r.getRight() - foW), (float) r.getY(), (float) r.getRight(), (float) r.getBottom());
                g.fillPath (p);
            }
            g.setColour (Colours::orange);
            if (fiW > 0) g.drawLine ((float) r.getX(), (float) r.getBottom(), (float) (r.getX() + fiW), (float) r.getY(), 1.5f);
            if (foW > 0) g.drawLine ((float) (r.getRight() - foW), (float) r.getY(), (float) r.getRight(), (float) r.getBottom(), 1.5f);
            if (trimClip == c.get())     // recorte en curso: oscurece lo que se va a quitar y marca el borde
            {
                const int tsX = ui::headerW + (int) ((double) trimStart / eng.sampleRate * pps);
                const int teX = ui::headerW + (int) ((double) trimEnd / eng.sampleRate * pps);
                g.setColour (Colours::black.withAlpha (0.65f));
                if (tsX > r.getX())     g.fillRect (r.getX(), r.getY(), tsX - r.getX(), r.getHeight());
                if (teX < r.getRight()) g.fillRect (teX, r.getY(), r.getRight() - teX, r.getHeight());
                g.setColour (Colours::yellow);
                g.drawVerticalLine (trimIsLeft ? tsX : teX, (float) r.getY(), (float) r.getBottom());
            }

            g.setColour (Colours::white);
            g.fillRect (r.getX() + fiW - 4, r.getY(), 8, 8);                    // tirador fade in
            g.fillRect (r.getRight() - foW - 4, r.getY(), 8, 8);                // tirador fade out

            g.setColour (Colours::white);
            g.setFont (12.0f);
            g.drawText (c->name, r.getX() + 14, r.getY() + 1, jmax (10, r.getWidth() - 28), 14, Justification::left, true);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.drawRect (r);
        }

        lanes.paint (g, ui::rowH, getWidth(), pps, true);    // curvas y puntos encima de todo

        g.setColour (Colours::black);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void mouseMove (const MouseEvent& e) override
    {
        MouseCursor::StandardCursorType cur = MouseCursor::NormalCursor;
        if (e.x >= ui::headerW && e.y < ui::rowH)
        {
            if (owner.getTool() == ui::toolHand)     cur = MouseCursor::DraggingHandCursor;
            else if (owner.getTool() == ui::toolCut) cur = MouseCursor::CrosshairCursor;
        }
        if (e.x >= ui::headerW && e.y < ui::rowH && owner.getTool() != ui::toolCut)
        {
            bool left = false;
            if (auto* c = clipAt (e.x))
                if (e.y > 14 && edgeHit (*c, e.x, left))
                    cur = MouseCursor::LeftRightResizeCursor;     // borde del clip: arrastra para recortar
        }
        setMouseCursor (cur);
    }

    void mouseDown (const MouseEvent& e) override
    {
        dragClip = nullptr;
        trimClip = nullptr;
        rightDown = false;
        if (e.y >= ui::rowH) { lanes.mouseDown (e, ui::rowH); return; }   // zona de carriles
        if (e.x < ui::headerW) return;

        auto* c = clipAt (e.x);

        // tiradores de fade (esquinas superiores del clip): arrastra para dar duración al fade in / out
        fadeClip = nullptr;
        if (c != nullptr && ! e.mods.isPopupMenu() && e.y <= 14)
        {
            const double pps = owner.pps();
            const int x0 = ui::headerW + (int) ((double) c->startSample.load() / eng.sampleRate * pps);
            const int x1 = x0 + (int) ((double) c->audio.getNumSamples() / eng.sampleRate * pps);
            const int fiX = x0 + (int) ((double) c->fadeInSamples.load() / eng.sampleRate * pps);
            const int foX = x1 - (int) ((double) c->fadeOutSamples.load() / eng.sampleRate * pps);

            const bool nearOut = std::abs (e.x - foX) <= 8;
            const bool nearIn  = std::abs (e.x - fiX) <= 8;
            if (nearOut || nearIn)
            {
                // si ambos tiradores coinciden (sin fades), manda el más cercano al borde pulsado
                fadeIsOut = nearOut && (! nearIn || std::abs (e.x - x1) < std::abs (e.x - x0));
                eng.undoBegin();                       // Ctrl+Z deshace el fade
                fadeClip = c;
                owner.setSelected (&track, c);
                return;
            }
        }

        if (e.mods.isPopupMenu())
        {
            // Clic derecho: si solo haces clic se abre el menú (al soltar); si arrastras = selección de tiempo.
            // Con Mayús (antes de arrastrar o al soltar) se abre el menú de la selección: exportar a otro DAW.
            if (c != nullptr) owner.setSelected (&track, c);
            rightDown = true;
            rightMoved = false;
            rightShift = e.mods.isShiftDown();
            rightClip = c;
            rightStart = owner.snapSample (sampleAt (e.x));
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

        // bordes del clip (por debajo de los tiradores de fade): arrastrar = recortar
        bool edgeLeft = false;
        if (c != nullptr && e.y > 14 && edgeHit (*c, e.x, edgeLeft))
        {
            eng.undoBegin();
            trimClip = c;
            trimIsLeft = edgeLeft;
            trimStart = c->startSample.load();
            trimEnd = trimStart + c->audio.getNumSamples();
            return;
        }

        if (c != nullptr && tool == ui::toolHand)
        {
            eng.undoBegin();
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
        if (rightDown)
        {
            if (e.getDistanceFromDragStart() > 4) rightMoved = true;
            if (rightMoved)
            {
                const int64 here = owner.snapSample (sampleAt (e.x));
                eng.selStart = jmin (rightStart, here);
                eng.selEnd = jmax (rightStart, here);
                owner.timeSelectionChanged();
            }
            return;
        }

        if (lanes.mouseDrag (e, ui::rowH)) return;

        if (fadeClip != nullptr)
        {
            if (! ownsClip (fadeClip)) { fadeClip = nullptr; return; }
            const int total = fadeClip->audio.getNumSamples();
            const int64 s = fadeClip->startSample.load();
            const int64 here = sampleAt (e.x);
            if (fadeIsOut) fadeClip->fadeOutSamples = (int) jlimit<int64> (0, total, s + total - here);
            else           fadeClip->fadeInSamples  = (int) jlimit<int64> (0, total, here - s);
            repaint();
            return;
        }

        if (trimClip != nullptr)
        {
            if (! ownsClip (trimClip)) { trimClip = nullptr; return; }
            const int64 s = trimClip->startSample.load();
            const int64 here = owner.snapSample (sampleAt (e.x));
            if (trimIsLeft) trimStart = jmin (here, s + trimClip->audio.getNumSamples() - 256);
            else            trimEnd = jmax (here, s + 256);
            repaint();
            return;
        }

        if (dragClip == nullptr) return;
        const auto delta = (int64) (e.getDistanceFromDragStartX() / owner.pps() * eng.sampleRate);
        dragClip->startSample = jmax<int64> (0, owner.snapSample (dragOrigStart + delta));
        repaint();
    }

    void mouseUp (const MouseEvent& e) override
    {
        if (rightDown)
        {
            rightDown = false;
            const bool shift = rightShift || e.mods.isShiftDown();
            auto* c = ownsClip (rightClip) ? rightClip : nullptr;
            rightClip = nullptr;

            if (rightMoved)
            {
                if (shift) owner.showRangeMenu (track);               // Mayús + arrastrar con clic derecho
            }
            else if (shift && eng.selEnd.load() > eng.selStart.load())
                owner.showRangeMenu (track);                          // Mayús + clic derecho con selección hecha
            else
                showContextMenu (c);                                  // clic derecho normal
            return;
        }

        lanes.mouseUp();

        if (fadeClip != nullptr)
        {
            fadeClip = nullptr;
            eng.undoEnd();               // si el fade no cambió, no queda paso de deshacer
        }

        if (trimClip != nullptr)
        {
            auto* tc = trimClip;
            trimClip = nullptr;
            if (ownsClip (tc))
            {
                auto* nc = eng.trimClip (track, tc, trimStart, trimEnd);
                owner.setSelected (&track, nc);
            }
            eng.undoEnd();
            owner.clipsChanged();
            return;
        }
        if (dragClip != nullptr)
        {
            auto* moved = dragClip;
            dragClip = nullptr;

            // soltado sobre otra pista: el clip pasa a esa pista (conserva su posición en el tiempo)
            if (auto* dst = owner.trackAtScreenY (e.getScreenY()))
                if (dst != &track && ownsClip (moved))
                {
                    eng.moveClip (track, *dst, moved);
                    owner.setSelected (dst, moved);
                }
            eng.undoEnd();               // un clic sin mover no deja paso de deshacer
            owner.clipsChanged();
        }
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

    // ¿el clic cae en el borde izquierdo o derecho del clip? (zona de 5 px, solo si el clip no es diminuto)
    bool edgeHit (const Clip& c, int x, bool& left) const
    {
        const double pps = owner.pps();
        const int x0 = ui::headerW + (int) ((double) c.startSample.load() / eng.sampleRate * pps);
        const int x1 = x0 + (int) ((double) c.audio.getNumSamples() / eng.sampleRate * pps);
        if (x1 - x0 < 14) return false;
        if (x >= x0 && x - x0 <= 5) { left = true;  return true; }
        if (x <= x1 && x1 - x <= 5) { left = false; return true; }
        return false;
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
        drawTimeGrid (g, owner, pps, visible, getHeight());
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

            PopupMenu fo, fi;
            for (int i = 0; i < 6; ++i)
            {
                const String label = i == 0 ? T ("Sin fade") : String (fadeSecs[i], 2) + " s";
                fo.addItem (30 + i, label);
                fi.addItem (40 + i, label);
            }
            m.addSubMenu (T ("Fade out (al final del audio)"), fo);
            m.addSubMenu (T ("Fade in (al inicio del audio)"), fi);
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

            if ((r >= 30 && r < 36) || (r >= 40 && r < 46))
            {
                const bool out = r < 40;
                self->eng.undoBegin();
                const int n = jmin (c->audio.getNumSamples(), (int) (fadeSecs[r - (out ? 30 : 40)] * self->eng.sampleRate));
                (out ? c->fadeOutSamples : c->fadeInSamples) = n;
                self->repaint();
                return;
            }
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
    TextButton btnMute, btnSolo, btnArm, btnFx, btnDel, btnSend, btnAuto, btnInFx, btnMon;
    bool lastHasInFx = false;
    Slider vol, pan;
    ComboBox inputBox;
    EnvLanes lanes { eng, owner, *this, &track };
    int64 lastPos = -1;

    static constexpr double fadeSecs[6] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0 };

    Clip* dragClip = nullptr;
    bool rightDown = false, rightMoved = false, rightShift = false;   // clic derecho en curso (selección de tiempo)
    Clip* rightClip = nullptr;
    int64 rightStart = 0;
    Clip* trimClip = nullptr;       // clip cuyo borde se está recortando
    bool trimIsLeft = true;
    int64 trimStart = 0, trimEnd = 0;
    Clip* fadeClip = nullptr;       // clip cuyo fade se está arrastrando
    bool fadeIsOut = true;
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

// Selección de tiempo (clic derecho + arrastrar): banda azul sobre todas las pistas y su duración en la regla
class SelectionOverlay : public Component
{
public:
    SelectionOverlay (Engine& e, std::function<double()> p) : eng (e), pps (std::move (p))
    {
        setInterceptsMouseClicks (false, false);
    }

    void paint (Graphics& g) override
    {
        const int64 s = eng.selStart.load(), en = eng.selEnd.load();
        if (en <= s) return;

        const double p = pps();
        const int x0 = ui::headerW + (int) ((double) s / eng.sampleRate * p);
        const int x1 = ui::headerW + (int) ((double) en / eng.sampleRate * p);
        const Colour col (0xff3aa0ff);

        g.setColour (col.withAlpha (0.16f));
        g.fillRect (x0, 0, x1 - x0, getHeight());
        g.setColour (col.withAlpha (0.9f));
        g.fillRect (x0, ui::rulerH - 6, x1 - x0, 6);
        g.drawVerticalLine (x0, 0.0f, (float) getHeight());
        g.drawVerticalLine (x1, 0.0f, (float) getHeight());

        const double secs = (double) (en - s) / eng.sampleRate;
        g.setColour (Colours::white);
        g.setFont (11.0f);
        g.drawText (String (secs, 2) + " s", x0 + 4, 2, 90, 14, Justification::left);
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
// ---------------------------------------------------------------------------
// Panel de envolventes (como en Reaper): lista todo lo automatizable de una pista (o del Master):
// volumen, pan, mute y, de cada efecto, su bypass y todos sus parámetros.
// "Activa" enciende/apaga la automatización; "Visible" muestra su carril bajo la pista.
// ---------------------------------------------------------------------------
class EnvelopePanel : public Component, private ListBoxModel
{
public:
    EnvelopePanel (Engine& e, RowOwner& o, Track* t) : eng (e), owner (o), target (t)
    {
        buildAll();
        applyFilter();

        search.setTextToShowWhenEmpty (T ("Buscar parámetro o efecto..."), Colours::grey);
        search.onTextChange = [this] { applyFilter(); };
        addAndMakeVisible (search);

        list.setModel (this);
        list.setRowHeight (24);
        addAndMakeVisible (list);

        info.setText (T ("Activa = la automatización se aplica · Visible = muestra su carril bajo la pista"),
                      dontSendNotification);
        info.setColour (Label::textColourId, Colours::grey);
        info.setFont (11.0f);
        addAndMakeVisible (info);

        setSize (520, 480);
    }

    ~EnvelopePanel() override { list.setModel (nullptr); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        search.setBounds (r.removeFromTop (26));
        r.removeFromTop (4);
        info.setBounds (r.removeFromBottom (18));
        list.setBounds (r);
    }

    void paint (Graphics& g) override { g.fillAll (Colour (0xff202020)); }

private:
    struct Entry
    {
        String label;
        int target = envVolume;
        AudioPluginInstance* plugin = nullptr;
        int paramIndex = -1;
        bool header = false;
    };

    void buildAll()
    {
        all.clear();
        all.push_back ({ T ("Pista"), envVolume, nullptr, -1, true });
        all.push_back ({ "Volumen", envVolume, nullptr, -1, false });
        all.push_back ({ "Pan",     envPan,    nullptr, -1, false });
        all.push_back ({ "Mute",    envMute,   nullptr, -1, false });

        for (auto& p : eng.pluginsOf (target))
        {
            const String fx = p->getName();
            all.push_back ({ fx, envBypass, p.get(), -1, true });
            all.push_back ({ fx + " > Bypass (apagar/encender)", envBypass, p.get(), -1, false });

            auto& ps = p->getParameters();
            for (int i = 0; i < ps.size(); ++i)
            {
                const String n = ps[i]->getName (40);
                if (n.isEmpty()) continue;
                all.push_back ({ fx + " > " + n, envParam, p.get(), i, false });
            }
        }
    }

    void applyFilter()
    {
        shown.clear();
        const String f = search.getText().trim();
        for (auto& e : all)
            if (f.isEmpty() || (! e.header && e.label.containsIgnoreCase (f)))
                shown.push_back (e);
        list.updateContent();
        list.repaint();
    }

    Envelope* envOf (const Entry& e)
    {
        return eng.findEnvelope (eng.envsOf (target), e.target, e.plugin, e.target == envParam ? e.paramIndex : -1);
    }

    int getNumRows() override { return (int) shown.size(); }

    void paintListBoxItem (int row, Graphics& g, int w, int h, bool) override
    {
        if (! isPositiveAndBelow (row, (int) shown.size())) return;
        const auto& e = shown[(size_t) row];

        if (e.header)
        {
            g.setColour (Colour (0xff343a42));
            g.fillRect (0, 0, w, h);
            g.setColour (Colours::white);
            g.setFont (Font (FontOptions (13.0f, Font::bold)));
            g.drawText (e.label, 8, 0, w - 16, h, Justification::centredLeft, true);
            return;
        }

        g.setColour (row % 2 ? Colour (0xff252525) : Colour (0xff2b2b2b));
        g.fillRect (0, 0, w, h);

        auto* env = envOf (e);
        const bool act = env != nullptr && env->active.load() && ! env->points.empty();
        const bool vis = env != nullptr && env->visible;

        g.setColour (Colours::white);
        g.setFont (12.0f);
        g.drawText (e.label, 8, 0, w - 180, h, Justification::centredLeft, true);

        auto box = [&] (int x, const String& text, bool on, Colour c)
        {
            g.setColour (on ? c : Colour (0xff444a52));
            g.fillRoundedRectangle ((float) x, 3.0f, 70.0f, (float) h - 6.0f, 3.0f);
            g.setColour (on ? Colours::white : Colours::lightgrey);
            g.setFont (11.0f);
            g.drawText (text, x, 3, 70, h - 6, Justification::centred);
        };
        box (w - 156, env != nullptr && env->active.load() ? "Activa" : "Inactiva",
             env != nullptr && env->active.load(), Colour (0xff2e9d4f));
        box (w - 80, vis ? "Visible" : "Oculta", vis, Colour (0xffb8891a));
        juce::ignoreUnused (act);
    }

    void listBoxItemClicked (int row, const MouseEvent& m) override
    {
        if (! isPositiveAndBelow (row, (int) shown.size())) return;
        const auto e = shown[(size_t) row];
        if (e.header) return;

        const int w = list.getVisibleRowWidth();
        const bool hitActive  = m.x >= w - 156 && m.x < w - 84;
        const bool hitVisible = m.x >= w - 80;
        if (! hitActive && ! hitVisible) return;

        auto* env = eng.getOrCreateEnvelope (eng.envsOf (target), e.target, e.plugin,
                                             e.target == envParam ? e.paramIndex : -1);
        if (env == nullptr) return;

        if (hitActive)  env->active = ! env->active.load();
        if (hitVisible) env->visible = ! env->visible;

        list.repaintRow (row);
        owner.clipsChanged();       // reorganiza las filas (aparece o desaparece el carril)
    }

    Engine& eng;
    RowOwner& owner;
    Track* target;

    std::vector<Entry> all, shown;
    TextEditor search;
    ListBox list;
    Label info;
};

class EnvelopeWindow : public DocumentWindow
{
public:
    EnvelopeWindow (const String& title, Component* content, std::function<void()> onCloseFn)
        : DocumentWindow (title, Colour (0xff202020), DocumentWindow::closeButton), onClose (std::move (onCloseFn))
    {
        setUsingNativeTitleBar (true);
        setContentOwned (content, true);
        setResizable (true, true);
        centreWithSize (getWidth(), getHeight());
        setAlwaysOnTop (true);
        setVisible (true);
    }

    void closeButtonPressed() override { if (onClose) onClose(); }

private:
    std::function<void()> onClose;
};

// ---------------------------------------------------------------------------
// Zona vacía al final de las pistas: clic para elegir si se añade una pista normal o un bus
// ---------------------------------------------------------------------------
class AddTrackArea : public Component
{
public:
    std::function<void (bool isBus)> onAdd;

    AddTrackArea() { setMouseCursor (MouseCursor::PointingHandCursor); }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff181818));
        g.setColour (Colour (0xff24282d));
        g.fillRect (0, 0, ui::headerW, getHeight());

        const float cx = 34.0f, cy = (float) getHeight() * 0.5f;
        g.setColour (hover ? Colour (0xff3d7cb8) : Colour (0xff4a4f57));
        g.fillEllipse (cx - 16.0f, cy - 16.0f, 32.0f, 32.0f);
        g.setColour (Colours::white);
        g.fillRect (cx - 8.0f, cy - 1.5f, 16.0f, 3.0f);
        g.fillRect (cx - 1.5f, cy - 8.0f, 3.0f, 16.0f);

        g.setFont (Font (FontOptions (13.0f, Font::bold)));
        g.drawText (T ("Añadir pista o bus"), 62, (int) cy - 18, ui::headerW - 70, 18, Justification::left);
        g.setColour (Colours::grey);
        g.setFont (11.0f);
        g.drawText (T ("Clic para elegir: pista normal o bus"), 62, (int) cy + 2, ui::headerW - 70, 16, Justification::left);

        g.setColour (Colour (0xff2b2f35));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void mouseEnter (const MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const MouseEvent&) override  { hover = false; repaint(); }

    void mouseDown (const MouseEvent&) override
    {
        PopupMenu m;
        m.addItem (1, T ("Pista normal (audio)"));
        m.addItem (2, T ("Bus (recibe envíos y pistas)"));

        SafePointer<AddTrackArea> sp (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [sp] (int r)
        {
            if (sp.getComponent() != nullptr && r != 0 && sp->onAdd)
                sp->onAdd (r == 2);
        });
    }

private:
    bool hover = false;
};

// ---------------------------------------------------------------------------
// Fila del canal Master al final del timeline: botones FX / Auto y sus carriles de automatización
// ---------------------------------------------------------------------------
class MasterRow : public Component
{
public:
    MasterRow (Engine& e, RowOwner& o) : eng (e), owner (o)
    {
        btnFx.setButtonText ("FX");
        btnFx.onClick = [this] { owner.showMasterMenu(); };
        btnFx.setWantsKeyboardFocus (false);
        addAndMakeVisible (btnFx);

        btnAuto.setButtonText ("Auto");
        btnAuto.setTooltip (T ("Automatización del Master: volumen, mute y parámetros de sus efectos"));
        btnAuto.onClick = [this] { owner.showEnvelopePanel (nullptr); };
        btnAuto.setWantsKeyboardFocus (false);
        addAndMakeVisible (btnAuto);
    }

    int desiredHeight() const { return ui::masterH + lanes.height(); }

    void resized() override
    {
        btnFx.setBounds (100, 8, 50, 24);
        btnAuto.setBounds (154, 8, 56, 24);
    }

    void updateCursor()
    {
        if (lanes.height() > 0)
        {
            const int64 p = eng.position.load();
            if (p != lastPos)
            {
                lastPos = p;
                repaint (0, ui::masterH, ui::headerW, lanes.height());
            }
        }
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff1a1a1a));
        g.setColour (Colour (0xff3a2c2c));
        g.fillRect (0, 0, ui::headerW, getHeight());

        g.setColour (Colours::white);
        g.setFont (Font (FontOptions (15.0f, Font::bold)));
        g.drawText ("MASTER", 10, 8, 86, 24, Justification::left);
        g.setColour (Colours::grey);
        g.setFont (11.0f);
        g.drawText (T ("Mezcla final: plugins y automatización"), 10, 38, ui::headerW - 20, 16, Justification::left);

        g.setColour (Colours::black);
        g.drawVerticalLine (ui::headerW - 1, 0.0f, (float) getHeight());

        const double pps = owner.pps();
        lanes.paint (g, ui::masterH, getWidth(), pps, false);
        drawTimeGrid (g, owner, pps, g.getClipBounds(), getHeight());
        lanes.paint (g, ui::masterH, getWidth(), pps, true);

        g.setColour (Colours::black);
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (e.y >= ui::masterH) lanes.mouseDown (e, ui::masterH);
        else if (e.x >= ui::headerW) owner.setCursorFromX (e.x);
    }

    void mouseDrag (const MouseEvent& e) override { lanes.mouseDrag (e, ui::masterH); }
    void mouseUp (const MouseEvent&) override     { lanes.mouseUp(); }

private:
    Engine& eng;
    RowOwner& owner;
    TextButton btnFx, btnAuto;
    EnvLanes lanes { eng, owner, *this, nullptr };
    int64 lastPos = -1;
};

// ---------------------------------------------------------------------------
// Barra inferior: metrónomo (botón + volumen del clic) y medidor RMS del Master (L / R)
// ---------------------------------------------------------------------------
class BottomBar : public Component
{
public:
    explicit BottomBar (Engine& e) : eng (e)
    {
        btnMetro.setButtonText (T ("Metrónomo"));
        btnMetro.setClickingTogglesState (true);
        btnMetro.setColour (TextButton::buttonOnColourId, Colour (0xffd9a400));
        btnMetro.setTooltip (T ("Clic al tempo (BPM) mientras se reproduce o graba. Tecla: M"));
        btnMetro.setWantsKeyboardFocus (false);
        btnMetro.onClick = [this] { eng.metronome = btnMetro.getToggleState(); };
        addAndMakeVisible (btnMetro);

        lblVol.setText (T ("Volumen clic"), dontSendNotification);
        lblVol.setColour (Label::textColourId, Colours::grey);
        lblVol.setFont (11.0f);
        addAndMakeVisible (lblVol);

        metroVol.setSliderStyle (Slider::LinearHorizontal);
        metroVol.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        metroVol.setRange (0.0, 1.0, 0.01);
        metroVol.setValue (eng.metroVol.load(), dontSendNotification);
        metroVol.setScrollWheelEnabled (false);
        metroVol.setWantsKeyboardFocus (false);
        metroVol.onValueChange = [this] { eng.metroVol = (float) metroVol.getValue(); };
        addAndMakeVisible (metroVol);
    }

    void toggleMetronome()
    {
        eng.metronome = ! eng.metronome.load();
        btnMetro.setToggleState (eng.metronome.load(), dontSendNotification);
    }

    void syncState() { btnMetro.setToggleState (eng.metronome.load(), dontSendNotification); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        btnMetro.setBounds (r.removeFromLeft (100));
        r.removeFromLeft (10);
        lblVol.setBounds (r.removeFromLeft (76));
        metroVol.setBounds (r.removeFromLeft (110));
        r.removeFromLeft (24);
        meterArea = r;
    }

    // la barra se repinta solo si el medidor cambió de verdad
    void updateMeters()
    {
        const float a = eng.masterRmsL.load(), b = eng.masterRmsR.load();
        const float pa = eng.masterPeakL.load(), pb = eng.masterPeakR.load();
        if (std::abs (a - lastA) > 0.0005f || std::abs (b - lastB) > 0.0005f
            || std::abs (pa - lastPA) > 0.002f || std::abs (pb - lastPB) > 0.002f)
        {
            lastA = a; lastB = b; lastPA = pa; lastPB = pb;
            repaint (meterArea.expanded (0, 6));
        }
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff1b1b1b));
        g.setColour (Colours::black);
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());

        if (meterArea.getWidth() < 200) return;

        auto area = meterArea;
        g.setColour (Colours::lightgrey);
        g.setFont (Font (FontOptions (12.0f, Font::bold)));
        g.drawText ("RMS", area.removeFromLeft (36), Justification::centredLeft);

        const int rowH2 = area.getHeight() / 2;
        drawRow (g, area.removeFromTop (rowH2).reduced (0, 1), "L", lastA, lastPA);
        drawRow (g, area.reduced (0, 1), "R", lastB, lastPB);
    }

private:
    static String dbText (float v, const String& suffix)
    {
        const float db = Decibels::gainToDecibels (v, -90.0f);
        return (db <= -60.0f ? String ("-inf") : String (db, 1)) + suffix;
    }

    // Escala de -60 a 0 dBFS. Barra = RMS; marca blanca = pico.
    void drawRow (Graphics& g, Rectangle<int> row, const String& label, float rms, float peak) const
    {
        g.setColour (Colours::lightgrey);
        g.setFont (11.0f);
        g.drawText (label, row.removeFromLeft (14), Justification::centred);

        auto txt = row.removeFromRight (190);
        auto bar = row.reduced (0, 1);

        g.setColour (Colours::black);
        g.fillRect (bar);

        auto pos = [&] (float v)
        {
            const float db = Decibels::gainToDecibels (v, -60.0f);
            return bar.getX() + (int) (jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * (float) bar.getWidth());
        };

        // zonas de color según el nivel
        const int x = pos (rms);
        const int xYellow = pos (Decibels::decibelsToGain (-18.0f));
        const int xRed = pos (Decibels::decibelsToGain (-6.0f));
        auto fill = [&] (int from, int to, Colour c)
        {
            if (to > from) { g.setColour (c); g.fillRect (from, bar.getY(), to - from, bar.getHeight()); }
        };
        fill (bar.getX(), jmin (x, xYellow), Colours::limegreen);
        fill (xYellow, jmin (x, xRed), Colours::yellow);
        fill (xRed, x, Colours::red);

        // marcas de escala
        g.setColour (Colours::white.withAlpha (0.18f));
        for (int db : { -48, -36, -24, -12, -6 })
            g.drawVerticalLine (pos (Decibels::decibelsToGain ((float) db)), (float) bar.getY(), (float) bar.getBottom());

        g.setColour (Colours::white);
        g.drawVerticalLine (jmin (bar.getRight() - 1, pos (peak)), (float) bar.getY(), (float) bar.getBottom());

        g.setColour (Colours::lightgrey);
        g.setFont (11.0f);
        g.drawText (dbText (rms, " dB RMS") + "   " + dbText (peak, " pico"), txt.withTrimmedLeft (6),
                    Justification::centredLeft, true);
    }

    Engine& eng;
    TextButton btnMetro;
    Label lblVol;
    Slider metroVol;
    Rectangle<int> meterArea;
    float lastA = 0.0f, lastB = 0.0f, lastPA = 0.0f, lastPB = 0.0f;
};

// ---------------------------------------------------------------------------
// Indicador visual mientras se arrastran archivos de audio sobre la ventana: marco azul, una pista "fantasma"
// por archivo al final de las pistas (con el ancho real del audio) y una línea en la posición de inicio.
// ---------------------------------------------------------------------------
class DropOverlay : public Component
{
public:
    struct Item { String name; double seconds = 0.0; };

    DropOverlay() { setInterceptsMouseClicks (false, false); setVisible (false); }

    void setInfo (std::vector<Item> i, int ghostTopY, int startXpos, double pixelsPerSec, int headerXpos)
    {
        items = std::move (i);
        ghostTop = ghostTopY;
        startX = startXpos;
        pps = pixelsPerSec;
        headerX = headerXpos;
        repaint();
    }

    void paint (Graphics& g) override
    {
        const Colour blue (0xff3aa0ff);
        g.setColour (blue.withAlpha (0.07f));
        g.fillAll();
        g.setColour (blue.withAlpha (0.9f));
        g.drawRect (getLocalBounds(), 3);

        const int n = jmin ((int) items.size(), 8);
        if (n == 0) return;

        // las pistas fantasma se pegan al final de las pistas; si quedan fuera de la vista, al borde inferior
        const int top = jlimit (0, jmax (0, getHeight() - n * ui::rowH), ghostTop);

        g.setColour (blue.withAlpha (0.8f));
        g.drawVerticalLine (jlimit (0, getWidth() - 1, startX), 0.0f, (float) getHeight());

        for (int k = 0; k < n; ++k)
        {
            const int y = top + k * ui::rowH;
            const auto& it = items[(size_t) k];

            g.setColour (blue.withAlpha (0.22f));
            g.fillRect (headerX, y, ui::headerW, ui::rowH);
            g.fillRect (jmax (0, headerX + ui::headerW), y, getWidth(), ui::rowH);

            g.setColour (Colours::white);
            g.setFont (Font (FontOptions (13.0f, Font::bold)));
            g.drawText (T ("+ Pista nueva"), headerX + 10, y + 10, ui::headerW - 20, 18, Justification::left);
            g.setFont (12.0f);
            g.drawText (it.name, headerX + 10, y + 30, ui::headerW - 20, 16, Justification::left, true);

            const int w = jmax (30, (int) (it.seconds * pps));
            Rectangle<int> r (startX, y + 4, w, ui::rowH - 8);
            g.setColour (blue.withAlpha (0.35f));
            g.fillRect (r);
            g.setColour (blue);
            g.drawRect (r, 2);
            g.setColour (Colours::white);
            g.setFont (12.0f);
            g.drawText (it.name, r.reduced (6, 2).withHeight (16), Justification::left, true);
        }

        g.setColour (Colours::white);
        g.setFont (Font (FontOptions (15.0f, Font::bold)));
        const String msg = items.size() == 1 ? T ("Suelta para crear una pista nueva")
                                             : T ("Suelta para crear ") + String ((int) items.size()) + T (" pistas nuevas");
        g.drawText (msg, getLocalBounds().removeFromTop (30), Justification::centred);
    }

private:
    std::vector<Item> items;
    int ghostTop = 0, startX = 0, headerX = 0;
    double pps = 100.0;
};

class MainComponent : public Component, public RowOwner, public FileDragAndDropTarget, private Timer
{
public:
    MainComponent()
    {
        setSize (1280, 720);

        auto err = engine.start();
        if (err.isNotEmpty())
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, T ("Audio"),
                T ("No se pudo abrir el dispositivo de audio por defecto.\nAbre 'Audio' y elige otro driver o tarjeta.\n\n") + err);

        mk (bHome, "|<", [this] { engine.setCursor (0); });
        // Play mientras suena = detener y volver a la posición de inicio (como Stop)
        mk (bPlay, "Play", [this] { if (engine.playing) doStop(); else engine.play(); });
        mk (bStop, "Stop", [this] { doStop(); });
        mk (bRec,  "Rec",  [this] { doRecord(); });
        mk (bAdd,  T ("+ Pista"), [this] { addNewTrack (false); });
        mk (bAddBus, "+ Bus", [this] { addNewTrack (true); });
        mk (bImport, "Importar", [this] { importAudio(); });
        mk (bZoomOut, "-", [this] { setZoom (pxPerSec / 1.25); });
        mk (bZoomIn, "+", [this] { setZoom (pxPerSec * 1.25); });
        mk (bAudio, "Audio", [this] { showAudioSettings(); });
        mk (bScan, "Escanear plugins", [this] { scanPlugins(); });
        mk (bReport, "Ver plugins", [this] { showReport(); });
        mk (bOpen, "Abrir", [this] { openProject(); });
        mk (bSave, "Guardar", [this] { saveProjectQuick(); });
        mk (bMasterFx, T ("FX Master"), [this] { showMasterFxMenu(); });

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
        addAndMakeVisible (bottomBar);
        addChildComponent (dropOverlay);       // encima del timeline; solo se ve mientras se arrastran archivos
        addArea.onAdd = [this] (bool isBus) { addNewTrack (isBus); };
        content.addAndMakeVisible (addArea);
        content.addAndMakeVisible (masterRow);
        content.addAndMakeVisible (selOverlay);
        content.addAndMakeVisible (loopOverlay);
        content.addAndMakeVisible (playhead);

        setWantsKeyboardFocus (true);
        layoutContent();
        startTimerHz (30);
    }

    ~MainComponent() override
    {
        stopTimer();
        envWindow.reset();
        windows.clear();      // cerrar editores antes de destruir los plugins
        rows.clear();
    }

    // ------------------------- RowOwner ------------------------------------
    double pps() const override { return pxPerSec; }
    void setCursorFromX (int x) override
    {
        const auto s = (int64) (jmax (0.0, (x - ui::headerW) / pxPerSec) * engine.sampleRate);
        engine.setCursor (snapSample (s));   // detenido: también pasa a ser la posición de inicio
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

    void showMasterMenu() override { showMasterFxMenu(); }

    Track* trackAtScreenY (int sy) override
    {
        for (auto& r : rows)
        {
            const auto b = r->getScreenBounds();
            if (sy >= b.getY() && sy < b.getBottom())
                return r->getTrack().isBus.load() ? nullptr : &r->getTrack();
        }
        return nullptr;
    }

    // Panel de envolventes (Reaper): todo lo automatizable de la pista (nullptr = Master)
    void showEnvelopePanel (Track* t) override
    {
        envWindow.reset();
        const String title = t != nullptr ? T ("Envolventes - ") + t->name : T ("Envolventes - Master");

        SafePointer<MainComponent> sp (this);
        envWindow = std::make_unique<EnvelopeWindow> (title, new EnvelopePanel (engine, *this, t), [sp]
        {
            MessageManager::callAsync ([sp]
            {
                if (auto* self = sp.getComponent()) self->envWindow.reset();
            });
        });
    }

    void addNewTrack (bool isBus)
    {
        if (isBus)
        {
            int n = 1;
            for (auto& t : engine.tracks) if (t->isBus.load()) ++n;
            addTrackRow (engine.addTrack ("Bus " + String (n), true));
        }
        else
        {
            addTrackRow (engine.addTrack ("Pista " + String (rows.size() + 1)));
        }
        layoutContent();
    }

    void removeTrack (Track& t) override
    {
        envWindow.reset();
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
        m.addSeparator();
        m.addItem (300, T ("Automatización de esta pista y sus efectos..."));

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
                self->envWindow.reset();      // la lista de parámetros cambia
                self->engine.addPlugin (*tp, std::move (inst));
                self->openEditor (*tp->plugins.back());
            }
            else if (r == 300)
            {
                self->showEnvelopePanel (tp);
            }
            else if (r >= 200 && r < 300)
            {
                const int idx = r - 200;
                if (idx < (int) tp->plugins.size())
                {
                    self->envWindow.reset();
                    self->closeWindowFor (tp->plugins[(size_t) idx].get());
                    self->engine.removePlugin (*tp, idx);   // también borra sus envolventes
                    self->clipsChanged();                   // quita los carriles que ya no existen
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

    void timeSelectionChanged() override { selOverlay.repaint(); }

    // Menú de la selección de tiempo: sacar el audio a otro DAW y traerlo de vuelta al proyecto
    void showRangeMenu (Track& t) override
    {
        const int64 a = engine.selStart.load(), b = engine.selEnd.load();
        if (b <= a) return;

        PopupMenu m;
        m.addSectionHeader (T ("Selección: ") + String ((double) (b - a) / engine.sampleRate, 2) + " s");
        m.addItem (1, T ("Exportar esta pista (selección) a WAV para otro DAW"), ! t.isBus.load());
        m.addItem (2, T ("Exportar todas las pistas (selección) en un solo WAV"));
        m.addSeparator();
        m.addItem (3, T ("Traer audio trabajado en otro DAW (a esta pista, al inicio de la selección)..."), ! t.isBus.load());
        m.addItem (4, T ("Abrir carpeta de exportados"));
        m.addSeparator();
        m.addItem (5, T ("Quitar selección"));

        SafePointer<MainComponent> sp (this);
        Track* tp = &t;
        m.showMenuAsync (PopupMenu::Options(), [sp, tp, a, b] (int r)
        {
            auto* self = sp.getComponent();
            if (self == nullptr || r == 0 || ! self->engine.hasTrack (tp)) return;

            if (r == 1 || r == 2)
            {
                String err;
                const auto f = self->engine.exportRange (r == 1 ? tp : nullptr, a, b, r == 1 ? tp->name : String ("Mezcla"), err);
                if (f == File())
                {
                    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, T ("Exportar"), err);
                    return;
                }
                self->statusLabel.setText (T ("Exportado: ") + f.getFullPathName(), dontSendNotification);
                self->statusTicks = 400;
                f.revealToUser();                 // abre la carpeta con el archivo ya marcado
            }
            else if (r == 3)
            {
                self->chooser = std::make_unique<FileChooser> (T ("Audio trabajado en otro DAW"), self->engine.exportDir(),
                                                               self->engine.formats.getWildcardForAllFormats());
                self->chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                            [sp, tp, a] (const FileChooser& fc)
                {
                    auto* s2 = sp.getComponent();
                    const auto f = fc.getResult();
                    if (s2 == nullptr || ! s2->engine.hasTrack (tp) || ! f.existsAsFile()) return;

                    auto clip = s2->engine.loadClip (f);
                    if (clip == nullptr)
                    {
                        AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Audio",
                            T ("No se pudo cargar: ") + f.getFileName());
                        return;
                    }
                    clip->startSample = a;                 // vuelve justo donde estaba la selección
                    s2->engine.addClip (*tp, std::move (clip));
                    s2->layoutContent();
                    s2->repaintRows();
                });
            }
            else if (r == 4)
            {
                self->engine.exportDir().revealToUser();
            }
            else if (r == 5)
            {
                self->engine.selEnd = self->engine.selStart.load();
                self->selOverlay.repaint();
            }
        });
    }

    // Input FX: plugins que procesan la entrada de la tarjeta ANTES de grabar (ej. tu cadena de voz)
    void showInputFxMenu (Track& t) override
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

        m.addSubMenu (T ("Añadir Input FX"), add);
        m.addSeparator();
        for (int i = 0; i < (int) t.inputPlugins.size(); ++i)
            m.addItem (100 + i, T ("Abrir: ") + t.inputPlugins[(size_t) i]->getName());
        for (int i = 0; i < (int) t.inputPlugins.size(); ++i)
            m.addItem (200 + i, T ("Quitar: ") + t.inputPlugins[(size_t) i]->getName());
        m.addSeparator();
        m.addItem (2, T ("(la pista debe estar armada con R; activa Monitor para oírte)"), false);

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
                self->engine.addInputPlugin (*tp, std::move (inst));
                self->openEditor (*tp->inputPlugins.back());
            }
            else if (r >= 200 && r < 300)
            {
                const int idx = r - 200;
                if (idx < (int) tp->inputPlugins.size())
                {
                    self->closeWindowFor (tp->inputPlugins[(size_t) idx].get());
                    self->engine.removeInputPlugin (*tp, idx);
                }
            }
            else if (r >= 100 && r < 200)
            {
                const int idx = r - 100;
                if (idx < (int) tp->inputPlugins.size())
                    self->openEditor (*tp->inputPlugins[(size_t) idx]);
            }
        });
    }

    // Menú del canal Master: añadir, abrir y quitar plugins (limitador, EQ de masterización...)
    void showMasterFxMenu()
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

        m.addSubMenu (T ("Añadir plugin al Master"), add);
        m.addSeparator();
        for (int i = 0; i < (int) engine.masterPlugins.size(); ++i)
            m.addItem (100 + i, T ("Abrir: ") + engine.masterPlugins[(size_t) i]->getName());
        for (int i = 0; i < (int) engine.masterPlugins.size(); ++i)
            m.addItem (200 + i, T ("Quitar: ") + engine.masterPlugins[(size_t) i]->getName());
        m.addSeparator();
        m.addItem (300, T ("Automatización del Master y sus efectos..."));

        SafePointer<MainComponent> sp (this);
        m.showMenuAsync (PopupMenu::Options(), [sp, types] (int r)
        {
            auto* self = sp.getComponent();
            if (self == nullptr || r == 0) return;
            auto& mp = self->engine.masterPlugins;

            if (r >= 1000)
            {
                String err;
                auto inst = self->engine.host.create (types[r - 1000], self->engine.sampleRate, self->engine.maxBlock, err);
                if (inst == nullptr)
                {
                    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Plugin", "No se pudo cargar:\n" + err);
                    return;
                }
                self->envWindow.reset();
                self->engine.addMasterPlugin (std::move (inst));
                self->openEditor (*mp.back());
            }
            else if (r == 300)
            {
                self->showEnvelopePanel (nullptr);
            }
            else if (r >= 200 && r < 300)
            {
                const int idx = r - 200;
                if (idx < (int) mp.size())
                {
                    self->envWindow.reset();
                    self->closeWindowFor (mp[(size_t) idx].get());
                    self->engine.removeMasterPlugin (idx);
                    self->clipsChanged();
                }
            }
            else if (r >= 100 && r < 200)
            {
                const int idx = r - 100;
                if (idx < (int) mp.size())
                    self->openEditor (*mp[(size_t) idx]);
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
        bMasterFx.setBounds (bar1.removeFromRight (84));
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

        bottomBar.setBounds (r.removeFromBottom (ui::bottomH));
        viewport.setBounds (r);
        dropOverlay.setBounds (r);
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

        // Ctrl+Z = deshacer; Ctrl+Y o Ctrl+Mayús+Z = rehacer
        if (mods.isCtrlDown() || mods.isCommandDown())
        {
            const int code = k.getKeyCode();
            if (code == 'Z' || code == 'z')
            {
                if (mods.isShiftDown()) doRedo(); else doUndo();
                return true;
            }
            if (code == 'Y' || code == 'y')
            {
                doRedo();
                return true;
            }
        }

        // Ctrl+R = empezar a grabar (otra vez Ctrl+R = parar)
        if ((mods.isCtrlDown() || mods.isCommandDown()) && (k.getKeyCode() == 'R' || k.getKeyCode() == 'r'))
        {
            doRecord();
            return true;
        }

        if (k == KeyPress::spaceKey)
        {
            if (engine.playing) doStop(); else engine.play();
            return true;
        }
        if (k == KeyPress::homeKey) { engine.setCursor (0); return true; }

        // Esc = quitar la selección de tiempo
        if (k == KeyPress::escapeKey && engine.selEnd.load() > engine.selStart.load())
        {
            engine.selEnd = engine.selStart.load();
            selOverlay.repaint();
            return true;
        }

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
            if (ch == 'm') { bottomBar.toggleMetronome(); return true; }
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

        // alto total: regla + pistas (cada una con sus carriles visibles) + zona "+" + fila Master
        int h = ui::rulerH;
        for (auto& r : rows) h += r->desiredHeight();
        h += ui::addH + masterRow.desiredHeight();

        const int cw = jmax (w, viewport.getMaximumVisibleWidth());
        content.setSize (cw, jmax (h, viewport.getMaximumVisibleHeight()));

        int y = ui::rulerH;
        for (auto& r : rows)
        {
            const int rh = r->desiredHeight();
            r->setBounds (0, y, cw, rh);
            y += rh;
        }
        addArea.setBounds (0, y, cw, ui::addH);
        y += ui::addH;
        masterRow.setBounds (0, y, cw, masterRow.desiredHeight());

        selOverlay.setBounds (content.getLocalBounds());
        loopOverlay.setBounds (content.getLocalBounds());
        loopOverlay.toFront (false);
        playhead.toFront (false);
        content.repaint();
    }

    void repaintRows()
    {
        for (auto& r : rows) r->repaint();
        masterRow.repaint();
    }

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

    void doUndo()
    {
        if (engine.recording) { setStatus (T ("No se puede deshacer mientras graba")); return; }
        if (engine.undo()) { afterUndoRedo(); setStatus (T ("Deshecho")); }
        else setStatus (T ("No hay nada que deshacer"));
    }

    void doRedo()
    {
        if (engine.recording) { setStatus (T ("No se puede rehacer mientras graba")); return; }
        if (engine.redo()) { afterUndoRedo(); setStatus (T ("Rehecho")); }
        else setStatus (T ("No hay nada que rehacer"));
    }

    void afterUndoRedo()
    {
        validateSelection();
        layoutContent();         // cambian los clips y los carriles visibles
        repaintRows();
        selOverlay.repaint();
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
        envWindow.reset();
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

        // al detenerse (Stop, Play otra vez o fin de la canción) el cursor vuelve a la posición de inicio:
        // si quedó fuera de la vista, se desplaza la vista hasta él
        const bool nowPlaying = engine.playing.load();
        if (wasPlaying && ! nowPlaying)
        {
            auto v = viewport.getViewArea();
            if (x > v.getRight() - 40 || x < v.getX())
                viewport.setViewPosition (jmax (0, x - 80), v.getY());
        }
        wasPlaying = nowPlaying;

        if (engine.playing)
        {
            auto v = viewport.getViewArea();
            if (x > v.getRight() - 40 || x < v.getX())
                viewport.setViewPosition (jmax (0, x - 80), v.getY());
        }

        for (auto& r : rows) r->updateMeters();
        masterRow.updateCursor();
        bottomBar.updateMeters();

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
                clip->startSample = engine.startPos.load();
                engine.addClip (*t, std::move (clip));
                addTrackRow (t);
            }
            layoutContent();
            repaintRows();
        });
    }

    // ---------------- Arrastrar audio desde el escritorio / el Explorador ----------------
    bool isAudioFile (const String& path) const
    {
        return engine.formats.findFormatForFileExtension (File (path).getFileExtension()) != nullptr;
    }

    // Una pista nueva por cada archivo, con el audio ya cargado (onda visible) a partir de la posición de inicio,
    // lista para darle a Play.
    void importDroppedFiles (const StringArray& paths)
    {
        StringArray failed;
        int added = 0;

        for (auto& p : paths)
        {
            const File f (p);
            if (! isAudioFile (p)) continue;

            auto clip = engine.loadClip (f);
            if (clip == nullptr) { failed.add (f.getFileName()); continue; }

            auto* t = engine.addTrack (f.getFileNameWithoutExtension());
            clip->startSample = engine.startPos.load();
            engine.addClip (*t, std::move (clip));
            addTrackRow (t);
            ++added;
        }

        layoutContent();
        repaintRows();

        if (added > 0)     // baja hasta las pistas nuevas
            viewport.setViewPosition (viewport.getViewPositionX(),
                                      jmax (0, content.getHeight() - viewport.getMaximumVisibleHeight()));

        if (added > 0)
            setStatus (added == 1 ? T ("Pista nueva creada") : String (added) + T (" pistas nuevas creadas"));

        if (failed.size() > 0)
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Audio",
                T ("No se pudo cargar:\n") + failed.joinIntoString ("\n")
                    + T ("\n(formato no soportado o archivo demasiado grande)"));
    }

    bool isInterestedInFileDrag (const StringArray& files) override
    {
        for (auto& p : files)
            if (isAudioFile (p)) return true;
        return false;
    }

    void fileDragEnter (const StringArray& files, int, int) override
    {
        std::vector<DropOverlay::Item> items;
        for (auto& p : files)
        {
            if (! isAudioFile (p)) continue;
            DropOverlay::Item it;
            it.name = File (p).getFileNameWithoutExtension();
            if (std::unique_ptr<AudioFormatReader> r { engine.formats.createReaderFor (File (p)) })   // solo lee la cabecera
                if (r->sampleRate > 0.0) it.seconds = (double) r->lengthInSamples / r->sampleRate;
            items.push_back (it);
        }

        dropOverlay.setInfo (std::move (items), addArea.getY() - viewport.getViewPositionY(),
                             ui::headerW - viewport.getViewPositionX()
                                 + (int) ((double) engine.startPos.load() / engine.sampleRate * pxPerSec),
                             pxPerSec, -viewport.getViewPositionX());
        dropOverlay.setVisible (true);
        dropOverlay.toFront (false);
    }

    void fileDragExit (const StringArray&) override { dropOverlay.setVisible (false); }

    void filesDropped (const StringArray& files, int, int) override
    {
        dropOverlay.setVisible (false);
        importDroppedFiles (files);
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

            envWindow.reset();
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
        for (auto& p : t.inputPlugins)
            closeWindowFor (p.get());
    }

    // ----------------------------- Datos -----------------------------------
    Engine engine;                                       // se destruye al final
    std::vector<std::unique_ptr<PluginWindow>> windows;
    std::vector<std::unique_ptr<TrackRow>> rows;

    std::unique_ptr<EnvelopeWindow> envWindow;           // panel de envolventes abierto (si hay)

    TimelineContent content;
    AddTrackArea addArea;
    MasterRow masterRow { engine, *this };
    SelectionOverlay selOverlay { engine, [this] { return pxPerSec; } };
    LoopOverlay loopOverlay { engine, [this] { return pxPerSec; } };
    Playhead playhead;
    Viewport viewport;
    BottomBar bottomBar { engine };
    DropOverlay dropOverlay;
    bool wasPlaying = false;

    TextButton bHome, bPlay, bStop, bRec, bAdd, bAddBus, bImport, bZoomOut, bZoomIn, bAudio, bScan, bReport, bOpen, bSave;
    TextButton bSelect, bHand, bCut, bLoop, bMasterFx;
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
