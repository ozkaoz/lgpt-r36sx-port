# Secuencia de desarrollo U2.10 → U2.22

## U2.10 — Saved Cuts + Phrase Export

Se corrigió la pérdida de boundaries al cerrar el modal. Los cortes se guardaban durante la sesión por proyecto+sample. Todavía se exportaban en lote a Phrase clonando instrumentos.

## U2.11 — Playhead + Selective Assign

Se añadió playhead visual en tiempo real y asignación selectiva. La prueba validó Chopper básico, pero mostró que la lógica de exportar/asignar desde Chopper no era la correcta para tracker.

## U2.12 / U2.13 — Sxx en Phrase

Se definió la lógica correcta: Chopper corta; Phrase decide qué slice dispara. La columna de nota pasa a `S01..Sxx` solo cuando el instrumento tiene chops. Los instrumentos sin chops conservan notas musicales.

## U2.14 / U2.15 — 100 chops, autosave, crop inicial

Se subió el límite a 100 chops y se eliminó la necesidad de pulsar `R1 + A` para que Phrase vea los chops. El estado se actualiza al crear, borrar o ajustar cortes. Se inició la edición destructiva de sample.

## U2.16 / U2.17 / U2.18 — Ajuste destructivo y problemas de mapeo

Se intentó añadir crop, undo/redo y pitch físico. La prueba mostró problemas con `L2`/`R2`, falta de feedback y error de escritura. Se concluyó que no convenía seguir agregando funciones sobre un mapeo confuso.

## U2.19 — SELECT habilitado y CROP SAMPLE exacto

El log de compilación reveló `TREEFROG_ENABLE_SELECT=0`. Se activó SELECT y se simplificó el flujo. `SELECT` se convirtió en entrada real a CROP SAMPLE. `R1 + A` quedó como keep range y `R1 + X` como undo/redo.

## U2.20 — Persistencia real

Se corrigió el problema crítico de guardado/carga. Los chops se persisten en sidecar `sample.wav.u2chop`. Chopper, Phrase y playback cargan el sidecar automáticamente.

## U2.21 — Limpieza de crop UI

`SELECT` quedó como entrada principal. `L2 + Y` dejó de entrar a crop y se reutilizó dentro de CROP SAMPLE como operación inversa: borrar/descarte físico de la selección. Se validaron keep, delete, undo/redo y persistencia.

## U2.22 — Pitch screen + overlay limpio

Se eliminó el pitch automático con `R1 + flechas`. Se añadió pantalla dedicada `PITCH SAMPLE` con selección manual de pitch, preescucha con `B` y aplicación con `A`. Se limpió el overlay de operación para evitar la barra verde superpuesta al texto.

## Estado estable

La versión estable confirmada es U2.22: regresión general OK, CROP SAMPLE OK, overlay OK, PITCH SAMPLE OK, persistencia OK.
