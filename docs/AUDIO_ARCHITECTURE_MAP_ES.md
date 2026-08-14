# Mapa de arquitectura de audio — LGPT R36SX (U2.71 + H40/H41/H42)

Estado: auditado sobre fuentes live del 2026-08-13; fixes H40-H42 desplegados y documentados.
- Core: `source/sources/Adapters/TREEFROG/` (rama `stabilize-bacon-1.2.1`)
- Daemons live: `/home/dafunknoise/sp404_abi7/` y `/home/dafunknoise/asrc_abi7/` (equivalen a `device/*.c`)
- Scripts desplegados: `/mnt/sdcard/lgpt/otg/bin/` (SD live) y `device/*.sh`
- Kernel: 4.4.186-release, `rt305x_defconfig`, MIPS32 R2 LE, módulos en `/mnt/sdcard/lgpt/otg/modules/4.4.186-release/`

---

## 0. Formato canónico

**48000 Hz / 2 canales / S16_LE / interleaved** (4 B/frame). El mono 48k solo existe como modo de recuperación explícito (perfil `MONO_48K`). Todo el transporte interno usa este contrato; no existe resampling 44.1→48 en el hot path (el resampler 160/147 del bridge degenera a identidad cuando `g_engine_rate==48000`, y la fase 0 del FIR es un impulso exacto → sample-exact a 1:1).

## 1. Componentes

| Componente | Archivo | Rol |
|---|---|---|
| Frontend picoarch | `/mnt/sdcard/cubegm/picoarch` (binario) | llama `retro_run`, `audio_batch_cb`, reloj del bucle |
| Core libretro | `Main/TreeFrogLibretro.cpp` | `retro_run`: budget wall-clock, render por chunks ≤2048, `audio_batch_cb` |
| Driver audio del core | `Audio/TreeFrogAudioDriver.cpp` | `Render()`: consume buffers del pool, submit al bridge, mezcla monitor |
| Bridge UAC2 | `Audio/TreeFrogUac2Bridge.cpp` (2731 líneas) | productor SPSC: `SubmitStereo48000` → resample opcional → write no bloqueante **con staging H40 (partial-write sin pérdida)** al FIFO; monitor; buzón de captura |
| SPSC transport | `Audio/TreeFrogWindowsSpscTransport.cpp` | **código muerto** (0 callers, compilado) — referencia de diseño |
| Daemon Windows | `r36s_u2523_usb_audio_io.c` (2069 líneas) | gadget UAC2 full-duplex: FIFO→ring→ASRC→ALSA pcmC0D0p; captura pcmC0D0c→WAV/monitor |
| Daemon SP404 | `r36s_sp404_host_audio_io.c` (3228 líneas) | host snd-usb-audio: FIFO→ring→ASRC→ALSA SP404; captura SP404→WAV/monitor |
| Daemon MIDI | `r36s_midi_host_io.c` | rawmidi → FIFO |
| Daemon AOA | `r36s_aoa_bulk_audio_io_h36` | Android AOA bulk → FIFO |
| Supervisor host | `otg_h37_host_runtime_supervisor.sh` | orquesta SP404/MIDI daemons, gadget teardown, live_flush |
| Selector de modo | `otg_h37_apply_driver_mode.sh` | LOCAL_CONSOLE / WINDOWS / ANDROID / USB_OUT(SP404) / MIDI |
| Setup Windows | `otg_u241_setup_once.sh` + `common.sh` + `apply_profile_once.sh` | gadget configfs `r36sx_lgpt_u2414`, insmod, daemon |
| Launcher | `lgpt_launcher_u241.sh` | copia root, config, arranca picoarch+core |
| Módulos kernel | `u2_38au8_sync_uac2/` (gadget: `usb_f_uac2.ko` 48k-stereo, snd*, snd-pcm, snd-timer, soundcore) y `host_usb_audio/` (snd-usb-audio.ko, snd-usbmidi-lib.ko, snd-rawmidi.ko, snd-hwdep, snd-seq-device, snd*, snd-pcm, snd-timer) | 4.4.186-release |

## 2. Rutas de audio (hop a hop)

### 2.1 LGPT → salida local de consola
`retro_run` (60 fps, budget wall-clock 48000·dt) → `TreeFrogAudioDriver::Render(dst, frames)` → `consumeOneFrame` (pool de 50 buffers, 2 lazy requests/Render) → `SubmitStereo48000` → resample (identidad) → FIFO de destino según modo (ninguno en LOCAL_CONSOLE: `fifo_fd=-1`, no escribe) → mezcla `MixUsbCaptureMonitorStereo48000` (mezcla captura USB si monitor ON) → `audio_batch_cb` → picoarch → códec SF3000 (48k).
- Sin threads en el core; un solo hilo (retro_run), callback síncrono.
- `GetClock()` = `gettimeofday` (wall clock real). Dominio de reloj: cristal de la consola.

