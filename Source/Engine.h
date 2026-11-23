#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <memory>
#include "PluginHost.h"

using namespace juce;

// ---------------------------------------------------------------------------
// Clip: un trozo de audio en la línea de tiempo (estéreo, ya remuestreado).
// ---------------------------------------------------------------------------
struct Clip
{
    String name;
    File file;
    AudioBuffer<float> audio;                 // siempre 2 canales
    std::atomic<int64> startSample { 0 };
    std::atomic<int64> srcOffset { 0 };       // muestra del archivo original donde empieza este clip (recorte/división)
    std::atomic<int> fadeInSamples { 0 };     // duración del fade in (0 = sin fade)
    std::atomic<int> fadeOutSamples { 0 };    // duración del fade out (0 = sin fade)
    std::unique_ptr<AudioThumbnail> thumb;
};

// ---------------------------------------------------------------------------
// Automatización: una envolvente es una curva de puntos (valores normalizados 0..1)
// que controla volumen, pan, mute, un parámetro de un plugin o su bypass.
// ---------------------------------------------------------------------------
enum EnvTarget { envVolume = 0, envPan = 1, envMute = 2, envParam = 3, envBypass = 4 };

// Escala del volumen en las envolventes: -60 dB .. +6 dB (centro de la curva en -12 dB, como los faders)
inline NormalisableRange<double> volumeRange()
{
    NormalisableRange<double> r (-60.0, 6.0);
    r.setSkewForCentre (-12.0);
    return r;
}

inline float envToGain (float norm)
{
    return Decibels::decibelsToGain ((float) volumeRange().convertFrom0to1 ((double) norm), -60.0f);
}

struct EnvPoint
{
    int64 sample = 0;
    float value = 0.0f;
};

struct Envelope
{
    int target = envVolume;
    AudioPluginInstance* plugin = nullptr;        // solo para envParam / envBypass
    AudioProcessorParameter* param = nullptr;     // solo para envParam
    int paramIndex = -1;
    String name;

    std::atomic<bool> active { true };            // si está desactivada, se ignora (los puntos se conservan)
    bool visible = false;                         // solo interfaz: mostrar la franja bajo la pista
    std::vector<EnvPoint> points;                 // ordenados por muestra
    float lastApplied = -1.0f;                    // solo hilo de audio: último valor enviado al plugin

    // Mute y bypass son de tipo escalón (encendido/apagado); el resto interpola en línea recta.
    bool isStep() const { return target == envMute || target == envBypass; }

    bool hasEffect() const { return active.load() && ! points.empty(); }

    float valueAt (int64 s) const
    {
        const size_t n = points.size();
        if (n == 0) return 0.0f;
        if (s <= points.front().sample) return points.front().value;
        if (s >= points.back().sample)  return points.back().value;

        size_t lo = 0, hi = n - 1;                    // lo: último punto con muestra <= s
        while (hi - lo > 1)
        {
            const size_t mid = (lo + hi) / 2;
            if (points[mid].sample <= s) lo = mid; else hi = mid;
        }

        const auto& a = points[lo];
        const auto& b = points[hi];
        if (isStep()) return a.value;

        const double t = (double) (s - a.sample) / (double) jmax<int64> (1, b.sample - a.sample);
        return (float) (a.value + (b.value - a.value) * t);
    }
};

using PluginList = std::vector<std::unique_ptr<AudioPluginInstance>>;
using EnvList = std::vector<std::unique_ptr<Envelope>>;

// ---------------------------------------------------------------------------
// Track: pista con clips, cadena de plugins, volumen, pan, mute/solo/armado.
// ---------------------------------------------------------------------------
struct Track
{
    String name { "Pista" };
    std::vector<std::unique_ptr<Clip>> clips;
    std::vector<std::unique_ptr<AudioPluginInstance>> plugins;
    EnvList envs;                                      // automatización de la pista y de sus plugins
    float lastGL = -1.0f, lastGR = -1.0f;              // solo hilo de audio: ganancias del bloque anterior (rampa)

    std::atomic<float> gain { 1.0f }, pan { 0.0f }, peakL { 0.0f }, peakR { 0.0f };
    std::atomic<bool> mute { false }, solo { false }, armed { false };
    std::atomic<int> inputChannel { 0 };

    // Input FX: efectos que se aplican a la entrada de la tarjeta ANTES de grabar (la grabación sale con ellos)
    std::vector<std::unique_ptr<AudioPluginInstance>> inputPlugins;
    std::atomic<bool> monitor { false };               // escuchar la entrada (con Input FX) mientras la pista está armada
    AudioBuffer<float> inBuf;                          // 8 canales de trabajo para la entrada

    // Buses y envíos
    std::atomic<bool> isBus { false };                 // true = pista de bus (recibe audio de otras pistas)
    std::atomic<Track*> outputBus { nullptr };         // a qué bus va la salida (nullptr = Master)

    // Envíos post-fader: una pista o un bus puede enviar a cualquier número de buses (un envío por bus destino).
    // La lista solo se cambia con el lock del Engine tomado; el nivel es atómico (se mueve sin lock).
    struct Send
    {
        std::atomic<Track*> target { nullptr };
        std::atomic<float> level { 0.0f };
    };
    std::vector<std::unique_ptr<Send>> sends;
    int busRank = -1;                                  // orden de proceso del bus (solo con el lock): un bus solo envía a rangos mayores

    AudioBuffer<float> buf;                   // 8 canales de trabajo
    std::unique_ptr<AudioFormatWriter::ThreadedWriter> writer;
    File recFile;
};

// ---------------------------------------------------------------------------
// Engine: dispositivo de audio (WASAPI / DirectSound / ASIO), transporte,
// mezcla, grabación, plugins y proyecto.
// ---------------------------------------------------------------------------
class Engine : public AudioIODeviceCallback
{
public:
    Engine() : thumbCache (32), writerThread ("La Pauta Writer")
    {
        formats.registerBasicFormats();   // WAV, AIFF, FLAC, OGG, MP3 (lectura) ...
        writerThread.startThread();
        mix.setSize (8, maxBlock);   // 2 canales de salida + canales extra que algunos plugins piden
    }

    ~Engine() override
    {
        deviceManager.removeAudioCallback (this);
        writerThread.stopThread (2000);
    }

    // Devuelve "" si todo bien, o el error del dispositivo.
    String start()
    {
        auto err = deviceManager.initialiseWithDefaultDevices (8, 2);
        deviceManager.addAudioCallback (this);
        return err;
    }

    // ------------------------------ Pistas ---------------------------------
    Track* addTrack (const String& name, bool bus = false)
    {
        auto t = std::make_unique<Track>();
        t->name = name;
        t->isBus = bus;
        t->buf.setSize (8, maxBlock);
        t->inBuf.setSize (8, maxBlock);
        auto* raw = t.get();
        const SpinLock::ScopedLockType sl (lock);
        tracks.push_back (std::move (t));
        rebuildBusOrder();
        return raw;
    }

    void removeTrack (Track* t)
    {
        clearUndo();      // los pasos guardados apuntan a clips y automatizaciones de esta pista
        std::unique_ptr<Track> dead;
        {
            const SpinLock::ScopedLockType sl (lock);
            for (size_t i = 0; i < tracks.size(); ++i)
                if (tracks[i].get() == t)
                {
                    // quitar cualquier ruta o envío que apunte a la pista eliminada
                    for (auto& o : tracks)
                    {
                        if (o->outputBus.load() == t) o->outputBus = nullptr;
                        for (size_t k = o->sends.size(); k-- > 0;)
                            if (o->sends[k]->target.load() == t)
                                o->sends.erase (o->sends.begin() + (long) k);
                    }
                    dead = std::move (tracks[i]);
                    tracks.erase (tracks.begin() + (long) i);
                    rebuildBusOrder();
                    break;
                }
        }
        // 'dead' se destruye aquí, fuera del hilo de audio y fuera del lock.
    }

    bool hasTrack (Track* t) const
    {
        for (auto& x : tracks) if (x.get() == t) return true;
        return false;
    }

    int indexOf (Track* t) const
    {
        for (size_t i = 0; i < tracks.size(); ++i)
            if (tracks[i].get() == t) return (int) i;
        return -1;
    }

    std::vector<Track*> busList() const
    {
        std::vector<Track*> v;
        for (auto& t : tracks) if (t->isBus.load()) v.push_back (t.get());
        return v;
    }

    // ------------------------ Envíos entre pistas y buses -------------------
    // ¿Puede 'from' enviar a 'to'? No si 'to' no es un bus, si es el mismo, o si 'to' ya le envía (directa o
    // indirectamente) a 'from': eso sería un lazo de realimentación infinito.
    bool canSend (const Track* from, const Track* to) const
    {
        if (from == nullptr || to == nullptr || from == to || ! to->isBus.load()) return false;
        return ! reaches (to, from);
    }

