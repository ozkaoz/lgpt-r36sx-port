# LGPT R36S TreeFrog U2.33 — Listen Preview Audio Fix Stable Candidate

## Objetivo

U2.32 restauró la semántica correcta del menú Instrument: `B` no debe preescuchar, `A` sobre `Listen` debe preescuchar, y `L2+B` debe detener. En hardware, U2.32 entraba correctamente a la acción Listen y mostraba `Listen preview`, pero no sonaba.

U2.33 corrige solo esa ruta de audio:

- `A` sobre `Listen` ya no muestra mensaje de éxito.
- `A` sobre `Listen` genera `samples:__u2_listen_preview.wav` y lo reproduce con `StartStreamingRangeAt()`, igual que la preview funcional de Pitch/Envelope.
- Si el WAV temporal no puede generarse, se usa fallback `StartStreamingRangeAt()` sobre el WAV seleccionado.
- `B` en Instrument sigue sin iniciar preescucha.
- `L2+B` sigue deteniendo preescucha.

## Archivos tocados

- `sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp`
- `sources/Application/Views/ModalDialogs/ImportSampleDialog.h`
- `sources/Application/Views/ModalDialogs/SampleChopperModal.cpp` solo cambia rótulos a U2.33.
- `sources/Application/Views/InstrumentView.cpp` solo actualiza marcador U2.33; mantiene `B` sin preview.

## No incluido

No se implementan chops renderizados/exportables. No se toca Phrase. No se cambia el campo `slices`. Esta versión cierra la estabilización de UI/audio antes de la siguiente fase.
