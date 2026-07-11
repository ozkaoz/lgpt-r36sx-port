# Estado técnico OTG/audio antes de tocar LGPT

Objetivo nuevo fijado: **U2.37-OTG-AUDIO**.

Objetivo funcional:

1. Detección USB/OTG.
2. Detección ALSA de dispositivos USB audio.
3. Salida de audio LGPT hacia interfaz USB.
4. Entrada/captura desde interfaz USB hacia WAV/importación LGPT.

## Distinción técnica crítica

La salida de audio USB puede resolverse primero a nivel launcher/ALSA/picoarch si el kernel enumera una interfaz USB Audio Class. La entrada de audio USB no está soportada actualmente por el core libretro de LGPT: TreeFrog entrega audio por `retro_audio_sample_batch`, pero libretro/picoarch no entrega audio de captura al core.

Para entrada real hay dos rutas posibles:

- Captura ALSA directa dentro del adapter TREEFROG.
- Herramienta externa `arecord` / captura WAV integrada al flujo de importación.

Por tanto, no conviene modificar el core LGPT a ciegas. Primero se debe confirmar el soporte real de hardware, kernel y rootfs.

## Diagnóstico ya observado

Primer resultado de gadget/ALSA:

```text
CONFIGFS=yes
USB_GADGET_DIR=yes
UDC_COUNT=2
LIBCOMPOSITE_AVAILABLE=yes
LIBCOMPOSITE_LOADED=yes
G_AUDIO_MODULE_AVAILABLE=no
UAC_FUNCTION_AVAILABLE=no
G_AUDIO_LOADED=no
PROC_ASOUND=no
DEV_SND=no
USB_NON_ROOT_DEVICE_COUNT=0
```

Interpretación: la R36SX tiene base parcial para USB gadget porque existen UDCs y `libcomposite` carga. El sistema stock no trae los módulos necesarios para audio USB gadget: faltan `g_audio`, `u_audio`, `usb_f_uac1`, `usb_f_uac2` y ALSA estándar (`snd`, `snd-pcm`, etc.).

## U2.37D ConfigFS Gadget Test

Resultado importante:

```text
CONFIGFS_CREATE=YES
UDC_COUNT=2
LIBCOMPOSITE_LOADED=YES
USB_F_MTP_LOADED=YES
USB_F_PTP_LOADED=YES
FUNCTION_CREATED=YES
FUNCTION_NAME=mtp.gs0
BOUND_ANY=NO
AUDIO_UAC_AVAILABLE=NO
```

Interpretación: ConfigFS puede montar, `libcomposite` carga, se puede crear un gadget MTP y existen dos UDC (`musb-hdrc.0.auto`, `musb-hdrc.1.auto`), pero el bind no quedó confirmado. Tampoco existen módulos UAC/audio.

## U2.37E USB Role Switch + Gadget Probe

Resultado importante:

- Se pudo escribir `peripheral` en archivos `mode` de MUSB.
- Después, `musb-hdrc.0.auto` pasó a:

```text
state=configured
current_speed=high-speed
```

Interpretación: el camino físico USB gadget/peripheral probablemente sí puede funcionar. El script marcó bind como error por `Device or resource busy`, pero el estado posterior del UDC contradice parcialmente ese fallo. La validación debe basarse en estado real del UDC, no solo en el código de retorno de `echo UDC`.

## Estado bloqueante

Sigue sin existir el stack de audio USB:

```text
ALSA: no detectado
/dev/snd: no existe
/proc/asound: no existe
g_audio / u_audio / usb_f_uac1 / usb_f_uac2: no disponibles
```

## Próximo probe correcto: U2.37F

Objetivo de U2.37F:

- Mantener activo el gadget durante una ventana de prueba.
- Confirmar si Windows ve algún dispositivo USB.
- Confirmar ABI/símbolos/módulos disponibles para ALSA/UAC.

Campos decisivos:

```text
GADGET_CONFIGURED=YES/NO
MODE_PERIPHERAL_WRITE_OK=YES/NO
ALSA_MODULES_AVAILABLE=YES/NO
ALSA_SYMBOLS_BUILTIN=YES/NO
AUDIO_UAC_MODULES_AVAILABLE=YES/NO
```

Decisión técnica:

- Si `GADGET_CONFIGURED=YES` y ALSA/UAC siguen en `NO`, U2.38 debe ser parche de módulos/rootfs para agregar `snd`, `snd-pcm`, `u_audio`, `usb_f_uac1`/`usb_f_uac2`.
- Solo después de eso tiene sentido conectar LGPT/picoarch a una ruta de audio USB.
- Si `MODE_WRITE_PERIPHERAL_OK=NO` y `BOUND_ANY=NO`, el siguiente paso no es LGPT: es kernel/DT/arranque USB para permitir modo peripheral/gadget.
