# UI_CONTROL_AUDIT — Auditoría de textos y controles de la UI LGPT R36SX

Versión: RC3 base (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, punto 14).
Método: escaneo de `DrawString`/`SetStatus`/`sprintf`/`help` en las vistas,
clasificación por fila y verificación de cada entrada contra su handler.

## Clasificaciones

| Código | Significado | Acción |
|---|---|---|
| KEEP_LABEL | Etiqueta estática correcta | Conservar |
| KEEP_STATUS | Mensaje de estado dinámico correcto | Conservar |
| KEEP_ERROR | Mensaje de error correcto | Conservar |
| MOVE_TO_HELP | Hint/leyenda que debe ir a la ayuda (SELECT+R1) | Migrar a HelpRegistry |
| UPDATE_AND_MOVE_TO_HELP | Texto que hay que actualizar al mover | Actualizar + migrar |
| REMOVE_STALE | Texto huérfano / sin handler | Eliminar |
| REMOVE_OBSOLETE_FEATURE | Texto de función obsoleta | Eliminar (ver OBSOLETE_FEATURE_AUDIT) |

## Leyendas permanentes de página (estado actual)

Líneas de hint inferiores que hoy se dibujan como texto fijo y deben migrar a
HelpRegistry (punto 26 del plan).

| file:line | Texto | Clasificación |
|---|---|---|
| MixerView.cpp drawFxParamPage | `UP/DN row  L/R edit  A coarse` | MOVE_TO_HELP — migrado (fase completa) |
| MixerView.cpp drawFxParamPage | `SELECT page  START play` | MOVE_TO_HELP (migrado, fase completa) |
| MixerView.cpp drawFxPages (MIX) | `A+UP/DN x10  A+L/R x1` | MOVE_TO_HELP (migrado, fase completa) |
| MixerView.cpp drawFxPages (MIX) | `L/R ch  L->MST  R1+B mute` | MOVE_TO_HELP (migrado, fase completa) |
| MixerView.cpp drawFxPages (MIX) | `START play  R1+A solo  R2+A instr` | MOVE_TO_HELP (migrado, fase completa) |
| MixerView.cpp drawFxPages (MIX) | `R2 edit VOL/RET  SELECT [1/5]` | KEEP_STATUS (retirado, no era navegación) |
| InstrumentView.cpp DrawView | `R1+RIGHT USB REC` | MOVE_TO_HELP (migrado, fase completa) |
| TableView/PhraseView printHelpLegend | descripción del comando bajo cursor | KEEP (contenido contextual legítimo) |

Estado de la migración: todas las filas `MOVE_TO_HELP` del Mixer e Instrument
ya migraron en la fase completa a `HelpRegistry` y se muestran vía HelpOverlay
(`SELECT+R1`). Las leyendas de la página MIX que eran meros estados
(`R2 edit VOL/RET`) se retiraron por no ser indicaciones de navegación
(commit `978d8a9`).

## Overlay de ayuda (RC3)

`SELECT+R1` abre HelpOverlay con el contexto de la vista activa. Registrado en
HelpRegistry por ViewType. `SELECT+R2` conserva el diálogo de Audio Driver
(AppWindow, latch por separado).

## Pendientes de auditoría exhaustiva (fase completa)

- Escaneo `DrawString`/`SetStatus` de Song/Chain/Phrase/Table/Groove/Instrument.
- Verificación de cada entrada contra su handler (file:line).
- Resolución de REMOVE_STALE y REMOVE_OBSOLETE_FEATURE.