# LGPT R36SX H38.6 — Volumen lineal, VU en vivo, Rename en el menú de inicio, editor de texto unificado

Build H38.6, ABI7, three-mode, frontend-safe. Corrige los tres fallos
detectados en las pruebas de hardware de H38.5 (volumen de frase,
VU del Mixer y Rename Project) y unifica la entrada de texto del port.

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
- Antes solo se redibujaban con el player en marcha.

### 3. Rename Project movido al menú de inicio (R1+A)

- El rename ya no vive en el menú Project (provocaba un crash al salir a
  TreeFrogUI y reentrar: el nombre no se aplicaba).
- Ahora: en la pantalla inicial **Load / New / Exit**, con el proyecto
  seleccionado pulsa **R1 + A** para renombrarlo.
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

## Instalación

1. Copia el contenido de este ZIP a la raíz de la tarjeta SD (carpetas
   `cubegm/`, `lgpt/`, `roms/`, `LGPT_OTG_LOGS/`, `ANDROID/`).
2. Arranca LGPT desde el menú de EmulationStation.
3. En el menú inicial selecciona el proyecto y pulsa **R1 + A** para
   probar el rename.

## Verificación

- `VERIFY_BUILD_H38_6_ABI7_OK`
- `VERIFY_SD_H38_6_ABI7_OK`
- Daemon ABI7 inalterado (golden SHA-256 conservado).
