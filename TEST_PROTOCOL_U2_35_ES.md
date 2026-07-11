# Protocolo de prueba U2.35

1. Abrir LGPT, cargar proyecto y entrar a Chopper.
2. Confirmar cabecera: `Graphical Chopper U2.35`.
3. Entrar a `L1+R1` y confirmar: `PITCH/ENV U2.35`.
4. Verificar regresión rápida de Pitch/Env: `B` preview, `L2+B` stop, `A` apply.
5. Volver a Instrument, campo sample, pulsar `A`.
6. Confirmar que `Listen / Import / Manage / Exit` se ve separado en dos filas y cada opción sigue seleccionable.
7. Importar dos veces el mismo WAV. Resultado esperado: el segundo import genera un nombre único tipo `_01`, no sobrescribe el primero.
8. Entrar a `Manage`.
9. Borrar con `A` un sample `--`. Debe borrarse.
10. Pulsar `A` sobre `I1`, `CH` o `C1`. Debe bloquearse.
11. Pulsar `X` sobre un sample con chops/asignación. Debe pedir confirmación.
12. Pulsar `X` otra vez sobre el mismo sample. Debe borrar, desasignar instrumentos y eliminar el sidecar de chops.
13. Pulsar `Y` purge. Debe eliminar solo libres y conservar asignados/chops.
14. Validar que Listen con `A`, Import, Chopper, Phrase y persistencia `.u2chop` siguen funcionando.
