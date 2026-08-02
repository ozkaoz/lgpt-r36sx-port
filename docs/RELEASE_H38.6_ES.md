# LGPT R36SX H38.6 — Mixer VU dinámico, menú de inicio Rename/Export/Delete, FX corregido

Build H38.6, ABI7, three-mode, frontend-safe. Corrige los tres fallos
detectados en las pruebas de hardware de H38.5 (volumen de frase,
VU del Mixer y Rename Project), unifica la entrada de texto del port y
rehace las barras del Mixer (onda en vivo + máster ajustable + menú FX).
Esta revisión añade las **barras VU dinámicas de verdad** (decaimiento
visible) y el **menú de acciones de proyecto** en la pantalla inicial.

## Correcciones en esta versión

### 1. Volumen de frase corregido (escala lineal, sin distorsión)

- **Causa raíz**: los bloques de Render de `SampleInstrument` sobrescribían
  la ganancia de fila en cada tick, anulando el volumen editado por fila, y
  la escala estaba invertida.
- **Solución**: nueva ganancia de fila persistente (`rowGain_`) aplicada en
  ambos bloques de Render; la escala ahora es **lineal**: `100 = pleno`,
  `1 ≈ 2%`, `0`/`FF` = silencio.
- Cada valor 0-100 suena con su intensidad real, sin clip ni distorsión.
- Al insertar una nota nueva en una fila vacía se escribe volumen `100`
  automáticamente (y el editor de una celda de volumen vacía arranca en 100).

### 2. VU del Mixer en tiempo real

- Las barras VU del Mixer ahora se actualizan por **cadencia de frame**
  (como el medidor de USB-C Record), independientes del transporte: siguen
  moviéndose con el player parado (decaimiento de picos).
- **Nuevo (feedback de hardware)**: el decaimiento del pico por buffer se
  aceleró (`0.85` por callback de audio), por lo que las barras **bajan
  visiblemente entre golpes** en vez de quedarse pegadas en blanco completo
  arriba-abajo. Siguen el nivel real de salida de cada canal y del máster.
- Al modificar el volumen, este afecta al sonido y **no** mueve la barra
  (correcto: la barra es el nivel, el número debajo es el volumen).

### 3. Menú de acciones de proyecto en la pantalla inicial (R1+A)

- En la pantalla inicial **Load / New / Exit**, con un proyecto seleccionado,
  pulsa **R1 + A** para abrir un menú con **tres opciones**:
  - **Rename**: renombra el proyecto (el editor de texto unificado).
  - **Export**: elige **Full project (master)** (mixdown `mixdown.wav`) o
    **Multitrack** (stems `channel0.wav` … `channel7.wav`). El proyecto se
    carga y se renderiza automáticamente.
  - **Delete**: pide confirmación antes de borrar el proyecto.
- Navega con `UP/DOWN`, confirma con `A`, cancela con `B`.
- El rename copia el directorio del proyecto en disco (sin proyecto cargado,
  sin riesgo de corrupción) y refresca la lista.

### 4. Editor de texto unificado en todo el port

- Nueva entrada de texto común (la misma del nombre de archivo de
  USB-C Record) usada en **Rename Project**, **New Project**,
  **Save Song As** y **Rename Sample**:
  - `X + UP/DOWN` = avanzar carácter de ±5
  - `UP/DOWN` = avanzar carácter de ±1
  - `L1 + X` = cambiar mayúsculas/minúsculas
  - `A` = confirmar · `B` = borrar carácter · `R1 + LEFT` = cancelar
- El teclado QWERTY en pantalla del New Project se retiró en favor de esta
  lógica uniforme.

## 5. Mixer: barras de onda en vivo, barra MST y menú Instrument FX

### Barras de canal dinámicas (forma de onda en tiempo real)

- El relleno de cada barra ahora sigue el **nivel de salida real del canal**
  (`GetChannelPeak`) con **decaimiento visible por buffer** (ver punto 2):
  volumen bajo = onda pequeña, volumen alto = onda grande, y las barras
  rebotan con la música.
- El ajuste de volumen del canal se dibuja como una **marca de color** sobre
  la barra y como número debajo (ya no es un relleno estático blanco).

### Barra maestra MST (nueva, a la izquierda)

- La barra **MST** (master) se dibuja en vivo **a la izquierda de la primera
  barra de canal**, en cian para distinguirse de los canales (blanco) y con
  su propio pico de nivel (también dinámico).
- Es **seleccionable**: `LEFT` desde el canal 0 entra en MST, `RIGHT` vuelve
  al canal 0.
- Con MST seleccionado, `A+UP/DOWN` ajusta el **volumen global** (10 en 10)
  y `A+LEFT/RIGHT` lo afina (1 en 1). El valor se guarda en el proyecto
  (igual que "Master" del menú Project) y se aplica al instante.

### Menú Instrument FX (R1+A / R2+A)

- Ya **no se cierra con A** al editar (A sola no hace nada).
- Se cierra con **B** o con el **mismo combo que lo abre** (R1+A / R2+A).
- Edición de valores **como en Song**: `A+UP/DOWN` = ±0x10 (grueso),
  `A+LEFT/RIGHT` = ±1 (fino).
- **A+B** restaura el valor por defecto de la opción seleccionada.

## Instalación

1. Copia el contenido de este ZIP a la raíz de la tarjeta SD (carpetas
   `cubegm/`, `lgpt/`, `roms/`, `LGPT_OTG_LOGS/`, `ANDROID/`).
2. Arranca LGPT desde el menú de EmulationStation.
3. En el menú inicial selecciona el proyecto y pulsa **R1 + A** para
   probar el menú Rename / Export / Delete.

## Verificación

- `VERIFY_BUILD_H38_6_ABI7_OK`
- `VERIFY_SD_H38_6_ABI7_OK`
- Daemon ABI7 inalterado (golden SHA-256 conservado).

## Actualización H38.6-r2 (feedback de hardware)

### Barras VU calibradas al volumen real (escala dB)

- El relleno de las barras ahora se mapea con una **curva perceptiva (dBFS,
  rango de 50 dB)** en vez de la escala lineal: `0 dB` = barra llena y el
  relleno cae conforme baja el volumen real. Antes, cualquier nivel por
  encima de unos pocos dB quedaba "blanco estático" y los volúmenes bajos
  también leían alto.
- El master refleja el nivel audible post-volumen: con volumen maestro bajo
  (10-17) la barra queda casi vacía (el sonido es casi inaudible) y crece
  en 30/50/100, en línea con lo que se escucha.
- El marcador de volumen sigue mostrando el ajuste (número + celda).

### Menú de acciones corregido (abre los diálogos sin liberarlos)

- **Causa raíz**: los diálogos anidados (editor Rename, selector de modo
  Export, confirmación Delete) se abrían desde el callback de cierre del
  menú, y el ciclo de vida de los modales los liberaba de inmediato
  (`SAFE_DELETE` tras el callback), dejando punteros colgando y, al borrar
  un proyecto, cuelgues al volver a entrar.
- **Solución**: la acción elegida se difiere y se lanza en el siguiente
  tick de frame (`OnFrameUpdate`), cuando el menú ya se ha liberado. Ahora
  Rename / Export / Delete abren sus diálogos correctamente y borrar un
  proyecto refresca la lista sin crashear.
