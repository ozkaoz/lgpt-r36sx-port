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
- [PENDIENTE] F3-2: SampleEditHistory (stacks con capture/restore por
  callbacks) y PitchEnvelopeTool (parametros pitch/env + buffer builder).
- [PENDIENTE] F3-3: ChopperView (dibujo) + PreviewService; queda
  ChopperController con la logica.
- [PENDIENTE] F3-4: Mixer (baseline en docs/F3_ARCHITECTURE_ES.md):
  MixerService, MixerMeters, FxNavigator.
- [PENDIENTE] F3-5: Phrase (baseline en docs/F3_ARCHITECTURE_ES.md):
  grid/edicion separados del dibujo.

## F4 - Audio backends extensibles (Application/Audio)
- Interfaz AudioBackend (open/start/caps/stream/write), AudioRouter con los
  modos actuales; AudioEngine sobre el puente UAC2 manteniendo batching,
  ASRC y fast-apply con paridad de FIFO intactos.
- Los daemons device/*.c NO cambian en esta fase (contrato 48k/2ch/S16_LE).
- Evidencia: caminos de datos identicos; validacion en consola (samples,
  sampler, USB out, stop/start frecuente).

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