# LGPT R36SX - Bacon 1.1 - FX Dev (release candidate RC4)

**ESTADO: RELEASE CANDIDATE — NO ES UNA VERSIÓN ESTABLE.**

Release candidate RC4 sobre RC3 full (Bacon 1.1 - FX Dev). Aplica el plan
obligatorio P0-P8 de la iteración RC4: Help crítico corregido y navegable,
bypass unificado, retirada de ayudas permanentes, menús centrados, `UiDraw`
global, chopper modernizado y auditorías. **No modifica el motor FX, la
persistencia ni la compatibilidad de proyectos.**

## Cambios RC4 (P0-P7 completos; P8 en curso)

### P0 - Help bloqueo crítico corregido
- `HelpOverlay::ProcessButtonMask` cierra determinísticamente con `B` o
  `SELECT+R1` vía `EndModal(0)` y consume todos los eventos mientras está
  abierto (el RC3 ignoraba la entrada y nunca cerraba, bloqueando la app).

### P1 - Help contextual navegable y sobre modales
- `HelpRegistry::GetSectionCount`/`GetSectionAt`; el overlay navega secciones
  (`L/R`, `L1/R1`), hace scroll (`UP/DN`) y alterna el índice (`A`).
- `View::PushModal`/`RestoreSuspendedModal`: Help se abre sobre modales
  activos suspendiéndolos y restaurándolos al cerrar.

### P2 - Bypass unificado
- `UiDraw::DrawBypassRow` (`BYPASS` + `[ ON ]`/`[ OFF ]`) aplicado en
  DELAY/REVERB/EQ/COMP de las páginas MASTER.

### P3 - Ayudas permanentes retiradas
- `R+UP Song` sale de la fila de título del Mixer; migrado a `HelpRegistry`
  (sección MIXER). No quedan hints permanentes en Song/Chain/Phrase/Table/
  Groove/Instrument/Project.

### P4 - Primitivas de centrado (primer paso, no centrado real)
- `UiDraw::CenterTextX` (`(40-len)/2`) y `UiDraw::MakeCenteredMenuLayout`
  (bloque centrado vertical en banda 1..29). Aclaración: en RC4 **solo se
  introducen las primitivas y un primer consumidor** (`TreeFrogProjectExitModal`
  centra su título e items), **no** se centraron todavía los bloques de las
  páginas MASTER del Mixer ni el resto de menús. El centrado completo de
  bloques tipo menú, los layouts por viewport/banda y el centrado real de
  modales sobre las 30 filas se entregan en RC5.

### P5 - UiDraw global + colores de severidad
- `CD_WARNING`/`CD_ERROR` añadidos al enum y mapeados a colores reales en
  AppWindow (dentro de los 7 bits de la caché de pantalla).
- `UiDraw::DrawSelectionRegion`, `DrawStatusMessage`, `DrawErrorMessage` con
  consumidores reales (HelpOverlay índice, SampleManagerDialog con
  `setStatus`/`setStatusError`).

### P6 - Chopper y primitivas de navegación
- El marco ASCII del Graphical Chopper se sustituye por celdas sólidas
  CD_BORDER con la misma geometría (sin desalinear el overlay framebuffer).
- `DrawTabs` (prev/current/next) e indicador de scroll proporcional con
  consumidores en HelpOverlay.

### P7 - Auditorías
- Los 4 docs de auditoría (`UI_VISUAL_AUDIT`, `UI_CONTROL_AUDIT`,
  `OBSOLETE_FEATURE_AUDIT`, `UI_STYLE_GUIDE`) actualizados a RC4.
- Nuevo `tests/test_rc4_p7_audit.py` (`RC4_P7_AUDIT_OK`): sin `#if 0`, docs
  reflejan P3-P6, primitivas RC4 con consumidor, primitivas RC3 sin consumidor
  documentadas como KEEP_HIDDEN, colores de severidad mapeados.

## Pruebas

- Suite completa en verde: tests FX + RC2 + RC3 base + RC3 full + RC4 P7 +
  auditoría (`AUDIT_CLEAN_MAIN_U2523_OK`).
- Build `BUILD_U2523_OK`; instalado y verificado en SD (`VERIFY_U2523_OK`,
  `ERRORS=0`).

## Problemas conocidos

- La validación manual y las capturas de pantalla requieren hardware R36SX
  real (no disponible); el estado visual por vista se rastrea en
  `UI_VISUAL_AUDIT.md`.
- P8 (tests finales / golden / validación hardware) queda para cerrar la
  iteración cuando haya dispositivo.
