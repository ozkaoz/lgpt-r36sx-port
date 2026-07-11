# U2.36 - Import dedup + layout Listen + uso real de samples

Base esperada: U2.35 validado en hardware.

Cambios:

- `Graphical Chopper U2.36`.
- `PITCH/ENV U2.36`.
- El menú `Listen / Import / Manage / Exit` vuelve a una sola línea, con separación fija para no quedar pegado.
- Importación deduplicada por contenido: si el WAV ya existe en el proyecto, aunque venga con otro nombre, `Import` reutiliza el sample existente y no crea `kick_01.wav`, `kick_02.wav`, etc.
- Si el archivo tiene el mismo nombre pero contenido diferente, mantiene protección anti-sobrescritura y usa nombre único.
- Sample Manager ahora considera `--` según uso real en secuencia: un sample asignado a un instrumento pero no usado en Phrase/Song aparece como libre y puede purgarse.
- Al borrar/purgar un sample libre que todavía estaba asignado a un instrumento sin uso, limpia esa asignación antes de compactar el SamplePool.

Sin cambios intencionales:

- CHOP normal.
- PITCH/ENV preview/apply/scope.
- Listen con A y stop con L2+B.
- Import funcional.
- B no preescucha directamente en Instrument.
- Force delete con X en Sample Manager.
