# LGPT R36SX - Bacon 1.0

Primer release oficial de la línea **Bacon** para LGPT R36SX. Incluye todas
las correcciones de la columna de Pitch (V2) y los dos ajustes finales de UI.

## Cambios de esta versión

### Columna de Pitch V2 (heredado de H38.7)

- Un step vacío se muestra como **`--`**.
- Un pitch de **0** semitonos se muestra como **`00`**.
- Valores negativos correctos (**`-01`**).
- Codificación V2 (`0x00` = vacío, `0x28..0x58` = -24..+24, offset `0x40`),
  eliminando la colisión `-1 (0xFF)` / vacío.
- Auto-pitch **`00`** al insertar una nota o asignar un chop.
- Transposición real de chops: el **mismo índice de chop** suena transpuesto
  vía `SetRowPitch` (no se cambia la nota ni el slice).

### Bloques de columnas separados en Phrase View (nuevo)

- La rejilla de phrase ahora se dibuja en **dos bloques visuales**:
  **N-V-P-I** (nota, volumen, pitch, instrumento) y, separado por un hueco,
  **FX1-P1-FX2-P2** (comandos y parámetros).

### Branding del menú de proyecto (nuevo)

- El mensaje de la esquina superior izquierda del menú de Project ahora
  muestra **`LGPT R36SX - Bacon 1.0`** (antes `Project (Build 1.6.0-bacon15)`).

## Instalación

1. Copia el contenido de este ZIP a la raíz de la tarjeta SD (carpetas
   `cubegm/`, `lgpt/`, `roms/`, `LGPT_OTG_LOGS/`, `ANDROID/`).
2. Arranca LGPT desde el menú de EmulationStation.

## Verificación

- Build MIPS del core con markers `PITCH_COLUMN` y `LGPT R36SX - Bacon 1.0`.
- Daemon ABI7 inalterado (golden SHA-256 conservado).