### 2.2 LGPT → Windows (USB Audio)
`retro_run` → Render → bridge escribe `/tmp/r36sx_uac2_bridge_fifo` (O_NONBLOCK; **H40: staging de partial-write** — si el write acepta solo una parte, el resto queda en `g_fifo_pending` (cap 16384 samples ≈ 341 ms, drop-oldest acotado con contador) y se drena al inicio del next submit) → daemon U2523: `drain_fifo` (reads 4096 B hasta EAGAIN) → ring 65536 samples → ASRC (stage 8192 frames, FIR Lanczos-4 8-taps 160 fases) → ALSA `/dev/snd/pcmC0D0p` (endpoint capture del gadget, "Mic Windows") 48k S16_LE, period 480 (240 lowlat), 4 periods, `write_frames_exact` (EINTR/EAGAIN retry, EPIPE→recover in place, cierre→reopen).
- **H41: ASRC PI siempre activo (eliminado `ASRC_PASSTHROUGH_NOW`).** El reloj del consumidor es el SOF del PC; el del productor es el wall clock de la consola → 2 dominios: el PI (target 2400, prop 1200, int 3, ±1200 ppm, fase 0 = impulso exacto → sample-exact en 1:1) corrige el drift lento y acota el ring. Reset del PI en cada reconnect/resync.
- Full-duplex: Windows→R36SX por el endpoint feedback/capture → `/dev/snd/pcmC0D0c` → WAV **solo tmpfs (H42, sin fallback SD)** + monitor FIFO `/tmp/r36sx_usb_capture_monitor_fifo` + VU levels en tmpfs.

### 2.3 Windows → R36SX → USB-REC
PCM del PC → pcmC0D0c (UAC2 capture) → `capture_tick` (period 480) → `write_all_bytes` al WAV en `/tmp/r36sx_lgpt_record/take_*.wav` → meta/estado en `/tmp/r36sx_lgpt_usb/usb_capture_{meta,status,level*}` → core lee (snapshot cada 2 frames) → Preview (streaming `AudioFileStreamer` desde tmpfs) → Save (staging `*.h32part.*` + fsync + rename atómico → `/mnt/sdcard/lgpt/samples/records/`) → `SamplePool::ImportSample` → copia a `samples/` del proyecto + asignación al instrumento. Discard: `unlink` del tmpfs solamente.
- **H42:** sin fallback SD — `open_capture_wav_with_fallback` solo escribe el path tmpfs solicitado; si falla, error limpio logueado al modal (v:180+ del daemon Windows, idem SP404).

### 2.4 Android → R36SX → USB-REC
AOA (Android Open Accessory) → `r36s_aoa_bulk_receiver_h36` → `/tmp/r36sx_aoa_bulk_pcm_fifo` → core detecta `U241_DEVICE_ANDROID` → daemon AOA (`r36s_aoa_bulk_audio_io_h36`) escribe WAV/monitor igual que 2.3. Bridge: `mode_has_in(ANDROID)=true`, sin OUT. Supervisor `otg_h37_android_runtime_supervisor.sh`.

### 2.5 LGPT → SP-404MKII (USB Host)
`retro_run` → bridge (device SP404) → `/tmp/r36sx_sp404_pcm_fifo` → daemon SP404 (`r36s_sp404_host_audio_io`, arg4=2 canales): drain→ring 65536→**ASRC PI siempre activo** (target 3600, ±30000 ppm, prop 6000, int 120 — tuning U2.71) → ALSA card del SP404 (`/dev/snd/pcmC%dD0p`, snd-usb-audio host) → SP-404 en `EXT SOURCE`.
- Reloj del consumidor: host controller de la consola (cristal de la consola = mismo dominio que el productor). Con el fix H39 del core (entrega exacta 48k), el PI converge a paso ≈0 ppm.
- Detección: `otg_h37_host_device_detect.sh` → `sp404_card`, `sp404_usb_id`, `sp404_playback_pcm` en tmpfs; guards: `GADGET_GUARD` (nunca streamear al gadget propio), `FIFO-GUARDIAN` (reader descarte solo si daemon muerto).
- Captura SP404→USB-REC: misma infra `passive_monitor_tick`/`capture_tick` sobre `/dev/snd/pcmC%dD0c`.

