# Análisis de candidatos y decisión de recompilación

## Resultado del inventario

No se encontró el módulo histórico `u2_38au8_sync_uac2`. Los candidatos recuperados fueron compilados para `bcm47xx`, `ci20`, `sead3`, `malta` y `rt305x`.

### `64d86c51...` — bcm47xx

Aunque tiene vermagic compatible, conserva:

```text
FS_OUT_ASYNC=1
HS_OUT_ASYNC_1024=2
FS_IN_ASYNC=1
```

Sus secciones funcionales de código y datos coinciden con el módulo actual que produce Código 10. La diferencia principal es información de depuración y relocación.

### `86155d54...` — ci20

Tiene ABI aparente compatible, pero conserva la misma topología y descriptores asíncronos del código base. No corresponde a AU8-SYNC.

### `f08d9b4c...` — sead3

Tiene un tamaño y construcción diferentes, pero también conserva los endpoints asíncronos y las cadenas genéricas. No hay evidencia de que sea la variante histórica funcional.

### `f7dcb0c1...` — malta

Su vermagic incluye:

```text
SMP
modversions
```

Por tanto, no corresponde al ABI del kernel de la consola y no debe instalarse.

### `25e74ed1...` — rt305x

Es el módulo que se ha cargado en U2.40–U2.41.3 y que Windows rechaza con Código 10.

## Evidencia histórica

Windows registró previamente:

```text
VID_1209&PID_38E8
R36SX-U2-38AU8-SYNC
```

Los runtimes históricos seleccionaban una carpeta separada:

```text
u2_38au8_sync_uac2
```

Esto indica que la solución no era un `defconfig` genérico, sino una variante compilada expresamente para cambiar el modo de sincronización USB.

## Decisión

La reconstrucción se realiza sobre el árbol Linux 4.4.186 con `rt305x_defconfig`, porque:

- corresponde al SoC/kernel usado por la consola;
- produce el vermagic que ya ha cargado correctamente;
- permite modificar el código fuente, no solo bytes del ELF;
- conserva el manejador de reloj que históricamente funcionó;
- reproduce explícitamente los cuatro endpoints `SYNC`.

Esta es una hipótesis de ingeniería basada en la evidencia histórica. La confirmación definitiva depende de que Windows cree los endpoints de Reproducción y Grabación sin Código 10.
