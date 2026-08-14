# REFACTOR_ROADMAP_ES

Plan por fases del refactor interno del port LGPT R36SX sobre
`refactor/bacon-1.2.1-preserve`. Precedido por
`docs/PORT_ARCHITECTURE_BASELINE_ES.md` (mapa + deuda + gate).

Regla de oro: preservar primero (codigo dorado `golden-bacon-1.2.1`).
Cada fase: commit pequeno, `scripts/audit.sh` verde; fases con audio/input
terminan ademas con validacion en consola real.

## F1 - Politica de input formal (Application/UI/Input)
- Catalogo de ActionId: Physical Input -> ChordResolver -> Semantic Action
  -> ContextPolicy -> Controller (aplica al view actual en la F1).
- InputManager/PhysicalInputAdapter sobre las mascaras EPBM_*; ChordResolver
  determina table-driven los acordes vigentes; ContextPolicy decide que
  acciones existen por vista/modal.
- Tests host de resolucion (acordes puros, prioridades, latcheo de hombros);
  los views se migran a las acciones semanticas sin cambiar mascaras enviadas.
- Evidencia: misma accion por cada mascara/acorde que hoy (catalogo de
  bindings documentado lado a lado).
- ESTADO: F1a cerrado (commit 8931f65: capa UI/Input + catalogo dorado
  transcrito del golden, 238 checks OK). F1b Chopper cerrado (commit c836118:
  ProcessButtonMask despacha via ChordResolver con contexto por modo; build
  MIPS OK; consola: MAIN/TRIM/PITCH OK; crash/freeze one-shot del primer
  edit destructivo en frio NO es regresion F1b: mismos callsites que el
  golden, solo el 1er intento tras arranque, sospecha de contencion
  daemon/SD en frio; A/B contra golden pendiente de confirmar). F1b Mixer
  cerrado (commit a820eac: catalogo CTX_MIXER/CTX_MIXER_FX ampliado con
  SELECT/R1(B>A>UP>START)/R2/L2/L1+A y filas FX single-fire; enumerador
  exhaustivo mixer_golden_enum: 32768 combos, 0 mismatches contra el modelo
  golden de MixerView.cpp:534-697; gateway masterSelected_ verificado
  (1538 mascaras); wiring de processNormalButtonMask despachado via
  ChordResolver con colas multi-fire; build MIPS OK; auditoria
  AUDIT_CLEAN_MAIN_U2523_OK; consola: protocolo 7 pasos MIX/FX todo OK).
  F1b AppWindow cerrado (commit 023eb1d: help/audio resueltos contra
  CTX_GLOBAL via ChordResolver_Matches/ChordAbsent; la regla V16
  L1+R1+X con historia de hombros y los latches siguen en el adapter
  como documenta la tabla; build MIPS OK; auditoria OK; core 6d87c7ba
  en SD; validacion en consola OK: help contextual, audio driver, undo/redo
  y nav dentro del help correctos; constatar por vista que el help abierto
  corresponde a la pantalla mostrada). F1b completo.

## F2 - Politica de menus formal + NavigationController
- Documentar el lenguaje de menus vigente (titulo centrado + bloque
  label/value + help contextual) y el ciclo push/suspend/restore de modales.
- [IMPLEMENTADO EN CURSO] NavigationController (capa pura
  Application/UI/Navigation): stack de modales con las transiciones del
  golden (Open/Replace/Push/CloseActive/RestoreSuspended + NavModal con
  OnFocus/OnSuspend/OnRestore/IsFinished). View conserva la API publica y
  los callbacks tipados (ModalViewCallback) como glue; el stack es una sola
  fuente. Sin cambios de orden de dibujado ni de exclusividad de prensas.
- [IMPLEMENTADO] Test host del stack (tests/host/navigation_host_test.cpp,
  tests/run_host_navigation.sh): push/suspend/restore/pop, machacado de
  suspendido con stack lleno, finalizacion del activo. Verde.
