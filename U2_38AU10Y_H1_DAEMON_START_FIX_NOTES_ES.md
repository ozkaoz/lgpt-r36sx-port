# U2.38AU10Y-H1 — corrección daemon-start y runtime USB seguro

Esta variante conserva el descriptor UAC2 dúplex estable de AU10Y (`p_chmask=1`, `c_chmask=1`) y corrige el fallo observado en la prueba AU10Y: el script `otg_38au10y_lgpt_sync_setup_once.sh` se detenía antes de iniciar `r36s_au10y_usb_audio_io` por una redirección shell hacia `/mnt/sdcard/lgpt/otg/usb_capture_status`.

Cambios:

- Se elimina la redirección obligatoria `: > /mnt/sdcard/.../usb_capture_status`.
- El estado runtime de USB-C Record se mueve a `/tmp/r36sx_lgpt_usb/`.
- El WAV final continúa guardándose en la ruta solicitada por LGPT, normalmente dentro de la carpeta de samples/proyecto.
- `USB_OUT_AUTO_MUTE` y `FULL_DUPLEX` comparten el mismo gadget UAC2 siempre activo; el modo solo controla si el core escribe o no al FIFO.
- Al entrar en grabación USB-C, el core deja de enviar salida LGPT al USB para evitar realimentación; la preescucha usa el monitor local desde la captura USB.
