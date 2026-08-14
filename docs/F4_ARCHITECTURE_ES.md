# F4 - Audio backends extensibles (F4a: tabla declarativa de modos)

## Objetivo F4

Convertir la ruta de audio del adaptador TREEFROG en backends extensibles:

```
AudioEngine (Application/Audio)
   -> AudioRouter (seleccion de modo/backend)
      -> AudioBackend (open/start/caps/stream/write)
         -> puente UAC2 (contrato 48k/2ch/S16_LE con los daemons device/*.c)
```

Los daemons device/*.c NO cambian (contrato 48k/2ch/S16_LE intacto).
Evidencia: caminos de datos identicos; validacion en consola (samples,
sampler, USB out, stop/start frecuente).

## F4a - AudioDriverModeTable (capa pura de datos)

### Que se hizo

Se extrajo del puente UAC2 (`Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp`)
toda la descripcion declarativa de los modos del driver a una capa pura
header-only C++03:

`Application/Audio/AudioDriverModeTable.h`

Estructura por modo (`AudioDriverModeInfo`): id, nombre, descripcion, token
de modo (escrito en `audio_driver_mode`), token de politica OTG, rama del
daemon (`audio_driver_*`) y capacidades declarativas de direccion.

### Datos golden (6 modos)

| id | nombre         | token        | policy         | branch daemon           |
|----|----------------|--------------|----------------|-------------------------|
| 0  | Local Console  | LOCAL_CONSOLE| LOCAL_CONSOLE  | audio_driver_local_console |
| 1  | Windows        | USB_DUPLEX   | USB_DUPLEX_OTG | audio_driver_usb_duplex |
| 2  | Android        | USB_IN       | USB_IN_OTG     | audio_driver_usb_in     |
| 3  | Sampler (USB_OUT) | USB_OUT   | USB_OUT_OTG    | audio_driver_usb_out    |
| 4  | MIDI           | MIDI         | MIDI_OTG       | audio_driver_midi       |
| 5  | SP404_IN       | SP404_IN     | USB_OUT_OTG    | audio_driver_sp404_in   |

- Conteo de UI = 5: SP404_IN (5) es un modo interno que no se lista ni se
  selecciona en el modal (coincide con `GetDriverModeCount()`).
- Fallback para ids fuera de rango: etiquetas de LOCAL_CONSOLE, selectable
  = 0 (replica el switch `default` del bridge).
- Selectable golden: 0..4 = si, 5 = no.
- Capacidades de direccion: USB_OUT sigue el toggle del sampler
  (out = !dir, in = dir); WINDOWS out+in fijos; ANDROID in fijo;
  SP404_IN in fijo.  La capa pura recibe la direccion por parametro
  (`AudioDriverModeHasOut(mode, samplerDirectionIn)`); el bridge pasa
  `g_sampler_direction_in`.

### Delegacion del bridge

`TreeFrogUac2Bridge.cpp` conserva sus helpers static como delegados
one-line a la capa pura (los ids del enum `U241_*` son los valores de
`kAudioDriverMode*`; el baseline F4a verifica el contrato de valores):

```c
static const char *mode_name(int mode) { return AudioDriverModeName(mode); }
static const char *mode_token(int mode) { return AudioDriverModeToken(mode); }
static const char *policy_token(int mode) { return AudioDriverModePolicyToken(mode); }
static const char *branch_name_for_mode(int mode) { return AudioDriverModeBranchName(mode); }
static const char *mode_desc(int mode) { return AudioDriverModeDescription(mode); }
static int selectable_mode(int mode) { return AudioDriverModeIsSelectable(mode); }
static int mode_has_out(int mode) { return AudioDriverModeHasOut(mode, g_sampler_direction_in); }
static int mode_has_in(int mode) { return AudioDriverModeHasIn(mode, g_sampler_direction_in); }
```

`GetDriverModeCount()` -> `AudioDriverModeCount()` (replica golden del 5).

Los casos `U241_DEVICE_*` de deteccion de dispositivo (nombres de host
Windows/Android/SP404MKII/MIDI y FIFOs) siguen viviendo en el bridge: son
estado de deteccion runtime, no tabla declarativa (tramo F4 posterior).

### Por que es la semilla del AudioRouter

`kAudioDriverModes[]` es una tabla de backends: cada modo declara identidad,
etiquetas, token de persistencia y capacidades de direccion sin logica.
El futuro `AudioRouter` (F4b+) consumira esta tabla para seleccionar
backend, y `AudioBackend` (F4c+) envolvera los contratos del puente con
paridad de FIFO intacta.

### Evidencia del tramo

- Host test `tests/host/audio_driver_modes_host_test.cpp` (oraculos golden
  byte-identicos a los switches originales), runner
  `tests/run_host_audio_driver_modes.sh` en `scripts/audit.sh`:
  `AUDIO_DRIVER_MODES_HOST_ALL_OK (68 checks)` ASAN/UBSAN.
- Baseline `tests/test_f4a_baseline.py`: `F4A_BASELINE_OK` (capa pura sin
  GUI/audio/POSIX/daemons; literales golden fuera del bridge; delegados
  one-line; contrato de valores del enum U241_*).
- Audit completo: `AUDIT_CLEAN_MAIN_U2523_OK`.
- Build MIPS `lgpt_r36sx_u2523.so` sha256 `7709b665...` desplegado en SD
  (== build); backup `LGPT_BEFORE_U2523_20260813_220015`.
- Commit: `d2c1069` (docs/F4_ARCHITECTURE_ES.md).

## F4b - AudioCapabilities (vocabulario declarativo de capacidades)

### Que se hizo

Dos capas puras nuevas en `Application/Audio/`:

1. `AudioCapabilities.h`: el lenguaje del objetivo 6 como datos puros.
   Ocho bits de capacidad con sus nombres legibles (fuente de verdad unica
   para Help/UI/diagnostico):

   | bit | capacidad       |
   |-----|-----------------|
   | 0   | Stereo Output   |
   | 1   | Stereo Input    |
   | 2   | USB Device      |
   | 3   | USB Host        |
   | 4   | MIDI            |
   | 5   | Capture         |
   | 6   | Clock Sync      |
   | 7   | Hotplug         |

   `Clock Sync` y `Hotplug` quedan reservados: ningun modo actual los
   declara (vocabulario para backends futuros, p.ej. multitrack USB).

2. `AudioDriverModeTable.h` (F4a + extension): `AudioDriverModeCapabilities
   (mode, samplerDirectionIn)` deriva la mascara de capacidades de cada
   modo EXCLUSIVAMENTE desde primitivos golden:

   - StereoOutput = hasOut || LOCAL_CONSOLE (la consola siempre suena).
   - StereoInput = hasIn.
   - UsbDevice = WINDOWS (unico rol periferico/gadget del bridge).
   - UsbHost = ANDROID, USB_OUT, SP404_IN, MIDI (host-role devices).
   - Midi = modo MIDI.
   - Capture = hasIn && != LOCAL_CONSOLE.

   Resultado por modo (con el toggle del sampler):

   | modo      | capacidades                                   |
   |-----------|-----------------------------------------------|
   | LOCAL     | Stereo Output                                 |
   | WINDOWS   | Out + In + USB Device + Capture               |
   | ANDROID   | In + USB Host + Capture                       |
   | USB_OUT   | dir0: Out + USB Host | dir1: In + USB Host + Capture |
   | MIDI      | USB Host + MIDI                               |
   | SP404_IN  | In + USB Host + Capture                       |
   | fallback  | (ninguna; selectable=0)                       |

### Por que el bridge no se toca

`AudioDriverModeCapabilities` es una proyeccion declarativa de la misma
semantica que el runtime ya declara en `mode_has_out/mode_has_in`: no
cambia ninguna ruta.  El core MIPS resultante es byte-identico al de F4a
(sha256 `7709b665`).  F4c conectara el `AudioRouter` (seleccion de backend
consumiendo la tabla + capacidades) y ahi el puente empezara a consultar
capacidades sin cambiar comportamiento.

### Evidencia del tramo

- Host test `tests/host/audio_capabilities_host_test.cpp`, runner
  `tests/run_host_audio_capabilities.sh` en `scripts/audit.sh`:
  `AUDIO_CAPABILITIES_HOST_ALL_OK (55 checks)` ASAN/UBSAN (vocabulario,
  proyeccion por modo y consistencia caps<->hasOut/hasIn).
- Baselines: `F4A_BASELINE_OK` (ahora permite como unico include la capa
  hermana pura) y `F4B_BASELINE_OK` (vocabulario puro, derivacion solo de
  primitivos golden, bridge sin consumir capacidades).
- Audit completo: `AUDIT_CLEAN_MAIN_U2523_OK`.
- Build MIPS byte-identico `7709b665` desplegado en SD (== build); backup
  `LGPT_BEFORE_U2523_20260813_220749`.
- Commit: `fd0f124`.

## F4c - AudioRouter (politica declarativa de seleccion/routing)

### Que se hizo

`Application/Audio/AudioRouter.h` (capa pura header-only C++03): la
politica de seleccion de backend del bridge como funciones puras que solo
consumen la tabla golden de modos y el vocabulario de capacidades:

- `AudioRouteEffectiveMode(mode, dir)`: replica golden del mapeo de
  SetDriverMode — USB_OUT con direccion sampler IN se ejecuta como
  SP404_IN; cualquier otro modo es identidad.
- `AudioRouteIsHostRoleMode(mode)`: replica golden de la clasificacion
  U2.52 HOST_ROLE_MODE_ALWAYS_APPLY (ANDROID/USB_OUT/SP404_IN/MIDI exigen
  apply completo de perfil + supervisor, nunca fast apply), derivada de la
  capacidad UsbHost declarada por el modo (misma fuente de verdad que las
  capacidades — no duplica la lista).
- `AudioRouteCycleNext`/`AudioRouteCyclePrev`: secuencia UI de 5 modos
  (SP404_IN no se lista), matematica de CycleDriverMode y del modal.

### Delegacion del bridge

```c
// SetDriverMode
const int effective = AudioRouteEffectiveMode(mode, g_sampler_direction_in);
...
if (AudioRouteIsHostRoleMode(g_driver_mode)) {   // antes: 4 comparaciones
    close_fifo_if_open("fifo closed host-role apply");
    launch_apply_profile_once(effective);
...
// CycleDriverMode
const int next = AudioRouteCycleNext(g_driver_mode);
```

La politica runtime NO se mueve: debounce de 180 ms, fast-apply
(runtime_ready_fast + fifo_compatible_with_mode), close_fifo_if_open,
write_mode_file y launch_apply_profile_once siguen viviendo en el bridge.

### Paridad byte-identica

El core MIPS resultante es byte-identico al de F4a/F4b (sha256 `7709b665`):
las funciones del router son inline y compilan al mismo codigo que las
comparaciones originales.  Es la evidencia mas fuerte posible de que la
politica no cambio.

### Evidencia del tramo

- Host test `tests/host/audio_router_host_test.cpp`, runner
  `tests/run_host_audio_router.sh` en `scripts/audit.sh`:
  `AUDIO_ROUTER_HOST_ALL_OK (30 checks)` ASAN/UBSAN (mapeo efectivo,
  host-role por modo, ciclo next/prev, coherencia host-role == UsbHost).
- Baseline `tests/test_f4c_baseline.py`: `F4C_BASELINE_OK` (capa pura con
  solo los 2 includes hermanos; politica runtime del bridge intacta:
  debounce, pending, fifo, profile).
- Audit completo: `AUDIT_CLEAN_MAIN_U2523_OK`.
- Build MIPS byte-identico `7709b665` desplegado en SD (== build); backup
  `LGPT_BEFORE_U2523_20260813_221336`.
- Commit: `8962ce6`.

## F4d - AudioBackend (registro declarativo de clases de backend)

### Que se hizo

`Application/Audio/AudioBackend.h` (capa pura header-only C++03): el
registro de clases de backend del objetivo 6 como datos puros, alineado
con los daemons reales del port:

| clase              | modos que sirve      | daemon real               |
|--------------------|----------------------|---------------------------|
| LocalAudioBackend  | LOCAL_CONSOLE        | (consola, sin USB)        |
| WindowsUac2Backend | WINDOWS              | r36s_u2523 (gadget UAC2)  |
| AndroidBackend     | ANDROID              | host-role AOA input-only  |
| Sp404Backend       | USB_OUT, SP404_IN    | r36s_sp404_host_audio_io  |
| MidiBackend        | MIDI                 | r36s_midi_host_io         |

Declara tambien:

- Contrato de operaciones de la interfaz `AudioBackend` (objetivo 6):
  `open`, `start`, `caps`, `stream`, `write`, `close` (nombres como
  fuente de verdad unica).
- `AudioBackendClassForMode(mode)`: mapa modo -> clase (replica del
  runtime real).
- `AudioBackendClassName(mode)`: nombre de la clase de backend del modo.
- `AudioBackendClassCapabilities(class)`: capacidades agregadas de la
  clase como UNION derivada de las capacidades de sus modos (ambas
  direcciones del sampler), nunca declarada a mano — misma fuente de
  verdad que AudioDriverModeCapabilities.

### Por que el bridge no se toca

El registro es la declaracion de arquitectura: el puente UAC2 (ruta
estable) no lo consume todavia.  AudioEngine (F4e) sera quien envuelva el
puente y use AudioBackend + AudioRouter + AudioCapabilities sin cambiar
datos ni timings.  El core MIPS resultante es byte-identico (sha256
`7709b665`).

### Evidencia del tramo

- Host test `tests/host/audio_backend_host_test.cpp`, runner
  `tests/run_host_audio_backend.sh` en `scripts/audit.sh`:
  `AUDIO_BACKEND_HOST_ALL_OK (48 checks)` ASAN/UBSAN (registro, mapa
  modo->clase, contrato de ops, capacidades por clase, consistencia
  subset-ALL).
- Baseline `tests/test_f4d_baseline.py`: `F4D_BASELINE_OK` (capa pura con
  solo los 3 includes hermanos; capacidades derivadas, sin literales de
  mascara; bridge sin consumir AudioBackend).
- Audit completo: `AUDIT_CLEAN_MAIN_U2523_OK`.
- Build MIPS byte-identico `7709b665` desplegado en SD (== build); backup
  `LGPT_BEFORE_U2523_20260813_223016`.
- Commit: <sha F4d>.
