# LGPT R36SX - Bacon 1.1 - FX Dev (release candidate RC2)

**ESTADO: RELEASE CANDIDATE — NO ES UNA VERSIÓN ESTABLE.**

Release candidate RC2 sobre RC1 (Bacon 1.1 - FX Dev). Integra la
normalización de etiquetas FX (tabla T1), el selector de comandos FX por
familias con paginación, la reverb wet-only, las páginas dedicadas
DELAY/REVERB MASTER con jerarquía de colores, las barras sólidas de
EFFECT SENDS en InstrumentView y la corrección del cursor de edición
hexadecimal. Pensado para probar en tarjeta SD; ver "Problemas conocidos".

## Cambios RC2

### 1. Etiquetas FX normalizadas (tabla T1)

Las etiquetas visibles de comandos FX se alinean con la tabla T1. Los
**FourCC internos (`I_CMD_*`) no cambian**: los proyectos guardados se
cargan y reproducen igual (bit-identicos), solo cambia lo que se muestra.

| Comando | Antes | Ahora |
|---|---|---|
| FBMX | `FBMX` | `CFM` |
| FBTN | `FBTN` | `CFT` |
| DLAY | `DLAY` | `NDL` |
| FLTR | `FLTR` | `FCR` |
| FCUT | `FCUT` | `FCU` |
| FRES | `FRES` | `FRS` |
| CRSH | `CRSH` | `BCR` |
| PFIN | `PFIN` | `PFT` |
| DLYS | `DLYS` | `DSE` |
| RVBS | `RVBS` | `RSE` |
| DLYT | `DLYT` | `DTM` |
| DLYF | `DLYF` | `DFB` |
| RVDC | `RVDC` | `RDC` |
| RVSZ | `RVSZ` | `RSZ` |
| CMPT | `CMPT` | `CTH` |

### 2. Selector FX por familias con paginación

El selector de comandos FX (Table/Phrase) se reorganiza por familias:

- **FX 1/2**: `INST` (NDL/PFT/BCR), `FILTER` (FCU/FRS/FCR), `DELAY`
  (DSE/DTM/DFB), `REVERB` (RSE/RSZ/RDC), `MASTER` (CTH).
- **FX 2/2**: `LEGACY COMB` (CFM/CFT).

Los proyectos antiguos con `FBMX`/`FBTN` siguen reproduciéndose: muestran
`CFM`/`CFT` y son editables. La paginación salta automáticamente de página
en el cruce de borde horizontal.

### 3. Reverb wet-only (envio/retorno real)

- `RVB MIX` ya **no existe en la UI** (la fila y `FX_P_RVB_MIX` se retiraron
  del Mixer). El nivel audible se controla con el send del instrumento y el
  `REVERB RETURN` del Mixer, no con un crossfade interno.
- El DSP es wet-only: la salida solo contiene la señal procesada (húmeda);
  el seco ya vive en el bus master. `RVB MIX` persistido en proyectos viejos
  se sigue leyendo/escribiendo pero es inerte (compatibilidad).
- **Headroom**: -3 dB de entrada fijo antes de los difusores.
- **Densidad**: suma de combs normalizada (`combNorm_` = 1/nCombs) + un 3er
  allpass por canal en modo NORMAL (más difusión, sin recorte).
- El **Delay conserva su dry/wet** actual (`DLY MIX` sigue en la página).

### 4. Páginas DELAY MASTER / REVERB MASTER rediseñadas

Menús dedicados de dos columnas (label / valor) con jerarquía de colores:

- Título en `CD_HILITE1` con posición `[2/5]`/`[3/5]`.
- Label de fila en `CD_NORMAL`, valor en `CD_HILITE1`.
- Fila editada invertida en `CD_HILITE2`.
- Formatos con unidades: `TIME ms`, `DECAY s`, toggles `ON/OFF`, modo reverb
  `ECO`/`NORMAL`. 7 filas en cada página (REVERB sin `RVB MIX`).

### 5. Barras sólidas en InstrumentView

Las barras de `EFFECT SENDS` (DRY/DELAY/REVERB) se dibujan ahora como un
bloque sólido de celdas invertidas (estilo MixerView) en vez del ASCII
`[====----]`; `-1` sigue mostrando `INH` (inherit) y limpia la barra vieja.

### 6. Cursor de edición hexadecimal corregido

