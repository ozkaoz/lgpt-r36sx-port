# U2.43 TEST - Phrase chop workflow + Phrase volume column

Estado: paquete de prueba para SD. No marcar como estable hasta validar en R36SX.

## Cambios funcionales

### Mute / Solo

- `R1+B`: mute / unmute del track actual.
- `R1+A`: solo del track actual.
- `R1+A` nuevamente: limpia solo/mute y deja todos los tracks audibles.
- Se elimina la dependencia práctica de `L+R` para unmute all.

### Undo / Redo global

- `R1+X`: undo/redo para ediciones destructivas de sample/chop donde exista snapshot válido.
- En Phrase se expone el mismo flujo para deshacer ediciones realizadas desde Chopper/Pitch-Envelope.

### Phrase: nueva columna Volume

La vista Phrase ahora queda:

```text
Note | Vol | Inst | Cmd1 | Param1 | Cmd2 | Param2
```

La columna `Vol` está entre Note e Instrument.

- `V--`: sin override de volumen; usa el volumen del instrumento.
- `V00..VFE`: override por step, aplicado como comando interno `VOLM` al reproducir esa fila.
- Los proyectos antiguos cargan con `V--` por defecto porque el bloque `VOLUMES` es opcional.
- Al guardar, se escribe un nuevo bloque XML `VOLUMES`.

### Phrase: workflow de chops

- `B`: pre-escucha de la fila actual en Phrase.
- `R2+A`: asignar chop y avanzar fila.
- `R2+LEFT/RIGHT`: chop anterior / siguiente.
- `R2+UP/DOWN`: salto de chops de cuatro en cuatro.
- Doble `A` sobre una fila con chop asignado: abre el mismo menú `Pitch/Envelope` del Chopper apuntando al chop asignado.
- `L1+X`: primer toque inicia selección; segundo toque corta la selección.

### Chopper / Pitch-Envelope

- Se mantiene el flujo U2.42: edición de chop, release, normalización, ajuste de duración y entrada directa al modo Pitch/Envelope.
- Desde Phrase, doble `A` entra al modal de Chopper en modo Pitch/Envelope para el chop asignado.

## Archivos tocados principales

```text
sources/Application/Model/Phrase.h
sources/Application/Model/Phrase.cpp
sources/Application/Model/Song.cpp
sources/Application/Model/Project.cpp
sources/Application/Player/Player.cpp
sources/Application/Views/PhraseView.cpp
sources/Application/Views/PhraseView.h
sources/Application/Views/ChainView.cpp
sources/Application/Views/SongView.cpp
sources/Application/Views/UIController.cpp
sources/Application/Views/ModalDialogs/SampleChopperModal.cpp
sources/Application/Views/ModalDialogs/SampleChopperModal.h
```

## Compilación recomendada en WSL

```bash
cd ~
rm -rf lgpt_u243_test_build
mkdir -p lgpt_u243_test_build
cd lgpt_u243_test_build

unzip -q "/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_43_PHRASE_VOLUME_TEST_SOURCE.zip"
cd LGPT_PORT_U2_43_PHRASE_VOLUME_TEST_SOURCE

bash VERIFY_U2_36_FINAL_PRE_OTG_SOURCE.sh
bash BUILD_U2_36_STABLE_WSL.sh
```

## Instalación en SD

Ajustar `F` si la SD tiene otra letra.

```bash
cp -f dist/lgpt_libretro.so "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_43_TEST.so"
bash INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh F "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_43_TEST.so"
```

## Prueba mínima en SD

1. Abrir solo mediante `roms/lgpt/start.lgpt`.
2. Abrir un proyecto con chops guardados.
3. En Phrase verificar columnas: `Note | Vol | Inst | Cmd1 | Param1 | Cmd2 | Param2`.
4. Asignar chops con `R2+A`.
5. Ajustar volumen por step en la columna `Vol`.
6. Pre-escuchar con `B`.
7. Doble `A` sobre un chop asignado debe abrir Pitch/Envelope del chop.
8. `R1+A` debe hacer solo; `R1+A` otra vez debe limpiar solo/mute.
9. `R1+X` debe hacer undo/redo si hay snapshot de sample/chop.
10. Guardar, reiniciar, cargar proyecto y confirmar que los valores `Vol` persisten.

## Notas de compatibilidad

- Los proyectos anteriores no contienen `VOLUMES`; al cargarlos se usa `V--` en todas las filas.
- Los proyectos guardados con U2.43 tendrán bloque `VOLUMES`; builds anteriores pueden ignorarlo si su parser conserva tolerancia XML, pero no se recomienda volver a U2.42 tras guardar proyectos críticos sin backup.
