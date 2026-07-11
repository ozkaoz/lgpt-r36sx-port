# U2.38 WAV export runtime fix

Objetivo: corregir el caso donde LGPT muestra mensajes de render/exportación, pero no aparecen WAVs en `exports`.

Cambios:

- La salida WAV ya no depende de `project:exports` como ruta primaria.
- La salida se escribe en `root:exports/<nombre_proyecto>/`.
- En R36SX/TreeFrog esto corresponde a `/mnt/sdcard/lgpt/exports/<nombre_proyecto>/`.
- En Windows, con la SD montada, debe verse como `X:\lgpt\exports\<nombre_proyecto>\`.
- `Session WAV` genera `session.wav`, `session_001.wav`, etc.
- `Multitrack` genera una carpeta `multitrack`, `multitrack_001`, etc. con `track_01.wav` a `track_08.wav`.
- Se crea `/mnt/sdcard/lgpt/exports` al boot del core.
- Se añade diagnóstico runtime en `/mnt/sdcard/lgpt/wav_export_debug.log`.
- La UI ya no debe decir que empezó el render si el writer no abrió realmente; en ese caso muestra `WAV export failed; see log`.

Uso recomendado:

1. En LGPT: `Project > Render > Session WAV` o `Multitrack`.
2. Pulsa `START` para iniciar playback/render.
3. Deja correr la canción.
4. Pulsa `START` otra vez para detener y cerrar los WAV.
5. Revisa la SD en `lgpt/exports/<proyecto>/`.

Comando WSL para buscar renders después de montar la SD:

```bash
find /mnt/d/R36S -iname '*.wav' -o -name 'wav_export_debug.log'
```

Si no aparecen WAVs, revisar:

```bash
cat /ruta/de/la/SD/lgpt/wav_export_debug.log
cat /ruta/de/la/SD/lgpt/lgpt.log | grep -i 'wav\|render\|mixer'
```
