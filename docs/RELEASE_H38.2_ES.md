# LGPT R36SX H38.2 — Tres drivers de audio (Local / Windows / Android)

Build H38.2, ABI7, three-mode, frontend-safe.

## Novedades en esta versión

### 1. Tres drivers de audio USB seleccionables

Ahora LGPT ofrece tres modos de salida de audio USB desde el menú Audio Driver
(dentro de Settings), y el sistema los gestiona de forma automática:

- **Local**: audio interno de la R36SX (estado por defecto y más seguro).
- **Windows**: audio bidireccional con PC/Windows mediante el daemon UAC2
  (ABI7, golden `r36s_u241_usb_audio_io`).
- **Android**: audio bidireccional con dispositivos Android mediante los
  daemons h36 (`r36s_aoa_bulk_audio_io_h36` + `r36s_aoa_bulk_receiver_h36`).

La selección se aplica con `otg_u241_apply_profile_once.sh` (3 modos) y el
selector `otg_h37_apply_driver_mode.sh` (ABI7), que conviven en la SD con los
daemons h36 supervisados por `otg_h37_android_runtime_supervisor.sh`.

### 2. Interfaz de mezclador (Mixer) con FX por instrumento

- Barras de nivel (VU) por canal y por bus, dinámicas.
- Menú de efectos por instrumento: `R2 + A` en la vista Mixer abre
  **INSTRUMENT FX** con:

  - RVB reverb, DLY delay, FLT cutoff, EQ reso, CMP crush, PIT detune
  - Controles por fila: `L/R` valor, `A+L/R` ×16, `X/Y` SOLO/MUTE
  - `A` acepta, `B` cierra.

### 3. Vista frontal (sidebar) del port

- Tabla de patrones (`P G`) reubicada abajo a la derecha de la pantalla
  (base x=36, y=27), sin chocar con el grid de 16 canales.

### 4. Phrase FX solo para beatmaking

El selector de comandos de Phrase se limita a 9 comandos útiles para
beatmaking, con navegación circular (wrap):

- `FBMX` (FX de phrasemix), `FBTN` (bend hacia abajo), `DLAY` (delay),
- `FLTR` (filtro), `FCUT` (corte de filtro = EQ), `FRES` (resonancia),
- `CRSH` (crush/compresión), `PTCH` (pitch), `PFIN` (fine tune).

Nombres mostrados: RVB, RVT, DLY, FLT, EQ, RES, CMP, PIT, PFI.

### 5. Seguridad del frontend (H38.2)

- El core **no** detiene el frontend en ningún flujo normal.
- El daemon golden ABI7 se verifica por SHA-256 al instalar y al verificar.

## Contenido del ZIP (copiar a la raíz de la SD)

El ZIP de la release está preparado para instalarse **copiando directamente**
sus carpetas a la raíz de la SD (copiar `cubegm`, `lgpt`, `roms`,
`LGPT_OTG_LOGS` y `ANDROID` combinando carpetas y reemplazando archivos).

- `cubegm/cores/lgpt_r36sx_port_libretro.so` — core H38.2
  `SHA256=9f37e01725a084e291d1d50bb9dfb9493f5fc4473caa86d3c7f1cbe3cd408d8e`
- `lgpt/otg/bin/r36s_u241_usb_audio_io` — daemon golden ABI7
  `SHA256=53258f2b8b3749c866af248814eb147f0762a1b17cfffb644adb573167b52815`
- `lgpt/otg/bin/otg_u241_apply_profile_once.sh` — aplicador de perfil 3 modos
- `lgpt/otg/bin/otg_h37_apply_driver_mode.sh` — selector ABI7
- `lgpt/otg/bin/otg_h37_android_runtime_supervisor.sh` — supervisor Android h36
- `lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36` — daemon Android h36
- `lgpt/otg/bin/r36s_aoa_bulk_receiver_h36` — receptor Android h36
- `ANDROID/LGPTUsbAudioBridge-H36-debug.apk` — APK del puente de audio Android

El core lleva el marcador `H38_2_ABI7_THREE_MODE_LOCAL_WINDOWS_ANDROID_SAFE_FRONTEND`
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
