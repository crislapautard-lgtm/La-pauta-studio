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
    std::unique_ptr<AudioThumbnail> thumb;
};

// ---------------------------------------------------------------------------
// Track: pista con clips, cadena de plugins, volumen, pan, mute/solo/armado.
// ---------------------------------------------------------------------------
struct Track
{
    String name { "Pista" };
    std::vector<std::unique_ptr<Clip>> clips;
    std::vector<std::unique_ptr<AudioPluginInstance>> plugins;

    std::atomic<float> gain { 1.0f }, pan { 0.0f }, peakL { 0.0f }, peakR { 0.0f };
    std::atomic<bool> mute { false }, solo { false }, armed { false };
    std::atomic<int> inputChannel { 0 };

    // Buses y envíos
    std::atomic<bool> isBus { false };                 // true = pista de bus (recibe audio de otras pistas)
    std::atomic<Track*> outputBus { nullptr };         // a qué bus va la salida (nullptr = Master)
    std::atomic<Track*> sendTarget[2];                 // 2 envíos post-fader
    std::atomic<float> sendLevel[2];

    Track()
    {
        for (int i = 0; i < 2; ++i) { sendTarget[i] = nullptr; sendLevel[i] = 0.0f; }
    }

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
        mix.setSize (2, maxBlock);
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
        auto* raw = t.get();
        const SpinLock::ScopedLockType sl (lock);
        tracks.push_back (std::move (t));
        return raw;
    }

    void removeTrack (Track* t)
    {
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
                        for (int s = 0; s < 2; ++s)
                            if (o->sendTarget[s].load() == t) o->sendTarget[s] = nullptr;
                    }
                    dead = std::move (tracks[i]);
                    tracks.erase (tracks.begin() + (long) i);
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

    void addClip (Track& t, std::unique_ptr<Clip> c)
    {
        const SpinLock::ScopedLockType sl (lock);
        t.clips.push_back (std::move (c));
    }

    void removeClip (Track& t, Clip* c)
    {
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
    }

    void splitClip (Track& t, Clip* c, int64 at)
    {
        const int64 s = c->startSample.load();
        const int64 rel = at - s;
        const int total = c->audio.getNumSamples();
        if (rel <= 0 || rel >= total) return;

        AudioBuffer<float> a (2, (int) rel), b (2, total - (int) rel);
        for (int ch = 0; ch < 2; ++ch)
        {
            a.copyFrom (ch, 0, c->audio, ch, 0, (int) rel);
            b.copyFrom (ch, 0, c->audio, ch, (int) rel, total - (int) rel);
        }
        auto ca = makeClip (c->name, c->file, std::move (a), s);
        auto cb = makeClip (c->name, c->file, std::move (b), at);

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
        std::unique_ptr<AudioPluginInstance> dead;
        {
            const SpinLock::ScopedLockType sl (lock);
            if (! isPositiveAndBelow (index, (int) t.plugins.size())) return;
            dead = std::move (t.plugins[(size_t) index]);
            t.plugins.erase (t.plugins.begin() + index);
        }
        dead->releaseResources();
    }

    // ----------------------------- Transporte ------------------------------
    void play()  { playing = true; }
    void pause() { playing = false; }

    // Devuelve true si se grabó algo (para que la UI se refresque).
    bool stop()
    {
        playing = false;
        if (recording.load())
            return stopRecording();
        return false;
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
                clip->startSample = jmax<int64> (0, recStart - latency);   // compensa latencia
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
            te->setAttribute ("bus", t->isBus.load());
            te->setAttribute ("out", indexOf (t->outputBus.load()));
            for (int s = 0; s < 2; ++s)
            {
                te->setAttribute ("send" + String (s), indexOf (t->sendTarget[s].load()));
                te->setAttribute ("lvl" + String (s), (double) t->sendLevel[s].load());
            }

            for (auto& c : t->clips)
            {
                auto* ce = te->createNewChildElement ("CLIP");
                ce->setAttribute ("file", c->file.getFullPathName());
                ce->setAttribute ("start", String (c->startSample.load()));
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
        {
            std::vector<std::unique_ptr<Track>> old;
            {
                const SpinLock::ScopedLockType sl (lock);
                old.swap (tracks);
            }
        }   // pistas viejas destruidas aquí

        masterGain = (float) xml->getDoubleAttribute ("master", 1.0);
        bpm = jlimit (20.0, 400.0, xml->getDoubleAttribute ("bpm", 120.0));
        gridId = xml->getIntAttribute ("grid", 4);
        loopStart = xml->getStringAttribute ("loopStart").getLargeIntValue();
        loopEnd = xml->getStringAttribute ("loopEnd").getLargeIntValue();
        loopOn = xml->getBoolAttribute ("loopOn", false);
        position = 0;
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
                    c->startSample = ce->getStringAttribute ("start").getLargeIntValue();
                    addClip (*t, std::move (c));
                }
                else ++missing;
            }

            for (auto* fx : te->getChildWithTagNameIterator ("FX"))
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
                    addPlugin (*t, std::move (inst));
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
            for (int s = 0; s < 2; ++s)
            {
                t.sendTarget[s] = at (te->getIntAttribute ("send" + String (s), -1));
                t.sendLevel[s] = (float) te->getDoubleAttribute ("lvl" + String (s), 0.0);
            }
        }

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
        mix.setSize (2, maxBlock);

        for (auto& t : tracks)
        {
            t->buf.setSize (8, maxBlock);
            for (auto& p : t->plugins)
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

        bool anySolo = false;
        for (auto& t : tracks) if (t->solo) anySolo = true;

        // Los buses se limpian primero; luego pasada 0 = pistas normales, pasada 1 = buses.
        for (auto& t : tracks)
            if (t->isBus.load()) t->buf.clear (0, n);

        for (int pass = 0; pass < 2; ++pass)
        for (auto& t : tracks)
        {
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

            if (isRec && t->writer != nullptr && in != nullptr && numIn > 0)
            {
                const int ch = jmin ((int) t->inputChannel.load(), numIn - 1);
                if (in[ch] != nullptr)
                {
                    const float* data[1] = { in[ch] };
                    t->writer->write (data, n);
                }
            }

            for (auto& p : t->plugins)
            {
                const int nc = jmin (8, jmax (2, p->getTotalNumInputChannels(), p->getTotalNumOutputChannels()));
                for (int c = 2; c < nc; ++c) b.clear (c, 0, n);
                AudioBuffer<float> view (b.getArrayOfWritePointers(), nc, n);
                midi.clear();
                p->processBlock (view, midi);
            }

            const bool silent = t->mute.load() || (! bus && anySolo && ! t->solo.load());
            const float g = silent ? 0.0f : t->gain.load();
            const float pn = t->pan.load();
            b.applyGain (0, 0, n, g * (pn > 0 ? 1.0f - pn : 1.0f));
            b.applyGain (1, 0, n, g * (pn < 0 ? 1.0f + pn : 1.0f));

            t->peakL = jmax (b.getMagnitude (0, 0, n), t->peakL.load() * 0.85f);
            t->peakR = jmax (b.getMagnitude (1, 0, n), t->peakR.load() * 0.85f);

            // Envíos (post-fader) hacia buses
            if (! bus)
                for (int s = 0; s < 2; ++s)
                {
                    Track* st = t->sendTarget[s].load();
                    const float lvl = t->sendLevel[s].load();
                    if (st != nullptr && st != t.get() && st->isBus.load() && lvl > 0.0f)
                    {
                        st->buf.addFrom (0, 0, b, 0, 0, n, lvl);
                        st->buf.addFrom (1, 0, b, 1, 0, n, lvl);
                    }
                }

            // Salida: a un bus o al Master
            Track* o = bus ? nullptr : t->outputBus.load();
            if (o != nullptr && o != t.get() && o->isBus.load())
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

        mix.applyGain (0, n, masterGain.load());

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

        for (int c = 0; c < jmin (2, numOut); ++c)
            if (out[c] != nullptr)
                FloatVectorOperations::clip (out[c], out[c], -1.0f, 1.0f, n);

        masterPeakL = jmax (mix.getMagnitude (0, 0, n), masterPeakL.load() * 0.85f);
        masterPeakR = jmax (mix.getMagnitude (1, 0, n), masterPeakR.load() * 0.85f);

        if (isPlaying)
        {
            // Si la interfaz movió el cursor durante este bloque, se respeta el cursor nuevo.
            int64 expected = pos;
            position.compare_exchange_strong (expected, useLoop ? lS + (n - len1) : pos + n);
        }
    }

    // ------------------------------ Datos ----------------------------------
    AudioDeviceManager deviceManager;
    AudioFormatManager formats;
    AudioThumbnailCache thumbCache;
    PluginHost host;
    TimeSliceThread writerThread;   // debe declararse ANTES que 'tracks' (los writers dependen de él)

    std::vector<std::unique_ptr<Track>> tracks;
    std::atomic<bool> playing { false }, recording { false };
    std::atomic<int64> position { 0 };
    std::atomic<float> masterGain { 1.0f }, masterPeakL { 0.0f }, masterPeakR { 0.0f };

    // Tempo, bucle y rejilla (la rejilla es solo de la interfaz, se guarda con el proyecto)
    std::atomic<double> bpm { 120.0 };
    std::atomic<bool> loopOn { false };
    std::atomic<int64> loopStart { 0 }, loopEnd { 0 };
    int gridId = 4;                                   // 4 = 1/4 (ver la lista de rejillas en UI.h)

    double sampleRate = 44100.0;
    int blockSize = 512;
    int maxBlock = 2048;

private:
    // Mezcla la parte del clip que cae entre 'pos' y 'pos + n' (posición en la línea de tiempo)
    // dentro del buffer 'b', empezando en la muestra 'dstOff' del buffer.
    static void mixClip (Clip& c, AudioBuffer<float>& b, int64 pos, int dstOff, int n)
    {
        const int64 cs = c.startSample.load();
        const int64 ce = cs + c.audio.getNumSamples();
        if (n <= 0 || pos + n <= cs || pos >= ce) return;

        const int64 from = jmax (pos, cs), to = jmin (pos + n, ce);
        const int dst = dstOff + (int) (from - pos), src = (int) (from - cs), len = (int) (to - from);
        for (int ch = 0; ch < 2; ++ch)
            b.addFrom (ch, dst, c.audio, ch, src, len);
    }

    SpinLock lock;
    AudioBuffer<float> mix;
    MidiBuffer midi;
    int64 recStart = 0;
};
