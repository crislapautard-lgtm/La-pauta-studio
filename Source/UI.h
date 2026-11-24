#pragma once
#include <JuceHeader.h>
#include <map>
#include <algorithm>
#include <limits>
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
    virtual void trackDragMove (Track&, int screenY) = 0;          // arrastrando una pista por su cabecera
    virtual void trackDragEnd (Track&, bool drop) = 0;
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
    virtual bool isClipSelected (const Clip*) const = 0;            // ¿está en la selección (una o varias)?
    virtual int selectionCount() const = 0;
    virtual void groupDragBegin() = 0;                              // mover varios clips seleccionados a la vez
    virtual void groupDragTo (int64 deltaSamples) = 0;
    virtual void groupDragEnd() = 0;
    virtual void marqueeBegin (Point<int> screenPos) = 0;           // cuadro de selección con el ratón
    virtual void marqueeDrag (Point<int> screenPos) = 0;
    virtual void marqueeEnd() = 0;
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
    // 'ownerHandle' = ventana principal del DAW: así la ventana del plugin queda siempre por encima de ella
    // (no se esconde detrás al darle Play) y se minimiza con ella. 'spaceFn' = barra espaciadora = Play / Stop.
    PluginWindow (AudioPluginInstance& p, std::function<void (PluginWindow*)> closeCb,
                  void* ownerHandle = nullptr, std::function<void()> spaceFn = {})
        : DocumentWindow (p.getName(), Colour (0xff202020), DocumentWindow::closeButton),
          plugin (&p), onClose (std::move (closeCb)), onSpace (std::move (spaceFn))
    {
        setUsingNativeTitleBar (true);

        AudioProcessorEditor* ed = p.hasEditor() ? p.createEditorIfNeeded() : nullptr;
        if (ed == nullptr)
            ed = new GenericAudioProcessorEditor (p);

        setContentOwned (ed, true);
        setResizable (ed->isResizable(), false);
        centreWithSize (getWidth(), getHeight());
        if (ownerHandle != nullptr)
            addToDesktop (getDesktopWindowStyleFlags(), ownerHandle);
        setVisible (true);
    }

    ~PluginWindow() override { clearContentComponent(); }
    void closeButtonPressed() override { onClose (this); }

    bool keyPressed (const KeyPress& k) override
    {
        if (k == KeyPress::spaceKey && onSpace) { onSpace(); return true; }
        return DocumentWindow::keyPressed (k);
    }

    AudioPluginInstance* plugin;

