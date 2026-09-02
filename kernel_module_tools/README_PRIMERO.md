# LGPT R36SX U2.41.4 — RÉPLICA AU8-SYNC

Esta entrega abandona los parches binarios sobre el módulo asociado al Código 10 y reconstruye desde fuente la variante histórica `AU8-SYNC`.

No modifica:

- el core LGPT;
- el selector `SELECT + R2`;
- el sampler;
- Chopper;
- proyectos o samples;
- el daemon ya compilado.

Solo compila e instala un nuevo `usb_f_uac2.ko` y reemplaza el runtime del gadget USB.

## Por qué se recompila

El inventario disponible no contiene:

```text
u2_38au8_sync_uac2/usb_f_uac2.ko
```

Los módulos genéricos encontrados proceden de otros `defconfig`. El módulo actual de `rt305x_defconfig`, hash `25e74e...`, es el que Windows rechaza con Código 10.

La evidencia histórica conserva esta identidad:

```text
USB\VID_1209&PID_38E8\R36SX-U2-38AU8-SYNC
```

Por tanto, U2.41.4 reproduce:

```text
VID:       1209
PID:       38E8
Serial:    R36SX-U2-38AU8-SYNC
Producto:  R36SX USB AUDIO
```

## Diferencia de fuente

Linux 4.4.186 define cuatro endpoints isócronos como:

```c
USB_ENDPOINT_SYNC_ASYNC
```

La réplica AU8-SYNC los compila como:

```c
USB_ENDPOINT_SYNC_SYNC
```

Se aplica a:

- Full-Speed OUT;
- High-Speed OUT;
- Full-Speed IN;
- High-Speed IN.

No se altera el código que responde `GET CUR` o `GET RANGE`, la topología de reloj, los tamaños de paquete ni el motor ALSA.

## ABI objetivo

```text
Kernel:          4.4.186-release
Arquitectura:    MIPS32R2 little-endian
Preemption:      PREEMPT
SMP:             desactivado
MODVERSIONS:     desactivado
Vermagic:        4.4.186-release preempt MIPS32_R2 32BIT
```

## Ejecución completa

Con la SD montada como `F:`:

```bash
cd "/mnt/d/Toolchains/R36SX/LGPT_R36SX_U2414_REPLICAR_AU8_SYNC_UAC2_20260716"

bash scripts/04_BUILD_INSTALL_U2414_COMPLETO.sh
```

El proceso:

1. localiza o descarga Linux 4.4.186;
2. copia la fuente a `$HOME`;
3. aplica el parche AU8-SYNC;
4. configura `rt305x_defconfig`;
5. compila solamente `drivers/usb/gadget/function`;
6. valida ELF, vermagic, marcador y descriptores;
7. respalda U2.41.3;
8. instala el módulo en la ruta histórica;
9. instala el runtime;
10. verifica la SD.

## Salida esperada

```text
KERNEL_SOURCE_READY=...
PATCH_U2414_AU8_SYNC_SOURCE_OK
BUILD_U2414_AU8_SYNC_OK
INSTALL_U2414_AU8_SYNC_OK
VERIFY_U2414_AU8_SYNC_SD_OK
U2414_BUILD_INSTALL_COMPLETE
```

## Restauración

```bash
bash scripts/06_RESTAURAR_U2413_RUNTIME.sh
```
