# LGPT R36SX H38.7 — Columna de Pitch V2: vacío "--", edición 00/-01, auto-pitch y transposición real de chops

Build H38.7, ABI7, three-mode, frontend-safe. Corrige los fallos de la
columna de Pitch detectados en las pruebas de hardware de H38.6.

## Correcciones en esta versión

### 1. Display de la columna de Pitch corregido

- Un step **vacío** (sin pitch) se muestra como **`--`** (antes mostraba
  el byte de pitch como texto basura).
- Un pitch de **0** semitonos se muestra como **`00`** (antes se dibujaba
  como `--` porque colisionaba con el marcador de "vacío").
- Los valores negativos se muestran correctamente: **`-01`** en vez de
  los dos caracteres rotos de la codificación anterior.

### 2. Nueva codificación V2 del pitch (sin colisión vacío / -1)

- La codificación anterior usaba `0xFF` para el valor -1, que colisionaba
  con el marcador de "step sin pitch". A partir de esta versión:
  - `0x00` = sin pitch (vacío, se muestra `--`).
  - `0x28..0x58` = **-24..+24** semitonos (offset `0x40` = 0).
- Esto **rompe la compatibilidad binaria de los proyectos guardados en
  H38.6 con pitch editado** (los proyectos viejos cargan su pitch como 0
  al ser valores fuera del nuevo rango). Es un cambio deliberado para
  eliminar la ambigüedad.

### 3. Edición de -01 y 00 en la columna de Pitch

- Navegar `LEFT/RIGHT` sobre un pitch vacío arranca desde **0** (mostrando
  `00`), y `DOWN` llega hasta **-01** correctamente.
- `A+UP/DOWN` ajusta de 10 en 10 dentro del rango -24..+24.

### 4. Auto-pitch 00 al insertar una nota

- Al insertar una nota nueva en un step vacío (o asignar un chop) la
  columna de Pitch se rellena automáticamente con **`00`** (pitch 0),
  igual que el volumen se rellena con 100.

### 5. Transposición real de chops (mismo chop, sonido transpuesto)

- Antes, aplicar pitch a una fila de chop (notas `S01..S99`) cambiaba el
  índice del chop seleccionado (sonaba otro slice o nada).
- Ahora en una fila con chop la nota **no se toca** y el pitch se aplica
  al *playback* del chop ya seleccionado vía `SetRowPitch`: se ajusta la
  velocidad de reproducción de la voz por `2^(pitch/12)`. Resultado: el
  **mismo índice de chop** suena **transpuesto** los semitonos indicados.
- Los steps sin chop siguen sumando el pitch a la nota (clamp 0-127).

## Instalación

1. Copia el contenido de este ZIP a la raíz de la tarjeta SD (carpetas
   `cubegm/`, `lgpt/`, `roms/`, `LGPT_OTG_LOGS/`, `ANDROID/`).
2. Arranca LGPT desde el menú de EmulationStation.
3. Abre el Phrase View y comprueba la columna de Pitch: vacío = `--`,
   edita `00` y `-01`, e inserta una nota para ver el auto-pitch.

## Verificación

- `VERIFY_BUILD_H38_6_ABI7_OK` (build MIPS del core H38.7)
- `VERIFY_SD_H38_6_ABI7_OK`
- Daemon ABI7 inalterado (golden SHA-256 conservado).