private:
    std::function<void (PluginWindow*)> onClose;
    std::function<void()> onSpace;
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

    int height() const
    {
        int h = 0;
        for (auto* e : visibleLanes()) h += e->laneH;
        return h;
    }

    // ¿El ratón está en el borde inferior de la cabecera de un carril? (arrastra para cambiar su alto)
    bool onGrip (const MouseEvent& e, int top0) const
    {
        if (e.x >= ui::headerW || e.y < top0) return false;
        int top = top0;
        for (auto* env : visibleLanes())
        {
            const int bottom = top + env->laneH;
            if (e.y >= bottom - 6 && e.y < bottom) return true;
            top = bottom;
        }
        return false;
    }

    void resetGrip (const MouseEvent& e, int top0)
    {
        if (! onGrip (e, top0)) return;
        int top = top0;
        for (auto* env : visibleLanes())
        {
            const int bottom = top + env->laneH;
            if (e.y >= bottom - 6 && e.y < bottom) { env->laneH = 72; owner.clipsChanged(); return; }
            top = bottom;
        }
    }

    // curves = false: fondo y cabeceras (antes de la rejilla); curves = true: líneas y puntos (encima)
    void paint (Graphics& g, int top0, int width, double pps, bool curves)
    {
        auto lanes = visibleLanes();

        int nextTop = top0;
        for (size_t k = 0; k < lanes.size(); ++k)
        {
            const Envelope& e = *lanes[k];
            const int top = nextTop;
            const int laneH = e.laneH;
            nextTop += laneH;
            const bool on = e.active.load();

            if (! curves)
            {
                g.setColour (Colour (0xff23272c));
                g.fillRect (0, top, ui::headerW, laneH);
                g.setColour (Colour (0xff12171c));
                g.fillRect (ui::headerW, top, jmax (0, width - ui::headerW), laneH);
                g.setColour (Colours::black);
                g.drawHorizontalLine (top, 0.0f, (float) width);
                g.drawVerticalLine (ui::headerW - 1, (float) top, (float) (top + laneH));
                g.setColour (Colour (0xff555555));
                g.fillRect (ui::headerW / 2 - 14, top + laneH - 4, 28, 2);        // agarrador de alto

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
                g.drawHorizontalLine (top + laneH / 2, (float) ui::headerW, (float) width);

                if (e.points.empty()) continue;

                auto yOf = [top, laneH] (float v) { return (float) top + 4.0f + (1.0f - v) * (float) (laneH - 8); };
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
        int k = -1, top = top0;
        for (size_t i = 0; i < lanes.size(); ++i)
        {
            if (e.y < top + lanes[i]->laneH) { k = (int) i; break; }
            top += lanes[i]->laneH;
        }
        if (k < 0) return false;

        auto* env = lanes[(size_t) k];

        if (e.x < ui::headerW && ! e.mods.isPopupMenu() && e.y >= top + env->laneH - 6)    // agarrador: cambia el alto del carril
        {
            resizeEnv = env;
            resizeStartH = env->laneH;
            return true;
        }

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
        if (resizeEnv != nullptr)
        {
            if (! exists (resizeEnv)) { resizeEnv = nullptr; return true; }
            const int nh = jlimit (40, 300, resizeStartH + e.getDistanceFromDragStartY());
            if (nh != resizeEnv->laneH) { resizeEnv->laneH = nh; owner.clipsChanged(); }
            return true;
        }
        if (dragEnv == nullptr || dragIdx < 0) return false;

        const float v = valueFromY (*dragEnv, e.y - dragTop);
        eng.envSetPoint (*dragEnv, dragIdx, sampleAt (e.x), v);
        dragValue = v;
        comp.repaint();
        return true;
    }

    void mouseUp()
    {
        resizeEnv = nullptr;
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
        const float v = jlimit (0.0f, 1.0f, 1.0f - (float) (yInLane - 4) / (float) (env.laneH - 8));
        return env.isStep() ? (v >= 0.5f ? 1.0f : 0.0f) : v;
    }

    int pointAt (const Envelope& env, int top, int x, int y) const
    {
        for (size_t i = 0; i < env.points.size(); ++i)
        {
            const double px = ui::headerW + (double) env.points[i].sample / eng.sampleRate * owner.pps();
            const double py = top + 4.0 + (1.0 - env.points[i].value) * (env.laneH - 8);
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
    Envelope* resizeEnv = nullptr;
    int resizeStartH = 72;
    int dragIdx = -1;
    int dragTop = 0;
    float dragValue = 0.0f;
};

// Selector de color libre (se abre desde el menú de color de una pista)
class TrackColourSelector : public ColourSelector, private ChangeListener
{
public:
    TrackColourSelector (Colour init, std::function<void (Colour)> cb)
        : ColourSelector (ColourSelector::showSliders | ColourSelector::showColourspace, 4, 4), onColour (std::move (cb))
    {
        setCurrentColour (init, dontSendNotification);
        addChangeListener (this);
        setSize (260, 280);
    }
    ~TrackColourSelector() override { removeChangeListener (this); }

private:
    void changeListenerCallback (ChangeBroadcaster*) override { if (onColour) onColour (getCurrentColour()); }
    std::function<void (Colour)> onColour;
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
        btnSend.setTooltip (T ("Salida y envíos a buses (un fader por bus)"));
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

        if (track.isBus.load())   // un bus no graba ni tiene solo (sí tiene envíos hacia otros buses)
        {
            btnInFx.setVisible (false);
            btnMon.setVisible (false);
            btnSolo.setVisible (false);
            btnArm.setVisible (false);
            inputBox.setVisible (false);
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
    bool resizing = false, headerDrag = false, headerMoved = false;
    Clip* gainClip = nullptr;         // clip cuya perilla de volumen se arrastra
    double gainStartDb = 0.0;
    int resizeStartH = 96;

    // alto de la fila: la pista + un carril por cada automatización visible
    int baseH() const { return track.uiHeight.load(); }

    // Perilla de volumen de cada clip (como en Reaper): arrastra arriba/abajo; doble clic = 0 dB
    static constexpr int knobD = 14;
    Rectangle<int> knobRect (const Clip& c) const
    {
        const int x0 = ui::headerW + (int) ((double) c.startSample.load() / eng.sampleRate * owner.pps());
        const int w = (int) ((double) c.audio.getNumSamples() / eng.sampleRate * owner.pps());
        if (w < 70) return {};
        return { x0 + 12, 4 + 1, knobD, knobD };
    }

    Clip* knobAt (const MouseEvent& e) const
    {
        if (e.y > 22) return nullptr;
        for (auto& c : track.clips)
        {
            auto k = knobRect (*c);
            if (! k.isEmpty() && k.expanded (3).contains (e.getPosition())) return c.get();
        }
        return nullptr;
    }

    static double clipDb (const Clip& c)
    {
        const float g = c.gain.load();
        return g <= 0.0f ? -60.0 : (double) Decibels::gainToDecibels (g, -60.0f);
    }

    // Color de la pista: clic derecho en su cabecera
    Colour trackColour() const { return Colour (track.color.load()); }

    void showColourMenu()
    {
        static const struct { const char* name; uint32 argb; } presets[] =
        {
            { "Rojo", 0xffc0392b }, { "Naranja", 0xffd9822b }, { "Amarillo", 0xffd4b021 }, { "Verde", 0xff2e9d4f },
            { "Turquesa", 0xff1f9a9a }, { "Azul", 0xff2b6cb0 }, { "Morado", 0xff7d4fb3 }, { "Rosa", 0xffc2549a },
            { "Marrón", 0xff8a5a3c }, { "Gris", 0xff6b7280 }
        };

        PopupMenu m;
        m.addSectionHeader (T ("Color de la pista"));
        for (int i = 0; i < 10; ++i)
            m.addColouredItem (i + 1, T (presets[i].name), Colour (presets[i].argb));
        m.addSeparator();
        m.addItem (100, T ("Otro color..."));
        m.addItem (101, T ("Sin color (por defecto)"));

        SafePointer<TrackRow> sp (this);
        m.showMenuAsync (PopupMenu::Options(), [sp] (int res)
        {
            auto* self = sp.getComponent();
            if (self == nullptr || res == 0) return;

            if (res >= 1 && res <= 10) self->track.color = presets[res - 1].argb;
            else if (res == 101)       self->track.color = 0;
            else if (res == 100)
            {
                const Colour start = self->track.color.load() != 0 ? self->trackColour() : Colour (0xff2b5a85);
                auto box = std::make_unique<TrackColourSelector> (start, [sp] (Colour c)
                {
                    if (auto* s2 = sp.getComponent()) { s2->track.color = c.withAlpha (1.0f).getARGB(); s2->repaint(); }
                });
                CallOutBox::launchAsynchronously (std::move (box), self->getScreenBounds().withWidth (ui::headerW).withHeight (28), nullptr);
                return;
            }
            self->repaint();
        });
    }
    int desiredHeight() const { return baseH() + lanes.height(); }

    // Cambiar el tamaño de la pista: arrastra el borde inferior de su cabecera (doble clic = tamaño original)
    bool onGrip (const MouseEvent& e) const { return e.x < ui::headerW && e.y >= baseH() - 7 && e.y < baseH() + 2; }
    static constexpr int minH = 48, maxH = 400;

    void resized() override
    {
        const bool roomy = baseH() >= 90;       // en pistas bajas se ocultan fader, pan y Input FX
        vol.setVisible (roomy); pan.setVisible (roomy); btnInFx.setVisible (roomy); btnMon.setVisible (roomy);
        nameLabel.setBounds (6, 4, 128, 22);
        btnSend.setBounds (138, 4, 50, 22);
        btnAuto.setBounds (192, 4, 52, 22);
        if (track.isBus.load())      // un bus no tiene Solo, Armar ni entrada: los botones van juntos, sin huecos
        {
            btnMute.setBounds (6, 30, 32, 24);
            btnFx.setBounds (42, 30, 48, 24);
            btnDel.setBounds (94, 30, 32, 24);
        }
        else
        {
            btnMute.setBounds (6, 30, 32, 24);
            btnSolo.setBounds (42, 30, 32, 24);
            btnArm.setBounds (78, 30, 32, 24);
            btnFx.setBounds (114, 30, 32, 24);
            btnDel.setBounds (150, 30, 32, 24);
            inputBox.setBounds (188, 30, 56, 24);
        }
        vol.setBounds (6, 58, 150, 18);
        pan.setBounds (6, 76, 150, 18);
        btnInFx.setBounds (160, 58, 84, 18);
        btnMon.setBounds (160, 76, 84, 18);
    }

    // Pon el nombre en edición (al crear un bus nuevo)
    void beginRename() { nameLabel.showEditor(); }

    void updateMeters()
    {
        // el Mezclador puede mover fader, pan, mute y solo: la fila los sigue
        if (! vol.isMouseButtonDown())
        {
            const double db = Decibels::gainToDecibels (track.gain.load(), -60.0f);
            if (std::abs (vol.getValue() - db) > 0.05) vol.setValue (db, dontSendNotification);
        }
        if (! pan.isMouseButtonDown() && std::abs (pan.getValue() - (double) track.pan.load()) > 0.005)
            pan.setValue (track.pan.load(), dontSendNotification);
        if (btnMute.getToggleState() != track.mute.load()) btnMute.setToggleState (track.mute.load(), dontSendNotification);
        if (btnSolo.getToggleState() != track.solo.load()) btnSolo.setToggleState (track.solo.load(), dontSendNotification);

        const float l = track.peakL.load(), r = track.peakR.load();
        if (std::abs (l - lastL) > 0.002f || std::abs (r - lastR) > 0.002f)
        {
            lastL = l; lastR = r;
            repaint (248, 0, 52, baseH());
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
                repaint (0, baseH(), ui::headerW, lanes.height());
            }
        }
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff1c1c1c));
        const Colour base = track.isBus.load() ? Colour (0xff3a2a4a) : Colour (0xff2a2a2a);   // buses en morado
        g.setColour (track.color.load() != 0 ? base.interpolatedWith (trackColour(), 0.35f) : base);
        g.fillRect (0, 0, ui::headerW, getHeight());
        if (track.color.load() != 0)
        {
            g.setColour (trackColour());
            g.fillRect (0, 0, 5, getHeight());
        }
        if (headerMoved)       // se está moviendo esta pista
        {
            g.setColour (Colours::white.withAlpha (0.16f));
            g.fillRect (0, 0, ui::headerW, getHeight());
        }
        g.setColour (Colours::black);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        g.drawVerticalLine (ui::headerW - 1, 0.0f, (float) getHeight());
        g.setColour (Colour (0xff555555));
        g.fillRect (ui::headerW / 2 - 14, baseH() - 4, 28, 2);        // agarrador de tamaño

        auto bar = [&] (int x, float v)
        {
            const float db = Decibels::gainToDecibels (v, -60.0f);
            const float f = jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
            const int mh = jmax (20, baseH() - 12);
            const int h = (int) (f * (float) mh);
            g.setColour (Colours::black);
            g.fillRect (x, 6, 14, mh);
            g.setColour (db > -3.0f ? Colours::red : (db > -12.0f ? Colours::yellow : Colours::limegreen));
            g.fillRect (x, 6 + mh - h, 14, h);
        };
        bar (252, track.peakL.load());
        bar (272, track.peakR.load());

        const double pps = owner.pps();
        const auto visible = g.getClipBounds();

        lanes.paint (g, baseH(), getWidth(), pps, false);   // fondo y cabeceras de los carriles
        drawGrid (g, pps, visible);

        for (auto& c : track.clips)
        {
            const double startSec = (double) c->startSample.load() / eng.sampleRate;
            const double lenSec = (double) c->audio.getNumSamples() / eng.sampleRate;
            Rectangle<int> r (ui::headerW + (int) (startSec * pps), 4, jmax (2, (int) (lenSec * pps)), baseH() - 8);

            g.setColour (owner.isClipSelected (c.get()) ? Colour (0xffd08a2e)
                         : (track.color.load() != 0 ? trackColour().darker (0.35f) : Colour (0xff2b5a85)));   // seleccionado = naranja
            g.fillRect (r);

            auto vis = r.getIntersection (visible);
            if (! vis.isEmpty() && c->thumb != nullptr)
            {
                g.setColour (Colours::white.withAlpha (0.85f));
                const double t0 = (vis.getX() - r.getX()) / pps;
                const double t1 = (vis.getRight() - r.getX()) / pps;
                c->thumb->drawChannels (g, vis.withTrimmedTop (16).withTrimmedBottom (2), t0, t1, jlimit (0.0f, 4.0f, c->gain.load()));
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

            // perilla de volumen del clip
            const auto kr = knobRect (*c);
            if (! kr.isEmpty())
            {
                const bool changed = std::abs (c->gain.load() - 1.0f) > 0.001f;
                const auto kf = kr.toFloat();
                g.setColour (Colour (0xff1b1b1b));
                g.fillEllipse (kf);
                g.setColour (changed ? Colours::orange : Colours::white.withAlpha (0.85f));
                g.drawEllipse (kf.reduced (0.5f), 1.2f);
                const double norm = jlimit (0.0, 1.0, (clipDb (*c) + 60.0) / 72.0);
                const float ang = (float) ((norm - 0.5) * 1.5 * MathConstants<double>::pi);     // -135° .. +135°
                const auto ctr = kf.getCentre();
                g.drawLine (ctr.x, ctr.y, ctr.x + std::sin (ang) * 5.0f, ctr.y - std::cos (ang) * 5.0f, 1.6f);
            }

            String label = c->name;
            if (! kr.isEmpty() && std::abs (c->gain.load() - 1.0f) > 0.001f)
                label << (c->gain.load() <= 0.0f ? String (" · -inf dB") : String (" · ") + String (clipDb (*c), 1) + " dB");

            g.setColour (Colours::white);
            g.setFont (12.0f);
            const int textX = r.getX() + (kr.isEmpty() ? 14 : 14 + knobD + 4);
            g.drawText (label, textX, r.getY() + 1, jmax (10, r.getRight() - textX - 14), 14, Justification::left, true);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.drawRect (r);
        }

        lanes.paint (g, baseH(), getWidth(), pps, true);    // curvas y puntos encima de todo

        g.setColour (Colours::black);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void mouseMove (const MouseEvent& e) override
    {
        MouseCursor::StandardCursorType cur = MouseCursor::NormalCursor;
        if (onGrip (e) || lanes.onGrip (e, baseH())) { setMouseCursor (MouseCursor::UpDownResizeCursor); return; }
        if (e.x >= ui::headerW && e.y < baseH() && knobAt (e) != nullptr) { setMouseCursor (MouseCursor::UpDownResizeCursor); return; }
        if (e.x >= ui::headerW && e.y < baseH())
        {
            if (owner.getTool() == ui::toolHand)     cur = MouseCursor::DraggingHandCursor;
            else if (owner.getTool() == ui::toolCut) cur = MouseCursor::CrosshairCursor;
        }
        if (e.x >= ui::headerW && e.y < baseH() && owner.getTool() != ui::toolCut)
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
        if (onGrip (e) && ! e.mods.isPopupMenu())
        {
            resizing = true;
            resizeStartH = baseH();
            return;
        }
        dragClip = nullptr;
        trimClip = nullptr;
        rightDown = false;
        emptyDown = false;
        groupDrag = false;
        if (e.y >= baseH()) { lanes.mouseDown (e, baseH()); return; }   // zona de carriles
        if (e.x < ui::headerW)
        {
            if (e.mods.isPopupMenu()) showColourMenu();
            else { headerDrag = true; headerMoved = false; }      // clic en la cabecera: arrastra para cambiar de lugar
            return;
        }

        auto* c = clipAt (e.x);

        // perilla de volumen del clip: arrastrar arriba / abajo
        if (! e.mods.isPopupMenu())
            if (auto* kc = knobAt (e))
            {
                eng.undoBegin();                       // Ctrl+Z deshace el cambio de volumen
                gainClip = kc;
                gainStartDb = clipDb (*kc);
                owner.setSelected (&track, kc);
                return;
            }

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
        const bool ctrlDown = e.mods.isCtrlDown() || e.mods.isCommandDown();

        // Varios clips seleccionados con el cuadro: arrastrar uno de ellos mueve todos juntos
        if (c != nullptr && tool != ui::toolCut && ! ctrlDown && owner.isClipSelected (c) && owner.selectionCount() > 1)
        {
            eng.undoBegin();
            owner.groupDragBegin();
            groupDrag = true;
            groupAnchor = c->startSample.load();
            return;
        }

        owner.setSelected (c != nullptr ? &track : nullptr, c);

        if (c != nullptr && tool == ui::toolCut)
        {
            // Herramienta Cortar: divide el clip donde haces clic (con imán a la rejilla)
            eng.splitClip (track, c, owner.snapSample (sampleAt (e.x)));
            owner.setSelected (nullptr, nullptr);
            owner.clipsChanged();
            return;
        }

        // Ctrl + clic izquierdo sobre un clip: se crea una copia y se arrastra (también a otra pista).
        // Si sueltas sin moverla, no se queda la copia.
        if (c != nullptr && (e.mods.isCtrlDown() || e.mods.isCommandDown()) && ! e.mods.isShiftDown())
        {
            eng.undoBegin();
            if (auto* copy = eng.duplicateClip (track, c, c->startSample.load(), false))
            {
                dragClip = copy;
                dragOrigStart = copy->startSample.load();
                ctrlCopy = true;
                owner.setSelected (&track, copy);
                owner.clipsChanged();
            }
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
        else if (c == nullptr && tool != ui::toolCut)
        {
            // espacio vacío: clic = deseleccionar y mover el cursor; arrastrar = cuadro de selección
            emptyDown = true;
            emptyMoved = false;
            emptyX = e.x;
            owner.marqueeBegin (e.getScreenPosition());
        }
        else
        {
            owner.setCursorFromX (e.x);
        }
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (headerDrag)
        {
            if (! headerMoved && e.getDistanceFromDragStart() < 5) return;
            if (! headerMoved) { headerMoved = true; setMouseCursor (MouseCursor::DraggingHandCursor); repaint(); }
            owner.trackDragMove (track, e.getScreenY());
            return;
        }
        if (gainClip != nullptr)
        {
            const double db = jlimit (-60.0, 12.0, gainStartDb - e.getDistanceFromDragStartY() * 0.25);
            gainClip->gain = db <= -59.9 ? 0.0f : Decibels::decibelsToGain ((float) db, -60.0f);
            repaint();
            return;
        }
        if (resizing)
        {
            const int nh = jlimit (minH, maxH, resizeStartH + e.getDistanceFromDragStartY());
            if (nh != baseH()) { track.uiHeight = nh; resized(); owner.clipsChanged(); }
            return;
        }
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

        if (emptyDown)
        {
            if (e.getDistanceFromDragStart() > 4) emptyMoved = true;
            if (emptyMoved) owner.marqueeDrag (e.getScreenPosition());
            return;
        }

        if (groupDrag)
        {
            const auto delta = (int64) (e.getDistanceFromDragStartX() / owner.pps() * eng.sampleRate);
            owner.groupDragTo (owner.snapSample (groupAnchor + delta) - groupAnchor);
            return;
        }

        if (lanes.mouseDrag (e, baseH())) return;

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

    void mouseDoubleClick (const MouseEvent& e) override
    {
        if (onGrip (e)) { track.uiHeight = 96; resized(); owner.clipsChanged(); }
        else if (auto* kc = knobAt (e))
        {
            eng.undoBegin();
            kc->gain = 1.0f;
            eng.undoEnd();
            repaint();
        }
        else lanes.resetGrip (e, baseH());
    }

    void mouseUp (const MouseEvent& e) override
    {
        if (headerDrag)
        {
            const bool moved = headerMoved;
            headerDrag = headerMoved = false;
            setMouseCursor (MouseCursor::NormalCursor);
            if (moved) owner.trackDragEnd (track, true);
            repaint();
            return;
        }
        if (gainClip != nullptr) { gainClip = nullptr; eng.undoEnd(); repaint(); return; }
        if (resizing) { resizing = false; return; }
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

        if (emptyDown)
        {
            emptyDown = false;
            if (emptyMoved) owner.marqueeEnd();
            else owner.setCursorFromX (emptyX);
            emptyMoved = false;
            return;
        }

        if (groupDrag)
        {
            groupDrag = false;
            owner.groupDragEnd();
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
            auto* dst = owner.trackAtScreenY (e.getScreenY());
            const bool toOtherTrack = dst != nullptr && dst != &track;

            // Ctrl+clic sin mover la copia: se cancela (se deshace el paso y no queda rehacer)
            if (ctrlCopy)
            {
                ctrlCopy = false;
                if (! toOtherTrack && ownsClip (moved) && moved->startSample.load() == dragOrigStart)
                {
                    eng.undo();
                    eng.dropRedo();
                    owner.setSelected (nullptr, nullptr);
                    owner.clipsChanged();
                    return;
                }
            }

            // soltado sobre otra pista: el clip pasa a esa pista (conserva su posición en el tiempo)
            if (toOtherTrack && ownsClip (moved))
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
    bool emptyDown = false, emptyMoved = false, groupDrag = false;   // cuadro de selección / mover grupo
    int emptyX = 0;
    int64 groupAnchor = 0;
    bool ctrlCopy = false;           // el clip que se arrastra es una copia hecha con Ctrl+clic
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
class SendRow : public Component
{
public:
    SendRow (Engine& e, Track& from, Track& to) : eng (e), src (from), dst (to)
    {
        const float cur = eng.getSendLevel (&src, &dst);
        const bool allowed = eng.canSend (&src, &dst) || cur > 0.0f;     // false = enviarle crearía un lazo

        lbl.setText (allowed ? dst.name : dst.name + T (" (lazo: no permitido)"), dontSendNotification);
        lbl.setColour (Label::textColourId, allowed ? Colours::white : Colours::grey);
        lbl.setMinimumHorizontalScale (0.7f);
        addAndMakeVisible (lbl);

        sl.setSliderStyle (Slider::LinearHorizontal);
        sl.setTextBoxStyle (Slider::TextBoxRight, true, 58, 20);
        sl.setRange (-60.0, 6.0, 0.1);
        sl.setSkewFactorFromMidPoint (-12.0);
        sl.setDoubleClickReturnValue (true, 0.0);
        sl.setScrollWheelEnabled (false);
        sl.textFromValueFunction = [] (double v) { return v <= -59.9 ? String ("off") : String (v, 1) + " dB"; };
        sl.setValue (cur > 0.0f ? Decibels::gainToDecibels (cur, -60.0f) : -60.0, dontSendNotification);
        sl.setEnabled (allowed);
        sl.onValueChange = [this]
        {
            const double v = sl.getValue();
            eng.setSendLevel (&src, &dst, v <= -59.9 ? 0.0f : Decibels::decibelsToGain ((float) v, -60.0f));
        };
        addAndMakeVisible (sl);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4, 2);
        lbl.setBounds (r.removeFromLeft (96));
        sl.setBounds (r);
    }

private:
    Engine& eng;
    Track& src;
    Track& dst;
    Label lbl;
    Slider sl;
};

class SendPanel : public Component
{
public:
    SendPanel (Engine& e, Track& t) : eng (e), track (t)
    {
        buses = eng.busList();
        buses.erase (std::remove (buses.begin(), buses.end(), &track), buses.end());   // un bus no se envía a sí mismo

        constexpr int panelW = 280, rowH = 28;
        int y = 8;

        // Salida principal: pistas y buses. Un bus puede salir a otro bus (o al Master); lo que cerraría un lazo se bloquea.
        {
            lblOut.setText (track.isBus.load() ? T ("Salida del bus") : T ("Salida de la pista"), dontSendNotification);
            lblOut.setBounds (10, y, panelW - 20, 18);
            addAndMakeVisible (lblOut);
            y += 20;

            outBox.addItem ("Master", 1);
            for (size_t i = 0; i < buses.size(); ++i)
            {
                const bool blocked = track.isBus.load() && eng.reaches (buses[i], &track);
                outBox.addItem (buses[i]->name + (blocked ? T ("  (bloqueado: lazo)") : String()), (int) i + 2);
                outBox.setItemEnabled ((int) i + 2, ! blocked);
            }
            outBox.setSelectedId (idOf (track.outputBus.load()), dontSendNotification);
            outBox.onChange = [this]
            {
                const int id = outBox.getSelectedId();
                Track* dst = id >= 2 ? buses[(size_t) id - 2] : nullptr;
                if (! eng.setOutput (&track, dst))
                {
                    outBox.setSelectedId (idOf (track.outputBus.load()), dontSendNotification);
                    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, T ("Salida bloqueada"),
                        T ("Esa salida formaría un lazo de realimentación infinito."));
                }
            };
            outBox.setBounds (10, y, panelW - 20, 24);
            addAndMakeVisible (outBox);
            y += 32;
        }

        lblSends.setText (track.isBus.load() ? T ("Este bus envía a:") : T ("Envíos a buses (un fader por bus):"), dontSendNotification);
        lblSends.setBounds (10, y, panelW - 20, 18);
        addAndMakeVisible (lblSends);
        y += 22;

        if (buses.empty())
        {
            hint.setText (T ("Crea un bus con '+ Bus de envío' en la barra superior."), dontSendNotification);
            hint.setFont (Font (FontOptions (12.0f)));
            hint.setBounds (10, y, panelW - 20, 18);
            addAndMakeVisible (hint);
            y += 24;
        }
        else
        {
            for (size_t i = 0; i < buses.size(); ++i)
            {
                auto row = std::make_unique<SendRow> (eng, track, *buses[i]);
                row->setBounds (0, (int) i * rowH, panelW - 10, rowH);
                body.addAndMakeVisible (*row);
                sendRows.push_back (std::move (row));
            }
            body.setSize (panelW - 10, (int) buses.size() * rowH);

            const int vh = jmin (6 * rowH, (int) buses.size() * rowH + 2);
            vp.setViewedComponent (&body, false);
            vp.setScrollBarsShown (true, false);
            vp.setBounds (0, y, panelW, vh);
            addAndMakeVisible (vp);
            y += vh + 6;
        }

        setSize (panelW, y + 4);
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
    Label lblOut, lblSends, hint;
    ComboBox outBox;
    std::vector<std::unique_ptr<SendRow>> sendRows;      // se declara antes que 'body' y 'vp' (se destruyen después)
    Component body;
    Viewport vp;
};

// ---------------------------------------------------------------------------
// Mezclador: una tira por pista y por bus (fader, pan, mute, solo, medidor, envíos) y el Master
// ---------------------------------------------------------------------------
class MixerStrip : public Component
{
public:
    static constexpr int stripW = 88;

    // track == nullptr: tira del Master
    MixerStrip (Engine& e, Track* t, std::function<void (Track&, Component&)> sendFn, std::function<void (double)> masterFn)
        : eng (e), track (t), onMasterDb (std::move (masterFn))
    {
        const bool master = track == nullptr;
        const bool bus = track != nullptr && track->isBus.load();

        nameLabel.setText (master ? String ("MASTER") : track->name, dontSendNotification);
        nameLabel.setJustificationType (Justification::centred);
        nameLabel.setColour (Label::textColourId, Colours::white);
        nameLabel.setFont (Font (FontOptions (12.0f, Font::bold)));
        addAndMakeVisible (nameLabel);

        fader.setSliderStyle (Slider::LinearVertical);
        fader.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        fader.setRange (-60.0, 6.0, 0.1);
        fader.setSkewFactorFromMidPoint (-12.0);
        fader.setDoubleClickReturnValue (true, 0.0);
        fader.setScrollWheelEnabled (false);
        fader.setPopupDisplayEnabled (true, true, nullptr);
        fader.onValueChange = [this]
        {
            const double db = fader.getValue();
            const float gn = Decibels::decibelsToGain ((float) db, -60.0f);
            if (track == nullptr)
            {
                eng.masterGain = gn;
                if (onMasterDb) onMasterDb (db);
            }
            else if (eng.hasTrack (track))
                track->gain = gn;
            showDb (db);
        };
        addAndMakeVisible (fader);

        dbLabel.setJustificationType (Justification::centred);
        dbLabel.setFont (Font (FontOptions (11.0f)));
        dbLabel.setColour (Label::textColourId, Colours::lightgrey);
        addAndMakeVisible (dbLabel);

        if (! master)
        {
            pan.setSliderStyle (Slider::LinearHorizontal);
            pan.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
            pan.setRange (-1.0, 1.0, 0.01);
            pan.setDoubleClickReturnValue (true, 0.0);
            pan.setScrollWheelEnabled (false);
            pan.setPopupDisplayEnabled (true, true, nullptr);
            pan.onValueChange = [this] { if (eng.hasTrack (track)) track->pan = (float) pan.getValue(); };
            addAndMakeVisible (pan);

            btnEnv.setButtonText (T ("Envíos"));
            btnEnv.setTooltip (T ("Salida y envíos a buses"));
            btnEnv.onClick = [this, sendFn] { if (eng.hasTrack (track)) sendFn (*track, btnEnv); };
            addAndMakeVisible (btnEnv);

            btnM.setButtonText ("M");
            btnM.setClickingTogglesState (true);
            btnM.setColour (TextButton::buttonOnColourId, Colour (0xffd9a400));
            btnM.onClick = [this] { if (eng.hasTrack (track)) track->mute = btnM.getToggleState(); };
            addAndMakeVisible (btnM);

            if (! bus)
            {
                btnS.setButtonText ("S");
                btnS.setClickingTogglesState (true);
                btnS.setColour (TextButton::buttonOnColourId, Colour (0xff2e9d4f));
                btnS.onClick = [this] { if (eng.hasTrack (track)) track->solo = btnS.getToggleState(); };
                addAndMakeVisible (btnS);
            }
        }

        refresh();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        nameLabel.setBounds (r.removeFromTop (20));
        if (track != nullptr)
        {
            btnEnv.setBounds (r.removeFromTop (22));
            pan.setBounds (r.removeFromTop (18));
            auto row = r.removeFromTop (22);
            const bool bus = track->isBus.load();
            btnM.setBounds (bus ? row : row.removeFromLeft (row.getWidth() / 2 - 1));
            if (! bus) btnS.setBounds (row.withTrimmedLeft (2));
        }
        else r.removeFromTop (62);

        dbLabel.setBounds (r.removeFromBottom (18));
        fader.setBounds (r.removeFromLeft (40));
        meter = r.reduced (8, 4);
    }

    // sincroniza con los cambios hechos fuera del mezclador (filas de pista, automatización...)
    void refresh()
    {
        if (track != nullptr && ! eng.hasTrack (track)) return;

        if (nameLabel.getText() != (track != nullptr ? track->name : String ("MASTER")))
            nameLabel.setText (track != nullptr ? track->name : String ("MASTER"), dontSendNotification);

        const float gn = track != nullptr ? track->gain.load() : eng.masterGain.load();
        const double db = Decibels::gainToDecibels (gn, -60.0f);
        if (! fader.isMouseButtonDown() && std::abs (fader.getValue() - db) > 0.05)
            fader.setValue (db, dontSendNotification);
        showDb (db);

        if (track != nullptr)
        {
            if (! pan.isMouseButtonDown()) pan.setValue (track->pan.load(), dontSendNotification);
            btnM.setToggleState (track->mute.load(), dontSendNotification);
            if (! track->isBus.load()) btnS.setToggleState (track->solo.load(), dontSendNotification);
        }
        repaint (meter.expanded (2));
    }

    void paint (Graphics& g) override
    {
        const bool master = track == nullptr;
        if (! master && ! eng.hasTrack (track)) return;

        g.fillAll (master ? Colour (0xff3a2c2c) : (track->isBus.load() ? Colour (0xff3a2a4a) : Colour (0xff2a2a2a)));
        g.setColour (Colours::black);
        g.drawRect (getLocalBounds());

        const float l = master ? eng.masterRmsL.load() : track->peakL.load();
        const float r = master ? eng.masterRmsR.load() : track->peakR.load();
        const int bw = meter.getWidth() / 2 - 1;
        auto bar = [&] (int x, float v)
        {
            const float db = Decibels::gainToDecibels (v, -60.0f);
            const float f = jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
            const int h = (int) (f * (float) meter.getHeight());
            g.setColour (Colours::black);
            g.fillRect (x, meter.getY(), bw, meter.getHeight());
            g.setColour (db > -3.0f ? Colours::red : (db > -12.0f ? Colours::yellow : Colours::limegreen));
            g.fillRect (x, meter.getBottom() - h, bw, h);
        };
        bar (meter.getX(), l);
        bar (meter.getX() + bw + 2, r);
    }

private:
    void showDb (double db)
    {
        dbLabel.setText (db <= -59.9 ? String ("-inf") : String (db, 1) + " dB", dontSendNotification);
    }

    Engine& eng;
    Track* track;
    std::function<void (double)> onMasterDb;
    Label nameLabel, dbLabel;
    Slider fader, pan;
    TextButton btnEnv, btnM, btnS;
    Rectangle<int> meter;
};

class MixerPanel : public Component, private Timer
{
public:
    MixerPanel (Engine& e, std::function<void (Track&, Component&)> sendFn, std::function<void (double)> masterFn,
                std::function<void()> addBusFn)
        : eng (e), onSend (std::move (sendFn)), onMaster (std::move (masterFn))
    {
        btnAddBus.setButtonText (T ("+ Bus de envío"));
        btnAddBus.setTooltip (T ("Crea un bus de envío nuevo (puedes ponerle nombre: Reverb, Delay, Coro...)"));
        btnAddBus.onClick = [addBusFn] { if (addBusFn) addBusFn(); };
        addAndMakeVisible (btnAddBus);

        hint.setText (T ("Los buses (morado) reciben envíos de pistas y de otros buses. Botón 'Envíos' de cada tira."), dontSendNotification);
        hint.setColour (Label::textColourId, Colours::grey);
        hint.setFont (Font (FontOptions (12.0f)));
        addAndMakeVisible (hint);

        vp.setViewedComponent (&holder, false);
        vp.setScrollBarsShown (false, true);
        addAndMakeVisible (vp);

        setSize (820, 420);
        rebuild();
        startTimerHz (20);
    }

    ~MixerPanel() override { stopTimer(); }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (30).reduced (6, 4);
        btnAddBus.setBounds (top.removeFromLeft (130));
        hint.setBounds (top.reduced (8, 0));
        vp.setBounds (r);
        layoutStrips();
    }

private:
    void timerCallback() override
    {
        bool same = known.size() == eng.tracks.size();
        for (size_t i = 0; same && i < known.size(); ++i)
            same = known[i] == eng.tracks[i].get();

        if (! same) { rebuild(); return; }
        for (auto& s : strips) s->refresh();
    }

    void rebuild()
    {
        strips.clear();
        known.clear();
        for (auto& t : eng.tracks)
        {
            known.push_back (t.get());
            auto s = std::make_unique<MixerStrip> (eng, t.get(), onSend, onMaster);
            holder.addAndMakeVisible (*s);
            strips.push_back (std::move (s));
        }
        auto m = std::make_unique<MixerStrip> (eng, nullptr, onSend, onMaster);
        holder.addAndMakeVisible (*m);
        strips.push_back (std::move (m));
        layoutStrips();
    }

    void layoutStrips()
    {
        const int h = jmax (220, vp.getHeight() - vp.getScrollBarThickness());
        int x = 0;
        for (size_t i = 0; i < strips.size(); ++i)
        {
            if (i + 1 == strips.size()) x += 12;       // el Master va separado, al final
            strips[i]->setBounds (x, 0, MixerStrip::stripW, h);
            x += MixerStrip::stripW + 2;
        }
        holder.setSize (x, h);
    }

    Engine& eng;
    std::function<void (Track&, Component&)> onSend;
    std::function<void (double)> onMaster;
    std::vector<Track*> known;
    TextButton btnAddBus;
    Label hint;
    std::vector<std::unique_ptr<MixerStrip>> strips;     // antes que 'holder' y 'vp'
    Component holder;
    Viewport vp;
};

// ---------------------------------------------------------------------------
// Ventana de FX (como REAPER): bypass por plugin, Bypass All, subir/bajar (flechas y arrastrando),
// clic derecho -> Reemplazar / Abrir / Quitar. El plugin de arriba procesa primero.
// ---------------------------------------------------------------------------
struct FxCallbacks
{
    std::function<void (AudioPluginInstance&)> open;
    std::function<void()> add;
    std::function<void (int)> replace;
    std::function<void (int)> remove;
    std::function<void()> automation;
};

class FxBody : public Component
{
public:
    int dropLine = -1;
    static constexpr int rowH = 32;

    void paintOverChildren (Graphics& g) override
    {
        if (dropLine < 0) return;
        g.setColour (Colour (0xffd08a2e));
        g.fillRect (0, dropLine * rowH - 1, getWidth(), 3);
    }
};

class FxChainPanel : public Component, private Timer
{
public:
    FxChainPanel (Engine& e, Track* t, FxCallbacks cb) : eng (e), track (t), cbs (std::move (cb))
    {
        btnAll.setButtonText (T ("Bypass All"));
        btnAll.setTooltip (T ("Apaga todos los efectos a la vez; otra vez los vuelve a prender"));
        btnAll.setColour (TextButton::buttonOnColourId, Colour (0xffb03030));
        btnAll.onClick = [this] { eng.setAllFxOff (track, ! eng.allFxOff (track)); refreshState(); };
        addAndMakeVisible (btnAll);

        btnAdd.setButtonText (T ("+ Añadir FX"));
        btnAdd.onClick = [this] { if (cbs.add) cbs.add(); };
        addAndMakeVisible (btnAdd);

        btnAuto.setButtonText (T ("Automatización..."));
        btnAuto.onClick = [this] { if (cbs.automation) cbs.automation(); };
        addAndMakeVisible (btnAuto);

        hint.setText (T ("Arrastra un efecto para cambiar su orden (el de arriba procesa primero) · clic derecho: Reemplazar"),
                      dontSendNotification);
        hint.setFont (Font (FontOptions (11.0f)));
        hint.setColour (Label::textColourId, Colours::grey);
        addAndMakeVisible (hint);

        vp.setViewedComponent (&body, false);
        vp.setScrollBarsShown (true, false);
        addAndMakeVisible (vp);

        setSize (460, 340);
        rebuild();
        startTimerHz (15);
    }

    ~FxChainPanel() override { stopTimer(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        auto top = r.removeFromTop (28);
        btnAll.setBounds (top.removeFromLeft (100));
        top.removeFromLeft (6);
        btnAdd.setBounds (top.removeFromLeft (100));
        top.removeFromLeft (6);
        btnAuto.setBounds (top.removeFromLeft (130));
        r.removeFromTop (4);
        hint.setBounds (r.removeFromBottom (30));
        vp.setBounds (r);
        layoutRows();
    }

    void paint (Graphics& g) override { g.fillAll (Colour (0xff202020)); }

private:
    struct Row : public Component
    {
        Row (FxChainPanel& o, int i, AudioPluginInstance& p) : owner (o), index (i), plugin (&p)
        {
            bypass.setClickingTogglesState (false);
            bypass.setTooltip (T ("Prender / apagar este efecto (bypass)"));
            bypass.onClick = [this] { owner.eng.setFxOff (owner.track, plugin, ! owner.eng.isFxOff (owner.track, plugin)); update(); };
            addAndMakeVisible (bypass);

            name.setText (String (i + 1) + ".  " + p.getName(), dontSendNotification);
            name.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (name);

            up.setButtonText (CharPointer_UTF8 ("\xE2\x96\xB2"));
            up.setTooltip (T ("Subir"));
            up.onClick = [this] { owner.eng.movePlugin (owner.track, index, index - 1); };
            addAndMakeVisible (up);

            down.setButtonText (CharPointer_UTF8 ("\xE2\x96\xBC"));
            down.setTooltip (T ("Bajar"));
            down.onClick = [this] { owner.eng.movePlugin (owner.track, index, index + 1); };
            addAndMakeVisible (down);

            open.setButtonText (T ("Abrir"));
            open.onClick = [this] { if (owner.cbs.open) owner.cbs.open (*plugin); };
            addAndMakeVisible (open);

            del.setButtonText ("x");
            del.setTooltip (T ("Quitar"));
            del.onClick = [this] { if (owner.cbs.remove) owner.cbs.remove (index); };
            addAndMakeVisible (del);

            up.setEnabled (i > 0);
            down.setEnabled (i + 1 < (int) owner.chainSize());
            update();
        }

        void update()
        {
            const bool off = owner.eng.isFxOff (owner.track, plugin);
            bypass.setButtonText (off ? "OFF" : "ON");
            bypass.setColour (TextButton::buttonColourId, off ? Colour (0xff7a2a2a) : Colour (0xff2e9d4f));
            bypass.setColour (TextButton::textColourOffId, Colours::white);
            name.setColour (Label::textColourId, off ? Colours::grey : Colours::white);
            repaint();
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (3, 3);
            bypass.setBounds (r.removeFromLeft (44));
            r.removeFromLeft (4);
            del.setBounds (r.removeFromRight (26));
            open.setBounds (r.removeFromRight (52));
            down.setBounds (r.removeFromRight (26));
            up.setBounds (r.removeFromRight (26));
            name.setBounds (r);
        }

        void paint (Graphics& g) override
        {
            const bool off = owner.eng.isFxOff (owner.track, plugin);
            g.setColour (dragging ? Colour (0xff3d5a80) : (off ? Colour (0xff262626) : Colour (0xff303a30)));
            g.fillRoundedRectangle (getLocalBounds().reduced (1).toFloat(), 4.0f);
        }

        void mouseDown (const MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { owner.showRowMenu (index); return; }
            dragging = false;
        }

        void mouseDrag (const MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 4) return;
            if (! dragging) { dragging = true; repaint(); }
            owner.dragTo (e.getEventRelativeTo (&owner.body).y);
        }

        void mouseUp (const MouseEvent& e) override
        {
            if (dragging) { dragging = false; owner.dropFrom (index); }
            juce::ignoreUnused (e);
        }

        FxChainPanel& owner;
        int index;
        AudioPluginInstance* plugin;
        bool dragging = false;
        TextButton bypass, up, down, open, del;
        Label name;
    };

    size_t chainSize() { return eng.pluginsOf (track).size(); }

    void timerCallback() override
    {
        auto& list = eng.pluginsOf (track);
        bool same = known.size() == list.size();
        for (size_t i = 0; same && i < known.size(); ++i)
            same = known[i] == list[i].get();

        if (! same) rebuild();
        else refreshState();
    }

    void refreshState()
    {
        for (auto& r : rows) r->update();
        const bool all = eng.allFxOff (track);
        btnAll.setToggleState (all, dontSendNotification);
        btnAll.setButtonText (all ? T ("Encender todos") : T ("Bypass All"));
    }

    void rebuild()
    {
        rows.clear();
        known.clear();
        int i = 0;
        for (auto& p : eng.pluginsOf (track))
        {
            known.push_back (p.get());
            auto row = std::make_unique<Row> (*this, i++, *p);
            body.addAndMakeVisible (*row);
            rows.push_back (std::move (row));
        }
        layoutRows();
        refreshState();
    }

    void layoutRows()
    {
        const int w = jmax (100, vp.getWidth() - vp.getScrollBarThickness());
        for (size_t i = 0; i < rows.size(); ++i)
            rows[i]->setBounds (0, (int) i * FxBody::rowH, w, FxBody::rowH);
        body.setSize (w, jmax (FxBody::rowH, (int) rows.size() * FxBody::rowH));
    }

    void dragTo (int bodyY)
    {
        body.dropLine = jlimit (0, (int) rows.size(), (bodyY + FxBody::rowH / 2) / FxBody::rowH);
        body.repaint();
    }

    void dropFrom (int from)
    {
        const int line = body.dropLine;
        body.dropLine = -1;
        body.repaint();
        if (line < 0) return;
        const int to = line > from ? line - 1 : line;
        if (to != from) eng.movePlugin (track, from, to);      // la lista se redibuja sola (timer)
    }

    void showRowMenu (int idx)
    {
        PopupMenu m;
        m.addItem (1, T ("Reemplazar..."));
        m.addItem (2, T ("Abrir"));
        m.addItem (3, T ("Subir"), idx > 0);
        m.addItem (4, T ("Bajar"), idx + 1 < (int) rows.size());
        m.addSeparator();
        m.addItem (5, T ("Quitar"));

        SafePointer<FxChainPanel> sp (this);
        m.showMenuAsync (PopupMenu::Options(), [sp, idx] (int r)
        {
            auto* self = sp.getComponent();
            if (self == nullptr || r == 0 || idx >= (int) self->known.size()) return;
            if (r == 1 && self->cbs.replace) self->cbs.replace (idx);
            else if (r == 2 && self->cbs.open) self->cbs.open (*self->eng.pluginsOf (self->track)[(size_t) idx]);
            else if (r == 3) self->eng.movePlugin (self->track, idx, idx - 1);
            else if (r == 4) self->eng.movePlugin (self->track, idx, idx + 1);
            else if (r == 5 && self->cbs.remove) self->cbs.remove (idx);
        });
    }

    Engine& eng;
    Track* track;
    FxCallbacks cbs;
    std::vector<AudioPluginInstance*> known;
    TextButton btnAll, btnAdd, btnAuto;
    Label hint;
    std::vector<std::unique_ptr<Row>> rows;      // antes que 'body' y 'vp'
    FxBody body;
    Viewport vp;
};

// ---------------------------------------------------------------------------
// Ventana de exportar (WAV / MP3, calidad, destino, rango, Master o pistas por separado)
// ---------------------------------------------------------------------------
class ExportDialog : public Component
{
public:
    ExportDialog (Engine::ExportSettings init, bool hasSelection, std::function<void (const Engine::ExportSettings&)> goFn)
        : st (std::move (init)), go (std::move (goFn))
    {
        auto lab = [this] (Label& l, const String& t) { l.setText (t, dontSendNotification); addAndMakeVisible (l); };
        lab (lFmt, T ("Formato"));  lab (lQ, T ("Calidad")); lab (lSr, T ("Frecuencia")); lab (lRange, T ("Rango"));
        lab (lMode, T ("Contenido")); lab (lName, T ("Nombre")); lab (lFolder, T ("Carpeta"));

        fmt.addItem ("WAV", 1);
        fmt.addItem ("MP3", 2);
        fmt.setSelectedId (st.mp3 ? 2 : 1, dontSendNotification);
        fmt.onChange = [this] { fillQuality (true); };
        addAndMakeVisible (fmt);

        addAndMakeVisible (quality);
        fillQuality (false);

        sr.addItem ("44100 Hz", 44100);
        sr.addItem ("48000 Hz", 48000);
        sr.setSelectedId (st.sampleRate == 48000 ? 48000 : 44100, dontSendNotification);
        addAndMakeVisible (sr);

        range.addItem (T ("Toda la canción"), 1);
        range.addItem (T ("Selección de tiempo"), 2);
        range.setItemEnabled (2, hasSelection);
        range.setSelectedId (st.useSelection && hasSelection ? 2 : 1, dontSendNotification);
        addAndMakeVisible (range);

        mode.addItem (T ("Mezcla final (Master)"), 1);
        mode.addItem (T ("Pista por pista (stems)"), 2);
        mode.setSelectedId (st.stems ? 2 : 1, dontSendNotification);
        addAndMakeVisible (mode);

        name.setText (st.baseName, dontSendNotification);
        addAndMakeVisible (name);
        folder.setText (st.folder.getFullPathName(), dontSendNotification);
        addAndMakeVisible (folder);

        browse.setButtonText ("...");
        browse.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> (T ("Carpeta de destino"), File (folder.getText()));
            SafePointer<ExportDialog> sp (this);
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                                  [sp] (const FileChooser& fc)
            {
                if (auto* self = sp.getComponent())
                    if (fc.getResult() != File()) self->folder.setText (fc.getResult().getFullPathName(), dontSendNotification);
            });
        };
        addAndMakeVisible (browse);

        info.setText (T ("Incluye efectos, buses, envíos y automatización. Se añaden 2 s de cola al final."), dontSendNotification);
        info.setFont (Font (FontOptions (11.5f)));
        info.setColour (Label::textColourId, Colours::grey);
        addAndMakeVisible (info);

        ok.setButtonText (T ("Exportar"));
        ok.onClick = [this] { accept(); };
        addAndMakeVisible (ok);

        cancel.setButtonText (T ("Cancelar"));
        cancel.onClick = [this] { close(); };
        addAndMakeVisible (cancel);

        setSize (460, 330);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 12);
        auto row = [&] (Label& l, Component& c)
        {
            auto line = r.removeFromTop (28);
            l.setBounds (line.removeFromLeft (90));
            c.setBounds (line);
            r.removeFromTop (6);
        };
        row (lFmt, fmt); row (lQ, quality); row (lSr, sr); row (lRange, range); row (lMode, mode); row (lName, name);

        auto line = r.removeFromTop (28);
        lFolder.setBounds (line.removeFromLeft (90));
        browse.setBounds (line.removeFromRight (34));
        line.removeFromRight (4);
        folder.setBounds (line);
        r.removeFromTop (6);

        info.setBounds (r.removeFromTop (34));
        auto btns = r.removeFromBottom (30);
        cancel.setBounds (btns.removeFromRight (100));
        btns.removeFromRight (8);
        ok.setBounds (btns.removeFromRight (100));
    }

    void paint (Graphics& g) override { g.fillAll (Colour (0xff262626)); }

private:
    void fillQuality (bool resetSel)
    {
        const bool mp3 = fmt.getSelectedId() == 2;
        quality.clear (dontSendNotification);
        if (mp3)
        {
            for (int k : { 128, 192, 256, 320 }) quality.addItem (String (k) + " kbps", k);
            quality.setSelectedId (resetSel || ! (st.kbps == 128 || st.kbps == 192 || st.kbps == 256 || st.kbps == 320) ? 192 : st.kbps,
                                   dontSendNotification);
        }
        else
        {
            quality.addItem ("16 bits", 16);
            quality.addItem ("24 bits", 24);
            quality.setSelectedId (resetSel ? 24 : (st.bits == 16 ? 16 : 24), dontSendNotification);
        }
    }

    void accept()
    {
        Engine::ExportSettings s = st;
        s.mp3 = fmt.getSelectedId() == 2;
        if (s.mp3) s.kbps = quality.getSelectedId(); else s.bits = quality.getSelectedId();
        s.sampleRate = sr.getSelectedId();
        s.useSelection = range.getSelectedId() == 2;
        s.stems = mode.getSelectedId() == 2;
        s.baseName = name.getText().trim();
        s.folder = File (folder.getText().trim());

        if (s.baseName.isEmpty() || folder.getText().trim().isEmpty())
        {
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, T ("Exportar"),
                                              T ("Escribe un nombre y elige una carpeta de destino."));
            return;
        }
        auto cb = go;
        close();
        if (cb) cb (s);
    }

    void close()
    {
        if (auto* dw = findParentComponentOfClass<DialogWindow>()) dw->exitModalState (0);
    }

    Engine::ExportSettings st;
    std::function<void (const Engine::ExportSettings&)> go;
    Label lFmt, lQ, lSr, lRange, lMode, lName, lFolder, info;
    ComboBox fmt, quality, sr, range, mode;
    TextEditor name, folder;
    TextButton browse, ok, cancel;
    std::unique_ptr<FileChooser> chooser;
};

// Exporta en un hilo aparte con ventana de progreso (barra + botón Cancelar)
class ExportJob : public ThreadWithProgressWindow
{
public:
    ExportJob (Engine& e, Engine::ExportSettings s, std::function<void (bool, StringArray, String)> doneFn)
        : ThreadWithProgressWindow (T ("Exportando..."), true, true, 5000, T ("Cancelar")),
          eng (e), st (std::move (s)), done (std::move (doneFn)) {}

    void run() override
    {
        setStatusMessage (T ("Procesando la mezcla (efectos, buses, envíos y automatización)..."));
        ok = eng.renderExport (st, [this] (double p) { setProgress (jlimit (0.0, 1.0, p)); return ! threadShouldExit(); }, files, err);
    }

    void threadComplete (bool) override { if (done) done (ok, files, err); }

private:
    Engine& eng;
    Engine::ExportSettings st;
    std::function<void (bool, StringArray, String)> done;
    bool ok = false;
    StringArray files;
    String err;
};

// Línea naranja que marca dónde caerá la pista que se está moviendo
class TrackDropMarker : public Component
{
public:
    TrackDropMarker() { setInterceptsMouseClicks (false, false); }
    void paint (Graphics& g) override { g.fillAll (Colour (0xffd08a2e)); }
};

// Cuadro de selección (arrastrar con el botón izquierdo en un espacio vacío): rectángulo naranja translúcido
class MarqueeOverlay : public Component
{
public:
    MarqueeOverlay() { setInterceptsMouseClicks (false, false); }

    void setRect (Rectangle<int> r)
    {
        const auto dirty = rect.getUnion (r).expanded (3);
        rect = r;
        repaint (dirty);
    }

    void paint (Graphics& g) override
    {
        if (rect.isEmpty()) return;
        g.setColour (Colour (0xffd08a2e).withAlpha (0.18f));
        g.fillRect (rect);
        g.setColour (Colour (0xffd08a2e).withAlpha (0.95f));
        g.drawRect (rect, 1);
    }

private:
    Rectangle<int> rect;
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
        mk (bAddBus, T ("+ Bus de envío"), [this] { addNewTrack (true); });
        mk (bMixer, "Mezclador", [this] { showMixer(); });
        mk (bImport, "Importar", [this] { importAudio(); });
        mk (bZoomOut, "-", [this] { setZoom (pxPerSec / 1.25); });
        mk (bZoomIn, "+", [this] { setZoom (pxPerSec * 1.25); });
        mk (bAudio, "Audio", [this] { showAudioSettings(); });
        mk (bScan, "Escanear plugins", [this] { scanPlugins(); });
        mk (bReport, "Ver plugins", [this] { showReport(); });
        mk (bOpen, "Abrir", [this] { openProject(); });
        mk (bSave, "Guardar", [this] { saveProjectQuick(); });
        mk (bMasterFx, T ("FX Master"), [this] { showMasterFxMenu(); });
        mk (bExport, "Exportar", [this] { showExportDialog (false); });

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
        content.addAndMakeVisible (marquee);
        content.addAndMakeVisible (playhead);

        setWantsKeyboardFocus (true);
        layoutContent();
        startTimerHz (30);
    }

    ~MainComponent() override
    {
        stopTimer();
        envWindow.reset();
        fxWindow.reset();
        mixerWindow.reset();
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
        selClips.clear();
        if (c != nullptr) selClips.push_back (c);
        repaintRows();
    }

    bool isClipSelected (const Clip* c) const override
    {
        return std::find (selClips.begin(), selClips.end(), c) != selClips.end();
    }

    int selectionCount() const override { return (int) selClips.size(); }

    // --- mover varios clips a la vez (selección con cuadro) ---
    void groupDragBegin() override
    {
        groupOrig.clear();
        for (auto* c : selClips) groupOrig.push_back ({ c, c->startSample.load() });
    }

    void groupDragTo (int64 delta) override
    {
        int64 minStart = std::numeric_limits<int64>::max();
        for (auto& go : groupOrig) minStart = jmin (minStart, go.second);
        delta = jmax (delta, -minStart);                  // ninguno pasa de la posición 0
        for (auto& go : groupOrig) go.first->startSample = go.second + delta;
        repaintRows();
    }

    void groupDragEnd() override
    {
        engine.undoEnd();         // si no se movió nada, no queda paso de deshacer
        clipsChanged();
    }

    // --- cuadro de selección: arrastra con el izquierdo en un espacio vacío de la pista ---
    void marqueeBegin (Point<int> screenPos) override
    {
        marqueeOrigin = content.getLocalPoint (nullptr, screenPos);
        marquee.setRect ({});
    }

    void marqueeDrag (Point<int> screenPos) override
    {
        const auto p = content.getLocalPoint (nullptr, screenPos);
        const auto r = Rectangle<int>::leftTopRightBottom (jmin (marqueeOrigin.x, p.x), jmin (marqueeOrigin.y, p.y),
                                                           jmax (marqueeOrigin.x, p.x), jmax (marqueeOrigin.y, p.y));
        marquee.setRect (r);
        selectInRect (r);
    }

    void marqueeEnd() override { marquee.setRect ({}); }      // la selección se queda marcada en naranja

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

        // bus nuevo: se desplaza hasta él y se deja el nombre listo para escribir (Reverb, Delay, Coro...)
        if (isBus && ! rows.empty())
        {
            auto& rw = *rows.back();
            viewport.setViewPosition (viewport.getViewPositionX(), jmax (0, rw.getY() - 40));
            rw.beginRename();
        }
    }

    void showMixer()
    {
        if (mixerWindow != nullptr) { mixerWindow->toFront (true); return; }

        SafePointer<MainComponent> sp (this);
        auto sendFn = [sp] (Track& t, Component& src) { if (auto* self = sp.getComponent()) self->showSendPanel (t, src); };
        auto masterFn = [sp] (double db) { if (auto* self = sp.getComponent()) self->master.setValue (db, dontSendNotification); };
        auto addFn = [sp] { if (auto* self = sp.getComponent()) self->addNewTrack (true); };

        mixerWindow = std::make_unique<EnvelopeWindow> (T ("Mezclador"), new MixerPanel (engine, sendFn, masterFn, addFn), [sp]
        {
            MessageManager::callAsync ([sp]
            {
                if (auto* self = sp.getComponent()) self->mixerWindow.reset();
            });
        });
    }

    void removeTrack (Track& t) override
    {
        envWindow.reset();
        fxWindow.reset();
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

    void showFxMenu (Track& t) override { openFxChain (&t); }

    void timeSelectionChanged() override { selOverlay.repaint(); }

    // ------------------- Ventana de FX, selector de plugins, exportar -------------------
    File jsfxFolderFile() const
    {
        return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("LaPautaStudio").getChildFile ("jsfx_folder.txt");
    }

    // Lista de plugins para añadir o reemplazar: VST3 por fabricante y JSFX (archivo suelto o carpeta).
    // Al elegir uno se crea la instancia y se entrega a 'onPicked'. Un JSFX con error muestra la línea y no se carga.
    void pickPlugin (std::function<void (std::unique_ptr<AudioPluginInstance>)> onPicked)
    {
        PopupMenu m, vst, js;
        auto types = engine.host.list.getTypes();

        std::map<String, PopupMenu> byVendor;
        for (int i = 0; i < types.size(); ++i)
            if (! types[i].isInstrument)
                byVendor[types[i].manufacturerName].addItem (1000 + i, types[i].name);
        for (auto& kv : byVendor)
            vst.addSubMenu (kv.first.isEmpty() ? String ("Otros") : kv.first, kv.second);
        if (byVendor.empty())
            vst.addItem (1, T ("(primero pulsa 'Escanear plugins')"), false);

        js.addItem (400, T ("Cargar archivo .jsfx..."));
        js.addItem (401, T ("Elegir carpeta de JSFX..."));

        Array<File> found;
        const File folder (jsfxFolderFile().loadFileAsString().trim());
        if (folder.isDirectory())
        {
            found = folder.findChildFiles (File::findFiles, true, "*.jsfx");
            found.sort();
            if (found.size() > 300) found.removeRange (300, found.size() - 300);
            if (found.size() > 0) js.addSeparator();
            for (int i = 0; i < found.size(); ++i)
                js.addItem (100000 + i, found[i].getRelativePathFrom (folder));
        }

        m.addSubMenu ("VST3", vst);
        m.addSubMenu ("JSFX", js);

        SafePointer<MainComponent> sp (this);
        m.showMenuAsync (PopupMenu::Options(), [sp, types, found, onPicked] (int r)
        {
            auto* self = sp.getComponent();
            if (self == nullptr || r == 0) return;

            if (r >= 100000)
            {
                if (r - 100000 < found.size()) self->loadJsfx (found[r - 100000], onPicked);
            }
            else if (r == 400)
            {
                self->chooser = std::make_unique<FileChooser> (T ("Elegir un archivo JSFX"), File(), "*.jsfx");
                self->chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                            [sp, onPicked] (const FileChooser& fc)
                {
                    auto* s2 = sp.getComponent();
                    if (s2 != nullptr && fc.getResult().existsAsFile()) s2->loadJsfx (fc.getResult(), onPicked);
                });
            }
            else if (r == 401)
            {
                self->chooser = std::make_unique<FileChooser> (T ("Carpeta con archivos .jsfx"));
                self->chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                                            [sp, onPicked] (const FileChooser& fc)
                {
                    auto* s2 = sp.getComponent();
                    if (s2 == nullptr || ! fc.getResult().isDirectory()) return;
                    s2->jsfxFolderFile().getParentDirectory().createDirectory();
                    s2->jsfxFolderFile().replaceWithText (fc.getResult().getFullPathName());
                    MessageManager::callAsync ([sp, onPicked] { if (auto* s3 = sp.getComponent()) s3->pickPlugin (onPicked); });
                });
            }
            else if (r >= 1000)
            {
                String err;
                auto inst = self->engine.host.create (types[r - 1000], self->engine.sampleRate, self->engine.maxBlock, err);
                if (inst == nullptr)
                {
                    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Plugin", T ("No se pudo cargar:\n") + err);
                    return;
                }
                onPicked (std::move (inst));
            }
        });
    }

    void loadJsfx (const File& f, const std::function<void (std::unique_ptr<AudioPluginInstance>)>& onPicked)
    {
        String err;
        auto inst = JsfxProcessor::create (f, engine.sampleRate, engine.maxBlock, err);
        if (inst == nullptr)
        {
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "JSFX", err);
            return;
        }
        onPicked (std::move (inst));
    }

    void addToChain (Track* t, std::unique_ptr<AudioPluginInstance> inst)
    {
        if (t != nullptr && ! engine.hasTrack (t)) return;
        envWindow.reset();
        AudioPluginInstance* raw = nullptr;
        if (t != nullptr) { engine.addPlugin (*t, std::move (inst)); raw = t->plugins.back().get(); }
        else { engine.addMasterPlugin (std::move (inst)); raw = engine.masterPlugins.back().get(); }
        clipsChanged();
        openEditor (*raw);
    }

    // Reemplazar: el nuevo plugin ocupa el mismo lugar y conserva el bypass; los demás no se tocan
    void replaceInChain (Track* t, int idx, std::unique_ptr<AudioPluginInstance> inst)
    {
        if (t != nullptr && ! engine.hasTrack (t)) return;
        auto& list = engine.pluginsOf (t);
        if (! isPositiveAndBelow (idx, (int) list.size())) return;

        envWindow.reset();
        closeWindowFor (list[(size_t) idx].get());
        if (auto* np = engine.replacePlugin (t, idx, std::move (inst)))
        {
            clipsChanged();
            openEditor (*np);
        }
    }

    void removeFromChain (Track* t, int idx)
    {
        if (t != nullptr && ! engine.hasTrack (t)) return;
        auto& list = engine.pluginsOf (t);
        if (! isPositiveAndBelow (idx, (int) list.size())) return;

        envWindow.reset();
        closeWindowFor (list[(size_t) idx].get());
        if (t != nullptr) engine.removePlugin (*t, idx); else engine.removeMasterPlugin (idx);
        clipsChanged();
    }

    // Abre la ventana de FX de una pista (nullptr = Master)
    void openFxChain (Track* t)
    {
        if (t != nullptr && ! engine.hasTrack (t)) return;
        if (fxWindow != nullptr && fxTarget == t) { fxWindow->toFront (true); return; }
        fxWindow.reset();
        fxTarget = t;

        SafePointer<MainComponent> sp (this);
        FxCallbacks cb;
        cb.open = [sp] (AudioPluginInstance& p) { if (auto* s = sp.getComponent()) s->openEditor (p); };
        cb.add = [sp, t]
        {
            if (auto* s = sp.getComponent())
                s->pickPlugin ([sp, t] (std::unique_ptr<AudioPluginInstance> inst)
                {
                    if (auto* s2 = sp.getComponent()) s2->addToChain (t, std::move (inst));
                });
        };
        cb.replace = [sp, t] (int idx)
        {
            if (auto* s = sp.getComponent())
                s->pickPlugin ([sp, t, idx] (std::unique_ptr<AudioPluginInstance> inst)
                {
                    if (auto* s2 = sp.getComponent()) s2->replaceInChain (t, idx, std::move (inst));
                });
        };
        cb.remove = [sp, t] (int idx)
        {
            MessageManager::callAsync ([sp, t, idx] { if (auto* s = sp.getComponent()) s->removeFromChain (t, idx); });
        };
        cb.automation = [sp, t] { if (auto* s = sp.getComponent()) s->showEnvelopePanel (t); };

        const String title = t != nullptr ? T ("FX - ") + t->name : T ("FX - Master");
        fxWindow = std::make_unique<EnvelopeWindow> (title, new FxChainPanel (engine, t, cb), [sp]
        {
            MessageManager::callAsync ([sp] { if (auto* s = sp.getComponent()) s->fxWindow.reset(); });
        });
        fxWindow->setAlwaysOnTop (false);
    }

    // Ventana de exportar -> render con barra de progreso -> aviso al terminar
    void showExportDialog (bool preferSelection)
    {
        if (exportJob != nullptr) return;
        if (engine.recording.load())
        {
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, T ("Exportar"), T ("Detén la grabación antes de exportar."));
            return;
        }

        const bool hasSel = engine.selEnd.load() > engine.selStart.load();
        auto init = lastExport;
        init.useSelection = preferSelection && hasSel;
        if (init.folder == File()) init.folder = engine.exportDir();
        if (currentProject != File()) init.baseName = currentProject.getFileNameWithoutExtension();

        SafePointer<MainComponent> sp (this);
        auto go = [sp] (const Engine::ExportSettings& s)
        {
            MessageManager::callAsync ([sp, s] { if (auto* self = sp.getComponent()) self->startExport (s); });
        };

        DialogWindow::LaunchOptions o;
        o.content.setOwned (new ExportDialog (init, hasSel, go));
        o.dialogTitle = T ("Exportar canción");
        o.dialogBackgroundColour = Colour (0xff262626);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }

    void startExport (const Engine::ExportSettings& s)
    {
        lastExport = s;
        if (engine.playing) doStop();
        envWindow.reset();

        SafePointer<MainComponent> sp (this);
        exportJob = std::make_unique<ExportJob> (engine, s, [sp, s] (bool ok, StringArray files, String err)
        {
            MessageManager::callAsync ([sp, s, ok, files, err]
            {
                auto* self = sp.getComponent();
                if (self == nullptr) return;
                self->exportJob.reset();

                if (ok)
                {
                    self->setStatus (T ("Exportación terminada"));
                    const File folder = s.folder;
                    AlertWindow::showOkCancelBox (MessageBoxIconType::InfoIcon, T ("Exportación terminada"),
                                                  T ("Archivos creados:\n") + files.joinIntoString ("\n"),
                                                  T ("Abrir carpeta"), T ("Cerrar"), nullptr,
                                                  ModalCallbackFunction::create ([folder] (int r) { if (r == 1) folder.revealToUser(); }));
                }
                else if (err == T ("Exportación cancelada"))
                    self->setStatus (err);
                else
                    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, T ("Exportar"), err);
            });
        });
        exportJob->launchThread();
    }

    // Menú de la selección de tiempo: sacar el audio a otro DAW y traerlo de vuelta al proyecto
    void showRangeMenu (Track& t) override
    {
        const int64 a = engine.selStart.load(), b = engine.selEnd.load();
        if (b <= a) return;

        PopupMenu m;
        m.addSectionHeader (T ("Selección: ") + String ((double) (b - a) / engine.sampleRate, 2) + " s");
        m.addItem (1, T ("Exportar esta pista (selección) a WAV para otro DAW"), ! t.isBus.load());
        m.addItem (2, T ("Exportar todas las pistas (selección) en un solo WAV"));
        m.addItem (6, T ("Exportar la selección a MP3 / WAV con efectos..."));
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
            else if (r == 6)
            {
                self->showExportDialog (true);
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
        PopupMenu m;
        m.addItem (1000, T ("Añadir Input FX (VST3 / JSFX)..."));
        m.addSeparator();
        for (int i = 0; i < (int) t.inputPlugins.size(); ++i)
            m.addItem (100 + i, T ("Abrir: ") + t.inputPlugins[(size_t) i]->getName());
        for (int i = 0; i < (int) t.inputPlugins.size(); ++i)
            m.addItem (200 + i, T ("Quitar: ") + t.inputPlugins[(size_t) i]->getName());
        m.addSeparator();
        m.addItem (2, T ("(la pista debe estar armada con R; activa Monitor para oírte)"), false);

        SafePointer<MainComponent> sp (this);
        Track* tp = &t;
        m.showMenuAsync (PopupMenu::Options(), [sp, tp] (int r)
        {
            auto* self = sp.getComponent();
            if (self == nullptr || r == 0 || ! self->engine.hasTrack (tp)) return;

            if (r == 1000)
            {
                self->pickPlugin ([sp, tp] (std::unique_ptr<AudioPluginInstance> inst)
                {
                    auto* s2 = sp.getComponent();
                    if (s2 == nullptr || ! s2->engine.hasTrack (tp)) return;
                    s2->engine.addInputPlugin (*tp, std::move (inst));
                    s2->openEditor (*tp->inputPlugins.back());
                });
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

    // FX del Master: misma ventana de FX que las pistas
    void showMasterFxMenu() { openFxChain (nullptr); }

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

        for (auto* b : { &bOpen, &bSave, &bExport, &bAudio, &bScan, &bReport })
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

        for (auto* b : { &bAdd, &bAddBus, &bMixer, &bImport, &bZoomOut, &bZoomIn })
        {
            b->setBounds (bar2.removeFromLeft ((b == &bZoomOut || b == &bZoomIn) ? 34 : (b == &bAddBus ? 112 : 76)));
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

        // Ctrl+B = copia automática del audio seleccionado, pegada justo después del original en la misma pista
        if ((mods.isCtrlDown() || mods.isCommandDown()) && (k.getKeyCode() == 'B' || k.getKeyCode() == 'b'))
        {
            duplicateSelectedClip();
            return true;
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

    // ---- cambiar el orden de pistas y buses arrastrando su cabecera ----
    int dropIndexAt (int screenY) const
    {
        const int y = content.getLocalPoint (nullptr, Point<int> (0, screenY)).y;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const int top = rows[i]->getY(), h = rows[i]->getHeight();
            if (y < top + h / 2) return (int) i;
        }
        return (int) rows.size();
    }

    void trackDragMove (Track& t, int screenY) override
    {
        juce::ignoreUnused (t);
        trackDropIdx = dropIndexAt (screenY);

        int y = ui::rulerH;
        if (trackDropIdx < (int) rows.size()) y = rows[(size_t) trackDropIdx]->getY();
        else if (! rows.empty()) y = rows.back()->getBottom();

        if (trackDropMarker.getParentComponent() == nullptr) content.addAndMakeVisible (trackDropMarker);
        trackDropMarker.setBounds (0, y - 1, content.getWidth(), 3);
        trackDropMarker.toFront (false);

        // desplazamiento automático al acercarse al borde de la lista
        const auto vb = viewport.getScreenBounds();
        if (screenY < vb.getY() + 24)            viewport.setViewPosition (viewport.getViewPositionX(), jmax (0, viewport.getViewPositionY() - 18));
        else if (screenY > vb.getBottom() - 24)  viewport.setViewPosition (viewport.getViewPositionX(), viewport.getViewPositionY() + 18);
    }

    void trackDragEnd (Track& t, bool drop) override
    {
        trackDropMarker.setVisible (false);
        content.removeChildComponent (&trackDropMarker);
        trackDropMarker.setVisible (true);
        const int to0 = trackDropIdx;
        trackDropIdx = -1;
        if (! drop || to0 < 0) return;

        int from = -1;
        for (size_t i = 0; i < rows.size(); ++i)
            if (&rows[i]->getTrack() == &t) { from = (int) i; break; }
        if (from < 0) return;

        const int to = to0 > from ? to0 - 1 : to0;
        if (to == from) return;

        engine.moveTrack (from, to);
        auto row = std::move (rows[(size_t) from]);
        rows.erase (rows.begin() + from);
        rows.insert (rows.begin() + to, std::move (row));
        layoutContent();
        repaintRows();
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
        marquee.setBounds (content.getLocalBounds());
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
        selClips.erase (std::remove_if (selClips.begin(), selClips.end(), [this] (Clip* c) { return trackOf (c) == nullptr; }),
                        selClips.end());
        if (selClip != nullptr && trackOf (selClip) == nullptr) { selClip = nullptr; selTrack = nullptr; }
        if (selClip == nullptr && ! selClips.empty()) { selClip = selClips.front(); selTrack = trackOf (selClip); }
    }

    // pista a la que pertenece un clip (nullptr si ya no existe)
    Track* trackOf (Clip* want) const
    {
        for (auto& t : engine.tracks)
            for (auto& c : t->clips)
                if (c.get() == want) return t.get();
        return nullptr;
    }

    // Selecciona todos los clips que toca el rectángulo (en coordenadas del contenido)
    void selectInRect (Rectangle<int> r)
    {
        selClips.clear();
        selClip = nullptr;
        selTrack = nullptr;

        for (auto& row : rows)
        {
            const int y0 = row->getY() + 4, y1 = row->getY() + row->baseH() - 4;
            if (r.getBottom() < y0 || r.getY() > y1) continue;

            for (auto& c : row->getTrack().clips)
            {
                const int x0 = ui::headerW + (int) ((double) c->startSample.load() / engine.sampleRate * pxPerSec);
                const int x1 = x0 + jmax (2, (int) ((double) c->audio.getNumSamples() / engine.sampleRate * pxPerSec));
                if (r.getRight() < x0 || r.getX() > x1) continue;

                selClips.push_back (c.get());
                if (selClip == nullptr) { selClip = c.get(); selTrack = &row->getTrack(); }
            }
        }
        repaintRows();
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

    // Ctrl+B: copia el/los audio(s) seleccionado(s) justo después del grupo, cada uno en su pista
    void duplicateSelectedClip()
    {
        validateSelection();
        if (selClips.empty())
        {
            setStatus (T ("Selecciona un audio (clic sobre él) y pulsa Ctrl+B para copiarlo"));
            return;
        }

        int64 minStart = std::numeric_limits<int64>::max(), maxEnd = 0;
        for (auto* c : selClips)
        {
            minStart = jmin (minStart, c->startSample.load());
            maxEnd = jmax (maxEnd, c->startSample.load() + (int64) c->audio.getNumSamples());
        }
        const int64 shift = maxEnd - minStart;

        engine.undoBegin();                  // todas las copias = un solo paso de Ctrl+Z
        ++engine.undoSuspend;
        std::vector<Clip*> copies;
        for (auto* c : selClips)
            if (auto* t = trackOf (c))
                if (auto* cp = engine.duplicateClip (*t, c, c->startSample.load() + shift, false))
                    copies.push_back (cp);
        --engine.undoSuspend;

        selClips = copies;                   // las copias quedan seleccionadas: Ctrl+B otra vez sigue copiando
        selClip = copies.empty() ? nullptr : copies.front();
        selTrack = selClip != nullptr ? trackOf (selClip) : nullptr;
        clipsChanged();
        setStatus (copies.size() > 1 ? T ("Audios copiados") : T ("Audio copiado"));
    }

    void deleteSelectedClip()
    {
        validateSelection();
        if (selClips.empty()) return;

        engine.undoBegin();                  // borrar varios = un solo paso de Ctrl+Z
        ++engine.undoSuspend;
        for (auto* c : selClips)
            if (auto* t = trackOf (c))
                engine.removeClip (*t, c);
        --engine.undoSuspend;

        setSelected (nullptr, nullptr);
        clipsChanged();
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
        selClips.clear();
        envWindow.reset();
        fxWindow.reset();
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

    static bool isJsfxFile (const String& path) { return File (path).hasFileExtension ("jsfx"); }

    bool isInterestedInFileDrag (const StringArray& files) override
    {
        for (auto& p : files)
            if (isAudioFile (p) || isJsfxFile (p)) return true;
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

        if (items.empty()) return;        // solo JSFX: no se crean pistas
        dropOverlay.setInfo (std::move (items), addArea.getY() - viewport.getViewPositionY(),
                             ui::headerW - viewport.getViewPositionX()
                                 + (int) ((double) engine.startPos.load() / engine.sampleRate * pxPerSec),
                             pxPerSec, -viewport.getViewPositionX());
        dropOverlay.setVisible (true);
        dropOverlay.toFront (false);
    }

    void fileDragExit (const StringArray&) override { dropOverlay.setVisible (false); }

    // Audio -> pista nueva. Archivo .jsfx -> se añade como plugin a la pista sobre la que se suelta
    // (o a la pista seleccionada); si tiene un error se avisa con la línea y no se carga.
    void filesDropped (const StringArray& files, int x, int y) override
    {
        dropOverlay.setVisible (false);
        importDroppedFiles (files);

        Track* target = trackAtScreenY (localPointToGlobal (Point<int> (x, y)).getY());
        if (target == nullptr && selTrack != nullptr && engine.hasTrack (selTrack) && ! selTrack->isBus.load()) target = selTrack;

        for (auto& p : files)
            if (isJsfxFile (p))
            {
                if (target == nullptr)
                {
                    AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "JSFX",
                        T ("Suelta el archivo .jsfx sobre una pista (o selecciona una pista primero)."));
                    break;
                }
                SafePointer<MainComponent> sp (this);
                Track* tp = target;
                loadJsfx (File (p), [sp, tp] (std::unique_ptr<AudioPluginInstance> inst)
                {
                    if (auto* self = sp.getComponent()) self->addToChain (tp, std::move (inst));
                });
            }
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
            fxWindow.reset();
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
        void* ownerHandle = getTopLevelComponent() != nullptr ? getTopLevelComponent()->getWindowHandle() : nullptr;
        windows.push_back (std::make_unique<PluginWindow> (p, [sp] (PluginWindow* w)
        {
            MessageManager::callAsync ([sp, w]
            {
                if (auto* self = sp.getComponent()) self->closeWindow (w);
            });
        }, ownerHandle, [sp]
        {
            if (auto* self = sp.getComponent())
            {
                if (self->engine.playing) self->doStop(); else self->engine.play();
            }
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
    std::unique_ptr<EnvelopeWindow> mixerWindow;         // mezclador abierto (si hay)
    std::unique_ptr<EnvelopeWindow> fxWindow;            // ventana de FX abierta (si hay)
    Track* fxTarget = nullptr;                           // de qué pista (nullptr = Master)
    std::unique_ptr<ExportJob> exportJob;
    Engine::ExportSettings lastExport;

    TimelineContent content;
    TrackDropMarker trackDropMarker;
    int trackDropIdx = -1;
    AddTrackArea addArea;
    MasterRow masterRow { engine, *this };
    SelectionOverlay selOverlay { engine, [this] { return pxPerSec; } };
    LoopOverlay loopOverlay { engine, [this] { return pxPerSec; } };
    MarqueeOverlay marquee;
    Playhead playhead;
    Viewport viewport;
    BottomBar bottomBar { engine };
    DropOverlay dropOverlay;
    bool wasPlaying = false;

    TextButton bHome, bPlay, bStop, bRec, bAdd, bAddBus, bMixer, bImport, bZoomOut, bZoomIn, bAudio, bScan, bReport, bOpen, bSave;
    TextButton bSelect, bHand, bCut, bLoop, bMasterFx, bExport;
    Label timeLabel, bpmLabel, gridLabel, statusLabel;
    Slider master, bpmSlider;
    ComboBox gridBox;

    int tool = ui::toolSelect;
    Track* selTrack = nullptr;
    Clip* selClip = nullptr;                              // el clip principal de la selección
    std::vector<Clip*> selClips;                          // todos los clips seleccionados (uno o varios con el cuadro)
    std::vector<std::pair<Clip*, int64>> groupOrig;       // posición inicial de cada clip al mover el grupo
    Point<int> marqueeOrigin;
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
