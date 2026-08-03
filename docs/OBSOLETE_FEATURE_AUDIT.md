# OBSOLETE_FEATURE_AUDIT — Auditoría de funciones obsoletas de la UI LGPT R36SX

Versión: RC4 (P0-P6 completos; P7 auditorías).
Objetivo: listar funciones obsoletas/sin uso con su file:line y decisión.
Decisiones: REMOVE / KEEP_HIDDEN / KEEP_COMPAT.

Regla transversal: las características de compatibilidad necesarias para
cargar y reproducir proyectos antiguos (legacy, migraciones, FourCC internos,
sends heredados) se conservan aunque no se muestren en la UI.

## Estado actual

| Función | file:line | Decisión | Motivo |
|---|---|---|---|
| `drawFxParamRow` (genérico) | MixerView.cpp | KEEP_COMPAT | Aún lo usan páginas residuales del bucle genérico |
| `drawEqPage`/`drawCompPage` | MixerView.cpp | KEEP (nuevo) | Menús dedicados EQ/COMP |
| `drawMasterFxRow` | MixerView.cpp | KEEP (nuevo) | Filas DELAY/REVERB MASTER |
| `RVB MIX` persistido | FxEngine/Persistencia | KEEP_HIDDEN | Lectura/escritura inerte por compatibilidad (RC2) |
| `FBMX`/`FBTN` (FourCC legacy) | CommandList | KEEP_HIDDEN | Se cargan proyectos antiguos (muestran CFM/CFT) |
| Frame sólido del Chopper | SampleChopperModal.cpp drawFrame | KEEP (modernizado, RC4 P6) | Antes era un borde ASCII `+----+`; ahora son celdas sólidas CD_BORDER con la misma geometría (filas 1..22, columnas 0/39), sin ASCII de caja |
| Overlay de progreso del Chopper | SampleChopperModal.cpp drawOperationOverlay | KEEP | Marco invertido que bloquea visualmente la onda durante el procesamiento; no usa DrawModalFrame a propósito (punto 24) |
| Panel Pitch/Env del Chopper | SampleChopperModal.cpp drawPitchScreen | KEEP | Panel invertido que reemplaza la banda de onda en pitch mode; bloqueo visual intencional sobre el framebuffer (punto 24/17) |
| Hint del TextEditor | TreeFrogTextEditor.cpp:99 | KEEP | Ayuda interna de la ventana de edición de texto (modal no soporta HelpOverlay: requiere `!HasModal`) |
| Hint del UsbRecordModal | UsbRecordModal.cpp:1440 | KEEP | Ayuda interna del modal USB record (mismo motivo) |

## Obsoletas retiradas en RC4

| Función | file:line | Decisión | Motivo |
|---|---|---|---|
| Hint permanente `R+UP Song` (fila de título del Mixer) | MixerView.cpp DrawView | REMOVED (RC4 P3) | Ayuda permanente retirada; migrada a HelpRegistry sección MIXER y mostrada vía HelpOverlay |
| Marco ASCII `+----+` del Chopper | SampleChopperModal.cpp drawFrame | REMOVED (RC4 P6) | Sustituido por celdas sólidas CD_BORDER; la geometría de la banda se conserva intacta |

## Hallazgos de auditoría estática (RC4 P7)

Escaneo `UiDraw::*` en todo el árbol (fuera de `BaseClasses/UiDraw.*`):

| Primitiva | Consumidores | Decisión |
|---|---|---|
| `DrawCenteredTitle` | 1 (MixerView) | KEEP |
| `DrawCenteredTitleAt` | 1 (MixerView) | KEEP |
| `DrawSectionHeader` | 7 (InstrumentView) | KEEP |
| `DrawBypassRow` | 4 (MixerView DELAY/REVERB/EQ/COMP) | KEEP |
| `DrawTabs` | 1 (HelpOverlay, RC4 P6) | KEEP |
| `DrawScrollIndicator` | 1 (HelpOverlay, RC4 P6) | KEEP |
| `DrawSelectionRegion` | 1 (HelpOverlay, RC4 P5) | KEEP |
| `DrawStatusMessage` | 1 (SampleManagerDialog, RC4 P5) | KEEP |
| `DrawErrorMessage` | 1 (SampleManagerDialog, RC4 P5) | KEEP |
| `CenterTextX` | 2 (ProjectView, RC4 P4) | KEEP |
| `MakeCenteredMenuLayout` | 1 (ProjectView, RC4 P4) | KEEP |
| `DrawValueRow` | 0 | KEEP_HIDDEN — primitiva RC3 disponible; sin vista adoptante aún (P8: migrar drawMasterFxRow u otra fila label/valor) |
| `DrawSolidBar` | 0 | KEEP_HIDDEN — primitiva RC3 disponible; los secuenciadores dibujan barras con loops propios (ChainView:676, GrooveView:180, PhraseView:1608) |
| `DrawBipolarBar` | 0 | KEEP_HIDDEN — primitiva RC3 disponible, sin adoptante |
| `DrawToggle` | 0 | KEEP_HIDDEN — superada por `DrawBypassRow` (que lo envuelve); sin uso directo |
| `DrawProgressBar` | 0 | KEEP_HIDDEN — primitiva RC3 disponible; el chopper usa su propio overlay de progreso framebuffer |
| `DrawModalFrame` | 0 | KEEP_HIDDEN — primitiva RC3 disponible; los modales usan `ModalView::SetWindow` centrado |
| `DrawSeparator` | 0 | KEEP_HIDDEN — primitiva RC3 disponible, sin adoptante |

Los tests RC3/RC4 exigen la existencia de todas estas primitivas en el header;
no se eliminan, pero quedan registradas como disponibles sin consumidor real
pendientes de adopción en P8.

## Pendientes (fase completa/RC4 P7-P8)

- Escaneo de funciones sin referencias (`git grep` por símbolo) en todas las
  vistas: hecho en RC4 P7 (tabla de hallazgos arriba).
- Revisión de `#if 0`, `#ifdef` de diagnóstico y ramas muertas: sin bloques
  `#if 0` en las vistas (escaneo RC4 P7).
- Decisión final por cada función: primitivas sin consumidor quedan
  KEEP_HIDDEN hasta adoptarlas en P8 (o retirarlas con test actualizado si el
  plan lo pide).