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

## Módulos host-side: USB audio + MIDI (modo host)

Para el driver unificado (modos SP404MKII y USB-MIDI) la R36S actúa como host
USB. Esos backends necesitan los módulos de `sound/usb`:

```text
snd-usb-audio.ko
snd-usbmidi-lib.ko
```

Se compilan con:

```bash
bash scripts/02_COMPILAR_HOST_USB_AUDIO.sh
```

Requisitos (iguales que U2414):

```text
Kernel fuente: $PROJECT_ROOT/KERNEL/U2534_KERNEL_SOURCE_PATH.txt  -> linux-4.4.186
Toolchain:     $HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
```

El script:

1. copia la fuente a `$HOME/r36sx-kernel-hostaudio`;
2. configura `rt305x_defconfig` y fuerza (vía Python) los símbolos host:

```text
CONFIG_MODULE_UNLOAD=n        (clave: sin mod_unload en el vermagic)
CONFIG_MODVERSIONS=n
CONFIG_MODULES=y
CONFIG_SOUND=m  CONFIG_SND=m  CONFIG_SND_TIMER=m  CONFIG_SND_PCM=m
CONFIG_SND_HWDEP=m  CONFIG_SND_RAWMIDI=m  CONFIG_SND_SEQUENCER=m
CONFIG_SND_RAWMIDI_SEQ=m
CONFIG_SND_USB=y  CONFIG_SND_USB_AUDIO=m
CONFIG_USB=y  CONFIG_USB_SUPPORT=y  CONFIG_USB_COMMON=y
CONFIG_USB_MUSB_HDRC=y  CONFIG_USB_MUSB_DUAL_ROLE=y
```

3. compila `M=sound/usb modules` con `HOSTCFLAGS=-fcommon` (GCC host 13);
4. valida ELF 32-bit MIPS relocatable y vermagic `4.4.186-release preempt MIPS32_R2 32BIT`;
5. copia el resultado a `$PROJECT_ROOT/BUILD/HOST_USB_AUDIO/`.

Salida esperada:

```text
CONFIG_HOST_AUDIO_ABI_VERIFY_OK
BUILD_HOST_USB_AUDIO_OK
MODULES=$PROJECT_ROOT/BUILD/HOST_USB_AUDIO/snd-usb-audio.ko $PROJECT_ROOT/BUILD/HOST_USB_AUDIO/snd-usbmidi-lib.ko
```

La copia funcional se conserva en:

```text
../recovery/host_usb_audio/snd-usb-audio.ko
../recovery/host_usb_audio/snd-usbmidi-lib.ko
```

`scripts/install.sh` instala estos módulos en la SD en:

```text
F:\lgpt\otg\modules\4.4.186-release\host_usb_audio\
```

y `scripts/verify.sh` comprueba que coinciden por SHA256 con el build.

