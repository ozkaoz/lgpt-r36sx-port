# LGPT R36SX - Bacon 1.1 - FX Dev (release candidate)

**ESTADO: RELEASE CANDIDATE — NO ES UNA VERSIÓN ESTABLE.**

Este release candidate integra el **rediseño completo del motor FX**
(Fases 0-18 de `docs/PLAN_FX_REDESIGN_ES.md`) sobre la línea Bacon.
Está pensado para probar en tarjeta SD; no se recomienda como build de
trabajo diario hasta validar los problemas conocidos de la sección final.

## Binario instalable

| Artefacto | Ruta | SHA-256 |
|---|---|---|
| Core (libretro MIPS) | `BUILD/U2523/lgpt_r36sx_u2523.so` | `fddc4b042742da0745edf4f24edeee66543e67b900ecb03b87196bc41f8764ec` |
| Daemon USB ABI7 | `BUILD/U2523/r36s_u2523_usb_audio_io` | `53258f2b8b3749c866af248814eb147f0762a1b17cfffb644adb573167b52815` |

- El daemon conserva el SHA-256 golden de Bacon 1.0 (ABI7 inalterado).
- `BUILD/U2523/SHA256SUMS.txt` contiene ambos hashes.
- La copia instalada en SD tiene los mismos hashes (`VERIFY_U2523_OK`,
  `ERRORS=0`, ver `LGPT_OTG_LOGS/INSTALL_STATE_U2523.txt`).

## Commit fuente

- Commit del release: `626e3f67ad24f04e62ee5ac67c77346221f485ea`
  (FX redesign phases 7-18 + branding Bacon 1.1 + suite de aceptación).
- Tag: `Bacon-1.1-FX-Dev-RC1`.

## Arquitectura FX (resultado del rediseño)

- **Instrument** define su nivel **Dry**, su **Delay Send** y su **Reverb Send**
  por instrumento (DRY/DLY/RVB, defaults `100/0/0`; `-1` solo en proyectos
  antiguos, que heredan el send per-track).
- **Phrase / Table** automatizan los envíos y parámetros globales mediante
  comandos (`DLYS`/`RVBS` son live por canal; `DLY`/`RVB`/`EQ`/`CMP` por
  comando), sin modificar los valores persistidos.
- **Mixer** controla volúmenes de pista y los **retornos de efectos**
  globales (`DLYRET`/`RVBRET`).
- **Delay / Reverb** configuran los procesadores compartidos (páginas
  dedicadas `[2/5]`/`[3/5]`).
- **EQ / Compressor** procesan la salida máster (páginas dedicadas
  `[4/5]`/`[5/5]`; `EQ BYPASS`/`CMP BYP` visibles y editables).

## Resultados de pruebas

- Suite automatizada completa en verde: `AUDIT_CLEAN_MAIN_U2523_OK`,
  **176 checks OK**, incluidos los 21 tests FX
  (`test_fx_phase*.py`, Fases 0-17) y la suite u-series
  (U2510/U2514/U2517/U2520/U2521/U2522/U2523).
- Tests corregidos en este release (fallos preexistentes, ajenos al
  rediseño FX):
  - `test_u2520_transactional_record_source.py`: marcador de ruta temp
    actualizado a `/tmp/r36sx_lgpt_record` (diseño tmpfs real usado por
    `UsbRecordModal` e `ImportSampleDialog`; la grabación runtime vive en
    tmpfs y se publica a FAT32 solo al guardar).
  - `test_u2521_browser_rename.py`: el modal `ImportBrowserRenameModal`
    fue sustituido en H38.x por el editor de texto unificado
    (`TreeFrogTextEditor`); el test verifica ahora el flujo real
    (rename/delete case-safe con confirmación).
  - `test_u2522_nested_rename_frame_forwarding.py`: verifica el puente
    `OnFrameUpdate`/`UpdateActiveModalFrame` del editor anidado.
  - `test_u2523_rename_caret_alignment.py`: verifica el alineado del caret
    en `TreeFrogTextEditor.cpp` (destino real del editor de rename).

