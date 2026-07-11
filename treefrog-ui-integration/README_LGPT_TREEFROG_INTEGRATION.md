# Integración LGPT con TreeFrogUI

## Estado actual

La fuente actual construye LGPT como core libretro:

```text
lgpt_libretro.so
```

Destino propuesto en la SD:

```text
cubegm/cores/lgpt_libretro.so
roms/lgpt/
```

## Trabajo pendiente

Confirmar cómo TreeFrogUI registra la relación entre carpeta y core en R36SX v2.6:

```text
LGPT -> cubegm/cores/lgpt_libretro.so
```

El ZIP revisado de TreeFrogUI no incluía el contenido del submódulo `frogui`, por lo que no se puede dejar todavía un parche exacto al código de FrogUI desde esta revisión.

## Alternativas técnicas

1. Mantener LGPT como libretro core y registrar la asociación en TreeFrogUI.
2. Crear una aplicación standalone en `cubegm/lgpt`, pero eso requeriría cambiar el port porque el código actual está orientado a libretro.

La opción 1 es la recomendada para esta rama.
