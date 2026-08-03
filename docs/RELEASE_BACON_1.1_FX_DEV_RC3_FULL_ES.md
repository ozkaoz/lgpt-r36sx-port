# LGPT R36SX - Bacon 1.1 - FX Dev (release candidate RC3 full)

**ESTADO: RELEASE CANDIDATE — NO ES UNA VERSIÓN ESTABLE.**

Release candidate RC3 (fase completa) sobre RC3 base (Bacon 1.1 - FX Dev).
Completa la modernización visual integral (plan de 33 puntos en
`docs/PLAN_RC3_MODERNIZACION_VISUAL_ES.md`, sección C): MixerView MASTER e
InstrumentView modernizados, chopper confirmado gráfico, widgets ASCII
retirados a la lista blanca documentada y leyendas migradas a la ayuda
contextual. **No modifica el motor FX, la persistencia ni la compatibilidad
de proyectos.**

## Cambios RC3 full

### 1. MixerView — páginas MASTER modernizadas

- Título de página centrado en la fila 1 con
  `UiDraw::DrawCenteredTitleAt` (reemplaza `DrawString(1,1,pageTitle)`).
- El bypass de DELAY/REVERB se renderiza con `UiDraw::DrawToggle`
  (`[ ON ]`/`[ OFF ]`).
- Las leyendas permanentes de las filas 22/23 (`UP/DN row  L/R edit  A
  coarse`, `SELECT page  START play`) y las de la página MIX (filas 19-21 y
  23) se retiraron de la pantalla y migraron a `HelpRegistry` (sección
  MIXER), visibles vía HelpOverlay (`SELECT+R1`).

### 2. InstrumentView modernizado

- Cabeceras de bloque (INSTRUMENT/FILTER/BITCRUSHER/PLAYBACK/EFFECT SENDS/
  AUTOMATION) renderizadas con `UiDraw::DrawSectionHeader`.
- El hint `R1+RIGHT USB REC` salió de la fila 0 y migró a la sección
  INSTRUMENT del HelpRegistry.

### 3. Graphical Chopper (punto 18)

Confirmado gráfico: la forma de onda real se renderiza por overlay
framebuffer (`tf_vline`/`tf_rect`) con amplitud por columna, región
seleccionada, marcadores de corte, eje central y cursor. El frame principal
usa celdas sólidas CD_BORDER y el submenú Pitch/Env (`drawPitchScreen`)
sigue el lenguaje gráfico del port (título centrado + bloque de filas
centrado, sin caja ASCII); el overlay de progreso de operación se conserva
como excepción documentada en `UI_VISUAL_AUDIT`. Los títulos se renombraron
a "Graphical Chopper" y "PITCH/ENV" (sin sufijos de versión).

### 4. Widgets ASCII y leyendas (puntos 17/26)

Sin widgets ASCII no documentados en la capa de vistas; `--`/`----` son
placeholders de valor legítimos. Los hints internos de los modales de texto
y USB record se conservan (no soportan HelpOverlay: requiere `!HasModal`).

## Pruebas

- Nuevo `tests/test_rc3_full_views_modernization.py`
  (`RC3_FULL_VIEWS_MODERNIZATION_OK`).
- Suite completa en verde: 21/21 tests FX + RC2 + RC3 base + RC3 full +
  auditoría (`AUDIT_CLEAN_MAIN_U2523_OK`).
- Build `BUILD_U2523_OK`; instalado y verificado en SD
  (`VERIFY_U2523_OK`, `ERRORS=0`).

## Problemas conocidos

- La validación manual y las capturas de pantalla requieren hardware R36SX
  real (no disponible); el estado visual por vista se rastrea en
  `UI_VISUAL_AUDIT.md`.
