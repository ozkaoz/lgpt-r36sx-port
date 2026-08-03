# UI_VISUAL_AUDIT — Auditoría visual de la UI LGPT R36SX

Versión: RC6 (centrado real de cuadrículas Phrase/Table/Instrument, barras
VU de MIX compactadas, títulos MASTER pegados a su menú; sobre el RC5 de
bloques tipo menú y modales centrados).
Marco normativo: UI_STYLE_GUIDE.md. Checklist por punto del plan.

Estado: auditoría en curso en la fase completa; las capturas ASCII de
referencia en pantalla real R36SX se completan cuando haya hardware; el
estado de cada vista se rastrea en el checklist por vista.

## Geometría normativa (RC5)

- Constantes compartidas: `kScreenWidth=40`, `kScreenHeight=30`,
  `kMenuBandTop=3`, `kMenuBandBottom=25`, `kFooterTop=27`, `kFooterBottom=29`
  en `UiDraw.h`.
- El cuerpo de un menú principal se centra en la banda 3..25, nunca en toda
  la pantalla ni en 1..29.
- `UiDraw::CenterTextX(text, viewportWidth)` y `UiDraw::MakeCenteredMenuLayout
  (...[, viewportWidth, contentTop, contentBottom])` con margen máximo de 1
  celda (2 para separadores impares).
- Las páginas MASTER del Mixer y el modal de salida se dibujan con estos
  helpers; las barras VU de la página MIX son de 1 celda con pitch 3 (RC6).
- Las cuadrículas Phrase/Table/Instrument se centran con desplazamientos
  locales por vista; `View::GetAnchor()` NO se modifica globalmente (RC6).

## Checklist RC3/RC4/RC5 (estado fase completa)

| Punto | Descripción | Estado |
|---|---|---|
| 2 | Título centrado (fila 0, `(40-len)/2`) | Implementado en UiDraw::DrawCenteredTitle |
| 2b | Título centrado en fila de subpágina | Implementado en UiDraw::DrawCenteredTitleAt (páginas FX) |
| 3 | Cabeceras de sección | Implementado en UiDraw::DrawSectionHeader |
| 4 | Filas de valor con jerarquía | Implementado en UiDraw::DrawValueRow + drawMasterFxRow |
| 5 | Toggles ON/OFF | Implementado en UiDraw::DrawToggle (envuelto por DrawBypassRow) |
| 6 | Barras sólidas | Implementado en UiDraw::DrawSolidBar/DrawBipolarBar |
| 7 | Bypass unificado (primera fila, ON=desactivado) | Implementado en MixerView (bypass primero) |
| 8 | Bloques/menús centrados 3..25 | RC5: DELAY/REVERB/EQ/COMP, MIX principal, ProjectExit (banda/viewport) |
| 17 | Widgets ASCII retirados | Chopper sin marco ASCII (P6); placeholders `--`/`----` son valores |
| 18 | Graphical Chopper modernizado | Implementado (overlay framebuffer) + frame sólido sin ASCII (P6) — **fuera de alcance RC5** |
| 19 | Librería de primitivas | UiDraw.h/.cpp (RC4 P5 + RC5 viewport/banda) |
| 20 | Colores semánticos | UiColors.h (P5: UI_COLOR_WARNING/ERROR) |
| 24 | Modales centrados | RC5: `SetWindow` sobre 30 filas con bordes en 0..29; ProjectExit local |
| 30-32 | Tests (colores/toggles/barras/layout) | RC2 + RC3 + RC4 + RC5 test_ui_centered_layout PASS |

## Avances RC4 (P3-P6)

### P3 — Ayudas permanentes retiradas
La navegación permanente de la fila de título del Mixer
(`DrawString(7,pos._y,"R+UP Song")`) fue retirada; la acción vive en
`HelpRegistry` (sección MIXER, `R+UP → back to Song`) y se muestra vía
HelpOverlay (SELECT+R1). No quedan hints permanentes en Song/Chain/Phrase/
Table/Groove/Instrument/Project.

