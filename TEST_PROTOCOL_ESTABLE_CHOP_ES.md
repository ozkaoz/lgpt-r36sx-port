# Protocolo de prueba — versión estable con CHOP integrado

## 1. Regresión general

1. Instrument → `R1 + A`.
2. Confirmar waveform visible.
3. `R2 + A` reproduce sample completo y mueve línea roja.
4. Durante reproducción, `A` crea cortes en tiempo real.
5. `B` reproduce chop seleccionado.
6. `R2 + LEFT/RIGHT` cambia chop.
7. `R1 + LEFT/RIGHT` cambia sample.

## 2. Phrase y Sxx

1. Crear chops en un instrumento, por ejemplo `I05`.
2. Ir a Phrase.
3. Usar la columna de nota con `I05`.
4. Confirmar `S01`, `S02`, `S03`.
5. Confirmar que todas las filas mantienen el mismo instrumento.
6. Probar otro instrumento sin chops: debe mostrar notas normales.

## 3. Persistencia

1. Crear chops.
2. Asignar `Sxx` en Phrase.
3. Guardar proyecto.
4. Cerrar/reiniciar.
5. Recargar proyecto.
6. Confirmar que Phrase conserva `Sxx` y Chopper muestra marcas.
7. Confirmar existencia de `sample.wav.u2chop` junto al WAV.

## 4. CROP SAMPLE

1. Chopper → `SELECT`.
2. Ajustar inicio con `A + LEFT/RIGHT`.
3. Ajustar final con `B + LEFT/RIGHT`.
4. `Y` preescucha inicio.
5. `X` preescucha último segundo antes del final.
6. `R1 + A` conserva selección.
7. Confirmar overlay centrado con `OK` y `Press A to continue`.
8. `A` para continuar.
9. `R1 + X` undo/redo.
10. `L2 + Y` descarta selección.

## 5. PITCH SAMPLE

1. `L1 + R1` entra.
2. Flechas cambian valor de pitch entre `-12` y `+12`.
3. `B` preescucha sin modificar WAV original.
4. `A` aplica físicamente.
5. Overlay centrado debe mostrar resultado.
6. `R1 + X` desde CROP SAMPLE permite undo/redo si aplica.
