# OBSOLETE_FEATURE_AUDIT — Auditoría de funciones obsoletas de la UI LGPT R36SX

Versión: RC3 base (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, punto 27).
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
| Marco ASCII `+----+` del Chopper | SampleChopperModal.cpp drawFrame | KEEP | Borde estructural que delimita la banda gráfica del overlay framebuffer (punto 18); coincidente con la geometría 320×240, excepción documentada del punto 17 |
| Overlay de progreso del Chopper | SampleChopperModal.cpp drawOperationOverlay | KEEP | Marco invertido que bloquea visualmente la onda durante el procesamiento; no usa DrawModalFrame a propósito (punto 24) |
| Panel Pitch/Env del Chopper | SampleChopperModal.cpp drawPitchScreen | KEEP | Panel invertido que reemplaza la banda de onda en pitch mode; bloqueo visual intencional sobre el framebuffer (punto 24/17) |
| Hint del TextEditor | TreeFrogTextEditor.cpp:99 | KEEP | Ayuda interna de la ventana de edición de texto (modal no soporta HelpOverlay: requiere `!HasModal`) |
| Hint del UsbRecordModal | UsbRecordModal.cpp:1440 | KEEP | Ayuda interna del modal USB record (mismo motivo) |

## Pendientes (fase completa)

- Escaneo de funciones sin referencias (`git grep` por símbolo) en todas las
  vistas.
- Revisión de `#if 0`, `#ifdef` de diagnóstico y ramas muertas.
- Decisión final por cada función y actualización del plan.