### P4 — Primitivas de centrado (primer paso)
`UiDraw::CenterTextX` centra un texto en la pantalla de 40 celdas
(`(40-len)/2`) y `UiDraw::MakeCenteredMenuLayout` calcula el bloque centrado
de un menú vertical (columnas label/valor opcionales, centrado vertical en la
banda 1..29). Primer consumidor: el modal de salida de proyecto
(`TreeFrogProjectExitModal` en ProjectView) centra título e items. Aclaración:
en RC4 **no** se centraron los bloques de las páginas MASTER del Mixer ni el
resto de menús; el centrado completo por viewport/banda llega en RC5.

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

## RC5 — Centrado real de bloques tipo menú y modales

Iteración exclusiva de layout sobre RC4. **No toca** motor FX/DSP, FxEngine,
ranges/defaults/enums, navegación, persistencia, colores, footer, Chopper ni
las cuadrículas del tracker.

### Geometría nueva
- `UiDraw.h`: constantes `kScreenWidth=40`, `kScreenHeight=30`,
  `kMenuBandTop=3`, `kMenuBandBottom=25`, `kFooterTop=27`, `kFooterBottom=29`,
  `kTitleRow=0`, `kSubtitleRow=1`.
- `CenterTextX(text, viewportWidth)` (por defecto `kScreenWidth`).
- `MakeCenteredMenuLayout(rowCount, labelWidth, valueWidth, spacing,
  viewportWidth, contentTop, contentBottom)` — por defecto viewport 40 y banda
  3..25; margen máximo de 1 celda (2 para separadores impares), clamp correcto,
  coordenadas nunca fuera del viewport. Los modales pasan su viewport local.
- `ModalView::SetWindow`: `top = (30 - height) / 2 + topOffset_`, clamped para
  que el marco completo (bordes en `top-2` y `top+height+1`) quede en las filas
  0..29. Guarda `width_`/`height_` con `GetWindowWidth()`/`GetWindowHeight()`.
- `TreeFrogProjectExitModal`: usa `GetWindowWidth/Height`, `CenterTextX` sobre
  el viewport local del modal y `MakeCenteredMenuLayout(3, 20, 0, 0, vw, 0,
  vh-1)`; se elimina `(void)ml`.
- `DrawBypassRow(view, labelX, valueX, y, on, selected)` usa la misma columna
  de valor que las filas de parámetros; la firma antigua delega con `x+8`.

### Bounding boxes (coordenadas de pantalla, sin el título de fila 1)

| Bloque | Antes (RC4) | Después (RC5) | Banda |
|---|---|---|---|
| MIX (9 barras) | x0=8..35 (barras de 2 celdas), y 2..16 | barras en x=7,10,...,31 (1 celda, pitch 3; bankWidth 25); labelY 6, barras 7..18, num 20, mute 21, RET 22 | 3..25 |
| DELAY (7 filas) | x=2..22, filas 2..8 | bloque x=10..28, filas 11..17 | 3..25 |
| REVERB (7 filas) | x=2..22, filas 2..8 | bloque x=11..28, filas 11..17 | 3..25 |
| EQ (16 filas) | x=13..26, y 2..18 | bloque x=11..27, filas 6..21 | 3..25 |
| COMP (9 filas + GR) | labelX=8, filas 2..10, GR y=12 | bloque x=9..30, filas 10..18, GR fila 19 | 3..25 |
| Project Exit (modal 28x6) | `(20-6)/2`+offset (offset 40x20), título en 40 cols | ventana x=6..33, y=12..17; título local x=8; items locales y=1..3 (abs 13..15) | 0..29 |
| Modal corto (MessageBox, alto 3, ancho según contenido) | top=(20-3)/2+offset | top=(30-3)/2=13, bordes 11..17 dentro 0..29 | 0..29 |
| Modal alto (UsbRecordModal 36x24) | top=(20-24)/2+offset (borde superior fuera) | top=(30-24)/2=3, bordes 1..26 dentro 0..29 | 0..29 |

