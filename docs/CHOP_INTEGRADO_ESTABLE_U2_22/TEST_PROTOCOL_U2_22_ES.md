# Protocolo de prueba U2.22

1. Regresión: `R1 + A` entra al Chopper, waveform visible, `R2 + A` reproduce sample completo, `A` corta en vivo, Phrase mantiene `S01..Sxx`, instrumentos sin chops mantienen `C-3/C-4`.
2. Persistencia: guardar proyecto, cerrar/reiniciar, recargar; Phrase debe conservar `Sxx` y Chopper debe mostrar marcas.
3. CROP SAMPLE: `SELECT` entra; `R1 + A` conserva selección; `L2 + Y` descarta selección; `R1 + X` undo/redo.
4. Overlay: tras crop/delete/undo/redo/pitch debe verse texto centrado con porcentaje, `OK`, `Press A to continue`; no debe verse la barra verde superpuesta.
5. Pitch screen: `L1 + R1` entra a `PITCH SAMPLE`; flechas ajustan `-12..+12` en pasos de 1; `B` preescucha; `A` aplica; `L1 + R1` sale.
