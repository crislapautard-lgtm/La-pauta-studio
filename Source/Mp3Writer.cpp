#include "Mp3Writer.h"

#include <cstdio>
#include <cstring>

#if defined (_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "mfreadwrite.lib")
#pragma comment (lib, "mfuuid.lib")
#pragma comment (lib, "ole32.lib")

namespace
{
    struct Mp3State
    {
        IMFSinkWriter* writer = nullptr;
        DWORD stream = 0;
        LONGLONG timeNs = 0;       // en unidades de 100 ns
        int sampleRate = 44100;
        bool comInit = false;
        bool started = false;
    };

    void setErr (char* err, int len, const char* what, HRESULT hr)
    {
        if (err != nullptr && len > 0)
            std::snprintf (err, (size_t) len, "%s (codigo 0x%08lX)", what, (unsigned long) hr);
    }

    void release (Mp3State* s)
    {
        if (s->writer != nullptr) { s->writer->Release(); s->writer = nullptr; }
        if (s->started) MFShutdown();
        if (s->comInit) CoUninitialize();
        delete s;
    }
}

void* lpMp3Open (const wchar_t* path, int sampleRate, int kbps, char* err, int errLen)
{
    auto* s = new Mp3State();
    s->sampleRate = sampleRate;

    HRESULT hr = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    s->comInit = SUCCEEDED (hr);          // si ya estaba iniciado de otro modo, se usa igual

    hr = MFStartup (MF_VERSION, MFSTARTUP_LITE);
    if (FAILED (hr)) { setErr (err, errLen, "No se pudo iniciar Media Foundation", hr); release (s); return nullptr; }
    s->started = true;

    hr = MFCreateSinkWriterFromURL (path, nullptr, nullptr, &s->writer);
    if (FAILED (hr)) { setErr (err, errLen, "No se pudo crear el archivo MP3", hr); release (s); return nullptr; }

    IMFMediaType* outType = nullptr;
    hr = MFCreateMediaType (&outType);
    if (SUCCEEDED (hr))
    {
        outType->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        outType->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_MP3);
        outType->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32) sampleRate);
        outType->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
        outType->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32) (kbps * 1000 / 8));
        outType->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
        hr = s->writer->AddStream (outType, &s->stream);
        outType->Release();
    }
    if (FAILED (hr))
    {
        setErr (err, errLen, "Este Windows no tiene codificador MP3 (o no admite esa calidad)", hr);
        release (s);
        return nullptr;
    }

    IMFMediaType* inType = nullptr;
    hr = MFCreateMediaType (&inType);
    if (SUCCEEDED (hr))
    {
        inType->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        inType->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_PCM);
        inType->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        inType->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32) sampleRate);
        inType->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
        inType->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
        inType->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32) (sampleRate * 4));
        hr = s->writer->SetInputMediaType (s->stream, inType, nullptr);
        inType->Release();
    }
    if (FAILED (hr)) { setErr (err, errLen, "No se pudo configurar el codificador MP3", hr); release (s); return nullptr; }

    hr = s->writer->BeginWriting();
    if (FAILED (hr)) { setErr (err, errLen, "No se pudo empezar a escribir el MP3", hr); release (s); return nullptr; }

    return s;
}

bool lpMp3Write (void* handle, const short* interleaved, int frames, char* err, int errLen)
{
    auto* s = static_cast<Mp3State*> (handle);
    if (s == nullptr || frames <= 0) return false;

    const DWORD bytes = (DWORD) frames * 4;
    IMFMediaBuffer* buffer = nullptr;
    HRESULT hr = MFCreateMemoryBuffer (bytes, &buffer);
    if (FAILED (hr)) { setErr (err, errLen, "Sin memoria para el MP3", hr); return false; }

    BYTE* data = nullptr;
    hr = buffer->Lock (&data, nullptr, nullptr);
    if (SUCCEEDED (hr))
    {
        std::memcpy (data, interleaved, bytes);
        buffer->Unlock();
        buffer->SetCurrentLength (bytes);

        IMFSample* sample = nullptr;
        hr = MFCreateSample (&sample);
        if (SUCCEEDED (hr))
        {
            sample->AddBuffer (buffer);
            sample->SetSampleTime (s->timeNs);
            const LONGLONG dur = (LONGLONG) frames * 10000000LL / s->sampleRate;
            sample->SetSampleDuration (dur);
            hr = s->writer->WriteSample (s->stream, sample);
            s->timeNs += dur;
            sample->Release();
        }
    }
    buffer->Release();

    if (FAILED (hr)) { setErr (err, errLen, "Fallo al escribir el MP3", hr); return false; }
    return true;
}

bool lpMp3Close (void* handle, char* err, int errLen)
{
    auto* s = static_cast<Mp3State*> (handle);
    if (s == nullptr) return false;

    HRESULT hr = S_OK;
    if (s->writer != nullptr) hr = s->writer->Finalize();
    release (s);

    if (FAILED (hr)) { setErr (err, errLen, "No se pudo cerrar el MP3", hr); return false; }
    return true;
}

#else

void* lpMp3Open (const wchar_t*, int, int, char* err, int errLen)
{
    if (err != nullptr && errLen > 0) std::snprintf (err, (size_t) errLen, "MP3 solo esta disponible en Windows");
    return nullptr;
}
bool lpMp3Write (void*, const short*, int, char*, int) { return false; }
bool lpMp3Close (void*, char*, int) { return false; }

#endif
