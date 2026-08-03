# LGPT R36SX - Bacon 1.1 - FX Dev (release candidate RC3 base)

**ESTADO: RELEASE CANDIDATE — NO ES UNA VERSIÓN ESTABLE.**

Release candidate RC3 (fase base) sobre RC2 (Bacon 1.1 - FX Dev). Integra el
arranque de la modernización visual integral (plan de 33 puntos en
`docs/PLAN_RC3_MODERNIZACION_VISUAL_ES.md`): bypass unificado en las páginas
master, librería compartida `UiDraw`, colores semánticos `UiColors` y el
sistema de ayuda centralizado. **No modifica el motor FX, la persistencia ni
la compatibilidad de proyectos.**

## Cambios RC3 base

### 1. Bypass unificado (primera fila)

En las páginas master DELAY, REVERB, EQ y COMP el parámetro `BYPASS` es la
**primera fila visual y lógica** (semántica `ON = efecto desactivado`, tal
como establece el plan punto 7). EQ/COMP ya lo tenían primero; DELAY/REVERB
se reordenaron. La navegación (`fxMoveRow`/`fxEditRow`/`fxResetRow`) y el
dibujo comparten los helpers `fxBypassId`/`fxIdForRow`/`fxCountOnPage`, de
modo que el cursor siempre cae sobre la fila que se pinta en pantalla. La
tabla `kFxParams_` y el enum `FxParamId` quedan **byte-idénticos**:
persistencia bit-idéntica, proyectos antiguos intactos.

### 2. Librería UiDraw

Nueva capa compartida `UiDraw` (`Views/BaseClasses/UiDraw.h/.cpp`):

- `DrawCenteredTitle` — título centrado `x=(40-len)/2` en fila 0.
- `DrawSectionHeader`, `DrawValueRow` (jerarquía label/valor/edición).
- `DrawToggle` (`[ ON ]` / `[ OFF ]`, nunca 0/1).
- `DrawSolidBar`, `DrawBipolarBar`, `DrawProgressBar` (celdas sólidas).
- `DrawTabs`, `DrawModalFrame`, `DrawScrollIndicator`, `DrawSeparator`.

Todas las primitivas clampa su salida a la pantalla lógica 40x30, por lo que
ninguna vista puede dibujar fuera de límites (plan punto 32).

### 3. Colores semánticos (UiColors)

`UiColors.h` define roles `UI_COLOR_TITLE/LABEL/VALUE/TEXT_EDIT/CURSOR/
BORDER/BACKGROUND/ACTIVE/DISABLED` mapeados a los `CD_*` existentes. Las
vistas dibujan con semántica, no con RGB directo (plan punto 20).

### 4. Ayuda centralizada (SELECT+R1)

- `HelpRegistry` registra una sección de ayuda por tipo de vista
  (SONG/CHAIN/PHRASE/PROJECT/INSTRUMENT/TABLE/GROOVE/MIXER).
- `HelpOverlay` abre con `SELECT+R1` desde cualquier pantalla: latch
  mientras se mantiene, no propaga input, no cambia de página, no interfiere
  con el estado.
- `SELECT+R2` conserva intacto el diálogo de Audio Driver (latch propio).

### 5. Auditorías y guía de estilo

- `docs/UI_STYLE_GUIDE.md` — estándar normativo de la UI.
- `docs/UI_CONTROL_AUDIT.md` — textos y controles (plan punto 14).
- `docs/OBSOLETE_FEATURE_AUDIT.md` — funciones obsoletas (plan punto 27).
- `docs/UI_VISUAL_AUDIT.md` — auditoría visual (plan punto 28).

## Pruebas

- Nuevo `tests/test_rc3_base_unified_bypass_uidraw_help.py`
  (`RC3_BASE_UNIFIED_BYPASS_UIDRAW_HELP_OK`).
- Suite completa en verde: 21/21 tests FX + RC2 + RC3 + auditoría
  (`AUDIT_CLEAN_MAIN_U2523_OK`).
- Build `BUILD_U2523_OK`; instalado y verificado en SD
  (`VERIFY_U2523_OK`, `ERRORS=0`). Core `dde0555…`.

## Problemas conocidos

- La migración completa de las leyendas permanentes de página a la ayuda
  (plan punto 26) se hace en la fase completa; en la fase base quedan in
  situ coordinadas con HelpRegistry.
- Falta la modernización de vistas restantes (Song/Chain/Phrase/Table/
  Groove/Instrument) y del Graphical Chopper (plan puntos 17-18): fases
  completas siguientes.
