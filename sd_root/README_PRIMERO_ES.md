# LGPT R36SX Bacon 1.5 — Synths nativos + multitrack + menú Sinth + EQ8 en Instrument (items 6-9)

Release de desarrollo sobre la arquitectura de Bacon 1.4 (ABI7, audio
48 kHz stereo, cuatro modos de audio USB Local / Windows / Android /
Sampler SP404MKII, frontend-safe y sin escrituras a la SD en runtime).

## Estado actual (items 6-9 + FAST_MATH + crash dump hex + fix diálogo, 18/08/2026)

Core **compilado e instalado** en esta SD (`cubegm/cores/lgpt_r36sx_port_libretro.so`,
SHA256 `9faaa7134204832e0fe9aa3de47525257b3bdceb848c03cedb752a6741418f72`).
Build device: `DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS`, `BUILD_U2523_OK`; regresión
host `AUDIT_CLEAN_MAIN_U2523_OK` + `F10_BASELINE_OK`. Daemons y módulo UAC2 sin
cambios (baseline Bacon 1.4, hashes `f7140072`/`968dfa61`/`3f0ea7a2`/`e9062ac5`).

## Fix DIÁLOGO PROYECTOS — crash SIGSEGV en el menú de inicio (18/08/2026)

Diagnóstico forense de los 5 dumps de `LGPT_OTG_LOGS/crash.txt` (disasm del core
viejo `2f14c0ac` y del nuevo `afcf5ba7`): **los 5 SIGSEGV eran en
`SelectProjectDialog`**, no en libm:

- dumps 1-3 (core viejo): `OnRenameProject` — `pc` agrupado en 0x6F574/0x6F6B4/
  0x6F6C4 (misma función), escritura con puntero NULL (`sb v0,32(s6)` con s6=0)
  y scan anidado que rebasaba el iterador de la lista tras el rename.
- dump 5 (core nuevo): `ProcessButtonMask` branch A→Load — `Path::GetName`
  (`lw s0,4(a1)` con a1=0) cuando la lista está vacía o el cursor quedó stale
  (pulsación de A rápida justo al abrir el diálogo de proyectos).

