# F3 - Division de clases grandes (Chopper, Mixer, Phrase)

(REFACTOR_ROADMAP_ES.md, seccion F3. Metodo: F2 misma disciplina, fases
pequenas `tests -> abstraccion -> adapter -> migracion -> cleanup`, cada
commit equiparable contra el golden Bacon 1.2.1.)

## Mapa de arquitectura original (baseline)

### 1. SampleChopperModal (ModalDialogs) — 3455 lineas .cpp / 261 .h

Monolito que mezcla: estado de operacion, dibujo de panel, edicion de
chops, pitch/env, preview/playback, historia undo/redo, export a phrase,
asignacion a instrumentos y overlay de progreso.

- Estado (header): sample/source info (sampleIndex_, sourceSize_,
  sourceChannels_, sourceRate_, cursorFrame_, viewStartFrame_,
  zoomPercent_, hasWaveform_, sampleName_, samplePath_), preview
  (previewActive_, previewStartFrame_, previewEndFrame_,
  playbackTriggered_), progreso (operationActive_, operationPercent_,
  operationMessage_, operationComboLabel_), chops (chopsInitialized_,
  trimMode_, selectedChop_, boundaryCount_, boundaries_[101]), historia
  (undoHistory_[24], redoHistory_[24], counts de ambas), pitch
  (pitchMode_, pitchSemitones_, pitchEditParam_, pitchAttackMs_,
  pitchSustainPercent_, pitchReleaseMs_, pitchScope_), misc
  (suspended_, statusMessage_, splitParts_, minColumn_[], maxColumn_[]).
- Grupos de metodos:
  - Geometria zoom/cursor (clampViewStart, centerViewOnCursor,
    ensureCursorVisible, nudgeCursorPixels, nudgeZoomPercent, frameToPixel,
    pixelToFrame, getCursorFrame, getViewFrameCount, getZoomFactor).
  - Chops (initializeChopsIfNeeded, addChopAtCursor, deleteSelectedChop,
    sortBoundaries, findBoundaryIndex, selectChop, hasUserChops,
    hasActiveSliceRange, selectedChopStartFrame, selectedChopEndFrame,
    nudgeSelectedStart, nudgeSelectedEnd, cropToSelectedRange,
    toggleTrimMode, splitSampleIntoEqualParts, cycleSplitParts,
    snapSelectedBoundaryToZeroCross, clearAllChops, deleteSelectedChop).
  - Historia logica (captureLogicalState, restoreLogicalState,
    pushLogicalUndo, undoLogicalEdit, redoLogicalEdit, clearLogicalRedo,
    clearLogicalHistory, undoLastChopperEdit, redoLastChopperEdit) +
    destructiva (destructiveCropToSelectedRange,
    destructiveDeleteSelectedRange, restoreLastDestructiveEdit,
    destructivePitchSample, normalizeSample).
  - Pitch/Env (buildPitchEnvelopeBufferFromRange,
    preparePitchEnvelopePreviewBuffer, applyEnvelopeToBuffer,
    drawPitchScreen, selectPitchEditParam, nudgePitchEnvelopeValue,
    nudgePitchSemitones, selectPitchTargetSample, togglePitchMode, reset).
  - Preview (playFullSample, playFromFrame, playFrameRange,
    playSelectedChop, setPreviewPlaybackRange, clearPreviewPlaybackRange,
    stopSamplePreview, previewTrimStart, previewTrimEnd).
  - Phrase/instrumento (assignSelectedChopToPhrase, exportChopsToPhrase,
    ensureCurrentPhraseSlot, configureChopInstrument,
    refreshCurrentInstrumentAfterSampleEdit, LGPTChopper* libres).
  - Dibujo (drawPitchScreen, drawOperationOverlay, drawTopBar, drawFrame,
    drawSampleInfo, drawEmptyWaveformText, drawControls, drawStringAbs,
    clearTextScreen, showOperationProgress, clearOperationProgress,
    publishOverlayState, clearOverlayState).

### 2. MixerView (Views) — 1956 lineas .cpp / 258 .h

