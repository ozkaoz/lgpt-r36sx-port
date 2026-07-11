# Protocolo de prueba U2.23

## Prueba principal Song

1. Abrir Song normal mode.
2. En una columna, crear una secuencia visible de varias cadenas consecutivas, por ejemplo `00`, `01`, `02`, `03`.
3. Colocar el cursor sobre `01`.
4. Pulsar `Y + X`.
5. Resultado esperado: `02` sube al lugar de `01`, `03` sube al lugar de `02`, y la última posición de la cola queda `--`; no debe quedar duplicada `03`.

## Regresión A+B

1. Colocar el cursor sobre una celda ocupada en Song.
2. Pulsar `A + B`.
3. Resultado esperado: la celda queda `--` sin subir filas.

## Regresión CHOP estable

1. `R1 + A` entra al Chopper desde Instrument.
2. `R2 + A` reproduce sample completo con playhead rojo.
3. `A` crea corte en vivo.
4. Phrase conserva `S01..Sxx` para instrumentos con chops y `C-3/C-4` para instrumentos sin chops.
5. `SELECT` entra a CROP SAMPLE.
6. `L1 + R1` entra a PITCH SAMPLE.
7. Guardar, cerrar y recargar: los chops deben persistir por `sample.wav.u2chop`.
