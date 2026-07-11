# Mapa de comandos — CHOP integrado estable

## Entrada y navegación general

- `R1 + A`: entrar al Chopper desde Instrument.
- `R1 + B`: salir del Chopper a Instrument.
- `LEFT / RIGHT`: mover cursor fino.
- `R1 + RIGHT / RIGHT`: mover cursor rápido.
- `UP / DOWN`: zoom.
- `R1 + LEFT / RIGHT`: cambiar sample cargado. Esta función fue validada y debe conservarse.

## Chopper principal

- `R2 + A`: reproducir sample completo.
- `A`: crear corte en cursor/playhead. Durante reproducción completa, corta en tiempo real donde va la línea roja.
- `B`: reproducir chop seleccionado. Si no hay chops, reproduce sample completo.
- `R2 + LEFT / RIGHT`: seleccionar chop.
- `Y`: borrar corte/chop seleccionado.

## Phrase con instrumento cortado

Si el instrumento tiene chops persistentes, la columna de nota usa slices:

- `S01 I05`
- `S02 I05`
- `S03 I05`

El número de instrumento permanece igual. La selección de slice no debe clonarse a instrumentos nuevos.

Si el instrumento no tiene chops, se mantiene comportamiento normal:

- `C-3 I05`
- `C-4 I05`
- etc.

## CROP SAMPLE

- `SELECT`: entrar/salir de CROP SAMPLE.
- `A + LEFT / RIGHT`: ajustar inicio de selección.
- `B + LEFT / RIGHT`: ajustar final de selección.
- `Y`: preescuchar desde el inicio seleccionado.
- `X`: preescuchar el último segundo antes del final seleccionado.
- `R1 + A`: conservar físicamente solo lo seleccionado.
- `L2 + Y`: borrar físicamente lo seleccionado, uniendo lo anterior y posterior.
- `R1 + X`: undo/redo del último crop/delete/pitch físico.

## PITCH SAMPLE

- `L1 + R1`: entrar/salir de PITCH SAMPLE.
- `LEFT / RIGHT`: bajar/subir pitch en pasos de 1 semitono.
- `UP / DOWN`: subir/bajar pitch en pasos de 1 semitono.
- Rango: `-12` a `+12`.
- `B`: preescuchar pitch seleccionado sin modificar el WAV original.
- `A`: aplicar físicamente el pitch al WAV.

## PTCH en Phrase

`PTCH` sigue siendo la vía no destructiva de pitch por patrón/FX. El pitch físico de PITCH SAMPLE modifica el WAV; PTCH modifica la reproducción desde Phrase.
