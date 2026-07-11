# U2.35 - Sample Manager: importación sin sobrescritura y borrado forzado controlado

Base esperada: U2.34 validado en hardware.

Cambios:

- `Listen / Import / Manage / Exit` se redistribuye en dos filas para evitar que se vea como `ListeImporManageExit`.
- Importar el mismo WAV varias veces ya no sobrescribe el archivo existente del proyecto. Si `kick.wav` ya existe, se crea `kick_01.wav`, luego `kick_02.wav`, etc.
- `PROJECT SAMPLE MANAGER` conserva el borrado normal con `A` solo para samples libres.
- Se agrega `X` como borrado forzado controlado en dos pasos para samples asignados y/o con chops.
- El purge con `Y` sigue siendo conservador: elimina solo libres; conserva asignados y conserva chops.

Etiquetas del manager:

- `--`: libre; `A` puede borrar.
- `I1`, `I2`, etc.: usado por instrumentos.
- `CH`: tiene chops, sin asignación.
- `C1`, `C2`, etc.: tiene chops y además está asignado por instrumentos.

Borrado forzado:

- Primer `X`: arma confirmación y muestra `X again: ...`.
- Segundo `X` sobre el mismo sample: desasigna instrumentos, borra sidecar `.u2chop`, borra el WAV y notifica al Chopper para ajustar índices.

No se cambia aún la fase de chops renderizados/exportables.
