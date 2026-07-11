# PORT Versión estable con CHOP integrado

Este ZIP es un punto de partida independiente para continuar el desarrollo del port LGPT/R36SX desde la versión validada U2.22.

Estado validado por prueba en consola:

- `R1 + A` entra al Chopper desde Instrument.
- Se muestra waveform correctamente.
- `R2 + A` reproduce el sample completo con playhead rojo en tiempo real.
- `A` permite crear cortes durante la reproducción, en la posición real del playhead.
- `R2 + LEFT/RIGHT` selecciona chops.
- `B` reproduce el chop seleccionado.
- Los chops se asignan en Phrase como `S01`, `S02`, `S03`, etc., manteniendo el mismo instrumento, por ejemplo `S01 I05`, `S02 I05`.
- Un instrumento sin chops conserva comportamiento normal de notas: `C-3`, `C-4`, etc.
- Los chops persisten al guardar/cerrar/recargar mediante sidecar `sample.wav.u2chop`.
- `SELECT` entra a `CROP SAMPLE`.
- `R1 + A` conserva físicamente solo la selección.
- `L2 + Y` descarta físicamente la selección.
- `R1 + X` hace undo/redo en CROP SAMPLE.
- El overlay de operación muestra texto centrado con porcentaje, `OK` y `Press A to continue`.
- `L1 + R1` entra a `PITCH SAMPLE`; flechas ajustan pitch manualmente entre `-12` y `+12`; `B` preescucha; `A` aplica físicamente.

Este paquete no incluye `dist/lgpt_libretro.so` compilado. Debe compilarse localmente en WSL Ubuntu 24 con el toolchain ya configurado.
