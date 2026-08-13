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

## F2 - Politica de menus formal + NavigationController
- Documentar el lenguaje de menus vigente (titulo centrado + bloque
  label/value + help contextual) y el ciclo push/suspend/restore de modales.
- NavigationController: stack de modales/views, transiciones identicas
  (DoModal, PushModal, HelpOverlay), sin cambios de orden de dibujado.
- Test host del stack (push/suspend/restore/pop; exclusividad de prensas).

## F3 - Division de clases grandes (Chopper, Mixer, Phrase)
- SampleChopperModal: extraer estado de operacion, dibujo de panel y logica
  de edicion a componentes; MixerView: paginas FX parametrizadas + servicios
  (MixerService); PhraseView: logica de grid/edicion separada del dibujo.
- Sin cambios en el orden de dibujado ni en los mensajes de estado.
- Evidencia: tests estaticos de existencia de metodos/simbolos + audit.

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