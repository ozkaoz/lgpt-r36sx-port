# UI_VISUAL_AUDIT — Auditoría visual de la UI LGPT R36SX

Versión: RC4 (P0-P6 completos; P7 auditorías, P8 tests finales).
Marco normativo: UI_STYLE_GUIDE.md. Checklist por punto del plan.

Estado: auditoría en curso en la fase completa; las capturas ASCII de
referencia en pantalla real R36SX se completan cuando haya hardware; el
estado de cada vista se rastrea en el checklist por vista.

## Checklist RC3/R C4 (estado fase completa)

| Punto | Descripción | Estado |
|---|---|---|
| 2 | Título centrado (fila 0, `(40-len)/2`) | Implementado en UiDraw::DrawCenteredTitle |
| 2b | Título centrado en fila de subpágina | Implementado en UiDraw::DrawCenteredTitleAt (pages FX) |
| 3 | Cabeceras de sección | Implementado en UiDraw::DrawSectionHeader |
| 4 | Filas de valor con jerarquía | Implementado en UiDraw::DrawValueRow + drawMasterFxRow |
| 5 | Toggles ON/OFF | Implementado en UiDraw::DrawToggle |
| 6 | Barras sólidas | Implementado en UiDraw::DrawSolidBar/DrawBipolarBar |
| 7 | Bypass unificado (primera fila, ON=desactivado) | Implementado en MixerView (bypass primero) |
| 8 | Bloques/menús centrados | Implementado en UiDraw::CenterTextX + MakeCenteredMenuLayout (P4) |
| 17 | Widgets ASCII retirados | Chopper sin marco ASCII (P6); placeholders `--`/`----` son valores |
| 18 | Graphical Chopper modernizado | Implementado (overlay framebuffer) + frame sólido sin ASCII (P6) |
| 19 | Librería de primitivas | UiDraw.h/.cpp (P5: DrawSelectionRegion/Status/Error) |
| 20 | Colores semánticos | UiColors.h (P5: UI_COLOR_WARNING/ERROR, CD_WARNING/CD_ERROR) |
| 24 | Modales unificados | Parcial; ProjectExit centrado (P4), resto conserva SetWindow centrado |
| 30-32 | Tests (colores/toggles/barras/layout) | RC2 + RC3 + RC4 suite PASS |

## Avances RC4 (P3-P6)

### P3 — Ayudas permanentes retiradas
La navegación permanente de la fila de título del Mixer
(`DrawString(7,pos._y,"R+UP Song")`) fue retirada; la acción vive en
`HelpRegistry` (sección MIXER, `R+UP → back to Song`) y se muestra vía
HelpOverlay (SELECT+R1). No quedan hints permanentes en Song/Chain/Phrase/
Table/Groove/Instrument/Project.

### P4 — Menús centrados
`UiDraw::CenterTextX` centra un texto en la pantalla de 40 celdas
(`(40-len)/2`) y `UiDraw::MakeCenteredMenuLayout` calcula el bloque centrado
de un menú vertical (columnas label/valor opcionales, centrado vertical en la
banda 1..29). Primer consumidor: el modal de salida de proyecto
(`TreeFrogProjectExitModal` en ProjectView) centra título e items. El resto
de modales ya centran su bloque con `SetWindow`.

### P5 — UiDraw global + colores de estado
- `CD_WARNING`/`CD_ERROR` añadidos al enum (View.h) y mapeados a colores
  reales en AppWindow (warningColor_/errorColor_), dentro de los 7 bits de la
  caché de pantalla.
- `UiDraw::DrawSelectionRegion` (relleno sólido CD_HILITE2), `DrawStatusMessage`
  (CD_WARNING) y `DrawErrorMessage` (CD_ERROR), todos clamped a 40x30.
- Consumidores reales: HelpOverlay índice (región de la sección activa),
  SampleManagerDialog (`setStatus`/`setStatusError` con `statusIsError_`).
- Roles semánticos UI_COLOR_WARNING/UI_COLOR_ERROR/UI_COLOR_SECTION/
  UI_COLOR_SELECTED/UI_COLOR_LEGACY añadidos en UiColors.h.

### P6 — Chopper y primitivas de navegación
- El marco ASCII del Graphical Chopper (`+---|`) se reemplazó por celdas
  sólidas CD_BORDER con la misma geometría (filas 1..22, columnas 0/39), sin
  desalinear el overlay framebuffer.
- `DrawTabs` (prev/current/next) y `DrawScrollIndicator` con consumidores en
  HelpOverlay: navegación de secciones y scroll proporcional de la sección.

## Capturas de referencia (estado anterior / RC2)

Se documentarán aquí en la fase completa las capturas ASCII antes/después por
vista (Song, Chain, Phrase, Table, Groove, Instrument, Mixer).

### Graphical Chopper (punto 18)
Ya modernizado: la forma de onda real se renderiza por framebuffer overlay
(`tf_vline`/`tf_rect` en `PLATFORM_TREEFROG`) con amplitud por columna,
región seleccionada (banda + raíles finos), marcadores de corte, eje central
y cursor. El marco estructural de la banda se dibuja ahora con celdas sólidas
CD_BORDER (P6); ya no usa caracteres ASCII de caja.

El overlay de progreso de operación (`drawOperationOverlay`) y el panel
Pitch/Env (`drawPitchScreen`) usan marcos invertidos propios que
sobrescriben visualmente la banda durante el procesamiento/edición; se
conservan como excepción documentada del punto 24/17 (no son
`DrawModalFrame` porque el bloqueo sólido sobre la forma de onda es
intencional).

## Checklist por vista

- [x] Mixer MASTER: título centrado (DrawCenteredTitleAt), toggles de bypass,
      leyendas a Help (P3).
- [x] Instrument: cabeceras de sección vía UiDraw, hint USB a Help.
- [x] Song: título con rol semántico (UI_COLOR_TITLE), filas con jerarquía.
- [x] Chain: título con rol semántico, cursor coherente.
- [x] Phrase: título con rol semántico, notas con CD_NORMAL/HILITE.
- [x] Table: título con rol semántico, comandos con barras/valores sólidos.
- [x] Chopper: frame sólido sin ASCII (P6), overlay de onda preservado.
- [x] SampleManager: estado con roles de severidad (P5).
- [x] ProjectExit: título e items centrados (P4).
- [ ] Modales: marco unificado, título centrado, sin desbordes (revisar en P8).