# Prompt para continuar desde U2.36 FINAL estable hacia U2.37-OTG-AUDIO

Contexto: estoy continuando el port de LittleGPTracker para R36S/R36SX con TreeFrogUI. La base congelada es **LGPT_PORT_U2_36_FINAL_ESTABLE_PRE_OTG_CHOPPER**. Esta base ya contiene todo el desarrollo estable hasta la última implementación validada del Chopper/U2.36 y no debe recibir cambios especulativos de OTG dentro del core.

## Base obligatoria

Usar como fuente de verdad:

```text
LGPT_PORT_U2_36_FINAL_ESTABLE_PRE_OTG_CHOPPER.zip
```

No retroceder a U2.22/U2.23 ni reaplicar parches antiguos. El árbol ya contiene los cambios U2.23-U2.36.

## Marcadores esperados

```text
Graphical Chopper U2.36
PITCH/ENV U2.36
TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
```

## Estado estable que no debe romperse

- Chopper normal: crear, borrar, seleccionar, preescuchar, persistir `.u2chop`.
- Phrase muestra `S01/S02/etc.` cuando hay chops.
- Crop Sample estable.
- Pitch/Envelope estable con Scope Sample/Chop.
- Listen/Import estable: `Listen Import Manage Exit`, `A` en Listen preescucha, `L2+B` detiene.
- Import deduplica por contenido y protege nombres repetidos con contenido distinto.
- Sample Manager borra libres `--`, purge conservador y force delete con confirmación.

## Nuevo objetivo: U2.37-OTG-AUDIO

Activación y validación del puerto OTG de R36SX para audio externo:

1. Detección USB/OTG.
2. Detección ALSA de dispositivos USB audio.
3. Salida de audio LGPT hacia interfaz USB.
4. Entrada/captura desde interfaz USB hacia WAV/importación LGPT.

## Restricción técnica

No modificar el core LGPT todavía. La salida USB puede resolverse primero a nivel launcher/ALSA/picoarch si el kernel enumera USB Audio Class. La entrada de audio USB no está soportada por libretro/picoarch hacia el core; requerirá captura ALSA directa en TREEFROG o herramienta externa `arecord` integrada a importación WAV.

## Diagnóstico actual

Se observó:

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

U2.37D logró ConfigFS/libcomposite/MTP function, pero no confirmó bind:

```text
BOUND_ANY=NO
AUDIO_UAC_AVAILABLE=NO
```

U2.37E mostró avance real:

```text
mode peripheral escribible
state=configured
current_speed=high-speed
```

Esto sugiere que gadget/peripheral puede funcionar físicamente, aunque la evaluación del bind por código de retorno fue demasiado estricta.

## Próximo paso recomendado

Ejecutar o reconstruir U2.37F USB Gadget Hold + ABI Probe. Debe mantener activo el gadget y registrar:

```text
GADGET_CONFIGURED=YES/NO
MODE_PERIPHERAL_WRITE_OK=YES/NO
ALSA_MODULES_AVAILABLE=YES/NO
ALSA_SYMBOLS_BUILTIN=YES/NO
AUDIO_UAC_MODULES_AVAILABLE=YES/NO
```

Mientras dura la prueba, conectar USB-C al PC y revisar en Windows Administrador de dispositivos si aparece MTP, dispositivo desconocido o gadget LGPT/R36SX.

## Decisión posterior

- Si `GADGET_CONFIGURED=YES` y ALSA/UAC siguen en `NO`: preparar parche SO/rootfs/kernel con `snd`, `snd-pcm`, `u_audio`, `usb_f_uac1/uac2`.
- Si `GADGET_CONFIGURED=NO` y no se puede forzar peripheral: revisar kernel/DT/arranque MUSB.
- Solo cuando exista ALSA/UAC real, conectar salida LGPT a USB audio.
- Para entrada USB real, diseñar captura WAV externa o captura ALSA en adapter TREEFROG. No asumir que libretro entregará entrada al core.