### 2.6 USB-REC → Preview → Save → sampler
Ver 2.3. Cero escrituras de PCM del core (el WAV lo escribe el daemon). `Discard` = unlink tmpfs. `Save` = única publicación a SD (staging+rename atómico) + copia al proyecto.

### 2.7 Capturas adicionales SP-404MKII
`passive_monitor_tick` (monitor en vivo: pcmC%dD0c → monitor FIFO → `MixUsbCaptureMonitor` en el core, mezcla aditiva 75 %, prebuffer 960 samples, ring 32768). La misma pcm sirve a `capture_tick` para USB-REC (handoff de descriptor `CAPTURE_PCM_HANDOFF_FROM_PASSIVE`).

## 3. Dominios de reloj (crítico)

| Trayecto | Productor | Consumidor | Dominios |
|---|---|---|---|
| Core→SP404 | wall clock consola | host controller consola (SOF) | **mismo cristal** (≈0 ppm) |
| Core→Windows | wall clock consola | SOF del PC | **2 dominios** (±50-200 ppm reales) |
| Windows→captura | SOF del PC | wall clock consola (sincronía del gadget) | 2 dominios, absorbe el gadget |
| Android→captura | reloj del teléfono (AOA bulk) | wall clock consola | 2 dominios, bulk = no isócrono |

**Gap cerrado (H41):** Windows playback ahora corre el micro-ASRC PI siempre (ppm lentos basta para 2 dominios); el SP404 daemon ya tenía PI activo. Fase 0 del FIR = impulso → salida sample-exact a 1:1.

## 4. Buffers y memoria

| Búfer | Tamaño | Quién |
|---|---|---|
| `audio_buffer` (core) | 2048×2 int16 | TreeFrogLibretro.cpp:90 |
| pool SOUND_BUFFER_COUNT | 50 slots ≤512 KB, 2 lazy/Render | TreeFrogAudioDriver |
| monitor ring (core) | 32768 samples | Uac2Bridge:147 |
| FIFO pipe | 64 KB (32768 frames) | kernel, O_NONBLOCK |
| pending bridge (H40) | 16384 samples (341 ms) | Uac2Bridge, staging partial-write |
| ring daemon | 65536 samples (32768 frames) | ambos daemons |
| stage ASRC | 8192 frames | ambos daemons |
| period ALSA | 480 (240 lowlat) × 4 | ambos daemons |
| WAV take | hasta 120 s en tmpfs | daemon |
| capture cmd buzón | /tmp/r36sx_lgpt_usb/usb_capture_cmd | atomic tmp+rename |

## 5. Escrituras a SD (auditoría completa)

**Estado H42 (desplegado):** en operación normal el runtime ya no escribe a SD.
- `live_flush` del supervisor host: **opt-in** vía `LGPT_LIVE_FLUSH=1` (por defecto OFF; el flush del clean-shutdown sigue persistiendo logs una vez).
- Espejos de estado de ambos daemons → `mirror_runtime_state` **compilado out por defecto** (`-DLGPT_SD_RUNTIME_MIRROR=1` para depuración de campo); los buzones live quedan en tmpfs.
- Core `au10z_mirror_runtime_file`: mkdirs SD eliminados (solo tmpfs).
- Fallbacks SD de captura eliminados (`open_capture_wav_with_fallback` tmpfs-only, sin staging dir SD, sin basename fallback).

**One-shot de boot (aceptables como config):**
- Launcher (`LGPT_OTG_LOGS/LGPT_U2524_*.log`), setup_once (`U2517_*` logs + snapshot), apply_profile_once, shutdown, `boot_debug.log` flush, mkdirs de arranque (`/mnt/sdcard/lgpt/...`).

**User-save (correctas):**
- `Save` USB-REC: staging+fsync+rename → `lgpt/samples/records/`. Import al proyecto. Proyectos/config/samples del usuario.

**Lecturas:**
- `audio_driver_mode`, `sp404_gain`, `lowlat_240`, `physical_master_volume_percent`, `last_project`, `projects/`, switches `stat()` cacheados 100 ms.

**Config por evento (mantenidas, no runtime):**
- Cambios de modo (`audio_driver_mode`, `active_audio_branch`, policy), volumen físico persistente, writes de setup/apply/shutdown por cambio de configuración.

## 6. Problemas detectados (orden de prioridad)

