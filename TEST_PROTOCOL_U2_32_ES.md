# Protocolo de prueba U2.32

## Consola

Después de aplicar U2.32, verificar:

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
CPP="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
INST="$SRC/sources/Application/Views/InstrumentView.cpp"
IMP="$SRC/sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp"
CORE="$SRC/dist/lgpt_libretro.so"

grep -n "TREEFROG_U2_32_LISTEN_MENU_RESTORE_STABLE" "$CPP" "$INST" "$IMP"
grep -n "Graphical Chopper U2.32" "$CPP"
grep -n "PITCH/ENV U2.32" "$CPP"
grep -n "buildListenPreviewWav" "$IMP"
grep -n "__u2_listen_preview.wav" "$IMP"
! grep -n "previewCurrentSampleInstrument\|plainBPreview\|__u2_instrument_preview" "$INST"

ls -lh "$CORE"
file "$CORE"
sha256sum "$CORE"
```

## Hardware R36S

1. Arrancar LGPT.
2. Cargar proyecto validado U2.31.
3. Entrar a Chopper y confirmar `Graphical Chopper U2.32`.
4. Entrar a `L1+R1` y confirmar `PITCH/ENV U2.32`.
5. Confirmar que preview `B`, stop `L2+B`, Scope Sample y Scope Chop siguen funcionando dentro de Pitch/Env.
6. Salir a Instrument.
7. Pulsar `B` sobre el instrumento/sample. Resultado esperado: no debe iniciar preescucha.
8. Pulsar `L2+B`. Resultado esperado: detiene si había una preescucha activa; no debe iniciar audio.
9. Pulsar `A` sobre el campo `sample` para abrir `Listen / Import / Exit`.
10. Seleccionar `Listen`.
11. Pulsar `A`. Resultado esperado: debe sonar la preescucha del WAV seleccionado.
12. Pulsar `L2+B`. Resultado esperado: debe detener la preescucha.
13. Seleccionar `Import` y pulsar `A`. Resultado esperado: la importación sigue funcionando.

Si el punto 11 falla, revisar si se crea `samples:__u2_listen_preview.wav` en la carpeta de samples del proyecto/SD. Si el archivo existe pero no suena, el fallo queda en streamer; si no existe, el fallo queda en apertura/render del WAV en `ImportSampleDialog`.
