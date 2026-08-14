#ifndef _AUDIO_ENGINE_H_
#define _AUDIO_ENGINE_H_

#include "Application/Audio/AudioDriverModeTable.h"
#include "Application/Audio/AudioCapabilities.h"
#include "Application/Audio/AudioRouter.h"

// F4e (docs/F4_ARCHITECTURE_ES.md): AudioEngine - politica de estado del
// motor de audio como capa pura (objetivo F4 / punto 6).  Declara las
// reglas golden de decision que el puente UAC2 evalua en cada ciclo de
// stream: cuando el mix local debe mutarse (ruta a USB activa), el paso
// ASRC del monitor de captura y la ganancia conservadora del monitor.
//
// Son replicas golden byte-identicas de la politica que vivia en el puente
// (should_mute_now y el bucle de MixUsbCaptureMonitorStereo48000):
//   - AudioEngineShouldMute: U2.52.5 ANDROID_NO_MUTE (AOA input-only nunca
//     silencia la consola) + mute cuando el modo tiene salida USB, hay USB
//     raw presente y no hay override disable_mute_local.
//   - AudioEngineMonitorStep: relacion de tasas usb/engine del ASRC del
//     monitor (fallback 1.0 si el engine no reporta tasa).
//   - kAudioEngineMonitorGainPercent: 75% de ganancia del monitor
//     (headroom para el mix local).
//
// El estado runtime (estado del puente: fifos, ring buffer)
// sigue siendo del bridge: esta capa solo decide.  No depende de GUI,
// audio, Player, daemons, POSIX ni del framebuffer.

// Prebuffer del monitor de captura golden (U2415_MONITOR_PREBUFFER_SAMPLES):
// el monitor no se activa hasta llenar 960 muestras mono en el ring.
static const int kAudioEngineMonitorPrebufferSamples = 960;

// Ganancia conservadora del monitor de captura golden (75/100): deja
// headroom para la mezcla local.
static const int kAudioEngineMonitorGainPercent = 75;

// Replica golden de should_mute_now():
//   - ANDROID nunca muta (U2.52.5 ANDROID_NO_MUTE: AOA input-only, el
//     usuario oye el proyecto local mientras el telefono graba).
//   - El resto muta si el modo tiene salida USB (mode_has_out), hay USB
//     raw presente y no existe el override disable_mute_local.
// Los parametros usbRawPresent/disableMuteFilePresent sustituyen el estado
// runtime del puente viaja como parametros (raw presente, override).
inline int AudioEngineShouldMute(int mode, int samplerDirectionIn,
                                 int usbRawPresent,
                                 int disableMuteFilePresent) {
    if (mode == kAudioDriverModeAndroid) return 0;
    return AudioDriverModeHasOut(mode, samplerDirectionIn) &&
           !disableMuteFilePresent &&
           usbRawPresent;
}

// Replica golden del paso ASRC del monitor de captura: usb_rate/engine_rate
// (fallback 1.0 si el engine no reporta tasa).  engineRate == 0 significa
// "sin tasa conocida" (replica `g_engine_rate > 0`).
inline double AudioEngineMonitorStep(int engineRate, int usbRate) {
    return (engineRate > 0)
               ? (double)usbRate / (double)engineRate
               : 1.0;
}

// Replica golden de la ganancia del monitor aplicada a cada muestra:
// left = left * kAudioEngineMonitorGainPercent / 100.
inline int AudioEngineMonitorApplyGain(int sample) {
    return (sample * kAudioEngineMonitorGainPercent) / 100;
}

#endif