- [PENDIENTE VALIDACION CONSOLA] Help por vista: constatar que el menu help
  se abre en la seccion correspondiente a la pantalla en foco (HelpRegistry
  por ViewType; AppWindow elige el modal superior si existe). Primera
  pasada en consola: MIXER abria en CHOPPER (desajuste ordinal
  kSections_/ViewType, corregido en 0722cb2); TABLE mostraba INSTRUMENT y
  el submodo Pitch del chopper mostraba CHOPPER (afinado en 69e9f01:
  VT_TABLE/VT_TABLE2->TABLE, VT_CHOPPITCH->CHOP PITCH segun pitchMode_).
  [VALIDADO EN CONSOLA salvo recheck del afinado]
- [IMPLEMENTADO] Correccion del crash de consola "R1 en HelpOverlay"
  (0722cb2): NavigationController trabaja con NavModal* y ModalView
  (View + NavModal) tiene el NavModal a un offset de subobjeto; los casts
  C-style en View.cpp/View.h devolvian un puntero desalineado por 168
  bytes y todo dispatch/dibujo de modal era UB. Se anadio NavModal::
  ModalSelf() (ajuste thunk, sin RTTI) y toda conversion pasa por el.
  Reproducido bajo ASAN/UBSAN en tests/host/help_overlay_host_test.cpp
  (escenarios song y chopper-suspendido, 19 prensas R1) e integrado en
  scripts/audit.sh. Core 505c49ae desplegado en SD (backup
  .pre_f2_helpfix_20260813). [PENDIENTE VALIDACION CONSOLA]

## F3 - Division de clases grandes (Chopper, Mixer, Phrase)
- SampleChopperModal: extraer estado de operacion, dibujo de panel y logica
  de edicion a componentes; MixerView: paginas FX parametrizadas + servicios
  (MixerService); PhraseView: logica de grid/edicion separada del dibujo.
- Sin cambios en el orden de dibujado ni en los mensajes de estado.
- Evidencia: tests estaticos de existencia de metodos/simbolos + audit.
- [IMPLEMENTADO 4db54bf] F3-1 Chopper: estado de cortes (boundaries,
  count, seleccion) extraido a ChopModel (header-only, algoritmos golden
  identicos: bubble sort, remove+reinit, split con cierre last, clamps de
  nudge, kMaxBoundaries=101). La vista delega y conserva mensajes,
  preview, historia, cursor y dibujo. Evidencia: host test de equivalencia
  golden contra el algoritmo original (tests/host/chop_model_host_test.cpp
  + tests/run_host_chop_model.sh) + test estatico de baseline
  (tests/test_f3_chopper_baseline.py: API publica, strings de estado,
  miembros raw fuera del header) + audit. Core 402cba10 desplegado
  (backup .pre_f3_1_20260813). [PENDIENTE VALIDACION CONSOLA]
- [VALIDADO EN CONSOLA] F3-1: chop/split/undo-redo OK. Incidente unico
  (crash L1+R1 en chopper, no reproducible tras reentrar): sin traza en
  /mnt/g/LGPT_OTG_LOGS (crash duro); causa raiz en el orden del pitch
  preview: `previewPitchSetting` reescribia el WAV compartido
  `samples:__u2_pitch_env_preview.wav` (ReplaceBuffer) mientras el stream
  anterior del mismo archivo podia seguir activo en el hilo de audio (el
  streamer mantiene su propio WavFile abierto sobre ese path); el Stop()+
  Sleep(80) iba DESPUES del rewrite, dejando una ventana de truncado en
  lectura -> un solo crash de timing. Fix 6114766 (U2.52.0): Stop+Sleep
  antes del rewrite (mismo stream audible). Core f72e346f desplegado
  (backup .pre_u2520_stream_order_20260813). [PENDIENTE VALIDACION CONSOLA]