    // Nivel (ganancia lineal) del envío from -> to; 0 si no hay envío.
    float getSendLevel (const Track* from, const Track* to) const
    {
        for (auto& s : from->sends)
            if (s->target.load() == to) return s->level.load();
        return 0.0f;
    }

    // Pone el nivel del envío from -> to (0 = quitar el envío). Devuelve false si no está permitido (lazo).
    bool setSendLevel (Track* from, Track* to, float level)
    {
        if (level <= 0.0f)
        {
            const SpinLock::ScopedLockType sl (lock);
            for (size_t k = from->sends.size(); k-- > 0;)
                if (from->sends[k]->target.load() == to)
                    from->sends.erase (from->sends.begin() + (long) k);
            rebuildBusOrder();
            return true;
        }

        if (! canSend (from, to)) return false;

        for (auto& s : from->sends)
            if (s->target.load() == to) { s->level = level; return true; }   // ya existe: solo el nivel (sin lock)

        auto ns = std::make_unique<Track::Send>();
        ns->target = to;
        ns->level = level;
        const SpinLock::ScopedLockType sl (lock);
        from->sends.push_back (std::move (ns));
        rebuildBusOrder();
        return true;
    }

    // ¿Hay camino de envíos de 'a' hasta 'b'? (siguiendo solo buses)
    bool reaches (const Track* a, const Track* b, int depth = 0) const
    {
        if (a == b) return true;
        if (depth > 64) return true;                  // red absurdamente profunda: se trata como lazo
        for (auto& s : a->sends)
            if (auto* nx = s->target.load())
                if (reaches (nx, b, depth + 1)) return true;
        return false;
    }

    // Ordena los buses para que cada uno se procese DESPUÉS de todos los que le envían (con el lock tomado).
    void rebuildBusOrder()
    {
        std::vector<Track*> pending, order;
        for (auto& t : tracks)
        {
            t->busRank = -1;
            if (t->isBus.load()) pending.push_back (t.get());
        }

        while (! pending.empty())
        {
            bool progressed = false;
            for (size_t i = 0; i < pending.size();)
            {
                Track* cand = pending[i];
                bool fed = false;
                for (auto* o : pending)
                    if (o != cand)
                        for (auto& s : o->sends)
                            if (s->target.load() == cand) fed = true;

                if (! fed)
                {
                    order.push_back (cand);
                    pending.erase (pending.begin() + (long) i);
                    progressed = true;
                }
                else ++i;
            }
            if (! progressed) { for (auto* p : pending) order.push_back (p); break; }   // lazo (no debería pasar)
        }

        for (size_t i = 0; i < order.size(); ++i) order[i]->busRank = (int) i;
        busOrder = std::move (order);
    }

    // ------------------------------ Clips ----------------------------------
    std::unique_ptr<Clip> makeClip (const String& name, const File& f, AudioBuffer<float> data, int64 start)
    {
        auto c = std::make_unique<Clip>();
        c->name = name;
        c->file = f;
        c->audio = std::move (data);
        c->startSample = start;
        c->thumb = std::make_unique<AudioThumbnail> (256, formats, thumbCache);
        auto hash = (int64) reinterpret_cast<std::uintptr_t> (c.get());
        c->thumb->setSource (&c->audio, sampleRate, hash);
        return c;
    }

    // Carga el archivo completo en RAM (v0.1). Devuelve nullptr si falla o es muy grande.
    std::unique_ptr<Clip> loadClip (const File& f)
    {
        std::unique_ptr<AudioFormatReader> r (formats.createReaderFor (f));
        if (r == nullptr || r->lengthInSamples <= 0) return nullptr;
        if (r->lengthInSamples * 8 > (int64) 400 * 1024 * 1024) return nullptr;   // tope 400 MB

        const int n = (int) r->lengthInSamples;
        AudioBuffer<float> src (2, n);
        r->read (&src, 0, n, 0, true, true);
        if (r->numChannels == 1)
            src.copyFrom (1, 0, src, 0, 0, n);

        AudioBuffer<float> out;
        if (std::abs (r->sampleRate - sampleRate) > 0.5)
        {
            const double ratio = r->sampleRate / sampleRate;
            const int m = jmax (1, (int) (n / ratio));
            out.setSize (2, m);
            for (int ch = 0; ch < 2; ++ch)
            {
                LagrangeInterpolator li;
                li.process (ratio, src.getReadPointer (ch), out.getWritePointer (ch), m);
            }
        }
        else
        {
            out = std::move (src);
        }
        return makeClip (f.getFileNameWithoutExtension(), f, std::move (out), 0);
    }

    // Copia el tramo [f0, f1) del archivo (posiciones del archivo) a un clip nuevo que empieza en 'start'.
    // 'origin' = posición del archivo donde empieza el buffer de 'src'.
    std::unique_ptr<Clip> makeSub (const Clip& src, int64 origin, int64 f0, int64 f1, int64 start)
    {
        const int n = (int) (f1 - f0);
        AudioBuffer<float> b (2, n);
        for (int ch = 0; ch < 2; ++ch)
            b.copyFrom (ch, 0, src.audio, ch, (int) (f0 - origin), n);
        auto c = makeClip (src.name, src.file, std::move (b), start);
        c->srcOffset = f0;
        return c;
    }

    // Recorta los bordes de un clip: el audio nuevo va de 'newStart' a 'newEnd' (muestras de la línea de tiempo).
    // Se puede volver a alargar: lo que sobra se recupera del archivo original. Devuelve el clip resultante.
    Clip* trimClip (Track& t, Clip* c, int64 newStart, int64 newEnd)
    {
        const int64 s = c->startSample.load();
        const int total = c->audio.getNumSamples();
        const int64 off = c->srcOffset.load();

        int64 f0 = jmax<int64> (0, off + (newStart - s));
        int64 f1 = off + (newEnd - s);

        std::unique_ptr<Clip> full;
        const Clip* srcClip = c;
        int64 origin = off;
        if ((f0 < off || f1 > off + total) && c->file.existsAsFile())
        {
            full = loadClip (c->file);                 // para alargar hay que volver al archivo completo
            if (full != nullptr) { srcClip = full.get(); origin = 0; }
        }

        f0 = jmax (f0, origin);
        f1 = jmin (f1, origin + (int64) srcClip->audio.getNumSamples());
        if (f1 - f0 < 256) return c;
        if (f0 == off && f1 == off + total) return c;

        auto nc = makeSub (*srcClip, origin, f0, f1, s + (f0 - off));
        const int newLen = (int) (f1 - f0);
        nc->fadeInSamples  = f0 == off ? jmin (c->fadeInSamples.load(), newLen) : 0;
        nc->fadeOutSamples = f1 == off + total ? jmin (c->fadeOutSamples.load(), newLen) : 0;
        Clip* result = nc.get();

        std::unique_ptr<Clip> dead;
        {
            const SpinLock::ScopedLockType sl (lock);
            for (size_t i = 0; i < t.clips.size(); ++i)
                if (t.clips[i].get() == c)
                {
                    dead = std::move (t.clips[i]);
                    t.clips[i] = std::move (nc);
                    break;
                }
        }
        retire (std::move (dead));
        return result;
    }

    // ------------------------- Deshacer / Rehacer (Ctrl+Z / Ctrl+Y) -------------------------
    // Cada acción guarda una "foto" del arreglo antes de cambiarlo: qué clips hay en cada pista (con posición y
    // fades) y los puntos de las automatizaciones. Los clips borrados o reemplazados (dividir, recortar) no se
    // destruyen: esperan en el "cementerio" mientras haya un paso de deshacer que los necesite.
    // No cubre: fader/pan/mute/solo, añadir o quitar pistas ni plugins (quitar una pista o un plugin borra el historial).
    // Todo esto corre solo en el hilo de la interfaz.
    struct ClipState  { Clip* clip; int64 start; int fadeIn; int fadeOut; };
    struct EnvState   { Envelope* env; std::vector<EnvPoint> points; bool active; bool visible; };
    struct TrackState { Track* track; std::vector<ClipState> clips; std::vector<EnvState> envs; };   // track == nullptr = Master
    using Snapshot = std::vector<TrackState>;

    // Empieza una acción deshacible (llamar ANTES de cambiar algo). Si es un gesto con arrastre, termina con undoEnd().
    void undoBegin()
    {
        if (undoSuspend > 0) return;
        pendingRedo = std::move (redoStack);
        redoStack.clear();
        undoStack.push_back (captureState());
        if (undoStack.size() > 100) undoStack.erase (undoStack.begin());
        purgeGraveyard();
    }

    // Fin de un gesto: si no cambió nada (un clic sin mover), se descarta el paso y se conserva el rehacer.
    void undoEnd()
    {
        if (! undoStack.empty() && sameState (undoStack.back(), captureState()))
        {
            undoStack.pop_back();
            if (! pendingRedo.empty()) redoStack = std::move (pendingRedo);
        }
        pendingRedo.clear();
    }

