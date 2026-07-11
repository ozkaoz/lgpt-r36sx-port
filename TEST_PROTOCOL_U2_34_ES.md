# Protocolo de prueba U2.34

## 1. Regresión previa

Validar primero que U2.33 no se rompió:

```text
1. LGPT arranca.
2. Carga proyecto.
3. Chopper muestra Graphical Chopper U2.34.
4. Pitch/Env muestra PITCH/ENV U2.34.
5. Pitch/Env: B preview, L2+B stop, A apply.
6. Listen/Import: A sobre Listen preescucha, L2+B detiene.
7. Import sigue funcionando.
```

## 2. Abrir Sample Manager

```text
Instrument -> sample -> A
Seleccionar Manage
A
```

Debe aparecer:

```text
PROJECT SAMPLE MANAGER
```

## 3. Ver lista de samples

La lista debe mostrar nombres y etiquetas:

```text
--   libre
I1   usado por instrumento
CH   con chops guardados
```

## 4. Borrado individual

Sobre un sample libre `--`:

```text
A
```

Resultado esperado: el sample desaparece de la lista y el archivo WAV se elimina de la carpeta del proyecto.

Sobre un sample usado `I1/I2`:

```text
A
```

Resultado esperado: no debe borrarse. Debe mostrar estado tipo `Blocked: Assigned x1`.

Sobre un sample con chops `CH`:

```text
A
```

Resultado esperado: no debe borrarse. Debe mostrar `Blocked: Has chops`.

## 5. Purge

```text
Y
```

Resultado esperado: elimina todos los samples libres `--`, conserva samples asignados y conserva samples `CH`.

## 6. Regresión después de purge

```text
1. Volver a Instrument.
2. Verificar que los instrumentos asignados siguen apuntando a sus samples.
3. Entrar a Chopper con un sample con chops.
4. Verificar que los chops siguen visibles.
5. Entrar a Phrase y verificar S01/S02/S03.
6. Guardar, cerrar, recargar y confirmar persistencia .u2chop.
```
