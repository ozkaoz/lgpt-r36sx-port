# U2.23 — Song Y+X cut tail clear fix

Aplicar sobre `PORT Versión estable con CHOP integrado` / U2.22.

Objetivo: corregir el compactado histórico de Song usado por `Y + X` en modo normal. La función `SongView::cutSelection()` ya subía las filas tras cortar, pero el bucle final que debía limpiar la cola usaba `j > clipboard_.height_`, por lo que nunca se ejecutaba. Esto podía dejar duplicada la última fila desplazada en la parte baja del patrón.

Cambio único:

- `sources/Application/Views/SongView.cpp`
- En `SongView::cutSelection()`, cambiar el bucle de limpieza final de `j > clipboard_.height_` a `j < clipboard_.height_`.

Alcance:

- Solo SongView.
- No toca input global.
- No toca Chopper, CROP SAMPLE, PITCH SAMPLE, Phrase, Chain, Table, Player, audio ni persistencia `.u2chop`.
