# U2.42 - Mute/Solo unificado, edición de chops y normalización

Base: U2.41 final WAV export.

## Objetivo

Esta versión continúa desde U2.41 y agrega tres bloques funcionales:

1. Unificación de combinaciones de mute/solo con el Mixer.
2. Edición posterior de chops ya cortados, con snap automático a zero-crossing y release de chop.
3. Normalización destructiva de samples desde el panel Pitch/Env.

## Cambios de input

Las combinaciones del Mixer quedan normalizadas en Song, Chain y Phrase cuando no hay conflicto lógico con una función específica del editor:

- `R+B`: mute/unmute del canal actual o selección.
- `R+A`: solo/un-solo del canal actual o selección.
- `R+L`: unmute all, donde ya existía.

Se elimina el retoggle al soltar botones en Song/Chain/Phrase. Esto evita que `R+B` o `R+A` activen y desactiven el estado durante el mismo gesto.

Nota: en selección de Phrase, `B+R` conserva su comportamiento histórico de interpolación; ahí no se fuerza mute porque ya existe una función de edición explícita.

## Cambios en Chopper

El antiguo modo presentado como `Crop` ahora se comporta como modo de edición de chop:

- `SELECT`: entra/sale de `EDIT CHOP`.
- `A+LEFT/RIGHT`: ajusta el inicio del chop seleccionado.
- `B+LEFT/RIGHT`: ajusta el final del chop seleccionado.
- `L1` aumenta el paso de edición.
- `Y`: preview del inicio.
- `X`: preview del final.
- `R1+A`: crop destructivo de la región seleccionada.
- `L2+Y`: delete destructivo de la región seleccionada.
- `R1+X`: undo/redo del último edit destructivo.

Para reducir clicks al cortar sobre valores no cercanos a cero, los cortes nuevos y los ajustes de inicio/final hacen snap automático al zero-crossing más cercano en una ventana corta. Si no hay cruce claro, se usa el punto de menor amplitud dentro de la ventana.

## Release de chop

Se agrega release lógico de chop:

- `R2+UP`: aumenta release en pasos de 5 ms.
- `R2+DOWN`: reduce release en pasos de 5 ms.
- Rango: 0 a 500 ms.

Este release no modifica destructivamente el WAV. Extiende el `END` de los instrumentos de chop clonados para dejar pasar una cola corta del sample y reducir cortes abruptos.

El valor se guarda en el sidecar `.u2chop` como `releaseMs`, manteniendo compatibilidad con sidecars anteriores que no contienen esa línea.

## Normalización en Pitch/Env

El panel `Pitch/Env` ahora incluye una fila:

```text
Normalize: Sample/Chop
```

Uso:

- `L1+R1`: abre Pitch/Env.
- `UP/DOWN`: selecciona parámetro.
- Seleccionar `Normalize`.
- `LEFT/RIGHT`: alterna `Sample` / `Chop`.
- `A`: aplica normalización destructiva.
- `L1+X`: undo/redo del último edit destructivo.

La normalización apunta a aproximadamente -0.2 dBFS para evitar clipping posterior.

## Archivos principales modificados

```text
sources/Application/Views/SongView.cpp
sources/Application/Views/ChainView.cpp
sources/Application/Views/PhraseView.cpp
sources/Application/Views/ModalDialogs/SampleChopperModal.h
sources/Application/Views/ModalDialogs/SampleChopperModal.cpp
```

## Build recomendado

Compilar desde una ruta sin espacios:

```bash
cd ~
rm -rf lgpt_u242_build
mkdir -p lgpt_u242_build
cd lgpt_u242_build
unzip -q "/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_42_MUTE_CHOP_NORMALIZE_SOURCE.zip"
cd LGPT_PORT_U2_42_MUTE_CHOP_NORMALIZE_SOURCE
bash VERIFY_U2_36_FINAL_PRE_OTG_SOURCE.sh
bash BUILD_U2_36_STABLE_WSL.sh
```

Instalación:

```bash
cp -f dist/lgpt_libretro.so "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_42.so"
bash INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh F "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_42.so"
```

Cambiar `F` si la SD tiene otra letra.

## Pruebas mínimas

1. En Mixer, Song, Chain y Phrase: probar `R+B`, `R+A`, `R+L`.
2. En Song: confirmar que `R+B` no se revierte al soltar botones.
3. En Chopper: cortar varios chops, entrar a `EDIT CHOP`, ajustar inicio/final y verificar preview.
4. En Chopper: ajustar `R2+UP/DOWN`, exportar/asignar chops y comprobar que los clones usan una cola mayor.
5. En Pitch/Env: normalizar `Sample`, luego undo/redo con `L1+X`.
6. En Pitch/Env con Scope/Normalize `Chop`: normalizar solo el chop seleccionado.
