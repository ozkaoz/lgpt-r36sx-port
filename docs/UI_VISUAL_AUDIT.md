# UI_VISUAL_AUDIT — Auditoría visual de la UI LGPT R36SX

Versión: RC3 base (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, punto 28).
Marco normativo: UI_STYLE_GUIDE.md. Checklist por punto del plan.

Estado: auditoría en curso en la fase base; las capturas ASCII de referencia
se toman en la fase completa (requieren hardware/pantalla R36SX).

## Checklist RC3 (estado fase base)

| Punto | Descripción | Estado fase base |
|---|---|---|
| 2 | Título centrado (fila 0, `(40-len)/2`) | Implementado en UiDraw::DrawCenteredTitle |
| 4 | Filas de valor con jerarquía | Implementado en UiDraw::DrawValueRow |
| 5 | Toggles ON/OFF | Implementado en UiDraw::DrawToggle |
| 6 | Barras sólidas | Implementado en UiDraw::DrawSolidBar/DrawBipolarBar |
| 7 | Bypass unificado (primera fila, ON=desactivado) | Implementado en MixerView (bypass primero) |
| 19 | Librería de primitivas | UiDraw.h/.cpp |
| 20 | Colores semánticos | UiColors.h |
| 24 | Modales unificados | Pendiente fase completa |
| 30-32 | Tests (colores/toggles/barras/layout) | Pendiente tests RC3 |
| 18 | Graphical Chopper modernizado | Pendiente fase completa |

## Capturas de referencia (estado anterior / RC2)

Se documentarán aquí en la fase completa las capturas ASCII antes/después por
vista (Song, Chain, Phrase, Table, Groove, Instrument, Mixer).

## Checklist por vista

- [ ] Song: título centrado, filas con jerarquía, hints mínimos.
- [ ] Chain: título centrado, cursor coherente.
- [ ] Phrase: título centrado, notas con CD_NORMAL/HILITE.
- [ ] Table: título centrado, comandos con barras/valores sólidos.
- [ ] Instrument: páginas con tabs, valores con jerarquía.
- [ ] Mixer: barras sólidas de canal/envíos, páginas FX con jerarquía.
- [ ] Modales: marco unificado, título centrado, sin desbordes.