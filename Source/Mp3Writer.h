#pragma once
// Escritor de MP3 usando el codificador MP3 que trae Windows (Media Foundation). Sin librerías externas.
// Se implementa en Mp3Writer.cpp (código Win32 puro, separado de JUCE a propósito).
// En otros sistemas siempre devuelve error.

// Abre el archivo MP3 de salida. Devuelve nullptr y escribe el motivo en 'err' si no se puede.
void* lpMp3Open (const wchar_t* path, int sampleRate, int kbps, char* err, int errLen);

// Escribe 'frames' muestras estéreo (L,R,L,R...) de 16 bits. Devuelve false si falla.
bool lpMp3Write (void* handle, const short* interleaved, int frames, char* err, int errLen);

// Termina el archivo y libera todo. Devuelve false si falló el cierre.
bool lpMp3Close (void* handle, char* err, int errLen);
