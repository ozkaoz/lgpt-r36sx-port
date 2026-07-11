# Protocolo de prueba U2.27

## Consola

Verificar marcadores:

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
CPP="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
HDR="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.h"
INST="$SRC/sources/Application/Views/InstrumentView.cpp"
CORE="$SRC/dist/lgpt_libretro.so"

grep -n "TREEFROG_U2_27_CHOPPER_PREVIEW_STOP_CHOP_UI_INSTRUMENT_FIX" "$CPP"
grep -n "Graphical Chopper U2.27" "$CPP"
grep -n "PITCH/ENV U2.27" "$CPP"
grep -n "Stop preview" "$CPP"
grep -n "Pitch chop %02d/%02d" "$CPP"
grep -n "refreshCurrentInstrumentAfterSampleEdit" "$CPP" "$HDR"
grep -n "onInstrumentChange();" "$INST"
ls -lh "$CORE"
file "$CORE"
sha256sum "$CORE"
```

## Hardware R36S

1. Confirmar cabecera: `Graphical Chopper U2.27`.
2. Confirmar que CROP sigue mostrando progreso: `SELECT`, `R1+A`, `L2+Y`, `R1+X`.
3. Entrar a `L1+R1`; debe verse `PITCH/ENV U2.27`, sin textos duplicados abajo.
4. Probar preview modificada: `Pitch +2`, `B`; debe sonar sin aplicar.
5. Probar detener preview: mientras suena, pulsar `L2+B`; debe detenerse.
6. Cambiar `Scope` a `Chop`; usar `R2+LEFT/RIGHT`; debe cambiar `Chop:xx/yy` dentro del panel.
7. Aplicar con `A`; salir con `L1+R1`; volver a Instrument y probar preescucha del sample desde Instrument. Debe sonar.
8. Repetir con `Attack`, `Sustain`, `Release` modificados.
