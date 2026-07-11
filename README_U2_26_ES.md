# U2.26 candidato: Chopper UI progress + Pitch/Envelope preview fix

Objetivo: corregir los defectos reportados en U2.25 antes de avanzar a chops renderizados/exportables.

Cambios:

- Chopper identifica `Graphical Chopper U2.26` y el panel muestra `PITCH / ENVELOPE U2.26`.
- `showOperationProgress()` fuerza un frame de video libretro con `TreeFrogForceVideoRefresh()` después de `w_.Flush()`. Esto busca que los porcentajes intermedios de CROP/PITCH se vean durante la operación, no solo al volver al loop principal.
- Preview Pitch/Envelope deja de usar el escritor RIFF local y usa `WavFile::ReplaceBuffer()` + `SaveBufferToPath()`, igual que los cambios destructivos que sí se escuchan tras aplicar.
- Preview modificada usa `StartStreamingRangeAt(path, 0, frames - 1)`.
- El panel Pitch/Envelope fue compactado para no superponerse con los controles inferiores.
- Se agregó parámetro `Sample` dentro de Pitch/Envelope: con `UP/DOWN` se selecciona, con `LEFT/RIGHT` cambia el sample objetivo conservando valores Pitch/Attack/Sustain/Release/Scope.
- `SELECT` sigue sin salir del menú Pitch/Envelope.
- `L1+X` mantiene undo/redo global de edición destructiva.

No incluido: persistencia/exportación de chops renderizados a carpeta del proyecto. Eso queda para U2.27/U2.26b después de validar este fix.
