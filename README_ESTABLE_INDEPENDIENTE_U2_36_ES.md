# LGPT R36S TreeFrogUI - U2.36 estable independiente

Estado: **validado en hardware R36S**.

Esta entrega consolida el port de LittleGPTracker para R36S/TreeFrogUI con CHOP, CROP SAMPLE, PITCH/ENVELOPE, gestor de samples del proyecto y restauración del flujo estable de `Listen / Import`.

## Qué contiene este ZIP

- Árbol fuente completo U2.36 listo para compilar en WSL Ubuntu 24.
- Scripts de build TreeFrog/R36S ya incluidos en la raíz del proyecto.
- Documentación de instalación, comandos de build/copia a SD y protocolo de prueba.
- Patch U2.36 y artefactos auxiliares en `release_assets/`.
- Prompt específico para continuar el desarrollo desde esta base estable.

No incluye `lgpt_libretro.so` compilado. Debe compilarse localmente con el toolchain SF3000/TreeFrog usado durante el desarrollo.

## Marcadores esperados en runtime

En Chopper debe verse:

```text
Graphical Chopper U2.36
```

En Pitch/Envelope debe verse:

```text
PITCH/ENV U2.36
```

## Funcionalidad validada

### Song

- `A+B`: limpia celda sin compactar.
- `Y+X`: corta/compacta y deja limpia la cola; corrección U2.23 validada.

### Chopper

- Entrada al Chopper desde Instrument.
- `A`: crea chop.
- `B`: preescucha chop/sample según contexto.
- `Y`: borra chop seleccionado.
- `R2+LEFT/RIGHT`: cambia chop seleccionado.
- Persistencia `.u2chop` validada al guardar/cerrar/recargar.
- Phrase muestra `S01`, `S02`, etc. para instrumentos con chops.

### Crop Sample

- `SELECT`: entra a CROP SAMPLE.
- `R1+A`: aplica keep/crop.
- `L2+Y`: borra selección.
- `R1+X`: undo/redo en Crop.
- Pantallas de progreso centradas y visibles durante la operación.

### Pitch / Envelope

- `L1+R1`: entra/sale de Pitch/Envelope.
- `UP/DOWN`: selecciona parámetro.
- `LEFT/RIGHT`: cambia valor.
- `B`: preview real de modificaciones.
- `L2+B`: detiene preview.
- `A`: aplica destructivamente.
- `L1+X`: undo/redo.
- `Scope: Sample`: aplica al sample completo.
- `Scope: Chop`: aplica al chop seleccionado.
- En `Scope: Chop`, `R2+LEFT/RIGHT` cambia el chop objetivo.
- Parámetros: `Pitch`, `Attack`, `Sustain`, `Release`, `Scope`, `Sample`.

### Listen / Import

Desde `Instrument -> sample -> A`:

```text
Listen    Import    Manage    Exit
```

- `A` sobre `Listen`: preescucha sin mensaje visual.
- `L2+B`: detiene preescucha.
- `A` sobre `Import`: importa sample.
- Si el mismo WAV ya existe en el proyecto, no se duplica.
- Si otro WAV tiene el mismo nombre pero contenido diferente, se protege contra sobrescritura con nombre único.

### Project Sample Manager

Desde `Instrument -> sample -> A -> Manage`:

```text
PROJECT SAMPLE MANAGER
```

- `UP/DOWN`: mover selección.
- `A`: borra sample libre `--`.
- `Y`: purge conservador de samples no usados en Phrase/Song.
- `X`: borrado forzado en dos pasos para samples asignados y/o con chops.
- `B` / `SELECT`: salir.
- `L2+B`: detiene preview activa.

Etiquetas:

```text
--   libre / no usado en Phrase/Song
I1   usado por 1 referencia real
I2   usado por 2 referencias reales
CH   tiene chops, no usado
C1   tiene chops y 1 uso real
C2   tiene chops y 2 usos reales
```

## Pendientes intencionales para el siguiente desarrollo

No avanzar sobre estos puntos sin crear una nueva rama incremental desde U2.36:

1. Chops renderizados/exportables a carpeta del proyecto.
2. Sobrescritura controlada de chops renderizados al guardar/modificar.
3. Integración de Phrase para usar solo chops cuando el instrumento seleccionado esté chopeado.
4. Posible uso del campo `slices` como acceso directo al Chopper.
5. Estrategia de exportación/portabilidad del proyecto con sus chops renderizados.

## Regla de conservación

No modificar la ruta de audio validada de `Listen`, `Pitch/Env preview`, `CHOP`, `.u2chop`, ni `Sample Manager` salvo que el cambio nuevo tenga prueba de regresión explícita en hardware.