- [IMPLEMENTADO 858be5c] F3-2: SampleEditHistory (stacks undo/redo con
  capture/restore por el dueno via PeekUndo/PeekRedo) y PitchEnvelopeTool
  (parametros pitch/env + buffer builder) extraidos como capas puras
  header-only; la vista delega (pitchEnvTool_/editHistory_) y conserva
  mensajes y gancho de destructivos. Evidencia: tests host de equivalencia
  golden bajo ASAN/UBSAN (tests/host/edit_history_host_test.cpp + 
  pitch_tool_host_test.cpp) y test estatico de baseline
  (tests/test_f3_2_baseline.py) + test_u2510_global_chop_history
  actualizado a la capa + audit. Build MIPS del core OK sin diagnosticos
  (el gate estricto solo reporta warnings pre-existentes de device/*.c,
  deuda F7). [VALIDADO EN CONSOLA: undo/redo, pitch/env y destructivos OK
  dentro de la validacion del build F3-3 completo]
- [IMPLEMENTADO 1a1e641] F3-3a: PreviewService (rango de preview y
  play/trim/playback-range golden) + ChopperView (geometria zoom/cursor/
  waveform golden) extraidos del SampleChopperModal como capas puras
  header-only. La vista conserva audio (Player::*Streaming), mensajes de
  estado, overlay (g_chopper*) y API publica; los algoritmos golden viven
  en las capas. Deactivate() anadido por el golden del pitch preview
  (solo apaga active, conserva start/end). Evidencia: host tests de
  equivalencia golden bajo ASAN/UBSAN (tests/host/preview_service_
  host_test.cpp 740 checks + chopper_view_host_test.cpp 452503 checks),
  baseline estatico (tests/test_f3_3a_baseline.py) + audit. [VALIDADO EN
  CONSOLA]
- [IMPLEMENTADO 096fbef] F3-3b: el dibujo textual del chopper (grilla
  40x30 de celdas con invert/color) se extrae a ChopperView (capa pura):
  ChopperGrid (Clear/SetText con recorte/SetInvert), DrawTopBar, DrawFrame
  (marco solid celda CD_BORDER, sin ASCII), DrawEmptyWaveformText,
  DrawControls (main/trim), DrawPitchHints y los compositors golden
  (pitch header/labels/valores, sample info, name/frame, operation
  status/percent). La vista conserva posiciones reales (MenuLayout),
  colores, titulo PITCH/ENV, audio, estado y overlay; solo drena la grilla
  (drainChopperGrid: SetColor/DrawString reales, 1 celda por llamada con
  misma semantica de invert/color que el golden). Evidencia: host test de
  equivalencia golden (tests/host/chopper_draw_host_test.cpp, 5222 checks
  ASAN/UBSAN, oraculos snprintf y grids campo a campo), baseline estatico
  (tests/test_f3_3b_baseline.py: literales sin duplicar en la vista),
  test_rc3 actualizados al layout por capas (placeholder y marco viven en
  la capa pura) + audit. Build MIPS del core OK sin diagnosticos (gate
  solo con deuda F7 pre-existente de device/*.c). [VALIDADO EN CONSOLA]
- Bug reportado (mismo tratamiento que el crash del chopper): lag del
  driver Windows USB Audio durante la prueba en consola del build F3-3
  (core 62cdc6ba desplegado en SD, backup
  LGPT_BEFORE_U2523_20260813_171423). Sintoma: lag pequeno/intermitente en
  el modo Windows UAC2 (daemon r36s_u241_usb_audio_io), no reproducible
  en modo local ni en los otros modos; el resto de la prueba (chopper
  completo, pitch/env, undo/redo, overlay de operacion, preview) OK.
  Pendiente: reproduccion con perfil Windows, log del daemon
  (/mnt/g/LGPT_OTG_LOGS) y backpressure/ASRC como primera hipotesis.
- [IMPLEMENTADO e08d6d7] F3-3c: la logica de edicion del chopper (12 flujos
  golden: initialize, add (con live cut por streaming), delete, select,
  toggleTrim, nudge start/end, crop logico U2.14, split, clearAll, cycle,
  snap a zero-cross; y helpers hasUserChops/hasActiveSliceRange/
  selectedChopStart/EndFrame) se extrae a ChopperController (capa pura
  header-only) con un Host interface para los efectos de vista
  (SetStatus/SetOperationCombo/PushLogicalUndo/SaveChopState/
  EnsureCursorVisible/PrepareWaveformPreview/PublishOverlayState/MarkDirty/
  SampleLoaded/QuerySnapBuffer/LiveStreamingPosition). Los action labels
  ("Add cut", "Merge cuts", "Move cut start/end", "Keep logical range",
  "Split sample", "Clear chops", "Snap start/end") y los mensajes golden
  viven en la capa. La vista conserva audio, destructivos, undo/redo
  fisico+logico, persistencia por sample, preview, overlay y dibujo; el
  controller se vincula por refs a chopModel_/preview_/sourceSize_/
  cursorFrame_/trimMode_/pitchMode_/splitParts_/chopsInitialized_ y el
  adapter ChopperHostAdapter (por valor en el header) traduce los efectos.
  Evidencia: host test de equivalencia golden
  (tests/host/chopper_controller_host_test.cpp, ALL OK 128 checks
  ASAN/UBSAN, oraculos con FakeHost: mensajes, estado final de boundaries/
  selected/cursor/trim/split y orden de escrituras), baseline estatico
  (tests/test_f3_3c_baseline.py: capa pura sin dependencias, strings en la
  capa y fuera de la vista, delegados one-line, adapter) + baselines
  F3-1/F3-2/F3-3a u2510 actualizados al layout por capas + audit
  (AUDIT_CLEAN_MAIN_U2523_OK). Build MIPS del core OK sin diagnosticos
  (0 warnings/errores en SampleChopperModal; gate solo con deuda F7
  pre-existente de device/*.c). Desplegado en SD (backup
  LGPT_BEFORE_U2523_20260813_174058; core en consola = build SHA
  a97a77e8). [VALIDADO EN CONSOLA]
- [IMPLEMENTADO] F3-4c: MixerMenu - menu L1+A del Mixer declarado como
  datos puros (Application/Mixer/MixerMenu.h): filas (6 MASTER / 5 TRACK),
  etiquetas en orden exacto, clamps golden (softclip 0..4, clip gain
  0..1), codificado de accion (fila master >= 2 -> pagina FX 1..4 =
  DELAY..COMP; fila track -> seccion 101..105) y hints FourCC de seccion
  (SIP_FILTMIX/SIP_CRUSH/SIP_INTERPOLATION/SIP_DRY/SIP_TABLEAUTO,
  MAKE_FOURCC puro).  MixerActionMenuModal (DrawView/ProcessButtonMask) y
  MixerActionMenuApplyCallback migrados a delegados de la capa (dibujo,
  Project y MixerService siguen en la vista).  Evidencia:
  tests/host/mixer_menu_host_test.cpp (40 checks golden con ASAN/UBSAN,
  runner tests/run_host_mixer_menu.sh en audit.sh) + test_f3_4c_baseline.py
  (capa pura sin GUI/audio, sin labels/hintIds inline en la vista).  Audit
  completo verde (AUDIT_CLEAN_MAIN_U2523_OK).  Build MIPS del core OK sin
  diagnosticos en MixerView/MixerMenu (gate solo con deuda F7
  pre-existente de device/*.c).  Desplegado en SD (backup
  LGPT_BEFORE_U2523_20260813_182741; core en consola = build SHA
  e176da18).
- [IMPLEMENTADO] F3-4d: FxNavigator - navegacion/edicion de las paginas FX
  del Mixer extraidas a capa pura (Application/Mixer/FxNavigator.h):
  estado del cursor (pagina, fila y target de edicion del MIX page
  0=VOL 1=DLY RET 2=RVB RET) con SetPage (rango + reset de fila),
  CyclePage (MIX->DELAY->REVERB->EQ->COMP->MIX), MoveRow (wrap por
  pagina), CycleEditTarget (VOL->DLY RET->RVB RET) e IdForRow (bypass
  primero); y la matematica golden de pasos EditValue/ResetValue (lineal
  fino 1 / grueso 10, filas bool-ish a paso 1, curva musical via
  fxEditCurveValue semitono/octava con floor y clamps, A+B restaura vdef).
  MixerView migrado: eliminados fxPage_/fxRow_/fxEditTarget_ del header
  (miembro FxNavigator navigator_), JumpToFxPage/cycleFxPage/fxMoveRow/
  fxEditRow/fxResetRow/fxEditCurve como delegados; el engine (fxGet/fxSet)
  y la historia undo (pushMixUndo, newValue solo en el path lineal, igual
  que golden) siguen en la vista.  Evidencia:
  tests/host/fx_navigator_host_test.cpp (49 checks golden con ASAN/UBSAN,
  runner tests/run_host_fx_navigator.sh en audit.sh) + test_f3_4d_baseline.py
  (capa pura sin GUI/audio, vista sin estado de cursor) + baselines
  phase14/phase4 actualizados al layout por capas.  Audit completo verde
  (AUDIT_CLEAN_MAIN_U2523_OK).  Build MIPS del core OK sin diagnosticos en
  MixerView/FxNavigator (gate solo con deuda F7 pre-existente de
  device/*.c).  Desplegado en SD (backup LGPT_BEFORE_U2523_20260813_184329;
  core en consola = build SHA 5a9b78a7).
- [IMPLEMENTADO] F3-4b: MixerMeters - capa pura de los medidores VU del
  Mixer (Application/Mixer/MixerMeters.h): smoothing golden
  (ataque instantaneo, release *0.6 por frame con piso 0.001, muestreo a 0
  al parar el transporte), BarLevel (mixVULevel*vol/100 clamp 0..1) y la
  metrica half-cell L/R (GeometryFor/RowStateFor: LEVEL_H 3, banda roja
  0 dB+ = 36/39, fill en pasos de 2 px, gap de 1 px).  MixerView migrado:
  miembro MixerMeters meters_, SmoothFrame en OnFrameUpdate (solo se
  muestrean los picos del Player), BarLevel en drawMeterBar, geometria en
  PostFlushDraw (la vista solo resuelve colores y escribe pixels);
  eliminados los arrays vuDisplay_/vuDisplayL_/vuDisplayR_ y el smoothing
  inline.  Evidencia: tests/host/mixer_meters_host_test.cpp (71 checks
  golden con ASAN/UBSAN, runner tests/run_host_mixer_meters.sh en audit.sh),
  test_f3_4b_baseline.py + test_ui_centered_layout actualizado.  Audit
  completo verde (AUDIT_CLEAN_MAIN_U2523_OK).  Build MIPS del core OK sin
  diagnosticos en MixerView/MixerMeters (gate solo con deuda F7
  pre-existente de device/*.c).  Desplegado en SD (backup
  LGPT_BEFORE_U2523_20260813_181756; core en consola = build SHA bfb0fa97).
- [IMPLEMENTADO] F3-4: Mixer completo (baseline en docs/F3_ARCHITECTURE_ES.md):
  FxPages, MixerMeters, MixerMenu, FxNavigator.  (MixerService es el
  servicio de audio DAW pre-existente, no una capa de este tramo.)
- [IMPLEMENTADO] F3-4a: FxPages - capa pura de las paginas FX parametrizadas
  del Mixer (Application/Mixer/FxPages.h).  Los enums FxPage/FxParamId, la
  tabla kFxParams_ (36 filas byte-identicas: DELAY 7, REVERB 7, EQ 13,
  COMP 9), y los helpers puros fxBypassId/fxCountOnPage/fxRowForId/
  fxIdForRow/fxIdOnPage/fxUsesCurve/fxEditCurveValue/mixVULevel/
  fxReturnPercent/fxReturnFromPercent se extrajeron de MixerView.{h,cpp}
  sin cambios de comportamiento; MixerView conserva su superficie publica
  como delegados one-line.  Naming: el diseno original proponia "MixerService"
  pero ese nombre ya lo usa el servicio de audio DAW
  (Application/Mixer/MixerService.h), asi que la capa se llama FxPages.
  Evidencia: tests/host/fx_pages_host_test.cpp (191 checks golden con
  ASAN/UBSAN, runner tests/run_host_fx_pages.sh en audit.sh), MixerView.cpp
  anadido al host_syntax_check, baselines FX (phase4/6/10/12/13/14/rc3/
  ui_centered) actualizados al layout por capas + test_f3_4a_baseline.py.
  Audit completo verde (AUDIT_CLEAN_MAIN_U2523_OK).  Build MIPS del core OK
  sin diagnosticos en MixerView/FxPages (gate solo con deuda F7 pre-existente
  de device/*.c).  Desplegado en SD (backup LGPT_BEFORE_U2523_20260813_180608;
  core en consola = build SHA b17d07bd).
- [IMPLEMENTADO] F3-5a: PhraseGridEdit - logica de grid/edicion de la Phrase
  extraida a capa pura (Application/Phrase/PhraseGridEdit.h, header-only
  con static const a nivel de archivo igual que FxPages para evitar ODR
  en C++03).  Constantes golden (kPhraseColCount=8, kPhraseCol{Note,Vol,
  Pitch,Instr,Cmd1,Param1,Cmd2,Param2}, kPhraseNoteLimit=119,
  kPhraseVolLimit=0x64, kPhrasePitchLimit=24, kPhraseInstrLimit=143,
  kPhraseVolFull=0x64, kPhraseStepOffsets[4][4] con N=+/-1/+/-12, V/P=+/-1,
  I=+/-1/+/-16) y toda la matematica golden: PhraseClampWrap (replica de
  View::updateData: nota wrap 119, vol clamp 0x64, pitch clamp 24, instr
  wrap 143), PhraseLimitFor/PhraseWrapFor, PhraseStepCell (paso fina 1 /
  gruesa 10, pitch en stored +-24, escala acustica con scale-snap de
  modulo normalizado [0..11] -el modulo sin normalizar del golden era UB-
  y auto-fill de nota vacia a vol 0x64 + pitch ZERO), PhrasePasteLast/
  PhrasePasteLastCommand (paran como golden cuando el campo origen es
  vacio), PhraseNormalizeRect/PhraseExtendSelection (anchor y cursor,
  extend desde cursor hasta anchor inclusive), PhraseClipboard/FillClipboard
  (layout identical al struct original, captura el valor bajo el cursor y
  avanza ---- por filas), PhraseCutSelectionCells, PhrasePasteClipboard
  (paste selectivo: comandos solo si el destino de cmd1 no esta vacio,
  params siempre, guard I_CMD_NONE) y PhraseInterpolateSelection (nota
  lineal sin clamp, pitch stored con clamp +/-24, params lineales,
  skip de filas sin nota, estados PINTERP_OK/SKIPPED/NO_NOTE_INFO).
  PhraseView migrado: offsets_ estatico y struct clipboard eliminados del
  header (miembro PhraseClipboard clipboard_), updateCursorValue pasa
  cols 0-3 por PhraseStepCell y conserva lastNote_/lastVol_/lastPitch_/
  lastInstr_ y el hook chop, pasteLast/extendSelection/getSelectionRect/
  fillClipboardData/cutSelection/pasteClipboard/interpolateSelection son
  delegados one-line.  Se quedan en la vista (acoplado, igual que golden):
  audition/Player, cmdEditField_ hex (cols 5/7), CommandSelector, chop,
  mute/solo (UIController), navegacion/updateCursor y DrawView.  Evidencia:
  tests/host/phrase_grid_edit_host_test.cpp (116 checks golden con
  ASAN/UBSAN incluyendo Scale.cpp, runner tests/run_host_phrase_grid_edit.sh
  en audit.sh) + test_f3_5a_baseline.py (capa pura sin GUI/audio/Player/
  viewData_/GUIRect, vista sin offsets_, delegados y math movido fuera).
  Baselines existentes (ui_centered, fx_phase17) pasan sin cambios.
  Audit completo verde (AUDIT_CLEAN_MAIN_U2523_OK).  Build MIPS del core OK
  sin diagnosticos en PhraseView/PhraseGridEdit (gate solo con deuda F7
  pre-existente de device/*.c).  Desplegado en SD (backup
  LGPT_BEFORE_U2523_20260813_190916; core en consola = build SHA
  9892f230).
- [IMPLEMENTADO] F3-5b: PhraseUndo - historia snapshot/restore de la
  Phrase extraida a capa pura (Application/Phrase/PhraseUndo.h): el struct
  PhraseUndoSnapshot (layout identico al PhraseEdit original: 10 arrays de
  16 pasos + currentPhrase), PhraseUndoCapture (snapshot del bloque
  16*currentPhrase del modelo), PhraseUndoSnapshotEqual (dedup V9),
  PhraseUndoPush (shift + cap kPhraseUndoHistorySize=16 + clear redo +
  guard de reentrada), PhraseUndoRestore (V8: publica el indice de frase
  editada sin mover el cursor) y PhraseUndoStep (paso undo/redo compartido
  que desapila el tope de FROM, lo inserta en TO y restaura el modelo;
  devuelve true con pila vacia, igual que golden).  PhraseView migrado:
  pushPhraseUndo es un delegado de PhraseUndoPush con el guard
  g_phraseUndoPushActive (politica de reentrada de la vista), GlobalUndo/
  GlobalRedo delegan en PhraseUndoStep y conservan el efecto local golden
  (reclaman el combo L1+X/R1+X con true, viewData_->phraseCurPos_ = row_,
  isDirty_); los arrays de historia pasan a ser PhraseUndoSnapshot con
  kPhraseHistorySize como alias publico de kPhraseUndoHistorySize y
  typedef PhraseEdit para compatibilidad.  Se queda en la vista la politica
  de push (que acciones capturan el estado pre-edit: updateCursorValue,
  pasteLast, cut/paste clipboard, interpolate, chop, command selector,
  VM_NEW A, VM_CLONE L+A).  Evidencia:
  tests/host/phrase_undo_host_test.cpp (7 suites con ASAN/UBSAN via
  stub de System + Phrase.cpp, runner tests/run_host_phrase_undo.sh en
  audit.sh) + test_f3_5b_baseline.py (capa pura sin GUI/audio/ViewData,
  mecanica fuera de la vista, politica y efecto local conservados).
  Baselines existentes (F3-5a, ui_centered, fx_phase17, rc3) pasan sin
  cambios.  Audit completo verde (AUDIT_CLEAN_MAIN_U2523_OK).  Build MIPS
  del core OK sin diagnosticos en PhraseView/PhraseUndo (gate solo con
  deuda F7 pre-existente de device/*.c).  Desplegado en SD (backup
  LGPT_BEFORE_U2523_20260813_213535; core en consola = build SHA
  7709b665).
- [IMPLEMENTADO] F3-5: Phrase completo (baseline en
  docs/F3_ARCHITECTURE_ES.md): F3-5a PhraseGridEdit (grid/edicion puro:
  pasos N/V/P/I, pasteLast, seleccion, clipboard, interpolacion) + F3-5b
  PhraseUndo (historia snapshot/restore golden).  Queda en PhraseView el
  dibujo, el hex editor de comandos, chop, audition/Player, navegacion y
  la politica de push de la historia.

## F4 - Audio backends extensibles (Application/Audio)
- Interfaz AudioBackend (open/start/caps/stream/write), AudioRouter con los
  modos actuales; AudioEngine sobre el puente UAC2 manteniendo batching,
  ASRC y fast-apply con paridad de FIFO intactos.
- Los daemons device/*.c NO cambian en esta fase (contrato 48k/2ch/S16_LE).
- Evidencia: caminos de datos identicos; validacion en consola (samples,
  sampler, USB out, stop/start frecuente).
- **F4a [IMPLEMENTADO]** (ver docs/F4_ARCHITECTURE_ES.md): tabla
  declarativa de los 6 modos del driver como datos puros
  `Application/Audio/AudioDriverModeTable.h` (semilla de datos del futuro
  AudioRouter).  El bridge delega (mode_name/mode_desc/mode_token/
  policy_token/branch_name_for_mode/selectable_mode/mode_has_out/mode_has_in
  + GetDriverModeCount) con paridad byte-identica y fallback LOCAL_CONSOLE;
  las capacidades de direccion del sampler se pasan por parametro.  Host
  test ASAN/UBSAN 68 checks `AUDIO_DRIVER_MODES_HOST_ALL_OK`; baseline
  `F4A_BASELINE_OK` (incluye contrato de valores del enum U241_*).  Audit
  completo `AUDIT_CLEAN_MAIN_U2523_OK`; build `7709b665` desplegado en SD
  (== build); backup `LGPT_BEFORE_U2523_20260813_220015`.

## F5 - Politica de storage/SD estricta (Service/Storage)
- StorageService clasifica: Volatile (cache, tmp), Persistent (config.xml,
  last_project) y Diagnostic (LGPT_OTG_LOGS). Inventario completo de
  reads/writes actuales; rutas derivadas de la raiz TreeFrogSystem.
- Los daemons y el core comparten la misma politica; nada nuevo escribe en la
  SD fuera de los tres tipos.
- Evidencia: inventario {ruta, tipo, quien, cuando} documentado y auditado.

## F6 - Arquitectura objetivo (estructura de carpetas)
- Aplicar: Application/Audio, Application/UI (Input, Menus, Navigation,
  Views), Services (Storage, Help, Sound), Platform (Adapters TREEFROG/Unix,
  device daemons). Movimientos = movimientos puros + include fixes, en commits
  separados por area; nunca mezclar con cambios de comportamiento.

## F7 - Deuda tecnica
- Borrar TreeFrogWindowsSpscTransport.cpp (muerto, 0 callers) y sus menciones.
- Warnings de host: variables sin uso en r36s_u2523_usb_audio_io.c (1709/1755)
  marcadas o limpiadas con justificacion.
- Unificar utilidades duplicadas (hex2char, mixVULevel, layouts) en UiDraw.

## F8 - Harness de regresion funcional en host
- Core compilable en x86_64 (make X64) con salida a null/SDL; runner que
  inyecta inputs EPBM_* y compara secuencias de acciones (ActionId) contra el
  comportamiento catalogado; base de datos de escenarios por vista.

## F9 - Riesgos y optimizaciones
- Documentar riesgos en el camino critico de audio (retro_run -> FIFO -> ASRC
  -> daemon) y los limites actuales (PPM 1200, hold floor 2400, reenum 8).
- Optimizaciones solo donde el analisis muestre ganancia medible y no cambien
  el comportamiento observable (sin tocar tiempos de arranque/fast-apply).

## F10 - Evidencia de preservacion + paquete
- Informe final: diff de comportamiento por area (input, menus, audio, SD),
  comparativa de la politicas catalogadas vs. realidad en consola, y release
  de regresion documentado (incluye pruebas de campo).