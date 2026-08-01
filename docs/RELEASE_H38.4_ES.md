# LGPT R36SX H38.4 — Ajustes tras prueba en hardware

Build H38.4, ABI7, three-mode, frontend-safe. Corrige y afina lo probado
en consola real con H38.3: centrado de menús, volumen de frase correcto,
FX con rangos seguros (sin saturar el puerto), y VU segmentado.

## Novedades en esta versión

### 1. Menús centrados de verdad

- El selector de comandos (`Phrase`, ambos `Table`) ahora se centra en
  pantalla calculando el desplazamiento vertical automáticamente
  (`top = (20 - alto) / 2`), en lugar del desplazamiento manual de H38.3.

### 2. Volumen de Phrase con comportamiento correcto

- `--` (sin valor) ahora **silencia** la fila.
- `00` también silencia (0%).
- `1` a `100` dan aumento **proporcional y lineal** del volumen
  (100 = pleno).
- Valores por encima de 100 quedan al máximo sin distorsión.
- La columna muestra el valor en decimal de 3 dígitos.

### 3. FX con rangos seguros (sin romper el sonido)

Los FX del instrumento ya no pueden saturar el bus del puerto:

- **FRES** (resonancia): máxima 50%.
- **FBMX** (mezcla de feedback): máxima 50%.
- **FBTN** (tono del feedback): máxima 50%.
- **FLTR** (resonancia del filtro): máxima 50%.
- **CRSH** (bitcrush): drive máximo 50%, crush máximo 8.
- **PTCH / PFIN** (pitch): sin cambios, siempre seguros.
- **DLAY / FCUT**: sin cambios de rango.

Explicación rápida de cada FX (leyenda inferior del mixer):

| FX | Qué hace |
|----|----------|
| PTCH | Afinación del canal (semitono / cent) |
| PFIN | Afinación fina del canal |
| DLAY | Eco/retardo (ms, toca con FCUT/FBMX) |
| FBMX | Cuánto del eco vuelve a entrar (feedback) |
| FBTN | Tono del eco que vuelve (más oscuro = más bajo) |
| FLTR | Filtro pasa-bajos (también su resonancia) |
| FCUT | Frecuencia de corte del filtro |
| FRES | Resonancia del filtro (máx. 50%) |
| CRSH | Degradado digital del sonido (bitcrush) |

### 4. VU del Mixer en segmentos (como Record)

- Las barras VU de canales y master ahora son segmentos LED pequeños
  (patrón 2 encendidos / 1 apagado) y responden en tiempo real.
- El nivel se mantiene visible entre actualizaciones de la pantalla
  (decaimiento más suave).

### 5. Resto conservado de H38.3

- Vista frontal (sidebar) abajo a la izquierda, visible también en Mixer.
- Nota en 3 caracteres (`C-3`, `C#3`, `B-1`).
- Paleta azul/violeta del port.
- `R1 + A` Solo, `R2 + A` Instrument FX, `R1 + B` mute.

## Contenido del ZIP (copiar a la raíz de la SD)

El ZIP de la release está preparado para instalarse **copiando directamente**
sus carpetas a la raíz de la SD (copiar `cubegm`, `lgpt`, `roms`,
`LGPT_OTG_LOGS` y `ANDROID` combinando carpetas y reemplazando archivos).

- `cubegm/cores/lgpt_r36sx_port_libretro.so` — core H38.4
  `SHA256=d01a8893b9d47c14136996fbc03574ad195a9f717243f0352f4e3b8ad69af5e1`
- `lgpt/otg/bin/r36s_u241_usb_audio_io` — daemon golden ABI7
  `SHA256=53258f2b8b3749c866af248814eb147f0762a1b17cfffb644adb573167b52815`
- `lgpt/config.xml` — paleta nueva del port
- `lgpt/otg/bin/otg_u241_apply_profile_once.sh` — aplicador de perfil 3 modos
- `lgpt/otg/bin/otg_h37_apply_driver_mode.sh` — selector ABI7
- `lgpt/otg/bin/otg_h37_android_runtime_supervisor.sh` — supervisor Android h36
- `lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36` — daemon Android h36
- `lgpt/otg/bin/r36s_aoa_bulk_receiver_h36` — receptor Android h36
- `ANDROID/LGPTUsbAudioBridge-H36-debug.apk` — APK del puente de audio Android

El core lleva el marcador `H38_4_ABI7_THREE_MODE_LOCAL_WINDOWS_ANDROID_SAFE_FRONTEND`
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
