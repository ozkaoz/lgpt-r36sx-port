# Validación U2.41.4

## Parcheador de fuente

El parcheador exige:

- archivo `f_uac2.c` de Linux 4.4;
- símbolos de reloj `USB_OUT_CLK_ID` y `USB_IN_CLK_ID`;
- estructura `cntrl_range_lay3`;
- exactamente cuatro definiciones `USB_ENDPOINT_SYNC_ASYNC`;
- ninguna definición `USB_ENDPOINT_SYNC_SYNC` previa;
- una ocurrencia de cada cadena genérica.

Después comprueba:

- cuatro endpoints `USB_ENDPOINT_SYNC_SYNC`;
- cero endpoints `USB_ENDPOINT_SYNC_ASYNC`;
- marcador de módulo `R36SX_U2414_AU8_SYNC_REPLICA`;
- nombre `R36SX USB AUDIO`;
- reloj, tamaños de paquetes y motor ALSA sin modificación intencional.

## Configuración del kernel

La compilación fuerza y valida:

```text
CONFIG_LOCALVERSION="-release"
CONFIG_LOCALVERSION_AUTO=n
CONFIG_SMP=n
CONFIG_PREEMPT=y
CONFIG_MODVERSIONS=n
CONFIG_MODULES=y
CONFIG_SOUND=m
CONFIG_SND=m
CONFIG_SND_PCM=m
CONFIG_USB_GADGET=y
CONFIG_USB_LIBCOMPOSITE=m
CONFIG_USB_CONFIGFS=m
CONFIG_USB_CONFIGFS_F_UAC2=m
```

## Módulo

La compilación solo se acepta cuando:

```text
ELF 32-bit
MIPS
relocatable
vermagic 4.4.186-release preempt MIPS32_R2 32BIT
```

También debe contener:

```text
R36SX_U2414_AU8_SYNC_REPLICA
R36SX USB AUDIO
```

y los patrones binarios de endpoints síncronos.

## Scripts

Todos los scripts incluidos pasan `bash -n`. El parcheador pasa `python3 -m py_compile`.

## Aislamiento

El instalador no sustituye el core, el launcher, el daemon ni archivos de proyecto. Respalda el runtime U2.41.3 antes de copiar el módulo y scripts nuevos.
