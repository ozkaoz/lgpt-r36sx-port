# F7 - Deuda tecnica

Tramo del refactor `refactor/bacon-1.2.1-preserve` (golden Bacon 1.2.1).
Este es el primer tramo que altera el binario DELIBERADAMENTE: elimina
codigo muerto y variables sin uso, de modo que el sha del core cambia de
`7709b665` a `ea7a80e4` (nuevo golden del refactor).

## Que se hizo

### 1. TreeFrogWindowsSpscTransport eliminado (codigo muerto)

`source/sources/Adapters/TREEFROG/Audio/TreeFrogWindowsSpscTransport.{cpp,h}`
tenia 0 callers vivos: el unico consumidor historico (TreeFrogUac2Bridge)
se migro a ABI por fifos en H38 y el unico resto que lo menciona es un
`.bak` (no compila).  Se elimino:

- Los dos ficheros (git rm).
- La linea `TreeFrogWindowsSpscTransport.o` del `Makefile.TREEFROG`.

Verificacion: `nm -D` del core nuevo no exporta ningun simbolo SPSC
(antes habia 8: BuildMarker, MixMonitorStereo44100, SetGainPercent,
SetMonitorEnabled, ShouldMuteLocal, Start, Stop, Submit).

### 2. Variables sin uso en el daemon r36s_u2523_usb_audio_io.c

Las cinco variables denunciadas por -Wall/-Wextra eran contadores de
diagnostico muertos: se escribian en el bucle pero nunca se leian ni se
reportaban (el reporte BRIDGE_PROGRESS usa starvation_events,
source_silence_periods, etc., no estas):

- `producer_burst_frames` (1709): nunca usada.
- `starvation_silence_periods` (1755): seteada, nunca leida.
- `latency_trimmed_samples` / `latency_trim_events` (1762/1763): nunca
  usadas.
- `starvation_since_ms` (1769): seteada en 6 puntos, nunca leida.

Se eliminaron las declaraciones y las 6 asignaciones.  Sin cambio de
comportamiento (ninguna influia en flujo ni en ningun reporte).

Quedan 2 warnings preexistentes fuera del alcance del roadmap (linea 534
`ring_drop_oldest_samples` no usada y linea 70 `RUNTIME_MIRROR_DIR` no
usada); se documentan como deuda pendiente.

### 3. Utilidades duplicadas: verificadas, ya unificadas

- `hex2char`: unica definicion en `Application/Utils/char.h`.
- `mixVULevel`: unica definicion en `Application/Mixer/FxPages.h`.
- `MakeCenteredMenuLayout`/`MenuLayout`: unicas en
  `Application/UI/Views/BaseClasses/UiDraw.h`.

No quedan duplicados; los tramos F3/F4 ya habian centralizado los usos.

## Evidencia

- Audit `AUDIT_CLEAN_MAIN_U2523_OK` (el daemon se valida en audit.sh con
  `gcc -Wall -Wextra -fsyntax-only` y pasa).
- Core MIPS nuevo golden `ea7a80e4` (simbolos SPSC: 0) desplegado en SD
  == build; daemon nuevo sha `4be71632` desplegado en SD
  (`cubegm/cores/` y `lgpt/otg/bin/`) == build.
- Gate diag `NO_DIAGNOSTICS_OUTSIDE_DEVICE`.
- Backup `LGPT_BEFORE_U2523_20260813_231021`.
- Commit: 73611c6.