# Prompt para continuar desde PORT Versión estable con CHOP integrado

Continuar el desarrollo sobre el ZIP `PORT Versión estable con CHOP integrado`, basado en U2.22 validado en consola.

Estado estable confirmado:

- Chopper entra con `R1 + A` desde Instrument.
- `R2 + A` reproduce sample completo con playhead rojo real.
- `A` corta en vivo durante reproducción.
- Chops hasta 100, persistentes por sidecar `sample.wav.u2chop`.
- Phrase usa `S01..Sxx` solo para instrumentos con chops; instrumentos sin chops conservan `C-3/C-4`.
- `SELECT` entra a `CROP SAMPLE`.
- `R1 + A` conserva físicamente la selección.
- `L2 + Y` descarta físicamente la selección.
- `R1 + X` undo/redo.
- Overlay de operación limpio, centrado, con `OK` y `Press A to continue`.
- `L1 + R1` entra a `PITCH SAMPLE`; flechas ajustan `-12..+12`; `B` preescucha; `A` aplica.

Metodología obligatoria:

1. Revisar el árbol antes de parchear.
2. Hacer cambios incrementales.
3. Generar script aplicable en WSL Ubuntu 24.
4. Incluir backup automático.
5. Compilar y exigir `BUILD_RC=0`.
6. Copiar a SD con verificación SHA256 local/SD.
7. Entregar protocolo de prueba y esperar reporte antes del siguiente cambio.
8. No avanzar sobre suposiciones si una prueba falla.

Próximas áreas sugeridas, solo si el usuario las solicita:

- Mejorar navegación de la lista Sxx en Phrase.
- Persistencia dentro del proyecto en vez de sidecar, si se decide romper/expandir formato.
- Mejorar visualización de chops largos/zoom.
- Audición contextual desde Phrase.
- Menú de administración de sidecars `*.u2chop`.