Causa: el diálogo se abre al arrancar (y en Open Project); con la lista de
proyectos vacía o `currentProject_` fuera de rango, los branches de
Load/Rename/Delete desreferenciaban un `Path *` nulo → SIGSEGV → CrashTrap
`_exit` → picoarch reiniciaba el core → audio muerto ("el instrumento dejó de
sonar") y estado perdido.

Fixes aplicados (`TREEFROG_DIALOG_NULL_GUARD_V1`):
- Branch A→Load: guard `if (current == 0) return;` (mismo patrón que el branch
  de borrado, que ya lo tenía).
- `OnRenameProject`: guard de ruta vacía (lista vacía/cursor stale) + el scan
  O(n²) anidado se reemplaza por un pase único con `foundIndex` (sin rebasar el
  iterador).
- `warpToNextProject`: con lista vacía el wrap dejaba `currentProject_` negativo
  (luego usado como índice de iterador en Draw/Load); ahora clamp a 0.

## Fix FAST_MATH — audio estable (16/08/2026)

Reporte en consola: "lag, sonido no estable" en modo Local Console, silencio en
la preescucha B de Bass/Piano, al asignar instrumentos a Phrase y tras editar
el EQ8. El lag venía de los sintetizadores llamando `sinf`/`powf`/`expf`
**por muestra** en el render (el R36S tiene una MIPS débil: son caros). La
causa de los SIGSEGV se re-diagnosticó después como el fix del diálogo de
arriba (los `pc` de libm del reporte original eran en realidad del core, en
`SelectProjectDialog`; los crashes de libm no se reprodujeron).

- **BassSynth**: oscilador seno, LFO (CUT/VOL/PIT) y pan equal-power pasan a
  tablas interpoladas compartidas (`SynthMath.h`: 1024 entradas de seno + 256
  para 2^x, el mismo patrón que PianoSynth ya usaba para sus parciales); el
  factor de decay del glide se hoistea fuera del bucle por muestra (era
  `expf` por muestra; ahora uno por buffer, bit-identical).
- **PianoSynth**: el pan (últimos `sinf`/`cosf` por buffer) usa también la
  tabla compartida.
- **CrashTrap**: el formatter hex escribía `"0x"` después de los dígitos
  (valores ilegibles tipo `724d69ac0x` en crash.txt); el dump ahora se puede
  parsear para futuros diagnósticos.
- Sin cambios de sonido perceptibles (error de interpolación < 1e-6) ni de
  persistencia; cero llamadas libm en el bucle por muestra de los synths.

## Qué añade esta release (último corte, item 9 + feedback)

- **Menú "Sinth" en el navegador del instrumento (Listen/Import/Manage/
  Sinth/Exit)**: Sinth abre un selector Bass/Piano (flechas + A; B cancela).
  Al confirmar, el slot actual se convierte a `IT_SYNTH` o `IT_PIANO` con el
  mismo protocolo seguro del selector `src` (detach del observer, Stop,
  `SetInstrumentType`, rebuild de la página). Con un sample asignado se
  bloquea con el aviso "Clear the sample to switch to a synth" y el
  navegador permanece abierto.
- **Preescucha con B en las páginas Bass/Piano**: B reproduce una nota del
  instrumento activo con su configuración actual (bass en C3, piano en C4);
  otro B la corta al instante, se auto-corta a los ~0.9 s y cualquier Start
  de reproducción la cancela. En la página Sample B mantiene su
  comportamiento original (sin acción).
- **EQ8 en el formulario Sample**: la sección heredada PLAYBACK
  (interpolation/loop mode/slices/start/loop start/loop end) se eliminó del
  editor y en su lugar va el bloque EQ8 (fila `EQ8` + `mask`, header "EQ8");
  A abre el editor gráfico `InstrumentEqModal` como en Bass/Piano. El menú
  L1+A del Mixer (TRACK) ahora salta a la sección EQ8 en vez de PLAYBACK.
  Los parámetros de PLAYBACK siguen existiendo en el modelo (persistencia y
  reproducción sin cambios), solo se quitan de la edición en pantalla.
- **Optimización del hilo de audio** (H38.8): el scratch de suma de módulos
  de `AudioMixer::Render` era `malloc` por buffer → ahora es un buffer
  estático (sin allocations en el audio thread); `pow(x,3)` del softclip →
  `x*x*x`; el `fmodf` del square wave del BassSynth → rama equivalente
  (bit-identical). Sin cambios de sonido ni de persistencia.

## Qué incluye esta release (items 6-8)

- **Item 6 — BassSynth nativo** (slots `0x90`-`0x9F`, tipo `IT_SYNTH`): mono por
  canal, osc PolyBLEP saw/square/tri/sine sin aliasing, subosc square, noise
  LFSR xorshift32, amp ADSR + filter ADSR por etapas, Filter V2 TPT SVF
  LP/HP/BP/NOTCH por canal a control rate, drive soft-clip, accent, LFO
  0-20 Hz (CUT/VOL/PIT), pan equal-power, EQ8, sends dry/delay/reverb, tabla +
  automatización. Comandos: VOLM/PAN/FCUT/FRES (0-255→0-100), PTCH (bend
  ±12 st), LEGA, DLYS/RVBS en vivo.
- **Item 7 — PianoSynth nativo** (slots `0xA0`-`0xAF`, tipo `IT_PIANO`): aditivo
  con tabla seno 256 (band-limited, sin aliasing), 4 voces por canal, modos EP
  y TINE (parciales armónicos), velocity, pitch bend, width Haas (~21 ms),
  Filter V2 por canal, sustains, re-strike y voice-stealing.
- **Item 8 — Multitrack / stems de export**: además de `channel0-7.wav` el
  render multipista escribe `project:delayret.wav`, `project:reveret.wav` y
  `project:master.wav`. Guard de espacio libre: el render aborta si quedan
  < 128 MB libres en la SD (`FileSystem::GetFreeSpace`, statvfs).
- **Selector `src` en InstrumentView (Bass/Piano)**: la fila `src` cicla
  Sample/Bass/Piano con las flechas. Con un sample asignado la conversión a
  sintetizador se bloquea con "Clear the sample to switch to a synth".
- **EQ8 gráfico fullscreen por instrumento**: en las formas Bass/Piano y Sample
  la fila `EQ8` abre con A el editor `InstrumentEqView` (reemplaza el modal).
  La curva se dibuja con los mismos coeficientes (`GetBandCoeffs`) que procesa
  el DSP y el canvas cubre toda la pantalla bajo la cabecera (sin letras
  residuales). A+B resetea la banda seleccionada a su valor por defecto. La
  preescucha suena por un canal de audición aislado a C-3 (+6 dB fijos, sigue
  sonando con la pista muteada/volumen 0) y el análisis de espectro se toma
  del MIX final del master (post-FxEngine), no del instrumento. Guard de
  estabilidad RBJ en `EqBiquad.h`: los boosts del peaking en baja frecuencia
  quedan limitados al 99.5% del margen (solo BELL) para que el filtro nunca
  diverja (aplica a InstrumentEq y ParametricEQ).
- **42 warnings del host syntax check resueltos** (GCC 13 host, GCC 6.3
  MIPS): firmas `const` de `FileSystem::Open`/`GetContent` y adaptadores,
  `WatchedVariable` (sobrecarga de listas), `Status::Set`,
  `Tiny2NosStub`/`TreeFrogUac2Bridge`/`UsbRecordModal`/`SampleChopperModal`/
  `ImportSampleDialog` (truncado explícito de snprintf) y `ChopperView.h`
  (guard `-Wformat-truncation` para GCC >= 7).

## Build del core (items 6-8)

En la máquina de compilación con la toolchain sf3000 (mipsel):

```bash
cd /mnt/d/R36S/PORT\ LPTRACKER/GITHUB/lgpt-r36sx-port
bash scripts/build.sh
```

El core queda en `BUILD/U2523/lgpt_r36sx_u2523.so`. Copiarlo a la SD:

```bash
cp BUILD/U2523/lgpt_r36sx_u2523.so sd_root/cubegm/cores/lgpt_r36sx_port_libretro.so
```

## Pruebas a hacer en consola (items 6-8)

1. **BassSynth**: crear instrumento en un canal, tipo SYNTH (slots 0x90-0x9F).
   Probar las 4 formas de onda (saw/square/tri/sine) sin aliasing en agudos;
   subosc, noise, glide/portamento (PTCH/LEGA), accent, LFO sobre CUT/VOL/PIT,
   drive, filtro LP/HP/BP/NOTCH, ADSR de amp y de filtro, pan, EQ8, sends.
2. **PianoSynth**: tipo PIANO (slots 0xA0-0xAF). Modos EP y TINE (parciales 1P
   a 4P), timbre, pdecay, width Haas, velocity, sustains (colas naturales),
   acordes de 4 notas (voice-stealing), filtro y sends.
3. **EQ8**: 8 bandas con la página EXT (5 bandas adicionales: frecuencia,
   ganancia, Q, tipo, bypass con crossfade) en Master y en instrumento.
4. **Sends**: dry 0-100 y delay/reverb -1..100 (0xFF hereda del track) en
   BassSynth y PianoSynth; hear return delay y reverb en el send master.
5. **Export multitrack**: render en modo MULTITRACK → `channel0-7.wav`,
   `delayret.wav`, `reveret.wav` y `master.wav`; comprobar longitudes iguales,
   que master = mezcla de canales + returns, y que el render aborta con aviso
   si la SD tiene < 128 MB libres.
6. **Regresión**: modos de audio (Local/Windows/Android/Sampler), Mixer,
   Chopper, persistencia de proyectos y arranque en frío.
7. **Selector `src`**: en una forma Bass/Piano, con las flechas en `src`
   ciclar Sample→Bass→Piano; comprobar que al pasar a Bass/Piano se
   despliega la página de parámetros del sintetizador y que con un sample
   asignado aparece "Clear the sample to switch to a synth" (no convierte).
8. **EQ8 de instrumento**: sobre la fila `EQ8` (Sample, Bass y Piano) pulsar
   A para abrir el editor gráfico (`INSTR EQ8  INS-xx`), editar bandas y
   comprobar que el layout no solapa la banda map/notas (y=27).
9. **Menú Sinth**: en Instrument, A sobre el instrumento → navegador → a la
   derecha de Manage la opción `Sinth` → A → selector Bass/Piano (flechas
   arriba/abajo, A confirma, B cancela). Confirmar Bass: el slot pasa a ser
   `SYNTH`, se ve la página de parámetros y se puede programar desde Phrase.
   Con un sample asignado: aviso "Clear the sample to switch to a synth" y
   el navegador se queda abierto.
10. **Preescucha con B**: en una página Bass/Piano (proyecto detenido), B
    suena una nota (bass C3 / piano C4) con el sonido actual; otro B la
    corta; esperar ~0.9 s para el auto-corte; Start corta la preescucha. En
    Sample, B sigue sin hacer nada.
11. **Menú TRACK del Mixer (L1+A)**: la fila EQ8 salta al campo EQ8 del
    instrumento del canal (antes PLAYBACK); comprobar que cae en la fila
    `EQ8` y que A abre el editor gráfico.
12. **EQ8 completo en sinths (U2.52.5)**: en Bass/Piano las 8 bandas editan
    (X+UP/DOWN ganancia, B tipo, L1+R1 frecuencia, Q), la curva se actualiza
    en vivo al editar, el canvas muestra el rango ±24 dB (grilla ±12/±24) y
    NINGUNA edición corta el sonido del preview ni de la frase. El volumen
    del sinth sostenido iguala al de una sample a volumen máximo (sustain
    por defecto 100).
13. **EQ8 transparente a 0 dB (U2.52.6)**: cualquier modo (BELL/LOWSH/HISHE/
    LOWPA/HIPAS/NOTCH/BANDP) con ganancia 0 dB NO altera el sonido — el
    filtro solo entra al mover la ganancia fuera de 0. En samples: cambiar
    el modo con 0 dB ya no mata el sonido (antes LOWPA a 80 Hz cortaba todo).
14. **Espectro 20 Hz-20 kHz (U2.52.6)**: las barras bajo el canvas del EQ8
    miden graves reales: un kick ilumina solo las barras bajas; un tono de
    1 kHz deja apagadas las de <300 Hz (antes cualquier sample encendía casi
    todas). Barra llena = 0 dBFS.
15. **Sinths +6 dB (U2.52.6)**: el sinth sostenido (saw por defecto) suena
    ~4 dB más fuerte que antes — a la par de las samples; pico 1.0 full
    scale, drive 0..100 de "boosteado" a "clip duro".

## Bugs conocidos

## Estado actual (U2.54, 14/08/2026)

Build marcada como lista para release tras prueba en consola completa
(arranque, navegación, Mixer, Chopper con Pitch/Env, audio Windows y
modos de audio). Core `e4fbbdc8`, daemon `f7140072`.

## Qué corrige esta release

- **Audio Windows entrecortado**: corregido el lag/backpressure del
  driver Windows UAC2 (dos fixes: estabilidad del stream de salida y
  arranque del modo USB). Confirmado en consola con PC.
- **Panel Pitch/Env del Chopper**:
  - Ahora se muestra con el proyecto detenido (antes no aparecía).
  - Texto legible: la fuente TreeFrog es scanline-major con 1 byte por
    píxel y semántica ZERO_IS_INK; el render del panel usa la misma
    semántica que el resto de la UI.
  - Pantalla completa (0,0,320,240): tapa la cabecera del chopper
    (Graphical chopper, inst, sampl, zoom, name, frame, chop), el status
    y los hints del char screen; título, header, ítems, hints y status
    se redibujan dentro del panel, coherentes con el resto de los menús
    (fila editada invertida en HILITE2).
- **Crash del preview de pitch** (rewrite del WAV compartido mientras el
  stream seguía activo): corregido (Stop antes del rewrite, U2.52.0).

## Bugs conocidos

- **Crash del Chopper (frío, one-shot)**: tras el arranque en frío, el
  primer edit destructivo puede crashear de forma puntual; no
  reproducible al repetir la operación. Sospecha: contención
  daemon/SD en frío, heredado de Bacon 1.3.

## Contenido del paquete

- `cubegm/cores/lgpt_r36sx_port_libretro.so` — core ABI7 (4 modos) Bacon 1.4.
- `cubegm/lgpt` — launcher del port.
- `lgpt/otg/bin/r36s_u241_usb_audio_io` — daemon Windows/origen UAC2.
- `lgpt/otg/bin/r36s_sp404_host_audio_io` — daemon SP404MKII (Sampler, UAC2 host).
- `lgpt/otg/bin/r36s_midi_host_io` — daemon USB-MIDI host.
- `lgpt/otg/bin/otg_*.sh` — selector de driver ABI7, supervisores y perfil UAC2.
- `lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36` + `r36s_aoa_bulk_receiver_h36` — daemons Android h36.
- `lgpt/otg/modules/4.4.186-release/` — módulos kernel ALSA/UAC2 48k stereo
  (`host_usb_audio` y `u2_38au8_sync_uac2`).
- `ANDROID/LGPTUsbAudioBridge-H36-debug.apk` y
  `ANDROID/LGPTUsbAudioBridge-H38-debug.apk` — APK del puente de audio Android.
- `LGPT_OTG_LOGS/` — directorio de logs del runtime (vacío; todo tmpfs).
- `roms/lgpt/` — projects/samples de arranque.
- `lgpt/` — config, projects, samples, instruments (stock).

## Instalación en la SD (copiar carpetas a la raíz)

1. Partir de una SD con sistema Stock y TreeFrogUI (o conservar la actual).
2. Extraer el ZIP de la release.
3. Copiar las carpetas `cubegm`, `lgpt`, `roms`, `LGPT_OTG_LOGS` y
   `ANDROID` del ZIP directamente a la raíz de la SD, combinando
   carpetas y reemplazando archivos.
4. Instalar `ANDROID/LGPTUsbAudioBridge-H38-debug.apk` en el dispositivo
   Android (solo para el modo Android).
5. Expulsar la SD de forma segura.
6. Iniciar LGPT desde TreeFrogUI.

## Modos de audio

1. **Local** — salida local de la consola.
2. **Windows** — LGPT como adaptador USB-C audio-out para PC.
3. **Android** — puente de audio USB con el dispositivo Android vía APK.
4. **Sampler** — SP404MKII como sampler (EXT SOURCE), entrada USB-UAC2.

## Checksums

```text
CORE   (lgpt_r36sx_port_libretro.so)  2f14c0ac93c6d6192ac57d246de341ef410a98127ef8befdd1b75e0061e02c8d
DAEMON (r36s_u241_usb_audio_io)       f7140072f9b83573e03caf904d17de6227374823c3719757c7d11a438bb1417d
SP404  (r36s_sp404_host_audio_io)     968dfa61e561d348fd4ec8006e39b23b4dd56a49f1912f885c2731f118983b83
MIDI   (r36s_midi_host_io)            3f0ea7a23db7390f1fb3b73cbda97f66316c6568d0c7574b838579a014baee80
MODULE (usb_f_uac2.ko U2414)          e9062ac5a37aa7706c98a3411a57dbe67bc98ef82621c430c1d4e7a89b2fbefe
APK    (LGPTUsbAudioBridge-H38)       89a99d50571948788f1f0d2a3c1ebf313e6bf7a531874b5fc0885ba754ef1c3a
```