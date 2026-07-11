# Protocolo de prueba U2.28

## Consola

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
CPP="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
INST="$SRC/sources/Application/Views/InstrumentView.cpp"
INSTH="$SRC/sources/Application/Views/InstrumentView.h"
CORE="$SRC/dist/lgpt_libretro.so"

grep -n "TREEFROG_U2_28_CHOPPER_PANEL_INSTRUMENT_PREVIEW_FIX" "$CPP"
grep -n "Graphical Chopper U2.28" "$CPP"
grep -n "PITCH/ENV U2.28" "$CPP"
grep -n "tf_rect(32, 52, 256, 96" "$CPP"
grep -n "UD item LR value   B preview" "$CPP"
grep -n "TREEFROG_U2_28_INSTRUMENT_B_PREVIEW_FIX" "$INST"
grep -n "previewCurrentSampleInstrument" "$INST" "$INSTH"
grep -n "StartStreamingRangeAt(path, start, end - 1)" "$INST"
ls -lh "$CORE"
file "$CORE"
sha256sum "$CORE"
```

## Hardware R36S

1. Arrancar LGPT y cargar proyecto.
2. Entrar a Chopper y verificar `Graphical Chopper U2.28`.
3. Entrar a `L1+R1` y verificar `PITCH/ENV U2.28`.
4. Confirmar que el panel es un rectángulo centrado y cerrado; no debe mostrar barras/waveform atravesando el lado derecho.
5. Confirmar que la ayuda está fuera del panel, en la zona inferior.
6. En Pitch/Env, probar `B preview` y `L2+B stop`.
7. En `Scope: Chop`, probar `R2+LEFT/RIGHT`; debe cambiar el chop objetivo y `B` debe preescuchar ese chop.
8. Aplicar un cambio con `A`, salir con `L1+R1`, volver a Instrument y pulsar `B` sobre el instrumento/sample. Debe sonar.
9. Revisar que `START` y `LOOP START` estén dentro de rango; `LOOP END` debe seguir coherente con el tamaño actual del sample.
10. Regresión rápida: CHOP normal, CROP con progreso, Phrase con `S01/S02`, persistencia `.u2chop`.
