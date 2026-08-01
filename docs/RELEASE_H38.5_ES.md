# LGPT R36SX H38.5 — FX3 eliminado, cuadrículas centradas, 1 = 100%, VU denso, Rename Project

Build H38.5, ABI7, three-mode, frontend-safe. Simplifica la frase a 2 FX
(como LittleGPTracker original) y aplica los ajustes de usabilidad
acordados tras las pruebas de H38.4.

## Novedades en esta versión

### 1. FX3 eliminado de la edición y de la reproducción

- **Phrase** pasa a 7 columnas: `00  N  V  I  FX1  P  FX2  P`.
- **Table** pasa a 4 columnas: `F1 P1 F2 P2`.
- El tercer slot de comandos de la frase ya no se edita ni se ejecuta
  (los datos antiguos se conservan en el archivo del proyecto para
  compatibilidad).
- El cursor del fraseador ya no salta sobre una tercera columna.

### 2. Cuadrículas centradas con cabeceras encima de su columna

- La cuadrícula de Phrase y la de Table se dibujan centradas en pantalla
  con márgenes simétricos.
- Cada columna lleva su cabecera (número de fila, nota, volumen,
  instrumento, FX1, P1, FX2, P2 / F1, P1, F2, P2) alineada sobre el centro
  de su columna.

### 3. Escala 1 = 100% (volumen y FX)

- El volumen de frase y la intensidad de los FX del instrumento ahora se
  interpretan como **1 = pleno** y valores más altos **atenúan**
  (2 ≈ 99%, 50 ≈ 51%, 100 ≈ 1%).
- Al insertar una nota o instrumento nuevo se escribe volumen `1`.
- `--` / `00` / `0` siguen silenciando o apagando el FX.

### 4. VU del Mixer denso (como Record)

- Las barras VU de canales y master ahora son segmentos LED contiguos
  (2 celdas de alto por paso), estilo medidor de Record, con decaimiento
  suave y colores del port.

### 5. Rename Project en el menú Project

- Nuevo campo **Rename Project** en el menú del Project View.
- El diálogo abre con el nombre actual del proyecto precargado para
  editarlo directamente.

## Contenido del ZIP (copiar a la raíz de la SD)

El ZIP de la release está preparado para instalarse **copiando directamente**
sus carpetas a la raíz de la SD (copiar `cubegm`, `lgpt`, `roms`,
`LGPT_OTG_LOGS` y `ANDROID` combinando carpetas y reemplazando archivos).

- `cubegm/cores/lgpt_r36sx_port_libretro.so` — core H38.5
  `SHA256=ed3c8afb2cf215773e3609e73b843fa6b4cdd02827b8b62acef66ba4d5f1d2f2`
- `lgpt/otg/bin/r36s_u241_usb_audio_io` — daemon golden ABI7
  `SHA256=53258f2b8b3749c866af248814eb147f0762a1b17cfffb644adb573167b52815`
- `lgpt/config.xml` — paleta nueva del port
- `lgpt/otg/bin/otg_u241_apply_profile_once.sh` — aplicador de perfil 3 modos
- `lgpt/otg/bin/otg_h37_apply_driver_mode.sh` — selector ABI7
- `lgpt/otg/bin/otg_h37_android_runtime_supervisor.sh` — supervisor Android h36
- `lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36` — daemon Android h36
- `lgpt/otg/bin/r36s_aoa_bulk_receiver_h36` — receptor Android h36
- `ANDROID/LGPTUsbAudioBridge-H36-debug.apk` — APK del puente de audio Android

El core lleva el marcador `H38_5_ABI7_THREE_MODE_LOCAL_WINDOWS_ANDROID_SAFE_FRONTEND`
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