    bool undo()
    {
        if (undoStack.empty() || recording.load()) return false;
        redoStack.push_back (captureState());
        auto s = std::move (undoStack.back());
        undoStack.pop_back();
        applyState (s);
        purgeGraveyard();
        return true;
    }

    bool redo()
    {
        if (redoStack.empty() || recording.load()) return false;
        undoStack.push_back (captureState());
        auto s = std::move (redoStack.back());
        redoStack.pop_back();
        applyState (s);
        purgeGraveyard();
        return true;
    }

    void clearUndo()
    {
        undoStack.clear();
        redoStack.clear();
        pendingRedo.clear();
        graveyard.clear();
    }

    int undoSuspend = 0;   // > 0 mientras se carga un proyecto (no guardar pasos)

private:
    std::vector<Snapshot> undoStack, redoStack, pendingRedo;
    std::vector<std::unique_ptr<Clip>> graveyard;

    static void captureEnvs (std::vector<EnvState>& out, const EnvList& list)
    {
        for (auto& e : list)
            out.push_back ({ e.get(), e->points, e->active.load(), e->visible });
    }

    Snapshot captureState() const
    {
        Snapshot s;
        for (auto& t : tracks)
        {
            TrackState ts;
            ts.track = t.get();
            for (auto& c : t->clips)
                ts.clips.push_back ({ c.get(), c->startSample.load(), c->fadeInSamples.load(), c->fadeOutSamples.load() });
            captureEnvs (ts.envs, t->envs);
            s.push_back (std::move (ts));
        }
        TrackState ms;
        ms.track = nullptr;
        captureEnvs (ms.envs, masterEnvs);
        s.push_back (std::move (ms));
        return s;
    }

    static bool sameState (const Snapshot& a, const Snapshot& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            auto& x = a[i];
            auto& y = b[i];
            if (x.track != y.track || x.clips.size() != y.clips.size() || x.envs.size() != y.envs.size()) return false;

            for (size_t k = 0; k < x.clips.size(); ++k)
            {
                auto& p = x.clips[k];
                auto& q = y.clips[k];
                if (p.clip != q.clip || p.start != q.start || p.fadeIn != q.fadeIn || p.fadeOut != q.fadeOut) return false;
            }
            for (size_t k = 0; k < x.envs.size(); ++k)
            {
                auto& p = x.envs[k];
                auto& q = y.envs[k];
                if (p.env != q.env || p.active != q.active || p.points.size() != q.points.size())
                    return false;
                for (size_t j = 0; j < p.points.size(); ++j)
                    if (p.points[j].sample != q.points[j].sample || p.points[j].value != q.points[j].value) return false;
            }
        }
        return true;
    }

    void applyState (const Snapshot& s)
    {
        const SpinLock::ScopedLockType sl (lock);

        // 1) se sacan los clips de las pistas de la foto; 2) se devuelven en el orden y posición de la foto;
        // 3) los que sobran (no estaban en la foto) van al cementerio
        std::vector<std::unique_ptr<Clip>> pool;
        for (auto& ts : s)
            if (ts.track != nullptr && hasTrack (ts.track))
            {
                for (auto& c : ts.track->clips) pool.push_back (std::move (c));
                ts.track->clips.clear();
            }

        auto takeFrom = [] (std::vector<std::unique_ptr<Clip>>& v, Clip* p) -> std::unique_ptr<Clip>
        {
            for (size_t i = 0; i < v.size(); ++i)
                if (v[i].get() == p)
                {
                    auto r = std::move (v[i]);
                    v.erase (v.begin() + (long) i);
                    return r;
                }
            return nullptr;
        };

        for (auto& ts : s)
        {
            if (ts.track == nullptr || ! hasTrack (ts.track)) continue;
            for (auto& cs : ts.clips)
            {
                auto c = takeFrom (pool, cs.clip);
                if (c == nullptr) c = takeFrom (graveyard, cs.clip);
                for (size_t i = 0; c == nullptr && i < tracks.size(); ++i)      // por si quedó en otra pista
                    c = takeFrom (tracks[i]->clips, cs.clip);
                if (c == nullptr) continue;

                c->startSample = cs.start;
                c->fadeInSamples = cs.fadeIn;
                c->fadeOutSamples = cs.fadeOut;
                ts.track->clips.push_back (std::move (c));
            }
        }
        for (auto& c : pool) graveyard.push_back (std::move (c));

        auto restoreEnvs = [] (const std::vector<EnvState>& states, EnvList& list)
        {
            for (auto& es : states)
                for (auto& e : list)
                    if (e.get() == es.env)
                    {
                        e->points = es.points;
                        e->active = es.active;               // (visible es solo de pantalla: no se deshace)
                        e->lastApplied = -1.0f;      // fuerza a reenviar el valor al plugin
                        break;
                    }
        };
        for (auto& ts : s)
        {
            if (ts.track == nullptr)            restoreEnvs (ts.envs, masterEnvs);
            else if (hasTrack (ts.track))       restoreEnvs (ts.envs, ts.track->envs);
        }
    }

    // Libera los clips del cementerio que ningún paso de deshacer/rehacer necesita ya
    void purgeGraveyard()
    {
        if (graveyard.empty()) return;

        std::vector<const Clip*> keep;
        for (auto* st : { &undoStack, &redoStack, &pendingRedo })
            for (auto& snap : *st)
                for (auto& ts : snap)
                    for (auto& cs : ts.clips)
                        keep.push_back (cs.clip);

        for (size_t i = graveyard.size(); i-- > 0;)
            if (std::find (keep.begin(), keep.end(), graveyard[i].get()) == keep.end())
                graveyard.erase (graveyard.begin() + (long) i);
    }

    // Un clip que sale de una pista (borrado, dividido, recortado) espera en el cementerio
    void retire (std::unique_ptr<Clip> c)
    {
        if (c == nullptr) return;
        graveyard.push_back (std::move (c));
        purgeGraveyard();
    }