Margen del cuerpo de cada página respecto a la banda 3..25:
- DELAY: 8 arriba / 8 abajo (11..17). REVERB: 8/8. EQ: 3 arriba / 4 abajo
  (6..21, 16 filas pares). COMP: 7 arriba / 6 abajo (10..19, incluye GR).
- MIX: 3 arriba (6) / 3 abajo (22), centrado vertical de su bloque completo.

### Snapshots textuales 40x30 (páginas MASTER y Project Exit)

```
MIX (fila 0: "Mixer" x=17; fila 1: "MIX" x=21 pre-existente)
row 6 : CH   MST  00   01   02   03   04   05   06   07
rows 7-18: barras verticales de 1 celda en x=7,10,13,16,19,22,25,28,31
row 20: VL   120  100  100  100  100  100  100  100  100
row 21:       M    M    M    M    M    M    M    M    M
row 22: RET  D:200  R:100  FX RETURNS   (D/R resaltados por separado)

DELAY (fila 10: "DELAY MASTER [1/5]" sobre el bloque; bloque filas 11..17)
       labelX=10              valueX=20
row 11: BYPASS                 [ ON ]
row 12: Time                  200 ms
row 13: Feedback                35 %
row 14: LFO Rate               1.00 Hz
row 15: LFO Amount               0 %
row 16: Stereo                  ON
row 17: Echo                    OFF

REVERB (fila 10: título sobre el bloque; bloque filas 11..17, labelX=11)
row 11: BYPASS                 [ ON ]
row 12: Pre-delay               0 ms
row 13: Decay                  1.20 s
row 14: Damping                50 %
row 15: Input Filter            OFF
row 16: Mode                    ECO
row 17: Stereo                  ON

EQ (fila 5: "MASTER EQ [4/5]" sobre el bloque; bloque filas 6..21, labelX=11)
row 6 : BYPASS                 [ ON ]
row 7 :        EN   FRQ    GAIN    Q
row 8 : LOW    ON  100 Hz  +0.0 dB 1.0
row 9 : MID    ON 1000 Hz  +0.0 dB 1.0
row 10: HIGH   ON 10000 Hz +0.0 dB 1.0
row 11:       ---- band separators ----

COMP (fila 9: "MASTER COMP [5/5]" sobre el bloque; bloque filas 10..18,
       GR fila 19, labelX=9)
row 10: BYPASS                 [ OFF ]
row 11: Threshold             -24.0 dB
row 12: Ratio                   4.0:1
row 13: Knee                    6.0 dB
row 14: Attack                 15.0 ms
row 15: Release               200.0 ms
row 16: Makeup                  0.0 dB
row 17: Stereo Link              ON
row 18: Soft Clip                ON
row 19: Gain Reduction          0.0 dB   (no seleccionable)

Project Exit (ventana 28x6, x=6..33, y=12..17)
row 12:  ====borde superior====
row 13:     Project Exit          (título local x=8)
row 14:       (item 0)
row 15:       (item 1)
row 16:       (item 2)
row 17:  ====borde inferior====
```

(En los snapshots, las posiciones de columna reales las fija el código en
`MixerView.cpp`/`ProjectView.cpp`; aquí se marcan los bloques y sus bandas.
La validación visual definitiva con pantalla real R36SX queda pendiente de
hardware.)

## RC6 — Centrado de cuadrículas Phrase/Table/Instrument y MIX compactado

Iteración exclusiva de layout sobre RC5. **No toca** motor FX/DSP, FxEngine,
ranges/defaults/enums, navegación, persistencia, colores, footer, Chopper,
las cuadrículas Song/Chain ni `View::GetAnchor()`.

### Barras VU de la página MIX (compactadas)
- `meterPitch` pasa de 4 a 3: `bankWidth=(9-1)*3+1=25`, `firstMeterX=7`
  (márgenes 7/8). Ejes en x=7,10,13,16,19,22,25,28,31.