`UIBigHexVarField::SetHexMode` ya no fuerza `position_=0`: el cursor de
nibble conserva el dígito que se estaba editando (solo se clampa al nuevo
rango de precisión) y el valor completo se clampa (o se envuelve, según
`wrap`) al nuevo `[min,max]` al cambiar el comando de la celda.

### 7. Pruebas RC2

- Nuevo `tests/test_fx_rc2_master_pages_solid_bars_hexmode.py`
  -> `FX_RC2_MASTER_PAGES_SOLID_BARS_HEXMODE_OK`.
- Actualizados a la semantica wet-only: `test_fx_phase10_wetonly_audit.py`,
  `test_fx_phase4_ui.py`, `test_fx_phase6_nav_ab_default.py`.
- Ventanas de dispatch EQ/COMP ampliadas en `test_fx_phase12_eq_menu.py` y
  `test_fx_phase13_comp_menu.py`; barras solidas en
  `test_fx_phase8_instrument_blocks.py`.
- Suite completa en verde (ver "Resultados de pruebas").

## Binario instalable

| Artefacto | Ruta | SHA-256 |
|---|---|---|
| Core (libretro MIPS) | `BUILD/U2523/lgpt_r36sx_u2523.so` | `c114863b8c43d6ae1300dd672edc5b6980970f59ec08bc29ceb658b58126bc20` |
| Daemon USB ABI7 | `BUILD/U2523/r36s_u2523_usb_audio_io` | `53258f2b8b3749c866af248814eb147f0762a1b17cfffb644adb573167b52815` |

- El daemon conserva el SHA-256 golden de Bacon 1.0 (ABI7 inalterado).
- `BUILD/U2523/SHA256SUMS.txt` contiene ambos hashes.
- La copia instalada en SD tiene los mismos hashes (`VERIFY_U2523_OK`,
  `ERRORS=0`).

## Commit fuente

- Commit del release: se fija al publicar RC2 (tag `Bacon-1.1-FX-Dev-RC2`).

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
  dedicadas `[2/5]`/`[3/5]`; reverb wet-only desde RC2).
- **EQ / Compressor** procesan la salida máster (páginas dedicadas
  `[4/5]`/`[5/5]`; `EQ BYPASS`/`CMP BYP` visibles y editables).

## Resultados de pruebas

- Suite automatizada completa en verde: `AUDIT_CLEAN_MAIN_U2523_OK`
  (176 checks OK, 27/27 grupos de audit), incluidos los 22 tests FX
  (`test_fx_phase*.py`, Fases 0-17 + RC2) y la suite u-series.
- `host_syntax_check` en verde; build MIPS con `BUILD_U2523_OK`; instalación
  y verificación en SD con `VERIFY_U2523_OK`/`ERRORS=0`.

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
PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/build.sh

# 2) Instalar en la SD (montada en /mnt/f)
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/install.sh

# 3) Verificar que la SD coincide con el build
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/verify.sh
# Esperado: ERRORS=0 / VERIFY_U2523_OK
```

Todo el proceso en un paso:

```bash
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/build_install.sh
```

## Rollback

`scripts/install.sh` deja una copia de seguridad en
`PROJECT_ROOT/BACKUPS/LGPT_BEFORE_U2523_<timestamp>/`
(este RC: `BACKUPS/LGPT_BEFORE_U2523_20260803_031938`).

```bash
# Restaura el core, daemon, launcher, scripts OTG y perfil de audio previos
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/restore.sh
# o con ruta explícita:
SD_MOUNT=/mnt/f bash scripts/restore.sh "/mnt/d/Toolchains/R36SX/BACKUPS/LGPT_BEFORE_U2523_20260803_031938"
```

## Problemas conocidos

- **Take record editor "physical edge" pendiente**: el core mantiene el
  marker `U2520_PENDING_TAKE_RECORD_EDITOR_PHYSICAL_EDGE_READY`; la parte
  de "physical edge" del editor de takes de USB record está pendiente de
  resolver (el flujo transaccional tmpfs->FAT32 ya está verificado).
- **Capturas de pantalla en resolución real R36SX pendientes**: la
  validación visual de las páginas rediseñadas (DELAY/REVERB MASTER, barras
  solidas, selector FX por familias) se entregará al flashear en hardware
  (no hay pantalla R36SX en esta máquina).
- Este es un **release candidate**: la línea FX (0-18) + RC2 es nueva y no
  tiene tiempo de uso en hardware real.
- `scripts/publish_bacon_to_github.sh` (Bacon 1.0) sigue siendo el flujo de
  publicación estable; este RC se publica como **prerelease**.