1. **P1 — Drift 2.3 % en el core (FIX DESPLEGADO U2.71 H39):** el cap `frames>2048` descartaba budget wall-clock en stalls del frontend → FIFO a 46.8k. Corregido: render por chunks ≤2048, cap de seguridad 48000. Core live `bb3144e2…` (SHA1), backup `.pre_h39_budget` (`929009d6…`). Pendiente de validación en consola.
2. **P1 — Partial write en el FIFO del bridge (FIX DESPLEGADO H40):** staging acotado `g_fifo_pending` (16384 samples, 341 ms), drop-oldest con contador, drenaje al inicio del submit; sin pérdida silenciosa. Core live `bb3144e2…`.
3. **P1 — `ASRC_PASSTHROUGH_NOW` en daemon Windows (FIX DESPLEGADO H41):** eliminado el shortcut; el PI (target 2400, ±1200 ppm) gobierna siempre el paso 48k→48k; reset en reconnect/resync (código ya existente). Daemon live `f43a2d45…` (FIR16).
4. **P1 — Escrituras SD periódicas (FIX DESPLEGADO H42):** `live_flush` opt-in, espejos runtime_state compilados out, mkdirs core eliminados, fallback WAV SD eliminado.
5. **P2 — Renombrado canónico (FIX DESPLEGADO):** `SubmitStereo48000`/`MixUsbCaptureMonitorStereo48000` (API pública; el contrato ya era 48k).
6. **P2 — Ambigüedad de nombres SP404:** `SP404_OUT`/`SP404_IN`/`USB_OUT`/`SP404_OTG` coexisten; `SP404_IN` (captura desde SP404) no tiene implementación de flujo independiente — el modo normal es `USB_OUT` (playback) con captura USB-REC sobre la misma pcm. (documentado, sin cambio de comportamiento)
7. **P2 — SPSC dead code:** `TreeFrogWindowsSpscTransport` compilado sin callers; la arquitectura SPSC real es FIFO+ring+ASRC entre procesos (documentar y/o limpiar).
8. **P3 — Reproducibilidad:** kernel source y toolchain fuera del repo (WSL local); scripts `kernel_module_tools/scripts/00-06` existen pero sin pines de versión/checksums ni pipeline completo verificado desde WSL limpio.
9. **P3 — Pruebas de aceptación:** no existe batería reproducible (matriz Windows/Android/SP404/USB-REC + métricas XRUN/drop/drift + contador de sectores SD).

**Desplegado 2026-08-13 (U2.71 H39+H40/H41/H42):** core `bb3144e23f52961245ad1cf6eb532826848084d8` en `/mnt/g/cubegm/cores/`; backups `.pre_h39_budget`, `.pre_h40_backpressure`, `.pre_h42_20260813`. Daemons en `/mnt/g/lgpt/otg/bin/`: Windows `r36s_u241_usb_audio_io` = `f43a2d45…` (FIR16), SP404 `r36s_sp404_host_audio_io` = `fc6be370…` (FIR16), backups `.pre_h42_20260813`. Supervisor `otg_h37_host_runtime_supervisor.sh` con `LGPT_LIVE_FLUSH` opt-in.

## 7. Glosario de rutas clave

| Path | Uso |
|---|---|
| `/tmp/r36sx_uac2_bridge_fifo` | FIFO core→daemon (Windows) |
| `/tmp/r36sx_sp404_pcm_fifo` | FIFO core→daemon SP404 |
| `/tmp/r36sx_midi_pcm_fifo` | FIFO core→daemon MIDI |
| `/tmp/r36sx_aoa_bulk_pcm_fifo` | FIFO AOA (Android) |
| `/tmp/r36sx_usb_capture_monitor_fifo` | monitor captura daemon→core |
| `/tmp/r36sx_lgpt_usb/` | buzón/estado (cmd, meta, status, levels, profile, pids) |
| `/tmp/r36sx_lgpt_record/` | takes de USB-REC (tmpfs) |
| `/tmp/r36sx_lgpt_logs/` | logs RAM (+ `mirror/` para setup) |
| `/mnt/sdcard/lgpt/otg/bin/` | daemons + scripts desplegados |
| `/mnt/sdcard/lgpt/samples/records/` | WAVs publicados en Save |
| `/mnt/sdcard/lgpt/otg/modules/4.4.186-release/` | módulos kernel |
| `/dev/snd/pcmC0D0p` / `pcmC0D0c` | gadget UAC2 (Windows) |
| `/dev/snd/pcmC%dD0p` / `pcmC%dD0c` | SP404 (host ALSA) |
