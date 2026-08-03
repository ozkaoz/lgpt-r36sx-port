# UI_STYLE_GUIDE — Guía de estilo de la interfaz LGPT R36SX

Versión: RC4 (P0-P6 completos; P7-P8 en curso).
Pantalla lógica: 40x30 caracteres (AppWindow flush loop).
Área segura: fila 0 = título, filas 1-25 = contenido, filas 26-29 = map/notes.

Esta guía es la fuente normativa de la auditoría visual (UI_VISUAL_AUDIT.md).
Cualquier desviación debe documentarse y justificarse allí.

---

## 1. Colores (semánticos)

Toda vista dibuja con roles semánticos `UI_COLOR_*` (UiColors.h), que resuelven
a los `CD_*` del renderer. Ninguna vista usa RGB directo.

| Rol | CD_* | Uso |
|---|---|---|
| UI_COLOR_TITLE | CD_HILITE1 | Título de página y encabezados de sección |
| UI_COLOR_LABEL | CD_NORMAL | Etiquetas estáticas, hints |
| UI_COLOR_VALUE | CD_HILITE1 | Valor actual de un parámetro |
| UI_COLOR_TEXT_EDIT | CD_HILITE2 | Fila/celda en edición (con inversión) |
| UI_COLOR_CURSOR | CD_HILITE2 | Acento de celda activa |
| UI_COLOR_BORDER | CD_BORDER | Marcos modales y separadores |
| UI_COLOR_BACKGROUND | CD_BACKGROUND | Interior de modales |
| UI_COLOR_ACTIVE | CD_HILITE1 | Estado ON / activo |
| UI_COLOR_DISABLED | CD_MUTE | Estado OFF / desactivado |
| UI_COLOR_SECTION | CD_HILITE1 | Cabeceras de familia/bloque (LOW/MID/HIGH) |
| UI_COLOR_TEXT | CD_NORMAL | Texto de cuerpo / comandos |
| UI_COLOR_SELECTED | CD_HILITE2 | Fila seleccionada (fondo/inversión) |
| UI_COLOR_SELECTED_TEXT | CD_HILITE2 | Texto de la fila seleccionada |
| UI_COLOR_WARNING | CD_WARNING | Estado no fatal (DrawStatusMessage) |
| UI_COLOR_ERROR | CD_ERROR | Estado de error (DrawErrorMessage) |
| UI_COLOR_SUCCESS | CD_HILITE1 | Resultado positivo |
| UI_COLOR_LEGACY | CD_MUTE | Items de combos legacy |
| UI_COLOR_BAR_FILL | CD_HILITE1 | Celdas sólidas de barra |
| UI_COLOR_BAR_EMPTY | CD_NORMAL | Celdas vacías de barra |
| UI_COLOR_WAVEFORM | CD_HILITE1 | Forma de onda del chopper |
| UI_COLOR_WAVEFORM_SELECTED | CD_HILITE2 | Región de onda seleccionada |
| UI_COLOR_MARKER | CD_CURSOR | Marcadores de corte |

Jerarquía de una fila de valor (patrón DELAY/REVERB MASTER):
- Label: CD_NORMAL.
- Valor: CD_HILITE1.
- Fila en edición: invertida con CD_HILITE2.

## 2. Título

- Posición: fila 0, centrado: `x = (40 - len(title)) / 2`.
- Color: CD_HILITE1.
- Sin padding manual de espacios: la posición se calcula, no se rellena.

## 3. Filas de valor

`DrawValueRow(label, value)` con la jerarquía del punto 1. El label y el valor
se dibujan como dos cadenas, no como un solo buffer con espacios fijos.

## 4. Toggles

`DrawToggle` renderiza `[ ON ]` / `[ OFF ]`. Prohibido 0/1 en la UI.
- ON: CD_HILITE1; OFF: CD_MUTE.
- Fila en edición: CD_HILITE2 + inversión.

Semántica de bypass (páginas master): **ON = efecto desactivado** (el efecto
pasa la señal intacta), **OFF = efecto activo**. Es la convención del port.

## 5. Barras

`DrawSolidBar`/`DrawBipolarBar` usan celdas invertidas (sólidas) para el
relleno y celdas CD_HILITE1 huecas para el resto. Prohibidos caracteres de
relleno repetidos (`====`, `####`, sprites ASCII) en widgets de nivel/envío.

## 6. Modales

`DrawModalFrame` (o `ModalView::SetWindow`) con marco CD_BORDER y título
centrado. El contenido no sobresale del marco. Aplicable a selector de
comandos, diálogos de confirmación y ayuda.

### 6b. Menús verticales centrados (RC4 P4)

`UiDraw::CenterTextX` centra un texto en la pantalla de 40 celdas
(`(40-len)/2`) y `UiDraw::MakeCenteredMenuLayout` calcula el bloque centrado
de un menú vertical (columnas label/valor opcionales, centrado vertical en la
banda 1..29). Los menús de modales con items se dibujan con estos helpers, no
con coordenadas fijas pegadas al borde.

### 6c. Mensajes de estado y error (RC4 P5)

`DrawStatusMessage` (CD_WARNING) para estados no fatales y `DrawErrorMessage`
(CD_ERROR) para errores. Los diálogos con estado distinguen severidad con un
flag (p. ej. `statusIsError_` en SampleManagerDialog). Prohibido pintar
errores con CD_HILITE1 como si fueran estados normales.

### 6d. Sin marcos ASCII (RC4 P6)

Prohibidos caracteres de caja ASCII (`+`, `-`, `|`) para dibujar marcos o
bordes en la capa de texto. Los bordes se dibujan con celdas sólidas del rol
`UI_COLOR_BORDER` (CD_BORDER) manteniendo la geometría exacta (el Graphical
Chopper modernizado lo aplica en filas 1..22 / columnas 0/39).

## 7. Pestañas conceptuales

`DrawTabs` renderiza `< izq [actual] der >` centrado, con CD_HILITE1. Se usa
en vistas con subpáginas (Instrument, Mixer, HelpOverlay) para indicar la
página/sección activa.

## 8. Scroll

`DrawScrollIndicator` pinta `^`/`v` en la columna 38 (filas 27/28) cuando el
contenido excede el viewport. RC4 P6: usado en HelpOverlay para indicar el
scroll de la sección (proporcional a la posición).

## 9. Hints de navegación

Mínimos y uniformes. Los hints de larga duración que describen atajos viven en
la ayuda (HelpRegistry / SELECT+R1), no como líneas permanentes en cada página.

## 10. Separadores

`DrawSeparator` usa celdas CD_BORDER. Prohibidos `----` manuales en layouts.

## 11. Texto y código

- Strings en la UI y comentarios en inglés.
- Documentación en `docs/` en español.
- Sin `#if 0`, sin código muerto, sin `TODO` heredados.

## 12. Compatibilidad

Lo oculto (legacy, migraciones, FourCC, sends heredados) que sea necesario
para cargar proyectos antiguos se conserva, aunque no se muestre en la UI.

## 13. Nomenclatura de componentes

- Componentes compartidos: prefijo `Ui` (UiDraw, UiColors, UIStyle...).
- Help central: `HelpRegistry` (datos) + `HelpOverlay` (render).
- Roles de color: prefijo `UI_COLOR_`.