## Resumen del audit

- Audit completo (`scripts/audit.sh`): 27/27 grupos OK, `AUDIT_CLEAN_MAIN_U2523_OK`.
- `host_syntax_check` y `mips_fx_syntax_fase5` en verde.
- El core contiene los markers de build: `U2523_RENAME_CARET_ALIGNMENT_GITHUB_FINAL`,
  `U2520_PENDING_TAKE_RECORD_EDITOR_PHYSICAL_EDGE_READY`, `R36SX_CAPTURE_ABI=2`
  y el branding `LGPT R36SX - Bacon 1.1`.

## Cambios de formato de proyecto (persistencia)

Solo se añaden campos nuevos, con fallback a legacy (los proyectos clásicos
siguen siendo bit-idénticos, ver golden):

- PARAMs de instrumento `DRY` / `DLY send` / `RVB send`
  (defaults `100/0/0`; `-1` solo en proyectos antiguos).
- `FXMASTER`: bloque persistido con los 41 parámetros del master FX.
- Sends per-track (`DELAYSEND`/`REVERBSEND`) conservados como capa de
  herencia (nunca se borran).
- Retornos globales `DLYRET`/`RVBRET` en el Mixer.

## Instalación en SD

Opción A — instalar sobre la SD existente (con backup automático):

```bash
# 1) Build (si no está BUILD/U2523)
PROJECT_ROOT="/mnt/d/R36S/PORT LPTRACKER" bash scripts/build.sh

# 2) Instalar en la SD (montada en /mnt/f)
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/R36S/PORT LPTRACKER" bash scripts/install.sh

# 3) Verificar que la SD coincide con el build
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/R36S/PORT LPTRACKER" bash scripts/verify.sh
# Esperado: ERRORS=0 / VERIFY_U2523_OK
```

Todo el proceso en un paso:

```bash
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/R36S/PORT LPTRACKER" bash scripts/build_install.sh
```

Opción B — copiar el contenido del ZIP copy-to-SD-root a la raíz de la
tarjeta (`cubegm/`, `lgpt/`, `roms/`, `LGPT_OTG_LOGS/`, `ANDROID/`) y
arrancar LGPT desde EmulationStation.

## Rollback

`scripts/install.sh` deja una copia de seguridad en
`PROJECT_ROOT/BACKUPS/LGPT_BEFORE_U2523_<timestamp>/`
(este RC: `BACKUPS/LGPT_BEFORE_U2523_20260803_013014`).

```bash
# Restaura el core, daemon, launcher, scripts OTG y perfil de audio previos
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/R36S/PORT LPTRACKER" bash scripts/restore.sh
# o con ruta explícita:
SD_MOUNT=/mnt/f bash scripts/restore.sh "/mnt/d/R36S/PORT LPTRACKER/BACKUPS/LGPT_BEFORE_U2523_20260803_013014"
```

## Problemas conocidos

- **Take record editor "physical edge" pendiente**: el core mantiene el
  marker `U2520_PENDING_TAKE_RECORD_EDITOR_PHYSICAL_EDGE_READY`; la parte
  de "physical edge" del editor de takes de USB record está pendiente de
  resolver (el flujo transaccional tmpfs->FAT32 ya está verificado).
- **Capturas de pantalla en resolución real R36SX pendientes**: la
  validación visual de cada página del Mixer/Instrument se entregará al
  flashear en hardware (no hay pantalla R36SX en esta máquina).
- Este es un **release candidate**: la línea FX (0-18) es nueva y no tiene
  tiempo de uso en hardware real.
- `scripts/publish_bacon_to_github.sh` (Bacon 1.0) sigue siendo el flujo de
  publicación estable; este RC se publica como **prerelease**.
