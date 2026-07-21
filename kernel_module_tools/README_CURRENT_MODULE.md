# Módulo UAC2 AU8-SYNC

Esta carpeta conserva los scripts y el parcheador usados para reconstruir el
módulo funcional `usb_f_uac2.ko` sobre Linux 4.4.186.

Fuente de kernel esperada:

```text
/tmp/r36sx_kernel_nospace_u241/linux-4.4.186
```

Toolchain:

```text
/home/dafunknoise/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
```

Configuración crítica:

```text
CONFIG_USB_CONFIGFS_F_UAC2=y
CONFIG_USB_F_UAC2=m
CONFIG_MODULE_UNLOAD=n
CONFIG_MODULE_FORCE_UNLOAD=n
CONFIG_PREEMPT=y
CONFIG_DEBUG_PREEMPT=n
```

Vermagic:

```text
4.4.186-release preempt MIPS32_R2 32BIT
```

La copia funcional también se encuentra en:

```text
../recovery/u2_38au8_sync_uac2/usb_f_uac2.ko
```
