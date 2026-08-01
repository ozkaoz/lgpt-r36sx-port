# LGPT R36SX H38.3 — Interface y control probados en hardware

Build H38.3, ABI7, three-mode, frontend-safe. Probado en R36SX real: los
3 drivers de audio (Local / Windows / Android) se ven correctamente.

## Novedades en esta versión

### 1. Vista frontal (sidebar) abajo a la izquierda

- La tabla de patrones (`P G` / `SCPI` / `M TT`) se movió a la esquina
  inferior izquierda (base x=2, y=27), con margen respecto al borde.
- Ahora también es visible en la vista **Mixer**.

### 2. Menú Phrase centrado más abajo

- El selector de comandos (FX de phrase) se desplaza un poco hacia abajo
  en pantalla (`SetWindowOffset`), manteniendo el grid de 6 columnas.

### 3. Columna de volumen de Phrase completamente editable

- Navegar sobre la casilla de volumen con `UP/DOWN` cambia el valor **±1**.
- `A + UP/DOWN` cambia el valor **±10**.
- Las celdas vacías parten de 99 (el nivel que asigna el reproductor al
  poner una nota), por lo que la columna siempre responde.

### 4. Nota en 3 caracteres

- Formato compacto de tracker: `C-3`, `C#3`, `B-1` (3 caracteres exactos)
  al poner y al modificar notas en la vista Phrase.

### 5. Colores del port aplicados

- Paleta azul/violeta nueva aplicada vía `lgpt/config.xml` y
  `lgpt/config.stock.xml`: fondo `0A0A18`, texto `E8E4F8`, bordes `3F5FBF`,
  highlights `5B8CFF`/`9D5BFF`, cursor `7FB8FF`, play `4AD8FF`,
  SONGVIEW `2A3E8F`/`1E2B66`, rows `5A7DF0`/`A86BFF`.
- El instalador respalda el `config.xml` anterior y escribe el nuevo tema.

### 6. VU del Mixer en tiempo real

- Las barras VU de canales y master rebotan en tiempo real durante la
  reproducción (redibujo por tick del reproductor con
  `MixerService::GetChannelPeak/GetMasterPeak`).

### 7. Controles R1/R2 en el Mixer

- `R1 + A` activa/desactiva **Solo** en el canal seleccionado.
- `R2 + A` abre el menú **INSTRUMENT FX** del canal.
- `R1 + B` sigue siendo mute; leyenda inferior actualizada.

## Contenido del ZIP (copiar a la raíz de la SD)

El ZIP de la release está preparado para instalarse **copiando directamente**
sus carpetas a la raíz de la SD (copiar `cubegm`, `lgpt`, `roms`,
`LGPT_OTG_LOGS` y `ANDROID` combinando carpetas y reemplazando archivos).

- `cubegm/cores/lgpt_r36sx_port_libretro.so` — core H38.3
  `SHA256=ca3baa9efb778f5168f53df3e3b089f0d85dde5bd0056d9539cded22bba80e75`
- `lgpt/otg/bin/r36s_u241_usb_audio_io` — daemon golden ABI7
  `SHA256=53258f2b8b3749c866af248814eb147f0762a1b17cfffb644adb573167b52815`
- `lgpt/config.xml` — paleta nueva del port
- `lgpt/otg/bin/otg_u241_apply_profile_once.sh` — aplicador de perfil 3 modos
- `lgpt/otg/bin/otg_h37_apply_driver_mode.sh` — selector ABI7
- `lgpt/otg/bin/otg_h37_android_runtime_supervisor.sh` — supervisor Android h36
- `lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36` — daemon Android h36
- `lgpt/otg/bin/r36s_aoa_bulk_receiver_h36` — receptor Android h36
- `ANDROID/LGPTUsbAudioBridge-H36-debug.apk` — APK del puente de audio Android

El core lleva el marcador `H38_3_ABI7_THREE_MODE_LOCAL_WINDOWS_ANDROID_SAFE_FRONTEND`
y el paquete se verifica con `scripts/06_VERIFY_SD.sh`.

## Instalación

1. Extraer el ZIP.
2. Copiar `cubegm`, `lgpt`, `roms`, `LGPT_OTG_LOGS` y `ANDROID` a la raíz de la SD.
3. Instalar la APK de `ANDROID/` en el dispositivo Android.
4. Expulsar la SD de forma segura e iniciar LGPT desde TreeFrogUI.

## Reproducibilidad

El ZIP incluye `SOURCE_AND_TOOLS/full_repository/`: snapshot completo del
repositorio en el commit publicado, con todo el código fuente, scripts de
build, tests y evidencia de validación.
