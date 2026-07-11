# AU11Z9 — estado técnico para continuar OTG

## Estado confirmado

- LGPT carga en R36SX v2.6 sobre TreeFrogUI.
- `lgpt_libretro.so` compila con el toolchain local.
- La instalación base en SD crea correctamente:
  - `/lgpt`
  - `/lgpt/config.xml`
  - `/roms/lgpt/start.lgpt`
  - `/cubegm/cores/lgpt_libretro.so`

## Diagnóstico de los paquetes previos AU11

Los paquetes AU11 ya intentaban una ruta USB Audio UAC2 por `configfs` usando:

- descriptor `idVendor=0x1209`, `idProduct=0x38EA`;
- producto `R36SX USB Audio`;
- función `uac2.usb0`;
- 48 kHz, 16 bits;
- `p_chmask=1`, `c_chmask=1`;
- daemon `r36s_au11_usb_audio_io`;
- FIFO `/tmp/r36sx_uac2_bridge_fifo`;
- scripts runtime en `/mnt/sdcard/lgpt/otg/bin`.

La documentación anterior indica que el punto crítico fue preservar/restaurar módulos kernel `.ko` bajo `/lgpt/otg/modules`. Si esos módulos no existen, el síntoma esperado es `LOAD_soundcore.ko_MISSING`, ausencia de `/dev/snd` o fallo de enumeración USB Audio.

## Almacenamiento por OTG

No está implementado todavía como función estable. Exponer la SD completa como USB Mass Storage mientras la consola la tiene montada puede corromper datos, porque Windows y Linux podrían escribir al mismo bloque simultáneamente.

Orden técnico recomendado:

1. Inventario de módulos y estado de la SD con `tools/wsl/06_otg_inventory_from_sd.sh`.
2. Extraer módulos stock de gadget desde el backup con `tools/wsl/07_extract_stock_gadget_modules_from_backup_zip.sh`.
3. Implementar primero modo diagnóstico de almacenamiento seguro, no escritura completa:
   - preferencia: MTP si hay daemon usable;
   - alternativa: modo exclusivo con desmontaje controlado, solo si se identifica partición segura;
   - evitar USB Mass Storage sobre `/mnt/sdcard` montado.
4. Solo después integrar una opción de menú `OTG FILE TRANSFER`.

## Audio USB OTG

Para Windows/Android, la línea viable sigue siendo UAC2/ALSA:

- cargar `soundcore.ko`, `snd.ko`, `snd-timer.ko`, `snd-pcm.ko`;
- cargar `libcomposite.ko`;
- cargar `usb_f_uac2.ko` compatible con kernel 4.4.186-release;
- crear gadget por `configfs`;
- conectar daemon PCM/FIFO con LGPT.

El backup stock revisado aporta `libcomposite.ko`, `usb_f_mtp.ko`, `usb_f_ptp.ko`, `f_ium.ko`, `ium.ko`, `f_iap.ko`, pero no aporta por sí solo todos los módulos ALSA/UAC2 requeridos. Por eso hay que localizar los módulos AU11 anteriores o recompilarlos contra el kernel exacto antes de prometer audio USB estable.

## Comandos de diagnóstico inicial

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"

bash tools/wsl/06_otg_inventory_from_sd.sh F "/mnt/d/R36S/PORT LPTRACKER"

bash tools/wsl/07_extract_stock_gadget_modules_from_backup_zip.sh \
  "/mnt/d/R36S/PORT LPTRACKER/R36SX V2.6 (0712) Minimal Backup.zip"
```

Subir después el inventario al repositorio si aporta datos:

```bash
git add docs tools/wsl r36sx_package/modules_cache/stock_gadget_modules
git commit -m "Add AU11Z9 PC installer and OTG inventory tooling"
git push origin main
git switch r36sx-v2.6-treefrog-au11z6
git merge main
git push origin r36sx-v2.6-treefrog-au11z6
git switch main
```