- Los textos `MST`, `00..07` y `%3d` se dibujan en `x-1` para asentar su
  columna derecha sobre el eje de 1 celda (la barra sigue en `x`).
- Filas sin cambios: labelY 6, barras 7..18, num 20, mute 21, RET 22.

### Títulos de subpágina MASTER (DELAY/REVERB/EQ/COMP)
- Cada página dedicada dibuja ahora su título con
  `DrawCenteredTitleAt(*this, ml.startY-1, title)`: el título queda sobre la
  fila superior de su bloque centrado (título+menú = una unidad centrada),
  en lugar de la fila 1 flotante del RC5. `drawFxParamPage` calcula el título
  y lo pasa por parámetro a `drawDelayPage/drawReverbPage/drawEqPage/
  drawCompPage`; el fallback genérico conserva la fila 1.
- Filas de título resultantes (sin colisiones con la banda 3..25 ni el
  header de fila 0): DELAY/REVERB 10, EQ 5, COMP 9.

### Phrase (centrado horizontal y vertical)
- `kColX` = {5,8,11,14,19,23,27,31} y `kColHeaderX` = {6,9,12,15,20,24,28,32}:
  el contenido pasa de 9..39 a 5..35 (centro 20, márgenes 5/5). Números de
  fila en x=2 y cursor de play en x=1 (antes 6/5).
- Desplazamiento vertical `anchor._y += 3` en los tres puntos `GetAnchor()`
  de la vista (DrawView, UpdateCursor/ProcessButtonMask, OnPlayerUpdate):
  cuadrícula 7..22, cabeceras fila 6.

### Table (centrado vertical; horizontal ya centrado)
- El contenido ya estaba centrado en horizontal (filas 9..30, centro 19.5),
  por lo que solo se aplica `anchor._y += 3` en sus tres puntos `GetAnchor()`
  (DrawView, updateCursor, OnPlayerUpdate): cuadrícula 7..22, coherente con
  Phrase.

### Instrument (centrado horizontal; vertical ya centrado)
- El bloque de campos (filas 4..26, centro 15) ya estaba centrado en
  vertical. Las dos columnas de campos (etiquetas en x=10 y x+16=26,
  contenido hasta ~37) se desplazan `position._x -= 4` (x=6 / x+16=22) en
  `fillSampleParameters()` y `fillMidiParameters()`, y `hp._x -= 4` en las
  cabeceras de bloque de `DrawView()`.

### Verificación
- `tests/test_ui_centered_layout.py` -> `UI_CENTERED_LAYOUT_OK` (13 checks:
  comprobación 8 con pitch 3 / firstMeterX 7 / textos en x-1; comprobación 10
  con títulos sobre el bloque; comprobación 13 nueva para Phrase/Table/
  Instrument y la no-modificación de `View::GetAnchor()`).

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

- [x] Mixer MASTER: título centrado sobre su bloque (RC6), toggles de bypass,
      leyendas a Help (P3), barras MIX pitch 3 (RC6).
- [x] Instrument: cabeceras de sección vía UiDraw, hint USB a Help, columnas
      centradas en x=6/x+16 (RC6).
- [x] Song: título con rol semántico (UI_COLOR_TITLE), filas con jerarquía.
- [x] Chain: título con rol semántico, cursor coherente.
- [x] Phrase: título con rol semántico, notas con CD_NORMAL/HILITE, cuadrícula
      centrada 5..35 / filas 7..22 (RC6).
- [x] Table: título con rol semántico, comandos con barras/valores sólidos,
      cuadrícula centrada en filas 7..22 (RC6).
- [x] Chopper: frame sólido sin ASCII (P6), overlay de onda preservado.
- [x] SampleManager: estado con roles de severidad (P5).
- [x] ProjectExit: título e items centrados (P4).
- [ ] Modales: marco unificado, título centrado, sin desbordes (revisar en P8).