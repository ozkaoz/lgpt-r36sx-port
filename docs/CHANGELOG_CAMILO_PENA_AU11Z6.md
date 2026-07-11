# AU11Z6 Camilo Peña — WSL24 marker write fix

## Motivo
AU11Z5 compilaba correctamente y alcanzaba `SUMMARY=PASS_FOR_AU11_INSTALL`, pero el instalador se detenía al escribir:

```text
/mnt/f/lgpt/otg/au11_active_usb_profile: Invalid argument
```

Esto ocurre en SD montada por WSL/DrvFs cuando una redirección directa `echo > archivo` falla sobre un archivo marcador. No es un fallo del core LGPT ni del daemon; es un fallo de instalación/metadata en la SD.

## Cambios
- `bin/02_INSTALL_AU11_TO_SD_FROM_WSL.sh` ahora usa `safe_write_text_required` y `safe_write_text_optional`.
- `audio_driver_mode` y `au11_usb_policy` son requeridos.
- `au11_active_usb_profile` es opcional porque la consola puede recrearlo mediante `otg_38au11_apply_profile_once.sh`/`otg_38au11_common.sh`.
- `bin/03_VERIFY_AU11_SD_INSTALL_FROM_WSL.sh` acepta marcadores AU11Z6.
- Nuevo diagnóstico: `00_WSL_UBUNTU24_DIAGNOSTICAR_SD_INVALID_ARGUMENT_AU11Z6.sh`.

## Estado esperado
No probar en consola hasta obtener:

```text
SUMMARY=PASS_AU11_SD_VERIFY
CORE_1_CMP=0
CORE_2_CMP=0
DAEMON_CMP=0
SUMMARY=PASS_AU11Z6_CAMILO_FULL_CLEAN_COPYFIX_VERIFYFIX_MARKERWRITEFIX
```
