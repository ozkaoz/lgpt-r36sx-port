# Prompt para continuar desde U2.22

Continuar el desarrollo sobre U2.22 — Pitch Screen + Clean Operation UI + Stable Candidate.

Estado validado antes de U2.22: Chopper base, live cuts, Sxx en Phrase, persistencia por sidecar, SELECT para CROP SAMPLE, keep/delete selection, undo/redo.

Cambios U2.22 a validar: `L1+R1` abre pantalla `PITCH SAMPLE`; flechas ajustan semitonos manualmente de -12 a +12; `B` preescucha; `A` aplica físicamente; overlay de operación sin barra verde, texto centrado con `OK` y `Press A to continue`.

Metodología: hacer cambios incrementales, entregar script aplicable en WSL Ubuntu 24, compilar con `BUILD_RC=0`, copiar a SD con hash local/SD, probar en consola, reportar regresiones antes de avanzar.
