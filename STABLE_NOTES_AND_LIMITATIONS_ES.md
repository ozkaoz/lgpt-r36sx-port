# Notas y limitaciones estables

- La persistencia de chops usa sidecar `sample.wav.u2chop`, no el formato interno del proyecto. Esto evita romper compatibilidad del proyecto LGPT.
- El sidecar debe viajar junto al WAV. Si se copia un proyecto a otra SD o carpeta, copiar también los `.u2chop`.
- Crop/delete/pitch físico modifican el WAV. Usar copias de samples importantes.
- Undo/redo físico está pensado para la sesión actual, no como historial permanente.
- `PITCH SAMPLE` cambia pitch y duración mediante resampling. Para pitch por patrón sin modificar WAV, usar comando `PTCH` en Phrase.
- El core compilado no está incluido en este ZIP fuente.