Ver segundo tramo F3 (no se toca en este commit; inventario completo alla).

### 3. PhraseView (Views) — 2150 lineas .cpp / 145 .h

Ver tercer tramo F3 (no se toca en este commit; inventario completo alla).

## Invariantes golden que todo movimiento debe preservar (Chopper)

- Mensajes de estado EXACTOS (verificados en consola Bacon 1.2.1):
  "Cannot chop at edge", "Chop already exists", "Max 100 chops reached",
  "No sample to chop", "Deleted cut", "Merge cuts", "No chop to delete",
  "Cannot delete edge", "Selected chop %02d", "Move cut start",
  "Move cut end", "Adjusted chop start", "Adjusted chop end",
  "Split sample in %d parts", "No cuts (L1+B to split again)",
  "Zero-cross %s %d", "Already at zero-cross", "Live chop %02d at %d",
  "Chop %02d at %d", "No user chops".
- Orden de estado el dia de cada edit: pushLogicalUndo (captura ANTES de
  mutar) -> mutacion del modelo -> sort -> seleccion/cursor ->
  saveChopStateForCurrentSample -> ensureCursorVisible ->
  prepareWaveformPreview -> publishOverlayState -> setStatus.
- Constantes: MAX_CHOP_BOUNDARIES = 101, MAX_LOGICAL_HISTORY = 24.
- Algoritmos exactos: sortBoundaries (bubble), findBoundaryIndex (lineal),
  RemoveChop (shift + reinit si count<2), SplitIntoEqualParts
  (partes 2..32, step = size/parts, cierre con last = size-1 y overrides
  de cap), clamps de nudge start/end (vecino +1 / vecino -1, bordes 0 y
  size-1).
- API publica de SampleChopperModal intacta (ProcessButtonMask, DrawView,
  OnFocus, OnPlayerUpdate, GetViewType, OnSuspend/OnRestore,
  undo/redoLastChopperEdit, addChopAtCursor, ...).

## Plan por tramos

- F3-1 [ESTE COMMIT] Chopper: extraer `ChopModel` (estado de cortes puro:
  boundaries/count/selected + algoritmos golden). La vista conserva
  mensajes, preview, historia, cursor y dibujo. Evidencia: test host del
  modelo con equivalencia golden + test estatico (strings y API intactos)
  + audit.
- F3-2 Chopper: `SampleEditHistory` (stacks LogicalHistoryState con
  capture/restore por callbacks) y `PitchEnvelopeTool` (parametros
  pitch/env + buffer builder).
- F3-3 Chopper: `ChopperView` (dibujo) y `PreviewService` (playback),
  dejando `ChopperController` con la logica.
- F3-4 Mixer: inventario, `FxPages` (paginas FX parametrizadas; el diseno
  original la llamaba "MixerService" pero ese nombre ya lo ocupa el servicio
  de audio DAW existente en Application/Mixer/MixerService.h, asi que la
  capa pura se llama FxPages y vive en Application/Mixer/FxPages.h),
  `MixerMeters`, `MixerMenu` (menu L1+A declarado como datos puros) y
  `FxNavigator` (estado del cursor pagina/fila/editTarget + matematica de
  pasos).  IMPLEMENTADO en F3-4a/b/c/d.
- F3-5 Phrase: separar logica de grid/edicion del dibujo.
- Cada tramo: host test + test estatico + audit + build + deploy + commit
  antes de pasar al siguiente.

## Evidencia de este tramo

- tests/host/chop_model_host_test.cpp: equivalencia golden de
  InitRange/Append/Sort/Find/RemoveChop/ClampSelected/Split/ClearAll/
  clamps de nudge (escenarios numericos explicitos).
- tests/test_f3_baseline_chopper.py: strings de estado intactos, API
  publica intacta, miembros privados raw (`boundaries_`, `boundaryCount_`,
  `selectedChop_`) eliminados del header a favor de `chopModel_`,
  kMaxBoundaries == MAX_CHOP_BOUNDARIES.
- scripts/audit.sh ejecuta ambos + los tests previos.