public:
    // ------------------- Exportar un tramo (para trabajarlo en otro DAW) -------------------
    File exportDir() const
    {
        auto d = File::getSpecialLocation (File::userDocumentsDirectory)
                     .getChildFile ("La Pauta Studio").getChildFile ("Exportados");
        d.createDirectory();
        return d;
    }

    // Escribe en un WAV el audio de los clips entre s0 y s1 (muestras de la línea de tiempo).
    // 'only' = una sola pista; nullptr = suma de todas las pistas no silenciadas. Va sin efectos ni fader.
    // Devuelve el archivo, o File() con el motivo en 'err'.
    File exportRange (Track* only, int64 s0, int64 s1, const String& baseName, String& err)
    {
        if (s1 <= s0) { err = "La selección está vacía"; return {}; }

        auto used = [&] (const Track& t)
        {
            if (t.isBus.load()) return false;
            if (only != nullptr) return &t == only;
            return ! t.mute.load();
        };

        bool any = false;
        for (auto& t : tracks)
            if (used (*t))
                for (auto& c : t->clips)
                    if (c->startSample.load() < s1 && c->startSample.load() + c->audio.getNumSamples() > s0)
                        any = true;
        if (! any) { err = "No hay audio dentro de la selección"; return {}; }

        auto file = exportDir().getNonexistentChildFile (File::createLegalFileName (baseName) + "_"
                                                          + Time::getCurrentTime().formatted ("%H%M%S"), ".wav");
        std::unique_ptr<FileOutputStream> fos (file.createOutputStream());
        if (fos == nullptr) { err = "No se pudo crear el archivo en " + file.getParentDirectory().getFullPathName(); return {}; }

        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (fos.get(), sampleRate, 2, only != nullptr ? 24 : 32, {}, 0));
        if (w == nullptr) { err = "No se pudo preparar el archivo WAV"; return {}; }
        fos.release();   // el writer es dueño del stream

        constexpr int chunk = 8192;
        AudioBuffer<float> tmp (2, chunk);
        for (int64 p = s0; p < s1; p += chunk)
        {
            const int n = (int) jmin<int64> (chunk, s1 - p);
            tmp.clear();
            for (auto& t : tracks)
                if (used (*t))
                    for (auto& c : t->clips)
                        mixClip (*c, tmp, p, 0, n);
            w->writeFromAudioSampleBuffer (tmp, 0, n);
        }
        w.reset();   // cierra el archivo
        return file;
    }

    // Copia de un clip (mismo audio, fades y tramo del archivo) que empieza en 'newStart'. Devuelve la copia.
    // recordUndo = true: la copia es un paso de Ctrl+Z (el gesto del ratón lo abre él mismo con undoBegin).
    Clip* duplicateClip (Track& t, Clip* c, int64 newStart, bool recordUndo)
    {
        if (recordUndo) undoBegin();

        AudioBuffer<float> b (c->audio);                 // copia del audio
        auto nc = makeClip (c->name, c->file, std::move (b), jmax<int64> (0, newStart));
        nc->srcOffset = c->srcOffset.load();
        nc->fadeInSamples = c->fadeInSamples.load();
        nc->fadeOutSamples = c->fadeOutSamples.load();
        Clip* result = nc.get();

        const SpinLock::ScopedLockType sl (lock);
        t.clips.push_back (std::move (nc));
        return result;
    }

    // Tras cancelar una copia con undo(): se recupera el rehacer que había antes del gesto
    void dropRedo() { redoStack = std::move (pendingRedo); pendingRedo.clear(); }

    // Pasa un clip de una pista a otra (misma posición en el tiempo)
    void moveClip (Track& from, Track& to, Clip* c)
    {
        if (&from == &to) return;
        const SpinLock::ScopedLockType sl (lock);
        for (size_t i = 0; i < from.clips.size(); ++i)
            if (from.clips[i].get() == c)
            {
                to.clips.push_back (std::move (from.clips[i]));
                from.clips.erase (from.clips.begin() + (long) i);
                return;
            }
    }

    void addClip (Track& t, std::unique_ptr<Clip> c)
    {
        undoBegin();
        const SpinLock::ScopedLockType sl (lock);
        t.clips.push_back (std::move (c));
    }

    void removeClip (Track& t, Clip* c)
    {
        undoBegin();
        std::unique_ptr<Clip> dead;
        {
            const SpinLock::ScopedLockType sl (lock);
            for (size_t i = 0; i < t.clips.size(); ++i)
                if (t.clips[i].get() == c)
                {
                    dead = std::move (t.clips[i]);
                    t.clips.erase (t.clips.begin() + (long) i);
                    break;
                }
        }
        retire (std::move (dead));
    }

    void splitClip (Track& t, Clip* c, int64 at)
    {
        const int64 s = c->startSample.load();
        const int64 rel = at - s;
        const int total = c->audio.getNumSamples();
        if (rel <= 0 || rel >= total) return;
        undoBegin();

        AudioBuffer<float> a (2, (int) rel), b (2, total - (int) rel);
        for (int ch = 0; ch < 2; ++ch)
        {
            a.copyFrom (ch, 0, c->audio, ch, 0, (int) rel);
            b.copyFrom (ch, 0, c->audio, ch, (int) rel, total - (int) rel);
        }
        auto ca = makeClip (c->name, c->file, std::move (a), s);
        auto cb = makeClip (c->name, c->file, std::move (b), at);
        ca->srcOffset = c->srcOffset.load();
        cb->srcOffset = c->srcOffset.load() + rel;
        ca->fadeInSamples = jmin (c->fadeInSamples.load(), (int) rel);                       // cada mitad conserva su borde
        cb->fadeOutSamples = jmin (c->fadeOutSamples.load(), total - (int) rel);

        std::unique_ptr<Clip> dead;
        {
            const SpinLock::ScopedLockType sl (lock);
            for (size_t i = 0; i < t.clips.size(); ++i)
                if (t.clips[i].get() == c)
                {
                    dead = std::move (t.clips[i]);
                    t.clips.erase (t.clips.begin() + (long) i);
                    break;
                }
            t.clips.push_back (std::move (ca));
            t.clips.push_back (std::move (cb));
        }
        retire (std::move (dead));
    }

    // ----------------------------- Plugins ---------------------------------
    void addPlugin (Track& t, std::unique_ptr<AudioPluginInstance> p)
    {
        p->setRateAndBufferSizeDetails (sampleRate, maxBlock);
        p->prepareToPlay (sampleRate, maxBlock);
        const SpinLock::ScopedLockType sl (lock);
        t.plugins.push_back (std::move (p));
    }

    void removePlugin (Track& t, int index)
    {
        clearUndo();      // quitar un plugin borra sus automatizaciones
        std::unique_ptr<AudioPluginInstance> dead;
        {
            const SpinLock::ScopedLockType sl (lock);
            if (! isPositiveAndBelow (index, (int) t.plugins.size())) return;
            dropEnvelopesOf (t.envs, t.plugins[(size_t) index].get());
            dead = std::move (t.plugins[(size_t) index]);
            t.plugins.erase (t.plugins.begin() + index);
        }
        dead->releaseResources();
    }

    // ------------------------ Input FX (efectos de entrada) -----------------
    void addInputPlugin (Track& t, std::unique_ptr<AudioPluginInstance> p)
    {
        p->setRateAndBufferSizeDetails (sampleRate, maxBlock);
        p->prepareToPlay (sampleRate, maxBlock);
        const SpinLock::ScopedLockType sl (lock);
        t.inputPlugins.push_back (std::move (p));
    }

    void removeInputPlugin (Track& t, int index)
    {
        std::unique_ptr<AudioPluginInstance> dead;
        {
            const SpinLock::ScopedLockType sl (lock);
            if (! isPositiveAndBelow (index, (int) t.inputPlugins.size())) return;
            dead = std::move (t.inputPlugins[(size_t) index]);
            t.inputPlugins.erase (t.inputPlugins.begin() + index);
        }
        dead->releaseResources();
    }

    // ------------------------ Plugins del canal Master ----------------------
    void addMasterPlugin (std::unique_ptr<AudioPluginInstance> p)
    {
        p->setRateAndBufferSizeDetails (sampleRate, maxBlock);
        p->prepareToPlay (sampleRate, maxBlock);
        const SpinLock::ScopedLockType sl (lock);
        masterPlugins.push_back (std::move (p));
    }

    void removeMasterPlugin (int index)
    {
        clearUndo();
        std::unique_ptr<AudioPluginInstance> dead;
        {
            const SpinLock::ScopedLockType sl (lock);
            if (! isPositiveAndBelow (index, (int) masterPlugins.size())) return;
            dropEnvelopesOf (masterEnvs, masterPlugins[(size_t) index].get());
            dead = std::move (masterPlugins[(size_t) index]);
            masterPlugins.erase (masterPlugins.begin() + index);
        }
        dead->releaseResources();
    }

    // ------------------------------ Automatización --------------------------
    // 't' = nullptr significa el canal Master.
    EnvList& envsOf (Track* t)       { return t != nullptr ? t->envs : masterEnvs; }
    PluginList& pluginsOf (Track* t) { return t != nullptr ? t->plugins : masterPlugins; }

    static String envDisplayName (int target, AudioPluginInstance* plugin, AudioProcessorParameter* param)
    {
        const String fx = plugin != nullptr ? plugin->getName() : String ("FX");
        switch (target)
        {
            case envVolume: return "Volumen";
            case envPan:    return "Pan";
            case envMute:   return "Mute";
            case envBypass: return fx + " > Bypass";
            default:        return fx + " > " + (param != nullptr ? param->getName (40) : String ("?"));
        }
    }

    Envelope* findEnvelope (EnvList& list, int target, AudioPluginInstance* plugin, int paramIndex)
    {
        for (auto& e : list)
            if (e->target == target && e->plugin == plugin && e->paramIndex == paramIndex)
                return e.get();
        return nullptr;
    }

    // Devuelve la envolvente pedida; si no existe la crea (vacía). Puede devolver nullptr si el parámetro no existe.
    Envelope* getOrCreateEnvelope (EnvList& list, int target, AudioPluginInstance* plugin, int paramIndex)
    {
        if (auto* found = findEnvelope (list, target, plugin, paramIndex))
            return found;

        auto e = std::make_unique<Envelope>();
        e->target = target;
        e->plugin = plugin;
        e->paramIndex = paramIndex;

        if (target == envParam)
        {
            if (plugin == nullptr) return nullptr;
            auto& ps = plugin->getParameters();
            if (! isPositiveAndBelow (paramIndex, ps.size())) return nullptr;
            e->param = ps[paramIndex];
        }
        e->name = envDisplayName (target, plugin, e->param);

        auto* raw = e.get();
        const SpinLock::ScopedLockType sl (lock);
        list.push_back (std::move (e));
        return raw;
    }

    // Edición de puntos (siempre desde el hilo de la interfaz)
    void envAddPoint (Envelope& e, int64 s, float v)
    {
        s = jmax<int64> (0, s);
        v = jlimit (0.0f, 1.0f, v);

        const SpinLock::ScopedLockType sl (lock);
        size_t i = 0;
        while (i < e.points.size() && e.points[i].sample < s) ++i;

        if (i < e.points.size() && e.points[i].sample == s)
            e.points[i].value = v;
        else
            e.points.insert (e.points.begin() + (long) i, EnvPoint { s, v });
    }

    // Mueve un punto sin saltarse a sus vecinos (así la lista siempre queda ordenada)
    void envSetPoint (Envelope& e, int idx, int64 s, float v)
    {
        const SpinLock::ScopedLockType sl (lock);
        if (! isPositiveAndBelow (idx, (int) e.points.size())) return;

        const int64 lo = idx > 0 ? e.points[(size_t) idx - 1].sample + 1 : 0;
        const int64 hi = idx + 1 < (int) e.points.size() ? e.points[(size_t) idx + 1].sample - 1
                                                          : std::numeric_limits<int64>::max();
        e.points[(size_t) idx] = EnvPoint { jlimit (lo, jmax (lo, hi), s), jlimit (0.0f, 1.0f, v) };
    }

    void envRemovePoint (Envelope& e, int idx)
    {
        const SpinLock::ScopedLockType sl (lock);
        if (isPositiveAndBelow (idx, (int) e.points.size()))
            e.points.erase (e.points.begin() + idx);
    }

    void envClear (Envelope& e)
    {
        const SpinLock::ScopedLockType sl (lock);
        e.points.clear();
    }

    // Se llama con 'lock' tomado: quita las envolventes que apuntan a un plugin que se va a destruir
    static void dropEnvelopesOf (EnvList& list, AudioPluginInstance* p)
    {
        list.erase (std::remove_if (list.begin(), list.end(),
                                    [p] (const std::unique_ptr<Envelope>& e) { return e->plugin == p; }),
                    list.end());
    }

    // ----------------------------- Transporte ------------------------------
    // Posición de inicio: el punto donde el usuario puso el cursor estando detenido. Al parar (o al llegar
    // al final) el cursor siempre vuelve aquí.
    void play()
    {
        if (playing.load()) return;
        position = startPos.load();
        playing = true;
    }

    // El usuario mueve el cursor. Si está detenido, ese punto pasa a ser la nueva posición de inicio;
    // si está sonando, solo salta (la posición de inicio no cambia).
    void setCursor (int64 s)
    {
        s = jmax<int64> (0, s);
        position = s;
        if (! playing.load())
            startPos = s;
    }

    // Detiene y devuelve el cursor a la posición de inicio. Devuelve true si se grabó algo (para refrescar la UI).
    bool stop()
    {
        playing = false;
        const bool recorded = recording.load() ? stopRecording() : false;
        position = startPos.load();
        return recorded;
    }

    bool anyArmed() const
    {
        for (auto& t : tracks) if (t->armed) return true;
        return false;
    }

    bool startRecording()
    {
        if (! anyArmed()) return false;

        auto dir = File::getSpecialLocation (File::userDocumentsDirectory)
                       .getChildFile ("La Pauta Studio").getChildFile ("Grabaciones");
        dir.createDirectory();

        recStart = position.load();
        WavAudioFormat wav;

        for (auto& t : tracks)
        {
            if (! t->armed) continue;

            auto file = dir.getNonexistentChildFile (t->name + "_" + Time::getCurrentTime().formatted ("%H%M%S"), ".wav");
            std::unique_ptr<FileOutputStream> fos (file.createOutputStream());
            if (fos == nullptr) continue;

            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (fos.get(), sampleRate, 1, 24, {}, 0));
            if (w == nullptr) continue;
            fos.release();   // el writer es dueño del stream

            auto tw = std::make_unique<AudioFormatWriter::ThreadedWriter> (w.release(), writerThread, 32768);
            t->recFile = file;
            const SpinLock::ScopedLockType sl (lock);
            t->writer = std::move (tw);
        }

        recording = true;
        playing = true;
        return true;
    }

    bool stopRecording()
    {
        recording = false;

        std::vector<std::pair<Track*, std::unique_ptr<AudioFormatWriter::ThreadedWriter>>> done;
        {
            const SpinLock::ScopedLockType sl (lock);
            for (auto& t : tracks)
                if (t->writer != nullptr)
                    done.push_back ({ t.get(), std::move (t->writer) });
        }

        int64 latency = 0;
        if (auto* d = deviceManager.getCurrentAudioDevice())
            latency = d->getInputLatencyInSamples() + d->getOutputLatencyInSamples();

        bool any = false;
        for (auto& p : done)
        {
            p.second.reset();   // vacía y cierra el archivo
            if (auto clip = loadClip (p.first->recFile))
            {
                int64 fxLat = 0;                                           // latencia de los Input FX (lookahead, etc.)
                for (auto& ip : p.first->inputPlugins) fxLat += ip->getLatencySamples();
                clip->startSample = jmax<int64> (0, recStart - latency - fxLat);   // compensa latencia
                addClip (*p.first, std::move (clip));
                any = true;
            }
        }
        return any;
    }

    // ----------------------------- Proyecto --------------------------------
    bool saveProject (const File& f)
    {
        XmlElement root ("LAPAUTA_PROJECT");
        root.setAttribute ("master", (double) masterGain.load());
        root.setAttribute ("bpm", bpm.load());
        root.setAttribute ("grid", gridId);
        root.setAttribute ("loopOn", loopOn.load());
        root.setAttribute ("loopStart", String (loopStart.load()));
        root.setAttribute ("loopEnd", String (loopEnd.load()));

        for (auto& t : tracks)
        {
            auto* te = root.createNewChildElement ("TRACK");
            te->setAttribute ("name", t->name);
            te->setAttribute ("gain", (double) t->gain.load());
            te->setAttribute ("pan", (double) t->pan.load());
            te->setAttribute ("mute", t->mute.load());
            te->setAttribute ("solo", t->solo.load());
            te->setAttribute ("armed", t->armed.load());
            te->setAttribute ("input", t->inputChannel.load());
            te->setAttribute ("monitor", t->monitor.load());
            te->setAttribute ("bus", t->isBus.load());
            te->setAttribute ("out", indexOf (t->outputBus.load()));
            for (auto& sd : t->sends)
            {
                auto* se = te->createNewChildElement ("SEND");
                se->setAttribute ("to", indexOf (sd->target.load()));
                se->setAttribute ("level", (double) sd->level.load());
            }

            for (auto& c : t->clips)
            {
                auto* ce = te->createNewChildElement ("CLIP");
                ce->setAttribute ("file", c->file.getFullPathName());
                ce->setAttribute ("start", String (c->startSample.load()));
                ce->setAttribute ("offset", String (c->srcOffset.load()));
                ce->setAttribute ("len", c->audio.getNumSamples());
                ce->setAttribute ("fadein", c->fadeInSamples.load());
                ce->setAttribute ("fadeout", c->fadeOutSamples.load());
            }

            for (auto& p : t->plugins)
            {
                auto* fx = te->createNewChildElement ("FX");
                if (auto desc = p->getPluginDescription().createXml())
                    fx->addChildElement (desc.release());
                MemoryBlock mb;
                p->getStateInformation (mb);
                fx->setAttribute ("state", mb.toBase64Encoding());
            }

            saveEnvelopes (*te, t->envs, t->plugins);

            if (! t->inputPlugins.empty())
            {
                auto* ie = te->createNewChildElement ("INPUTFX");
                for (auto& p : t->inputPlugins)
                {
                    auto* fx = ie->createNewChildElement ("FX");
                    if (auto desc = p->getPluginDescription().createXml())
                        fx->addChildElement (desc.release());
                    MemoryBlock mb;
                    p->getStateInformation (mb);
                    fx->setAttribute ("state", mb.toBase64Encoding());
                }
            }
        }

        auto* menv = root.createNewChildElement ("MASTERENV");
        saveEnvelopes (*menv, masterEnvs, masterPlugins);

        auto* mfx = root.createNewChildElement ("MASTERFX");
        for (auto& p : masterPlugins)
        {
            auto* fx = mfx->createNewChildElement ("FX");
            if (auto desc = p->getPluginDescription().createXml())
                fx->addChildElement (desc.release());
            MemoryBlock mb;
            p->getStateInformation (mb);
            fx->setAttribute ("state", mb.toBase64Encoding());
        }

        return root.writeTo (f);
    }

    bool loadProject (const File& f, String& err)
    {
        auto xml = XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName ("LAPAUTA_PROJECT"))
        {
            err = "Archivo de proyecto no valido";
            return false;
        }

        playing = false;

        clearUndo();                       // proyecto nuevo = historial nuevo
        struct UndoOff                     // mientras carga no se guardan pasos; al terminar se limpia
        {
            Engine& e;
            explicit UndoOff (Engine& x) : e (x) { ++e.undoSuspend; }
            ~UndoOff() { --e.undoSuspend; e.clearUndo(); }
        } undoOff { *this };

        {
            std::vector<std::unique_ptr<Track>> old;
            {
                const SpinLock::ScopedLockType sl (lock);
                old.swap (tracks);
                busOrder.clear();      // apuntaba a las pistas viejas
            }
        }   // pistas viejas destruidas aquí

        {
            std::vector<std::unique_ptr<AudioPluginInstance>> oldFx;
            EnvList oldEnvs;
            {
                const SpinLock::ScopedLockType sl (lock);
                oldFx.swap (masterPlugins);
                oldEnvs.swap (masterEnvs);
            }
            for (auto& p : oldFx) p->releaseResources();
        }   // plugins viejos del master destruidos aquí

        masterGain = (float) xml->getDoubleAttribute ("master", 1.0);
        bpm = jlimit (20.0, 400.0, xml->getDoubleAttribute ("bpm", 120.0));
        gridId = xml->getIntAttribute ("grid", 4);
        loopStart = xml->getStringAttribute ("loopStart").getLargeIntValue();
        loopEnd = xml->getStringAttribute ("loopEnd").getLargeIntValue();
        loopOn = xml->getBoolAttribute ("loopOn", false);
        position = 0;
        startPos = 0;
        int missing = 0;

        std::vector<const XmlElement*> trackXml;

        for (auto* te : xml->getChildWithTagNameIterator ("TRACK"))
        {
            trackXml.push_back (te);
            auto* t = addTrack (te->getStringAttribute ("name", "Pista"), te->getBoolAttribute ("bus"));
            t->gain = (float) te->getDoubleAttribute ("gain", 1.0);
            t->pan = (float) te->getDoubleAttribute ("pan", 0.0);
            t->mute = te->getBoolAttribute ("mute");
            t->solo = te->getBoolAttribute ("solo");
            t->armed = te->getBoolAttribute ("armed");
            t->inputChannel = te->getIntAttribute ("input", 0);

            for (auto* ce : te->getChildWithTagNameIterator ("CLIP"))
            {
                if (auto c = loadClip (File (ce->getStringAttribute ("file"))))
                {
                    const int64 start = ce->getStringAttribute ("start").getLargeIntValue();
                    const int64 off = ce->getStringAttribute ("offset").getLargeIntValue();
                    const int64 len = ce->getIntAttribute ("len", 0);
                    const int64 fullLen = c->audio.getNumSamples();

                    // clip recortado o dividido: se queda solo con su tramo del archivo
                    if (len > 0 && off >= 0 && off + len <= fullLen && (off > 0 || len < fullLen))
                        c = makeSub (*c, 0, off, off + len, start);

                    c->startSample = start;
                    c->fadeInSamples = jmax (0, ce->getIntAttribute ("fadein", 0));
                    c->fadeOutSamples = jmax (0, ce->getIntAttribute ("fadeout", 0));
                    addClip (*t, std::move (c));
                }
                else ++missing;
            }

            std::vector<AudioPluginInstance*> orig;    // posición original del plugin -> instancia (nullptr si no cargó)
            for (auto* fx : te->getChildWithTagNameIterator ("FX"))
            {
                PluginDescription d;
                auto* de = fx->getChildByName ("PLUGIN");
                if (de == nullptr || ! d.loadFromXml (*de)) { orig.push_back (nullptr); ++missing; continue; }

                String perr;
                if (auto inst = host.create (d, sampleRate, maxBlock, perr))
                {
                    MemoryBlock mb;
                    if (mb.fromBase64Encoding (fx->getStringAttribute ("state")))
                        inst->setStateInformation (mb.getData(), (int) mb.getSize());
                    auto* raw = inst.get();
                    addPlugin (*t, std::move (inst));
                    orig.push_back (raw);
                }
                else { orig.push_back (nullptr); ++missing; }
            }

            loadEnvelopes (*te, t->envs, orig);

            t->monitor = te->getBoolAttribute ("monitor", false);
            if (auto* ie = te->getChildByName ("INPUTFX"))
                for (auto* fx : ie->getChildWithTagNameIterator ("FX"))
                {
                    PluginDescription d;
                    auto* de = fx->getChildByName ("PLUGIN");
                    if (de == nullptr || ! d.loadFromXml (*de)) { ++missing; continue; }

                    String perr;
                    if (auto inst = host.create (d, sampleRate, maxBlock, perr))
                    {
                        MemoryBlock mb;
                        if (mb.fromBase64Encoding (fx->getStringAttribute ("state")))
                            inst->setStateInformation (mb.getData(), (int) mb.getSize());
                        addInputPlugin (*t, std::move (inst));
                    }
                    else ++missing;
                }
        }

        // Reconectar salidas y envíos (ya existen todas las pistas)
        auto at = [this] (int idx) -> Track*
        {
            return isPositiveAndBelow (idx, (int) tracks.size()) ? tracks[(size_t) idx].get() : nullptr;
        };
        for (size_t i = 0; i < trackXml.size() && i < tracks.size(); ++i)
        {
            auto* te = trackXml[i];
            auto& t = *tracks[i];
            t.outputBus = at (te->getIntAttribute ("out", -1));
            // envíos nuevos (<SEND to= level=>) y, para proyectos viejos, send0/lvl0 y send1/lvl1
            for (auto* se : te->getChildWithTagNameIterator ("SEND"))
                setSendLevel (&t, at (se->getIntAttribute ("to", -1)), (float) se->getDoubleAttribute ("level", 0.0));
            for (int s = 0; s < 2; ++s)
                if (te->hasAttribute ("send" + String (s)))
                    setSendLevel (&t, at (te->getIntAttribute ("send" + String (s), -1)),
                                  (float) te->getDoubleAttribute ("lvl" + String (s), 0.0));
        }

        std::vector<AudioPluginInstance*> origMaster;
        if (auto* mfx = xml->getChildByName ("MASTERFX"))
            for (auto* fx : mfx->getChildWithTagNameIterator ("FX"))
            {
                PluginDescription d;
                auto* de = fx->getChildByName ("PLUGIN");
                if (de == nullptr || ! d.loadFromXml (*de)) { origMaster.push_back (nullptr); ++missing; continue; }

                String perr;
                if (auto inst = host.create (d, sampleRate, maxBlock, perr))
                {
                    MemoryBlock mb;
                    if (mb.fromBase64Encoding (fx->getStringAttribute ("state")))
                        inst->setStateInformation (mb.getData(), (int) mb.getSize());
                    auto* raw = inst.get();
                    addMasterPlugin (std::move (inst));
                    origMaster.push_back (raw);
                }
                else { origMaster.push_back (nullptr); ++missing; }
            }

        if (auto* menv = xml->getChildByName ("MASTERENV"))
            loadEnvelopes (*menv, masterEnvs, origMaster);

        if (missing > 0)
            err = String (missing) + " elemento(s) no se pudieron cargar (archivo o plugin faltante)";
        return true;
    }

    // ------------------------- Callback de audio ---------------------------
    void audioDeviceAboutToStart (AudioIODevice* d) override
    {
        const double sr = d->getCurrentSampleRate();
        const int bs = d->getCurrentBufferSizeSamples();

        const SpinLock::ScopedLockType sl (lock);
        sampleRate = sr;
        blockSize = bs;
        maxBlock = jmax (bs, 2048);
        mix.setSize (8, maxBlock);
        clickBuf.assign ((size_t) maxBlock, 0.0f);

        for (auto& p : masterPlugins)
        {
            p->setRateAndBufferSizeDetails (sr, maxBlock);
            p->prepareToPlay (sr, maxBlock);
        }

        for (auto& t : tracks)
        {
            t->buf.setSize (8, maxBlock);
            t->inBuf.setSize (8, maxBlock);
            for (auto& p : t->plugins)
            {
                p->setRateAndBufferSizeDetails (sr, maxBlock);
                p->prepareToPlay (sr, maxBlock);
            }
            for (auto& p : t->inputPlugins)
            {
                p->setRateAndBufferSizeDetails (sr, maxBlock);
                p->prepareToPlay (sr, maxBlock);
            }
        }
    }

    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn,
                                           float* const* out, int numOut,
                                           int n, const AudioIODeviceCallbackContext&) override
    {
        for (int c = 0; c < numOut; ++c)
            if (out[c] != nullptr)
                FloatVectorOperations::clear (out[c], n);

        SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || n > maxBlock) return;

        mix.clear (0, n);
        const bool isPlaying = playing.load();
        const int64 pos = position.load();
        const bool isRec = recording.load();

        // Bucle: si el bloque cruza el final del bucle, la parte que sobra se toma del inicio del bucle.
        const int64 lS = loopStart.load(), lE = loopEnd.load();
        const bool useLoop = loopOn.load() && ! isRec && (lE - lS) >= n && pos < lE && pos + n >= lE;
        const int len1 = useLoop ? (int) (lE - pos) : n;

        const bool clickOn = isPlaying && metronome.load() && (int) clickBuf.size() >= n;
        if (clickOn)
        {
            renderMetronome (pos, 0, len1);
            if (useLoop && len1 < n)
                renderMetronome (lS, len1, n - len1);
        }

        bool anySolo = false;
        for (auto& t : tracks) if (t->solo) anySolo = true;

        // Los buses se limpian primero; luego pasada 0 = pistas normales, pasada 1 = buses.
        for (auto& t : tracks)
            if (t->isBus.load()) t->buf.clear (0, n);

        // pasada 0 = pistas normales; pasada 1 = buses en orden (un bus se procesa después de los que le envían)
        const int nTracks = (int) tracks.size();
        const int nBuses = (int) busOrder.size();
        for (int pass = 0; pass < 2; ++pass)
        for (int ti = 0, tcount = (pass == 0 ? nTracks : nBuses); ti < tcount; ++ti)
        {
            Track* const t = pass == 0 ? tracks[(size_t) ti].get() : busOrder[(size_t) ti];
            const bool bus = t->isBus.load();
            if (bus != (pass == 1)) continue;

            auto& b = t->buf;
            if (! bus) b.clear (0, n);

            if (isPlaying && ! bus)
                for (auto& c : t->clips)
                {
                    mixClip (*c, b, pos, 0, len1);
                    if (useLoop && len1 < n)
                        mixClip (*c, b, lS, len1, n - len1);
                }

            // Entrada de la tarjeta -> Input FX -> grabación (y monitor). Solo se procesa si hace falta.
            if (! bus && t->armed.load() && in != nullptr && numIn > 0
                && ((isRec && t->writer != nullptr) || t->monitor.load()))
            {
                const int ch = jmin ((int) t->inputChannel.load(), numIn - 1);
                if (in[ch] != nullptr)
                {
                    auto& ib = t->inBuf;
                    ib.copyFrom (0, 0, in[ch], n);
                    ib.copyFrom (1, 0, in[ch], n);

                    if (! t->inputPlugins.empty())
                    {
                        for (auto& p : t->inputPlugins)
                            processPlugin (*p, ib, n, false);

                        ib.addFrom (0, 0, ib, 1, 0, n);      // vuelve a mono (promedio) para grabar la voz en 1 canal
                        ib.applyGain (0, 0, n, 0.5f);
                    }

                    if (isRec && t->writer != nullptr)
                    {
                        const float* data[1] = { ib.getReadPointer (0) };
                        t->writer->write (data, n);
                    }

                    if (t->monitor.load())                   // escuchar la voz (ya con efectos) por la pista
                    {
                        b.addFrom (0, 0, ib, 0, 0, n);
                        b.addFrom (1, 0, ib, 0, 0, n);
                    }
                }
            }

            // Automatización: los parámetros de plugins se envían antes de procesar el bloque;
            // volumen, pan y mute actúan junto al fader (el fader sirve de ajuste fino, "trim").
            applyParamEnvelopes (t->envs, pos);

            float envGain = 1.0f, envPan = 0.0f;
            bool envMuted = false;
            for (auto& e : t->envs)
            {
                if (! e->hasEffect()) continue;
                const float v = e->valueAt (pos);
                if (e->target == envVolume)    envGain = envToGain (v);
                else if (e->target == envPan)  envPan = v * 2.0f - 1.0f;
                else if (e->target == envMute) envMuted = v >= 0.5f;
            }

            for (auto& p : t->plugins)
                processPlugin (*p, b, n, isBypassed (t->envs, p.get(), pos));

            const bool silent = t->mute.load() || envMuted || (! bus && anySolo && ! t->solo.load());
            const float g = silent ? 0.0f : t->gain.load() * envGain;
            const float pn = jlimit (-1.0f, 1.0f, t->pan.load() + envPan);
            const float gl = g * (pn > 0 ? 1.0f - pn : 1.0f);
            const float gr = g * (pn < 0 ? 1.0f + pn : 1.0f);
            if (t->lastGL < 0.0f) { t->lastGL = gl; t->lastGR = gr; }
            b.applyGainRamp (0, 0, n, t->lastGL, gl);       // rampa dentro del bloque: sin chasquidos
            b.applyGainRamp (1, 0, n, t->lastGR, gr);
            t->lastGL = gl;
            t->lastGR = gr;

            t->peakL = jmax (b.getMagnitude (0, 0, n), t->peakL.load() * 0.85f);
            t->peakR = jmax (b.getMagnitude (1, 0, n), t->peakR.load() * 0.85f);

            // Envíos (post-fader) hacia buses. Las pistas y también los buses pueden enviar; un bus solo envía a
            // buses que se procesan después de él (busRank mayor): así nunca hay realimentación.
            for (auto& sd : t->sends)
            {
                Track* st = sd->target.load();
                const float lvl = sd->level.load();
                if (st != nullptr && st != t && st->isBus.load() && lvl > 0.0f && st->busRank > t->busRank)
                {
                    st->buf.addFrom (0, 0, b, 0, 0, n, lvl);
                    st->buf.addFrom (1, 0, b, 1, 0, n, lvl);
                }
            }

            // Salida: a un bus o al Master
            Track* o = bus ? nullptr : t->outputBus.load();
            if (o != nullptr && o != t && o->isBus.load())
            {
                o->buf.addFrom (0, 0, b, 0, 0, n);
                o->buf.addFrom (1, 0, b, 1, 0, n);
            }
            else
            {
                mix.addFrom (0, 0, b, 0, 0, n);
                mix.addFrom (1, 0, b, 1, 0, n);
            }
        }

        // Canal Master: automatización y plugins en cadena antes del volumen general
        applyParamEnvelopes (masterEnvs, pos);

        float masterEnvGain = 1.0f;
        for (auto& e : masterEnvs)
            if (e->target == envVolume && e->hasEffect())
                masterEnvGain = envToGain (e->valueAt (pos));

        for (auto& p : masterPlugins)
            processPlugin (*p, mix, n, isBypassed (masterEnvs, p.get(), pos));

        const float mg = masterGain.load() * masterEnvGain;
        if (lastMasterG < 0.0f) lastMasterG = mg;
        mix.applyGainRamp (0, n, lastMasterG, mg);
        lastMasterG = mg;

        if (numOut >= 2)
        {
            if (out[0]) FloatVectorOperations::copy (out[0], mix.getReadPointer (0), n);
            if (out[1]) FloatVectorOperations::copy (out[1], mix.getReadPointer (1), n);
            for (int c = 2; c < numOut; ++c) if (out[c]) FloatVectorOperations::clear (out[c], n);
        }
        else if (numOut == 1 && out[0] != nullptr)
        {
            FloatVectorOperations::copy (out[0], mix.getReadPointer (0), n);
            FloatVectorOperations::add (out[0], mix.getReadPointer (1), n);
            FloatVectorOperations::multiply (out[0], 0.5f, n);
        }

        // Metrónomo: va directo a la salida (no pasa por los efectos del Master ni por el medidor)
        if (clickOn)
            for (int c = 0; c < jmin (2, numOut); ++c)
                if (out[c] != nullptr)
                    FloatVectorOperations::add (out[c], clickBuf.data(), n);

        for (int c = 0; c < jmin (2, numOut); ++c)
            if (out[c] != nullptr)
                FloatVectorOperations::clip (out[c], out[c], -1.0f, 1.0f, n);

        masterPeakL = jmax (mix.getMagnitude (0, 0, n), masterPeakL.load() * 0.85f);
        masterPeakR = jmax (mix.getMagnitude (1, 0, n), masterPeakR.load() * 0.85f);

        // RMS del Master (ventana de ~300 ms, como un medidor de mezcla)
        {
            const double coef = std::exp (-(double) n / (0.3 * sampleRate));
            const double rl = mix.getRMSLevel (0, 0, n), rr = mix.getRMSLevel (1, 0, n);
            rmsMsL = rmsMsL * coef + rl * rl * (1.0 - coef);
            rmsMsR = rmsMsR * coef + rr * rr * (1.0 - coef);
            masterRmsL = (float) std::sqrt (rmsMsL);
            masterRmsR = (float) std::sqrt (rmsMsR);
        }

        if (isPlaying)
        {
            const int64 next = useLoop ? lS + (n - len1) : pos + n;

            // Fin de la canción: pasado el último clip (+1 s de cola para reverbs) se detiene y el cursor
            // vuelve a la posición de inicio. No aplica al grabar ni dentro de un bucle activo.
            bool finished = false;
            if (! isRec && ! useLoop && ! (loopOn.load() && pos < lE))
            {
                int64 endSample = 0;
                for (auto& t : tracks)
                    if (! t->isBus.load())
                        for (auto& c : t->clips)
                            endSample = jmax (endSample, c->startSample.load() + (int64) c->audio.getNumSamples());
                finished = endSample > 0 && next >= endSample + (int64) sampleRate;
            }

            // Si la interfaz movió el cursor durante este bloque, se respeta el cursor nuevo.
            int64 expected = pos;
            if (finished)
            {
                if (position.compare_exchange_strong (expected, startPos.load()))
                    playing = false;
            }
            else
                position.compare_exchange_strong (expected, next);
        }
    }

    // ------------------------------ Datos ----------------------------------
    AudioDeviceManager deviceManager;
    AudioFormatManager formats;
    AudioThumbnailCache thumbCache;
    PluginHost host;
    TimeSliceThread writerThread;   // debe declararse ANTES que 'tracks' (los writers dependen de él)

    std::vector<std::unique_ptr<Track>> tracks;
    std::vector<std::unique_ptr<AudioPluginInstance>> masterPlugins;   // cadena de efectos del canal Master
    EnvList masterEnvs;                                                // automatización del Master
    std::atomic<bool> playing { false }, recording { false };
    std::atomic<int64> position { 0 };
    std::atomic<int64> startPos { 0 };                // posición de inicio: a donde vuelve el cursor al parar
    std::atomic<float> masterGain { 1.0f }, masterPeakL { 0.0f }, masterPeakR { 0.0f };
    std::atomic<float> masterRmsL { 0.0f }, masterRmsR { 0.0f };   // nivel RMS del Master (lineal)
    std::atomic<bool> metronome { false };                         // clic al tempo mientras se reproduce o graba
    std::atomic<float> metroVol { 0.5f };

    // Tempo, bucle y rejilla (la rejilla es solo de la interfaz, se guarda con el proyecto)
    std::atomic<double> bpm { 120.0 };
    std::atomic<int64> selStart { 0 }, selEnd { 0 };  // selección de tiempo (selEnd <= selStart = no hay)
    std::atomic<bool> loopOn { false };
    std::atomic<int64> loopStart { 0 }, loopEnd { 0 };
    int gridId = 4;                                   // 4 = 1/4 (ver la lista de rejillas en UI.h)

    double sampleRate = 44100.0;
    int blockSize = 512;
    int maxBlock = 2048;

