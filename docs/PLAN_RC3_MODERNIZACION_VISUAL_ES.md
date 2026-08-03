# PLAN_RC3_MODERNIZACION_VISUAL_ES — Iteración de modernización visual integral (Bacon 1.1 - FX Dev RC3)

Base: release candidate `Bacon-1.1-FX-Dev-RC2` (commit `9a626ac`, main).
Hardware objetivo: R36SX v2.6. Pantalla lógica ~40x30 caracteres.
Área segura: fila 0 (título), filas 1-25 contenido, columna 0-39.

El objetivo es convertir el port LGPT R36SX en una aplicación coherente,
moderna y visualmente acabada, respetando las garantías técnicas de las
iteraciones anteriores (cero allocs/disco/logging en el callback de audio,
compatibilidad bit-identica de proyectos, FourCC internos inmutables).

---

## A. Alcance (33 puntos)

### 1. Coherencia visual global
Todas las vistas (Song, Chain, Phrase, Table, Instrument, Mixer, Groove,
modales y listas) comparten los mismos patrones de layout, colores,
espaciado y tipo de cursor.

### 2. Título centrado
Toda página muestra su título centrado horizontalmente:
`x = (screenWidth - titleLength) / 2`, en fila 0, con `CD_HILITE1`.

### 3. Encabezados de sección
Cada sección dentro de una página usa el mismo estilo de encabezado
(`DrawSectionHeader`), alineado a la izquierda, con `CD_HILITE1`.

### 4. Filas de valor
Toda fila editable sigue el patrón `label: valor` con jerarquía de colores:
label en `CD_NORMAL`, valor en `CD_HILITE1`, fila en edición invertida
(`CD_HILITE2`).

### 5. Toggles
Los controles binarios usan un componente compartido `DrawToggle`
(`[ ON ]` / `[ OFF ]`, nunca 0/1). Bypass y modos siguen esta convención.

### 6. Barras sólidas
Los indicadores de nivel/envío usan barras sólidas de bloques (patrón
`UIIntVarField::Draw`), no caracteres de relleno repetidos ni texto largo.

### 7. Bypass unificado
Las páginas master (DELAY, REVERB, EQ, COMP) muestran en su PRIMERA fila
visual y lógica `BYPASS [ ON ]` / `BYPASS [ OFF ]` mediante el componente
compartido `DrawMasterBypassRow`. Semántica: ON = efecto desactivado (el
efecto pasa la señal intacta), OFF = efecto activo. Nunca 0/1.

### 8. Jerarquía de páginas master
Todas las páginas master (DELAY, REVERB, EQ, COMP) comparten el mismo
diseño: título centrado, fila de bypass arriba, luego los parámetros con
jerarquía de colores, y la fila en edición resaltada.

### 9. Send bars
Los envíos EFFECT SENDS usan barras sólidas de 8 celdas con celda de
cursor invertida; el estado "no usado" se muestra limpio (sin ruido ASCII).

### 10. Help global (SELECT+R1)
`SELECT+R1` abre la ayuda contextual de la vista activa desde cualquier
pantalla. Al soltar, el overlay desaparece y la vista no cambia.

### 11. SELECT+R2 Audio Driver
`SELECT+R2` conserva el selector de Audio Driver existente (no se rompe la
navegación ni el estado).

### 12. HelpRegistry
Registro centralizado de entradas de ayuda por vista (contexto). Cada
vista declara su sección de ayuda sin lógica de dibujo dispersa.

### 13. HelpOverlay
Overlay modal de ayuda: latched mientras se mantiene SELECT, contexto por
vista activa, índice global de atajos, texto envuelto a <=40 columnas. No
propaga el evento, no cambia de página, no interfiere con el estado.

### 14. Auditoría de textos de UI
`docs/UI_CONTROL_AUDIT.md` documenta cada `DrawString`/`SetStatus`/
`sprintf`/`HelpLegend` con clasificación:
- `KEEP_LABEL` / `KEEP_STATUS` / `KEEP_ERROR`
- `MOVE_TO_HELP`
- `UPDATE_AND_MOVE_TO_HELP`
- `REMOVE_STALE` (cadáveres de código)
- `REMOVE_OBSOLETE_FEATURE`
Cada entrada verificada contra su handler y file:line.

### 15. Énfasis de cursor/frame coherente
La celda o fila activa se resalta con inversión (`CD_HILITE2`) en todas las
vistas, con el mismo patrón de acento.

### 16. Pestañas conceptuales
Las vistas con subpáginas (Instrument, Mixer) usan indicadores de pestaña
consistentes (`<-Tabla->` o similar) con el mismo estilo.

### 17. Widgets ASCII
Se eliminan los widgets ASCII rudimentarios (barras `====`, marcos
`+-----+`, separadores `----`, sprites de caracteres) en favor de
componentes sólidos. Excepciones documentadas (solo contenido legítimamente
textual).

### 18. Graphical Chopper
Modernización del chopper gráfico: forma de onda real, región seleccionada
visible, cortes marcados. Sin sustitutos ASCII.

### 19. Librería UiDraw
Nueva capa compartida `UiDraw` con: `DrawCenteredTitle`, `DrawSectionHeader`,
`DrawValueRow`, `DrawToggle`, `DrawSolidBar`, `DrawBipolarBar`,
`DrawProgressBar`, `DrawTabs`, `DrawScrollIndicator`, `DrawModalFrame`,
`DrawSelectionRegion`.

