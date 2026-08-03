# UI_VISUAL_AUDIT — Auditoría visual de la UI LGPT R36SX

Versión: RC3 completa (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, punto 28).
Marco normativo: UI_STYLE_GUIDE.md. Checklist por punto del plan.

Estado: auditoría en curso en la fase completa; las capturas ASCII de
referencia en pantalla real R36SX se completan cuando haya hardware; el
estado de cada vista se rastrea en el checklist por vista.

## Checklist RC3 (estado fase completa)

| Punto | Descripción | Estado |
|---|---|---|
| 2 | Título centrado (fila 0, `(40-len)/2`) | Implementado en UiDraw::DrawCenteredTitle |
| 2b | Título centrado en fila de subpágina | Implementado en UiDraw::DrawCenteredTitleAt (pages FX) |
| 3 | Cabeceras de sección | Implementado en UiDraw::DrawSectionHeader |
| 4 | Filas de valor con jerarquía | Implementado en UiDraw::DrawValueRow + drawMasterFxRow |
| 5 | Toggles ON/OFF | Implementado en UiDraw::DrawToggle |
| 6 | Barras sólidas | Implementado en UiDraw::DrawSolidBar/DrawBipolarBar |
| 7 | Bypass unificado (primera fila, ON=desactivado) | Implementado en MixerView (bypass primero) |
| 17 | Widgets ASCII retirados | Placeholders `--`/`----` son valores, excepción documentada |
| 19 | Librería de primitivas | UiDraw.h/.cpp |
| 20 | Colores semánticos | UiColors.h |
| 24 | Modales unificados | Pendiente fase completa |
| 30-32 | Tests (colores/toggles/barras/layout) | RC2 + RC3 suite PASS |
| 18 | Graphical Chopper modernizado | Implementado (overlay framebuffer) |

## Avances fase completa

### MixerView (MASTER pages)
- Título de página centrado en fila 1 con `UiDraw::DrawCenteredTitleAt`
  (reemplaza `DrawString(1,1,pageTitle)`).
- Bypass de DELAY/REVERB renderizado con `UiDraw::DrawToggle`.
- Leyendas permanentes de las filas 22/23 (`UP/DN row  L/R edit  A coarse`,
  `SELECT page  START play`) retiradas; migradas a `HelpRegistry`
  (sección MIXER) y mostradas vía HelpOverlay (SELECT+R1).
- Leyendas de la página MIX (filas 19-21 y 23, `A+UP/DN x10`,
  `L/R ch L->MST R1+B mute`, `START play R1+A solo R2+A instr`,
  `R2 edit VOL/RET SELECT [1/5]`) retiradas a Help.
- Commits: `978d8a9`.

### InstrumentView
- Cabeceras de bloque (INSTRUMENT/FILTER/BITCRUSHER/PLAYBACK/EFFECT SENDS/
  AUTOMATION) vía `UiDraw::DrawSectionHeader`.
- Hint `R1+RIGHT USB REC` retirado de la fila 0; migrado a `HelpRegistry`
  (sección INSTRUMENT, `R1+RIGHT = USB record`).

## Capturas de referencia (estado anterior / RC2)

Se documentarán aquí en la fase completa las capturas ASCII antes/después por
vista (Song, Chain, Phrase, Table, Groove, Instrument, Mixer).

### Graphical Chopper (punto 18)
Ya modernizado: la forma de onda real se renderiza por framebuffer overlay
(`tf_vline`/`tf_rect` en `PLATFORM_TREEFROG`) con amplitud por columna,
región seleccionada (banda + raíles finos), marcadores de corte, eje central
y cursor. El borde ASCII `+----+` de la capa de texto es el marco estructural
de la banda (KEEP, ver OBSOLETE_FEATURE_AUDIT).

El overlay de progreso de operación (`drawOperationOverlay`) y el panel
Pitch/Env (`drawPitchScreen`) usan marcos invertidos propios que
sobrescriben visualmente la banda durante el procesamiento/edición; se
conservan como excepción documentada del punto 24/17 (no son
`DrawModalFrame` porque el bloqueo sólido sobre la forma de onda es
intencional).

## Checklist por vista

- [x] Mixer MASTER: título centrado (DrawCenteredTitleAt), toggles de bypass,
      leyendas a Help.
- [x] Instrument: cabeceras de sección vía UiDraw, hint USB a Help.
- [ ] Song: filas con jerarquía, hints mínimos.
- [ ] Chain: título centrado, cursor coherente.
- [ ] Phrase: título centrado, notas con CD_NORMAL/HILITE.
- [ ] Table: título centrado, comandos con barras/valores sólidos.
- [ ] Modales: marco unificado, título centrado, sin desbordes.