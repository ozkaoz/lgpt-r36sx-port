# Protocolo de prueba U2.36

1. Arrancar LGPT y cargar proyecto.
2. Entrar a Chopper. Debe verse `Graphical Chopper U2.36`.
3. Entrar a `L1+R1`. Debe verse `PITCH/ENV U2.36`.
4. Verificar regresión rápida de Pitch/Env: `B` preview, `L2+B` stop, `A` apply.
5. Ir a Instrument, campo sample, pulsar `A`.
6. Verificar que `Listen Import Manage Exit` aparece en una sola línea con separación legible.
7. Seleccionar `Listen`, pulsar `A`: debe sonar sample sin mensaje visual.
8. Pulsar `L2+B`: debe detener.
9. Seleccionar `Import`, pulsar `A` sobre un WAV no existente en proyecto: debe importar.
10. Volver a importar exactamente el mismo WAV: no debe crear copia duplicada ni `kick_01.wav`.
11. Importar otro WAV con mismo nombre pero contenido diferente: debe proteger contra sobrescritura con nombre único.
12. Entrar a `Manage`.
13. Un sample recién importado pero no usado en Phrase/Song debe aparecer como `--` aunque esté asignado a un instrumento.
14. `A` sobre `--` debe eliminarlo correctamente.
15. `Y` purge debe eliminar solo libres/no usados, conservando chops y samples usados en Phrase/Song.
16. `X` force delete debe seguir pidiendo confirmación para samples con chops o uso real.
17. Regresión: CHOP muestra chops, Phrase muestra `S01/S02`, guardar/cerrar/recargar conserva `.u2chop`.
