#ifndef _AUDIO_BACKEND_H_
#define _AUDIO_BACKEND_H_

#include "Application/Audio/AudioDriverModeTable.h"
#include "Application/Audio/AudioCapabilities.h"
#include "Application/Audio/AudioRouter.h"

// F4d (docs/F4_ARCHITECTURE_ES.md): registro declarativo de clases de
// backend de audio (objetivo F4 / punto 6).  Declara el mapa
// modo driver -> clase de backend y el contrato de operaciones de la
// interfaz AudioBackend (open/start/caps/stream/write/close) como datos
// puros.
//
// Los backends corresponden a los daemons reales del port:
//   - LocalAudioBackend:   consola, sin USB (modo LOCAL_CONSOLE).
//   - WindowsUac2Backend:  gadget UAC2 duplex hacia PC (modo WINDOWS).
//   - AndroidBackend:      host-role AOA input-only (modo ANDROID).
//   - Sp404Backend:        host-role SP404 OUT/IN (modos USB_OUT, SP404_IN).
//   - MidiBackend:         host-role MIDI (modo MIDI; daemon midi).
//
// El runtime actual (puente UAC2) sigue siendo la ruta estable: este
// registro es la declaracion de arquitectura que consumira AudioEngine
// (F4e) al envolver el puente, sin cambiar datos ni timings.
//
// No depende de GUI, audio, Player, daemons, POSIX ni del framebuffer:
// solo los datos puros de la tabla de modos, las capacidades y el router.

// Clases de backend declaradas (indices del registro).
static const int kAudioBackendLocal = 0;
static const int kAudioBackendWindowsUac2 = 1;
static const int kAudioBackendAndroid = 2;
static const int kAudioBackendSp404 = 3;
static const int kAudioBackendMidi = 4;

// Numero de clases de backend declaradas.
static const int kAudioBackendClassCount = 5;

// Nombres legibles de las clases (source de verdad para Help/diagnostico).
static const char *kAudioBackendClassNames[kAudioBackendClassCount] = {
    "LocalAudioBackend", "WindowsUac2Backend", "AndroidBackend",
    "Sp404Backend", "MidiBackend"};

// Contrato de operaciones de la interfaz AudioBackend (objetivo 6:
// open/start/caps/stream/write; close anadido para el cierre simetrico).
static const int kAudioBackendOpCount = 6;
static const char *kAudioBackendOps[kAudioBackendOpCount] = {
    "open", "start", "caps", "stream", "write", "close"};

// Mapeo golden modo driver -> clase de backend (daemons reales del port).
inline int AudioBackendClassForMode(int mode) {
    switch (mode) {
    case kAudioDriverModeWindows: return kAudioBackendWindowsUac2;
    case kAudioDriverModeAndroid: return kAudioBackendAndroid;
    case kAudioDriverModeUsbOut: return kAudioBackendSp404;
    case kAudioDriverModeSp404In: return kAudioBackendSp404;
    case kAudioDriverModeMidi: return kAudioBackendMidi;
    case kAudioDriverModeLocalConsole:
    default: return kAudioBackendLocal;
    }
}

// Nombre de la clase de backend que sirve al modo solicitado.
inline const char *AudioBackendClassName(int mode) {
    return kAudioBackendClassNames[AudioBackendClassForMode(mode)];
}

// Capacidades agregadas de la clase de backend = union de las capacidades
// de los modos que la usan (ambas direcciones del sampler).  Derivado,
// nunca declarado a mano: mismo lenguaje que AudioDriverModeCapabilities.
inline unsigned int AudioBackendClassCapabilities(int backendClass) {
    unsigned int caps = 0;
    for (int mode = 0; mode < 6; ++mode) {
        if (AudioBackendClassForMode(mode) != backendClass) continue;
        caps |= AudioDriverModeCapabilities(mode, 0);
        caps |= AudioDriverModeCapabilities(mode, 1);
    }
    return caps;
}

#endif