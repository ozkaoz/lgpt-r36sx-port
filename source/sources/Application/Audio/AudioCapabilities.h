#ifndef _AUDIO_CAPABILITIES_H_
#define _AUDIO_CAPABILITIES_H_

// F4b (docs/F4_ARCHITECTURE_ES.md): vocabulario declarativo de capacidades
// de los backends de audio (objetivo F4 / punto 6).  Declara los bits de
// capacidad como datos puros: cada backend/modo puede declarar un conjunto
// de capacidades (Stereo Output, Stereo Input, USB Device, USB Host, MIDI,
// Capture, Clock Sync, Hotplug) sin que la logica del sistema tenga que
// conocer hardware concreto.
//
// Es el lenguaje que consumira el futuro AudioRouter/AudioCapabilities
// (objetivo 6): agregar un backend nuevo (p.ej. multitrack USB) implica
// declarar sus capabilities, no modificar decenas de archivos.
//
// No depende de GUI, audio, Player, daemons, POSIX ni del framebuffer:
// solo tipos integrados de C++03.  Es datos puros, sin comportamiento.

// Bits de capacidad (mascara declarativa por backend/modo).
static const unsigned int kAudioCapStereoOutput = 1u << 0;
static const unsigned int kAudioCapStereoInput = 1u << 1;
static const unsigned int kAudioCapUsbDevice = 1u << 2;
static const unsigned int kAudioCapUsbHost = 1u << 3;
static const unsigned int kAudioCapMidi = 1u << 4;
static const unsigned int kAudioCapCapture = 1u << 5;
static const unsigned int kAudioCapClockSync = 1u << 6;
static const unsigned int kAudioCapHotplug = 1u << 7;

// Numero de capacidades declaradas (lenguaje fijo, ampliable solo
// anadiendo bits a la lista anterior).
static const int kAudioCapabilityCount = 8;

// Nombres legibles de las capacidades, en el orden de los bits (indice =
// posicion del bit).  Fuente de verdad unica para Help/UI/diagnostico:
// los controles y la ayuda no pueden divergir.
static const char *kAudioCapabilityNames[kAudioCapabilityCount] = {
    "Stereo Output", "Stereo Input", "USB Device", "USB Host",
    "MIDI", "Capture", "Clock Sync", "Hotplug"};

// Bit de capacidad por indice (0..kAudioCapabilityCount-1).
inline unsigned int AudioCapabilityBit(int index) {
    return 1u << index;
}

// Nombre de la capacidad por indice (0..kAudioCapabilityCount-1).
inline const char *AudioCapabilityName(int index) {
    return kAudioCapabilityNames[index];
}

// Conteo de capacidades declaradas.
inline int AudioCapabilityCount(void) {
    return kAudioCapabilityCount;
}

#endif
