#ifndef _AUDIO_ROUTER_H_
#define _AUDIO_ROUTER_H_

#include "Application/Audio/AudioDriverModeTable.h"
#include "Application/Audio/AudioCapabilities.h"

// F4c (docs/F4_ARCHITECTURE_ES.md): AudioRouter - politica declarativa de
// seleccion/routing de backends de audio (objetivo F4 / punto 6).  Toma la
// decision de que modo queda efectivamente activo y que clase de apply
// requiere, SIN conocer el hardware: solo consume la tabla golden de modos
// (AudioDriverModeTable.h) y el vocabulario de capacidades
// (AudioCapabilities.h).
//
// Son replicas golden byte-identicas de la politica que vivia en el puente
// UAC2 del adaptador (SetDriverMode/CycleDriverMode):
//
// No depende de GUI, audio, Player, daemons, POSIX ni del framebuffer:
// solo los datos puros de la tabla y las capacidades.  El estado runtime
// (FIFO abierto, daemon presente, debounce) sigue siendo del bridge.

// Replica golden del mapeo de modo efectivo de SetDriverMode:
// USB_OUT con direccion sampler IN se ejecuta como SP404_IN (grabacion);
// con direccion OUT se mantiene como USB_OUT (playback).
inline int AudioRouteEffectiveMode(int mode, int samplerDirectionIn) {
    if (mode == kAudioDriverModeUsbOut && samplerDirectionIn == 1)
        return kAudioDriverModeSp404In;
    return mode;
}

// Replica golden de la clasificacion host-role del bridge (U2.52
// HOST_ROLE_MODE_ALWAYS_APPLY): ANDROID, USB_OUT, SP404_IN y MIDI cargan
// modulos ALSA de host, conmutan el controlador musb a host role y arrancan
// el supervisor; exigen apply de perfil, nunca fast apply.  Derivada de la
// capacidad UsbHost del modo (AudioDriverModeCapabilities).
inline int AudioRouteIsHostRoleMode(int mode) {
    return (AudioDriverModeCapabilities(mode, 0) & kAudioCapUsbHost) != 0;
}

// Replica golden del ciclo de seleccion: siguiente modo de la secuencia
// UI (0..kAudioDriverModeUiCount-1, SP404_IN no se lista).  Es la
// matematica del CycleDriverMode del puente y del movimiento
// UP/DOWN del modal de driver.
inline int AudioRouteCycleNext(int mode) {
    return (mode + 1) % kAudioDriverModeUiCount;
}

// Replica golden del movimiento inverso del modal de driver (UP):
// retrocede un paso envuelto dentro de la secuencia UI.
inline int AudioRouteCyclePrev(int mode) {
    return (mode + kAudioDriverModeUiCount - 1) % kAudioDriverModeUiCount;
}

#endif