private:
    // Metrónomo (hilo de audio): escribe el clic de las muestras [pos, pos+len) en clickBuf desde dstOff.
    // Un clic por tiempo (según el BPM); el primer tiempo de cada compás (4 tiempos) suena más agudo.
    void renderMetronome (int64 pos, int dstOff, int len)
    {
        if (len <= 0) return;
        const double beatLen = sampleRate * 60.0 / jmax (1.0, bpm.load());
        int64 k = (int64) std::ceil ((double) pos / beatLen);
        int64 trig = (int64) std::ceil ((double) k * beatLen);
        const int clickLength = (int) (0.035 * sampleRate);
        const float vol = metroVol.load();

        for (int i = 0; i < len; ++i)
        {
            if (pos + i == trig)
            {
                clickN = 0;
                clickFreq = (k % 4 == 0) ? 1600.0f : 1000.0f;
                ++k;
                trig = (int64) std::ceil ((double) k * beatLen);
            }

            float v = 0.0f;
            if (clickN < clickLength)
            {
                const float env = 1.0f - (float) clickN / (float) clickLength;
                v = std::sin (MathConstants<float>::twoPi * clickFreq * (float) clickN / (float) sampleRate) * env * env * vol;
                ++clickN;
            }
            clickBuf[(size_t) (dstOff + i)] = v;
        }
    }

    std::vector<float> clickBuf;            // clic del bloque actual
    int clickN = 1 << 30;                   // muestras desde el inicio del clic (grande = no suena)
    float clickFreq = 1000.0f;
    double rmsMsL = 0.0, rmsMsR = 0.0;      // energía media suavizada del Master (hilo de audio)

    // Mezcla la parte del clip que cae entre 'pos' y 'pos + n' (posición en la línea de tiempo)
    // dentro del buffer 'b', empezando en la muestra 'dstOff' del buffer.
    static void mixClip (Clip& c, AudioBuffer<float>& b, int64 pos, int dstOff, int n)
    {
        const int64 cs = c.startSample.load();
        const int64 ce = cs + c.audio.getNumSamples();
        if (n <= 0 || pos + n <= cs || pos >= ce) return;

        const int64 from = jmax (pos, cs), to = jmin (pos + n, ce);
        const int dst = dstOff + (int) (from - pos), src = (int) (from - cs), len = (int) (to - from);

        const int total = c.audio.getNumSamples();
        const int fi = jlimit (0, total, c.fadeInSamples.load());
        const int fo = jlimit (0, total, c.fadeOutSamples.load());

        if (fi == 0 && fo == 0)
        {
            for (int ch = 0; ch < 2; ++ch)
                b.addFrom (ch, dst, c.audio, ch, src, len);
            return;
        }

        // con fades: ganancia lineal por muestra (sube al entrar, baja al salir)
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* in = c.audio.getReadPointer (ch);
            float* out = b.getWritePointer (ch);
            for (int i = 0; i < len; ++i)
            {
                const int r = src + i;
                float g = 1.0f;
                if (fi > 0 && r < fi)          g = (float) r / (float) fi;
                if (fo > 0 && r >= total - fo) g = jmin (g, (float) (total - r) / (float) fo);
                out[dst + i] += in[r] * g;
            }
        }
    }

    // --- automatización (hilo de audio) ---
    static void applyParamEnvelopes (EnvList& list, int64 pos)
    {
        for (auto& e : list)
        {
            if (e->target != envParam || e->param == nullptr || ! e->hasEffect()) continue;

            // Solo se envía cuando el valor de la curva cambia: así, si mueves el mando a mano
            // mientras la curva está quieta, tu ajuste se respeta.
            const float v = e->valueAt (pos);
            if (v != e->lastApplied)
            {
                e->param->setValue (v);
                e->lastApplied = v;
            }
        }
    }

    static bool isBypassed (const EnvList& list, AudioPluginInstance* p, int64 pos)
    {
        for (auto& e : list)
            if (e->target == envBypass && e->plugin == p && e->hasEffect())
                return e->valueAt (pos) >= 0.5f;
        return false;
    }

    void processPlugin (AudioPluginInstance& p, AudioBuffer<float>& b, int n, bool bypassed)
    {
        const int nc = jmin (8, jmax (2, p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()));
        for (int c = 2; c < nc; ++c) b.clear (c, 0, n);
        AudioBuffer<float> view (b.getArrayOfWritePointers(), nc, n);
        midi.clear();

        if (bypassed) p.processBlockBypassed (view, midi);
        else          p.processBlock (view, midi);
    }

    // --- guardar / cargar envolventes ---
    static int indexOfPlugin (const PluginList& v, AudioPluginInstance* p)
    {
        for (size_t i = 0; i < v.size(); ++i)
            if (v[i].get() == p) return (int) i;
        return -1;
    }

    static String pointsToString (const Envelope& e)
    {
        String s;
        for (auto& p : e.points)
            s << String (p.sample) << ":" << String (p.value, 6) << ",";
        return s;
    }

    void saveEnvelopes (XmlElement& parent, const EnvList& list, const PluginList& plugins) const
    {
        for (auto& e : list)
        {
            auto* ee = parent.createNewChildElement ("ENV");
            ee->setAttribute ("target", e->target);
            ee->setAttribute ("plugin", indexOfPlugin (plugins, e->plugin));
            ee->setAttribute ("param", e->paramIndex);
            ee->setAttribute ("active", e->active.load());
            ee->setAttribute ("visible", e->visible);
            ee->setAttribute ("pts", pointsToString (*e));
        }
    }

    // 'orig' = plugins de esa pista/canal en el orden en que se guardaron (nullptr si no se pudo cargar)
    void loadEnvelopes (const XmlElement& parent, EnvList& list, const std::vector<AudioPluginInstance*>& orig)
    {
        for (auto* ee : parent.getChildWithTagNameIterator ("ENV"))
        {
            const int target = ee->getIntAttribute ("target", envVolume);
            AudioPluginInstance* plugin = nullptr;

            if (target == envParam || target == envBypass)
            {
                const int idx = ee->getIntAttribute ("plugin", -1);
                if (! isPositiveAndBelow (idx, (int) orig.size()) || orig[(size_t) idx] == nullptr)
                    continue;                                   // su plugin ya no está: se descarta
                plugin = orig[(size_t) idx];
            }

            auto* e = getOrCreateEnvelope (list, target, plugin, target == envParam ? ee->getIntAttribute ("param", -1) : -1);
            if (e == nullptr) continue;

            std::vector<EnvPoint> pts;
            for (auto& token : StringArray::fromTokens (ee->getStringAttribute ("pts"), ",", ""))
            {
                auto parts = StringArray::fromTokens (token, ":", "");
                if (parts.size() == 2)
                    pts.push_back (EnvPoint { jmax<int64> (0, parts[0].getLargeIntValue()),
                                              jlimit (0.0f, 1.0f, parts[1].getFloatValue()) });
            }
            std::sort (pts.begin(), pts.end(), [] (const EnvPoint& a, const EnvPoint& b) { return a.sample < b.sample; });

            const SpinLock::ScopedLockType sl (lock);
            e->active = ee->getBoolAttribute ("active", true);
            e->visible = ee->getBoolAttribute ("visible", false);
            e->points = std::move (pts);
        }
    }

    std::vector<Track*> busOrder;      // buses en orden de proceso (solo con el lock)
    float lastMasterG = -1.0f;
    SpinLock lock;
    AudioBuffer<float> mix;
    MidiBuffer midi;
    int64 recStart = 0;
};
