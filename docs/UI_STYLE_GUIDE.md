# UI_STYLE_GUIDE — Guía de estilo de la interfaz LGPT R36SX

Versión: RC3 base (PLAN_RC3_MODERNIZACION_VISUAL_ES.md).
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

## 7. Pestañas conceptuales

`DrawTabs` renderiza `< izq [actual] der >` centrado, con CD_HILITE1. Se usa
en vistas con subpáginas (Instrument, Mixer) para indicar la página activa.

## 8. Scroll

`DrawScrollIndicator` pinta `^`/`v` en la columna 38 (filas 27/28) cuando el
contenido excede el viewport.

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
