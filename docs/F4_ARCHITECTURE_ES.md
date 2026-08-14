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
- Commit: <sha F4a> (docs/F4_ARCHITECTURE_ES.md).
