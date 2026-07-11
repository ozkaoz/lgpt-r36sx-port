# U2.34 - Sample Manager / Purge

Base esperada: U2.33 validado en hardware.

## Objetivo

Agregar una opción de gestión de samples del proyecto antes de consolidar la rama estable:

- Ver los samples cargados en el proyecto.
- Eliminar individualmente samples no asignados.
- Ejecutar purge de samples no asignados.
- Conservar samples con chops guardados.

## Acceso

Instrument -> campo `sample` -> `A` para abrir `Listen / Import`.

El menú ahora muestra:

```text
Listen   Import   Manage   Exit
```

Seleccionar `Manage` y pulsar `A` abre:

```text
PROJECT SAMPLE MANAGER
```

## Controles del Sample Manager

```text
UP/DOWN      mover selección
A            eliminar sample seleccionado si está libre
Y            purge de todos los samples libres
B            salir
SELECT       salir
L2+B         detener preview activa, si hubiera alguna
```

## Etiquetas

```text
--   sample no asignado; puede eliminarse
I1   sample asignado a 1 instrumento
I2   sample asignado a 2 instrumentos
CH   sample con chops; protegido por purge
```

## Política de seguridad

U2.34 no borra samples asignados a instrumentos. Tampoco borra samples con chops guardados, incluso si no están asignados.

Esto evita romper `SampleVariable`, Phrase y la persistencia `.u2chop`.

## No incluido todavía

No implementa exportación/renderizado de chops como WAVs independientes. Eso queda para la siguiente fase, después de consolidar el ZIP estable.