### 20. Colores semánticos
Sistema `UI_COLOR_*` mapeado a los `CD_*` existentes. Ninguna vista dibuja
RGB directo. La semántica es la fuente de verdad, no el valor del color.

### 21. Código limpio
Comentarios y strings en inglés, sin código muerto, sin `#if 0`, sin
`TODO` heredados, sin debug prints. Documentación en español solo en `docs/`.

### 22. Guía de estilo
`docs/UI_STYLE_GUIDE.md` define el estándar: títulos, colores, barras,
toggles, modales, tablas y nomenclatura de componentes.

### 23. Scroll/indicadores
Las vistas con contenido que supera el área muestran indicador de scroll
(`DrawScrollIndicator`) y posicionan el viewport coherentemente.

### 24. Modales unificados
Todos los modales (selector de comandos, dialogs de confirmación) usan
`DrawModalFrame` con el mismo estilo de marco y sombreado.

### 25. SELECT como segunda función
La tecla SELECT se documenta y muestra consistentemente como segunda
función en todas las vistas que la usan (Help).

### 26. Indicadores de navegación
Los hints de navegación son uniformes en todas las vistas (misma fuente,
mismo formato, misma posición), con la menor longitud posible.

### 27. Auditoría de funciones obsoletas
`docs/OBSOLETE_FEATURE_AUDIT.md` lista funciones obsoletas o sin usar con
su file:line y decisión (REMOVE / KEEP_HIDDEN / KEEP_COMPAT). Las
características ocultas de compatibilidad (legacy, migraciones, FourCC,
sends heredados) se conservan si son necesarias para la carga de proyectos
antiguos.

### 28. Auditoría visual
`docs/UI_VISUAL_AUDIT.md` documenta el estado visual de cada vista con
capturas ASCII de referencia (antes/después) y checklist por punto del plan.

### 29. UI_STYLE_GUIDE aplicado
La guía de estilo es el marco normativo de la auditoría visual (punto 28);
cualquier desviación documentada y justificada.

### 30. Tests
Suite de tests para: selector FX (colores, encabezados no seleccionables,
celdas vacías, página Legacy, cancelación), bypass unificado (semántica
ON/OFF en las 4 páginas), hex nibble (20->30/21/10/1F por nibble), Help
(latch, contexto, scroll conservado, audio), detección de widgets ASCII y
límites de layout.

### 31. Detección estática de widgets ASCII
Test estático que detecta caracteres de widget ASCII en vistas (con lista
de excepciones documentada por file:line).

### 32. Tests de límites de layout
Test estático que verifica que ninguna vista dibuja fuera de
`0 <= x < 40` y `0 <= y < 30`.

### 33. Compatibilidad preservada
Las restricciones de compatibilidad se conservan explícitamente: FourCC
internos inmutables, persistencia bit-identica, defaults legacy preservados,
sin efectos nuevos, sin tocar FxEngine/DSP callback en esta iteración.

---

## B. Fase base (primera entrega de esta iteración)

1. **Colores semánticos** (punto 20): crear mapeo `UI_COLOR_*` -> `CD_*` y
   aplicarlo en las vistas que se toquen.
2. **Librería UiDraw** (punto 19): implementar los componentes compartidos
   empezando por `DrawCenteredTitle`, `DrawToggle`, `DrawSolidBar`,
   `DrawModalFrame`.
3. **Help centralizado** (puntos 10-13): `HelpRegistry` + `HelpOverlay`,
   `SELECT+R1` abre/cierra con latch, `SELECT+R2` conserva Audio Driver.
4. **Bypass unificado** (punto 7): `DrawMasterBypassRow` aplicado a DELAY,
   REVERB, EQ y COMP.
5. **Auditorías** (puntos 14, 27, 28, 29): crear los cuatro documentos.
6. **Tests** (puntos 30-32): suite nueva + revalidación completa.

## C. Fase completa (siguientes entregas)

- Modernización de todas las vistas (Song, Chain, Phrase, Table, Groove,
  modales, listas) con los componentes UiDraw.
- Graphical Chopper (punto 18).
- Eliminación de widgets ASCII restantes (punto 17) y retirada de leyendas
  permanentes movidas a Help.
- Indicadores de scroll y tabs en todas las vistas con subpáginas.

---

## D. Criterios de aceptación

| Área | Criterio |
|---|---|
| Build | `BUILD_U2523_OK`, sin warnings nuevos en módulos tocados |
| Auditoría | 27/27 grupos de auditoría PASS + grupos nuevos RC3 |
| Tests | 21/21 tests FX + suite nueva RC3 PASS (Windows y WSL) |
| Compat | Proyectos RC2/R1 se cargan bit-identicos (hash sin cambios) |
| Rendimiento | Cero allocs/syscalls en callback de audio (sin regresión) |
| UI | Ninguna vista dibuja fuera de 40x30; sin widgets ASCII no documentados |
| Ayuda | SELECT+R1 abre ayuda contextual con latch; SELECT+R2 intacto |
| Release | Core instalado en SD `/mnt/f` verificado, docs y CHANGELOG actualizados |

---

## Reglas transversales
- No añadir efectos nuevos. No tocar FxEngine/DSP callback/identificadores
  internos/FourCC/persistencia/legacy/defaults que preservan proyectos.
- Cero allocs/disco/logging/mutex/syscalls en callback de audio.
- Código nuevo con comentarios en inglés; documentación en `docs/` en español.
- Test suite y auditoría se ejecutan en Windows y WSL antes de publicar.
- Versionado: commit por bloque autocontenido; release candidate siguiente
  tras validación completa.
