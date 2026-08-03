# LGPT R36SX - Bacon 1.1 - FX Dev (release candidate RC5)

**ESTADO: RELEASE CANDIDATE — NO ES UNA VERSIÓN ESTABLE.**

Release candidate RC5 sobre RC4 full (Bacon 1.1 - FX Dev). Iteración
**exclusiva de layout/renderizado**: centra horizontal y verticalmente cada
bloque tipo menú sobre la cuadrícula real de 40x30 y corrige el centrado
global de los diálogos modales. **No modifica el motor FX/DSP, el FxEngine,
la persistencia, la navegación, los colores, el footer, el Graphical Chopper
ni las cuadrículas del tracker** (Song/Chain/Phrase/Table/Groove no se
recentran: sus columnas, cursores, playback markers y overlays dependen de
coordenadas compartidas).

## Cambios RC5

### Geometría compartida centralizada
- Constantes en `UiDraw.h`: `kScreenWidth=40`, `kScreenHeight=30`,
  `kTitleRow=0`, `kSubtitleRow=1`, `kMenuBandTop=3`, `kMenuBandBottom=25`,
  `kFooterTop=27`, `kFooterBottom=29`.
- El cuerpo de un menú principal se centra en la banda **3..25**; el footer
  (filas 27..29) nunca participa en el centrado. La fila 26 queda como
  separación visual.

### API de layout ampliada (UiDraw)
- `CenterTextX(text, viewportWidth)` — por defecto `kScreenWidth`; los modales
  pasan el ancho **local** de su ventana.
- `MakeCenteredMenuLayout(rowCount, labelWidth, valueWidth, spacing,
  viewportWidth, contentTop, contentBottom)` — por defecto viewport 40 y banda
  3..25; margen máximo de 1 celda (2 para separadores impares), `labelX`/
  `valueX`/`startX`/`startY` calculados, coordenadas nunca fuera del viewport.
- Se elimina la dependencia conceptual de una cuadrícula 40x20.

### Modales centrados
- `ModalView::SetWindow`: `top=(30-height)/2+topOffset_`, clamped para que el
  marco completo (bordes en `top-2` y `top+height+1`) quede dentro de las filas
  0..29. Guarda `width_`/`height_` y expone `GetWindowWidth()`/
  `GetWindowHeight()`.
- `TreeFrogProjectExitModal`: centra título e items dentro de las **28
  columnas locales** del modal, usa el `MenuLayout` calculado y elimina
  `(void)ml`.
- Revisados todos los consumidores de `SetWindow` (AudioDriverModal, Help,
  CommandSelectorModal, ImportSample, MessageBox, SampleManager,
  TreeFrogMenu, SelectProject, UsbRecordModal, TreeFrogTextEditor,
  ProjectExit); ninguno queda fuera de pantalla con la nueva fórmula.

### Páginas MASTER del Mixer (bloques centrados en 3..25)
- **DELAY** / **REVERB** (7 filas): bloque horizontalmente centrado con
  columnas label/valor calculadas; `MakeCenteredMenuLayout(7,9,8,2)` /
  `(7,8,8,2)`; filas en `ml.startY+p`. Sin `x=2` ni `y=2+p`.
- **EQ** (16 filas): bloque completo (BYpass + cabecera + LOW/MID/HIGH +
  separadores) centrado vertical y horizontalmente; `MakeCenteredMenuLayout
  (16,6,9,2)`; todas las bandas visibles y seleccionables. Sin `x=13`.
- **COMP** (8 parámetros + Gain Reduction): `MakeCenteredMenuLayout(9,11,8,3)`
  (espaciado de 3 celdas para el label de 14 caracteres "Gain Reduction") y
  el readout GR en `ml.startY+9`, no seleccionable. Sin `labelX=8`/`p+2`/`y=12`.
- El **bypass** de cada página usa la misma columna `valueX` que sus filas de
  parámetros: `DrawBypassRow(view, labelX, valueX, y, on, selected)` (la firma
  antigua delega con `x+8`).
- El título de subpágina se mantiene centrado en la fila 1.

### Mixer principal y barras VU (página MIX)
- 9 barras de **una sola celda** de ancho: MASTER + 8 canales.
  `meterCount=SONG_CHANNEL_COUNT+1`, `meterWidth=1`, `meterPitch=4`,
  `bankWidth=(9-1)*4+1=33`, `firstMeterX=(40-33)/2=3` → posiciones
  `3,7,11,15,19,23,27,31,35`, todas dentro de 0..39.
- `drawVolumeBar`/`drawMasterBar`: una columna por barra (`totalCells=height`,
  sin el loop `c<2` ni `2*height`); el marcador de volumen y el número usan la
  misma escala vertical (resolución vertical de 22 a 11 pasos, aceptable).
- Textos centrados sobre el eje de cada barra: `MST`, `00..07`, valores `%3d`,
  indicador `M`. Se conservan colores (master cyan, selección purple, mute
  dim), `vuDisplay_`, `mixVULevel()`, `masterLevel`.
- La composición completa (encabezados, barras, valores, M, FX RETURNS) se
  centra en la banda 3..25 (labelY=6, barras 7..18, num 20, mute 21).
- **FX RETURNS** en su propia fila (`drawMixReturns(retY)`), debajo de los
  posibles indicadores `M`; no colisiona con un mute. Se conserva el
  resaltado independiente de cada valor (D/R no se concatenan en un solo
  buffer).

### Auditoría de vistas restantes
- Clasificación: menú/formulario, secuenciador/cuadrícula, overlay/modal,
  footer/estado. Se centran menús y modales cuando su bounding box está
  desplazado; **no** se recentran Song/Chain/Phrase/Table/Groove (cuadrículas)
  ni el Graphical Chopper. `View::GetAnchor()` no se toca globalmente.
- InstrumentView (FieldView con posiciones propias) y ConsoleView se
  conservan; CommandSelectorModal sigue centrado por `SetWindow` con
  coords. locales.

## Pruebas

- Nuevo `tests/test_ui_centered_layout.py` (12 comprobaciones de bounding box:
  constantes, CenterTextX por viewport, paridad de anchos/altos, DELAY/REVERB,
  EQ/COMP, ProjectExit local, SetWindow 30 filas, 9 metros, bloque del Mixer,
  columnas de bypass, tabla DSP/enums intactos, banda footer).
- Tests RC3/RC4 actualizados para comprobar semántica/layout en lugar de
  coordenadas mágicas el literal
  `UiDraw::DrawBypassRow(*this,x,2+p,...`.
- Suite completa sobre `AUDIT_CLEAN_MAIN_U2523_OK`.
- Build del target R36SX: `BUILD_U2523_OK` (core `lgpt_r36sx_u2523.so` +
  daemon `r36s_u2523_usb_audio_io`; marcadores U2523 y ABI7 presentes;
  `SHA256SUMS.txt`).

## Problemas conocidos

- La validación manual y las capturas reales en pantalla requieren hardware
  R36SX (no disponible); el estado visual por vista se rastrea en
  `UI_VISUAL_AUDIT.md` (incluye snapshots ASCII).
- `drawFxParamRow` genérico sigue disponible como KEEP_COMPAT para páginas
  residuales del bucle genérico.
- Pendiente de cierre: instalación/verificación en SD física y captura real.