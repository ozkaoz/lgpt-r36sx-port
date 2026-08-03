# UI_CONTROL_AUDIT — Auditoría de textos y controles de la UI LGPT R36SX

Versión: RC4 (P0-P6 completos; P7 auditorías).
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
| MixerView.cpp DrawView | `R+UP Song` (fila de título) | MOVE_TO_HELP — migrado (RC4 P3, HelpRegistry sección MIXER) |
| InstrumentView.cpp DrawView | `R1+RIGHT USB REC` | MOVE_TO_HELP (migrado, fase completa) |
| TableView/PhraseView printHelpLegend | descripción del comando bajo cursor | KEEP (contenido contextual legítimo) |

Estado de la migración: todas las filas `MOVE_TO_HELP` del Mixer e Instrument
ya migraron a `HelpRegistry` y se muestran vía HelpOverlay (`SELECT+R1`).
En RC4 P3 también se retiró el hint permanente `R+UP Song` de la fila de
título del Mixer (commit `f3634f4`). Las leyendas de la página MIX que eran
meros estados (`R2 edit VOL/RET`) se retiraron por no ser indicaciones de
navegación.

## Overlay de ayuda (RC3/RC4)

`SELECT+R1` abre HelpOverlay con el contexto de la vista activa. Registrado en
HelpRegistry por ViewType. `SELECT+R2` conserva el diálogo de Audio Driver
(AppWindow, latch por separado).

### RC4 P1 — Help navegable y sobre modales
El overlay es un modal real: navega secciones (`L/R`, `L1/R1`) y líneas
(`UP/DN`), alterna el índice con `A` y cierra con `B`/`SELECT+R1`; consume
todos los eventos mientras está abierto. Se abre sobre modales activos vía
`View::PushModal`/`RestoreSuspendedModal` (el diálogo de abajo queda
suspendido y se restaura al cerrar). RC4 P6 añade `DrawTabs`
(prev/current/next) e indicador de scroll proporcional.

### RC4 P5 — Mensajes de estado con severidad
SampleManagerDialog distingue estado informativo (`setStatus` →
`DrawStatusMessage`, CD_WARNING) de errores (`setStatusError` →
`DrawErrorMessage`, CD_ERROR) mediante `statusIsError_`. El bloqueo de
borrado (`Blocked: …`) usa severidad de error.

## Auditoría exhaustiva RC4 (P7)

- Escaneo `DrawString`/`SetStatus` de Song/Chain/Phrase/Table/Groove/Instrument
  completado en la fase RC3: no quedan hints permanentes (solo `--`/valores).
- La migración de `R+UP Song` (P3) y el cambio de marco del chopper (P6)
  verificados por tests (`RC3_BASE_UNIFIED_BYPASS_UIDRAW_HELP_OK`).
- Pendiente en P8: re-verificación en hardware real R36SX de cada combo
  (SELECT+R1/R2, navegación Help, A+B reset en páginas FX).