# Protocolo de prueba U2.26

## Consola

Validar marcadores:

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
CPP="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
HDR="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.h"
LIB="$SRC/sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp"
CORE="$SRC/dist/lgpt_libretro.so"

grep -n "TREEFROG_U2_26_CHOPPER_UI_PROGRESS_PREVIEW_FIX" "$CPP"
grep -n "Graphical Chopper U2.26" "$CPP"
grep -n "PITCH / ENVELOPE U2.26" "$CPP"
grep -n "TreeFrogForceVideoRefresh" "$CPP" "$LIB"
grep -n "selectPitchTargetSample" "$CPP" "$HDR"
grep -n "WavFile::Open(sourcePath.GetPath" "$CPP"
grep -n "StartStreamingRangeAt(path, 0" "$CPP"
file "$CORE"
sha256sum "$CORE"
```

## R36S

1. Arrancar LGPT y cargar proyecto validado.
2. Entrar a Chopper.
3. Confirmar que CHOP básico sigue OK: `A`, `B`, `Y`, `R2+LEFT/RIGHT`.
4. Entrar a CROP con `SELECT`.
5. Ejecutar `R1+A`, `L2+Y`, `R1+X`.
6. Confirmar que ahora aparecen porcentajes intermedios antes del mensaje final.
7. Entrar a Pitch/Envelope con `L1+R1`.
8. Confirmar panel compacto `PITCH / ENVELOPE U2.26` sin superposición grave.
9. Probar preview modificada con `B`:
   - `Scope Sample`, `Pitch +2`, `Attack 0`, `Sustain 100`, `Release 0`.
   - `Scope Sample`, `Pitch 0`, `Attack 20`, `Sustain 100`, `Release 40`.
   - `Scope Chop`, `Pitch +2`, `Attack 20`, `Sustain 100`, `Release 40`.
10. Confirmar que `A` aplica igual que U2.25.
11. Confirmar que `L1+X` hace undo/redo después de aplicar.
12. Confirmar que `SELECT` no sale de Pitch/Envelope.
13. Seleccionar parámetro `Sample` y cambiar con `LEFT/RIGHT`; debe cambiar el sample objetivo sin salir del menú.
