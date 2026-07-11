# Protocolo final U2.36 estable

## Identificación

1. Arrancar LGPT.
2. Entrar a Chopper.
3. Debe verse `Graphical Chopper U2.36`.
4. Entrar a `L1+R1`.
5. Debe verse `PITCH/ENV U2.36`.

## Song

1. En Song llenar varias filas con chains.
2. `A+B` sobre una celda: debe limpiar solo esa celda.
3. `Y+X` sobre una celda intermedia: debe compactar y dejar la última posición en `--`.

## CHOP

1. Entrar a Chopper.
2. `A`: crear chop.
3. `B`: preescuchar chop/sample.
4. `Y`: borrar chop.
5. `R2+LEFT/RIGHT`: cambiar chop seleccionado.
6. Guardar, cerrar y recargar: los chops deben persistir vía `.u2chop`.

## Phrase

1. Con instrumento chopeado, Phrase debe mostrar `S01`, `S02`, etc.
2. Con instrumento no chopeado, Phrase debe conservar notas normales.

## Crop Sample

1. `SELECT`: entrar a CROP.
2. `R1+A`: keep/crop.
3. `L2+Y`: borrar selección.
4. `R1+X`: undo/redo.
5. El overlay de progreso debe aparecer centrado y durante la operación.

## Pitch / Envelope

1. `L1+R1`: entrar.
2. `UP/DOWN`: cambiar parámetro.
3. `LEFT/RIGHT`: cambiar valor.
4. `B`: preview real.
5. `L2+B`: detener preview.
6. `A`: aplicar.
7. `L1+X`: undo/redo.
8. `Scope: Sample`: aplica al sample completo.
9. `Scope: Chop`: aplica al chop seleccionado.
10. En `Scope: Chop`, `R2+LEFT/RIGHT` cambia chop objetivo.
11. `L1+R1`: salir.

## Listen / Import / Manage

Desde Instrument, campo sample, pulsar `A`.

Debe verse una línea legible:

```text
Listen    Import    Manage    Exit
```

Pruebas:

1. `A` sobre `Listen`: debe sonar sample sin mensaje visual.
2. `L2+B`: debe detener.
3. `A` sobre `Import`: debe importar sample.
4. Reimportar exactamente el mismo WAV: no debe duplicar.
5. Importar WAV con mismo nombre y contenido distinto: debe proteger contra sobrescritura con nombre único.

## Project Sample Manager

1. Entrar a `Manage`.
2. Debe abrir `PROJECT SAMPLE MANAGER`.
3. Sample no usado en Phrase/Song debe verse `--`.
4. `A` sobre `--`: debe borrar.
5. `A` sobre usado/con chops: debe bloquear.
6. `X` sobre usado/con chops: debe pedir confirmación.
7. Segundo `X`: debe forzar borrado, borrar `.u2chop` si existe y desasignar instrumentos.
8. `Y`: purge conservador; elimina no usados, conserva chops y usados.
9. `B` o `SELECT`: salir.

## Regresión final

1. Listen con `A` suena.
2. `L2+B` detiene.
3. Import funciona.
4. Chopper sigue mostrando chops.
5. Phrase sigue mostrando `S01/S02/etc.`.
6. Guardar, cerrar y recargar conserva `.u2chop`.
7. El proyecto no debe perder samples usados ni chops protegidos durante purge conservador.
