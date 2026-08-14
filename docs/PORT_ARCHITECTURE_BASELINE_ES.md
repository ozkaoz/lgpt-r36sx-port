# PORT_ARCHITECTURE_BASELINE_ES

Mapa de referencia del port LGPT R36SX en su estado estable (Bacon 1.2.1) y
deuda/riesgos detectados. Base inamovible para el refactor por preservación.

Actualizado: 2026-08-13 (commit c3b4b0e, rama refactor/bacon-1.2.1-preserve)

## 1. Punto de partida (golden)

- Tag inmutable: `golden-bacon-1.2.1` == `951b7b3` (U2.72 H43, release Bacon 1.2.1).
- Rama de trabajo: `refactor/bacon-1.2.1-preserve` (en `c3b4b0e`).
- Regla del proyecto: ningun cambio de comportamiento (sonido, controles,
  navegacion, timings, sampler, USB, SD, compatibilidad). Evidencia por
  commit: `scripts/audit.sh` verde + razon escrita.

## 2. Gate de regresion (como correr)

1. `bash scripts/generate_golden_legacy.sh` — genera los 13 WAVs de
   `validation/PHASE0_GOLDEN/` con los modelos Python del repo (determinista;
   la carpeta esta en .gitignore, es artefacto regenerable, no se commitea).
2. `bash scripts/audit.sh` — 226 chequeos, salida 0. Los tests estaticos
   (`tests/test_*.py`) validan la realidad del codigo dorado; los que fueron
   escritos para fases RC anteriores y quedaron obsoletos se actualizaron a
   esa realidad en c3b4b0e (explicado en el mensaje del commit).

## 3. Mapa de la arquitectura original (Bacon 1.2.1)

### Ejecucion
- Core libretro MIPS32 r2 single-thread: `retro_run()` -> `AppWindow::DoEventLoop()`
  (`source/sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp`).
- Daemons en espacio de usuario fuera del core:
  - `device/r36s_sp404_host_audio_io.c` (3275 lineas; ASRC ~930-1100,
    REENUM ~2890): rol principal (USB STEREO_48K).
  - `device/r36s_u2523_usb_audio_io.c` (2047 lineas): USB directo (u2.52.3).
  - Supervisor + scripts de arranque en `sd_root`/`device`.
- Contrato de audio interproceso: 48000 Hz / 2ch / S16_LE; transporte
  FIFO + ring + ASRC; doc canonica: `docs/AUDIO_ARCHITECTURE_MAP_ES.md`.

### Capas del codigo
- `source/sources/Adapters/TREEFROG/` — acoplamiento al hardware:
  - `Audio/TreeFrogUac2Bridge.cpp` (2757): FIFO, modos, fast-apply condicionado
    a compatibilidad de FIFO.
  - `Audio/TreeFrogAudioDriver.cpp`: pupilas de audio del app (LOCAL_CONSOLE /
    WINDOWS / ANDROID / USB_OUT(MIDI)).
  - `System/TreeFrogSystem.cpp`: raiz `/mnt/sdcard/lgpt` (lineas 26-28),
    alias bin/root/project/samples; `bin:config.xml` y `bin:last_project`
    (AppWindow.h:136, SaveLastProject AppWindow.cpp:1473+) son las dos
    escrituras persistentes conocidas.
  - Codigo muerto confirmado: `TreeFrogWindowsSpscTransport.cpp` (0 callers).
- `source/sources/Application/` — UI y logica:
  - `Views/` — vistas de pantalla:
    - `ModalDialogs/SampleChopperModal.cpp` 3115 (mayor del codigo),
      `UsbRecordModal.cpp` 1412, `MixerActionMenuModal` (menus).
    - `MixerView.cpp` 1799, `PhraseView.cpp` 1901, `InstrumentView.cpp`.
  - `Views/BaseClasses/` — `View.cpp` (392), `HelpOverlay.cpp` (202),
    `HelpRegistry.cpp` (194, contexto/secciones), `ModalView.cpp` (62).
  - `Controllers/`, `Commands/`, `Audio/` (audio del app), `MixerService`.
- `source/sources/Adapters/Unix/FileSystem/UnixFileSystem.cpp` — fopen.
- `source/sources/Adapters/Unix/Audio/system.c` etc. — SDL/ALSA para builds de host.

### Input (estado actual, punto de partida de la politica de input)
- Cada view procesa `ProcessButtonMask(mask, pressed)` con mascaras `EPBM_*`
  y acordes hardcodeados en la vista (ej. MixerView: L2+A+B reset de pan,
  L1+A menu de accion, R2 solo cicla edicion; Chopper: L1+X undo, R1+X redo).
- AppWindow resuelve hombros latcheados y repite la resolucion en
  ET_PADBUTTONDOWN (fix Bacon 1.1.1 V16); un hombro suelto se retiene un poll.
- No existe una tabla central de acordes; la Fase 1 (ActionId) parte de AQUI
  con un catalogo explicito de los acordes vigentes (B preview, A apply,
  L1+X undo global, R1+X redo, L1+A menus, L2+A+B pan reset, R1+RIGHT usb rec,
  SELECT+R1 help, SELECT+R2 audio latch, etc.).

## 4. Problemas y deuda detectados

1. Sin StorageService: `config.xml`/`last_project` escritos directo y sin
   inventario completo de accesos persistentes (los daemons tambien abren
   archivos de log en la SD: LGPT_OTG_LOGS/).
2. Binding de input repartidos y no catalogados; sin politica formal de menus
   ni stack de navegacion (NavigationController) — los modales se empujan con
   DoModal/View::PushModal ad-hoc.
3. Clases grandes: SampleChopperModal (3115), PhraseView (1901),
   MixerView (1799) mezclan dibujo, input, undo y logica de negocio.
4. Audio: capacidades de cada backend dentro de un solo driver; el puente
   mezcla batching/ASRC con politica de modo; sin interfaz de backend
   extensible (objetivo: AudioEngine -> AudioRouter -> AudioBackend).
5. Codigo muerto / residuos: TreeFrogWindowsSpscTransport.cpp; warnings del
   build de host u2523 (`starvation_silence_periods` y
   `producer_burst_frames` sin usar, lineas 1709/1755).
6. Tests: solo estaticos de texto + generar WAVs; no hay harness de regresion
   funcional del core en host (host_syntax_check compila parcial).
7. Riesgo operativo: core MIPS sin debugger; cualquier cambio en el camino
   critico de audio (retro_run -> FIFO -> daemon) exige prueba en consola.

## 5. Compromiso y criterios de exito

- Cada fase del roadmap termina con: codigo equivalente, audit verde,
  y (para fases que tocan audio/input) validacion en la consola real.
- Los tests staticos evolucionan como red de seguridad; nuevos componentes
  llevan tests host cuando tengan logica pura (resolucion de acordes, politica
  de menus, rutas de StorageService).