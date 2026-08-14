# F10 - Evidencia de preservacion y paquete final

Tramo de cierre del refactor `refactor/bacon-1.2.1-preserve` (golden
Bacon 1.2.1 == tag `golden-bacon-1.2.1` == `951b7b3`).  Sin cambio de
binario: core `ea7a80e4`, daemon `4be71632` (goldens desde F7).

## 1. Resumen ejecutivo

El port fue reorganizado internamente en 9 tramos (F1-F9) sin reescribir
ninguna funcionalidad estable.  Cada tramo: capas puras que TRANSCRIBEN
el comportamiento golden (nunca lo reimplementan), host tests ASAN/UBSAN
de equivalencia, baselines estaticos, audit verde, build MIPS y deploy
SD == build.  Los primeros 6 tramos (F4a-F6) mantuvieron el core
BYTE-IDENTICO (`7709b665`); F7 altero el binario deliberadamente y solo
para eliminar codigo muerto (nuevo golden `ea7a80e4`, daemon
`4be71632`); F8-F10 no vuelven a tocarlo.

## 2. Mapa de la arquitectura original y problemas encontrados

Mapa completo: `docs/PORT_ARCHITECTURE_BASELINE_ES.md` (commit `c3b4b0e`).
Estructura: core libretro MIPS32 r2 single-thread, `retro_run()` ->
`AppWindow::DoEventLoop()`, vistas en `Application/Views`, puente UAC2 en
`Adapters/TREEFROG`, daemons host en `device/*.c` con contrato
48 kHz/2ch/S16_LE via FIFOs en tmpfs, supervisor y launcher en `device/`.

Problemas detectados y resueltos durante el refactor:

| Problema | Trama | Fix |
|---|---|---|
| Cada vista interpretaba botones fisicos con condiciones ad-hoc | F1 | ActionMap + ChordResolver (tabla dorada por contexto) |
| Help y bindings podian divergir (fuentes de verdad separadas) | F2 | HelpRegistry por ViewType + mismo ActionMap |
| Cast C-style a subobjeto desalineado: crash "R1 en HelpOverlay" (UB, puntero +168 bytes) | F2 | `NavModal::ModalSelf()` thunk; reproducido bajo ASAN/UBSAN |
| Crash one-shot del preview de pitch (rewrite del WAV mientras el stream anterior seguia activo) | F3-1 | `Stop()+Sleep(80)` antes del rewrite (U2.52.0) |
| Lag intermitente Windows USB Audio (no reproducible en local) | F3-3 | Pendiente: hipotesis backpressure/ASRC, requiere perfil Windows |
| Help de vista abria seccion equivocada (desajuste ordinal) | F2 | kSections_/ViewType alineado; TABLA/CHOP PITCH afinados |
| Clases gigantes con responsabilidades mezcladas (Chopper ~3300 lineas, Mixer, Phrase) | F3 | Split por capas puras header-only |
| `a.out` accidental commiteado | F7 | Removido con amend; proceso: nunca `g++` sin `-o` |

## 3. Arquitectura propuesta y aplicada

`Physical Input -> ChordResolver -> Semantic Action -> Context Policy -> Controller`
(F1), menus con una sola fuente de verdad (F2), clases divididas en
`Controller/View/Model/Tool/Service/History` (F3), audio
`AudioEngine -> AudioRouter -> AudioBackend + AudioCapabilities` (F4),
storage con politica Volatile/Persistent/Diagnostic (F5), estructura
objetivo (F6), deuda tecnica (F7), harness de regresion funcional (F8),
riesgos y limites documentados (F9).

## 4. Estructura final de carpetas (lo relevante del refactor)

```
source/sources/
  Application/
    Audio/          # F4: AudioDriverModeTable, AudioCapabilities, AudioRouter,
                    #     AudioBackend, AudioEngine (todo declarativo, puro)
    Mixer/          # F3-4: FxPages, MixerMeters, MixerMenu, FxNavigator
    Phrase/         # F3-5: PhraseGridEdit, PhraseUndo
    UI/
      Input/        # F1: PhysicalInput, ActionId, ActionMap, ChordResolver,
                    #     ScenarioCatalog (F8)
      Navigation/   # F2: NavigationController/NavigationStack/NavModal
      Views/        # F6 (movido de Application/Views)
  Services/
    Storage/        # F5: StoragePolicy + inventario auditado
  (el resto: Model/Player/Persistency/Commands/FX/Instruments/Utils sin
   cambios estructurales)
  Adapters/TREEFROG/ # puente UAC2; SPSC muerto eliminado (F7)
device/             # daemons r36s_u2523, sp404, midi + supervisor/launcher
tests/
  host/             # 24+ runners ASAN/UBSAN de equivalencia golden
  test_*.py         # 60+ baselines estaticos (ver seccion 8)
scripts/audit.sh    # gate unico
docs/               # mapa + F3..F9 + RELEASE_* + politicas
```

## 5. Politicas oficiales

- **Input** (F1/F8): `Application/UI/Input/` es la unica fuente de verdad
  de los acordes.  Los views ya despachan via `ChordResolver_Resolve` por
  contexto (`CTX_GLOBAL/MIXER/MIXER_FX/CHOPPER/CHOPPER_TRIM/CHOPPER_PITCH`);
  B->Preview y A->Apply (chopper/pitch), L1+A->menu (mixer) y el resto de
  bindings quedaron exactamente como golden (catalogo en ActionMap.cpp).
  Escenarios de comportamiento de orden superior: `ScenarioCatalog.h`.
- **Menus** (F2): HelpRegistry por vista + NavigationController con
  transiciones golden (Open/Replace/Push/CloseActive/RestoreSuspended);
  MenuRegistry y Help leen del mismo ActionMap (no pueden divergir).
- **Navegacion** (F2/F3): transiciones de modales solo via
  `NavigationController` (NavModal con OnFocus/OnSuspend/OnRestore/
  IsFinished); todo puntero Modal pasa por `NavModal::ModalSelf()`.
- **Storage/SD** (F5): raiz unica `/mnt/sdcard/lgpt`; tres categorias;
  inventario `{ruta,tipo,quien,cuando}` auditado por barrido estatico;
  todo estado runtime del camino critico en /tmp; USB-REC `Record ->
  RAM/tmpfs -> Preview -> Discard/Save` (sin fallback SD); config de modo
  por evento (lowlat_240, audio_driver_mode) y mirror diagnostic en
  `/otg/logs` como unicas excepciones (F9).
- **Audio** (F4/F9): `AudioEngine -> AudioRouter -> AudioBackend` +
  `AudioCapabilities` declarados y conectados al puente; la ruta runtime
  estable (FIFO 48k/2ch/S16_LE -> daemon -> ALSA/USB) intacta; limites
  ASRC/backlog/reenum documentados en F9.

## 6. Deuda tecnica pendiente, bugs y riesgos

- Deuda: integral ASRC divergente (1000 u2523 vs 30000 sp404) sin unificar
  hasta medir; constantes ASRC duplicadas entre daemons (header comun
  propuesto); exponer PPM en BRIDGE_PROGRESS; alarma de latency_trim;
  consolidar paths runtime en `/tmp/r36sx_lgpt_usb/`; build X64 en host
  bloqueado por dependencias (pkg-config/SDL2/ALSA); 4 warnings
  preexistentes en device/*.c (534/70 u2523, 2809/76 sp404).
- Bugs abiertos: lag Windows USB Audio (F3-3, no reproducible; hipotesis
  ASRC/backpressure); crash frio one-shot del primer edit destructivo
  (F1b, no reproducible, sospecha de contencion daemon/SD en arranque).
- Riesgos: ver `docs/F9_RISKS_ES.md` (cadena retro_run->FIFO->ASRC->daemon).

## 7. Evidencia de preservacion

- F4a-F6: core BYTE-IDENTICO `7709b665` (10 deploys, todos SD == build).
- F7: unico cambio deliberado: eliminacion de codigo muerto (0 simbolos
  SPSC) -> nuevo golden `ea7a80e4`; daemon `4be71632`.
- F8-F10: `ea7a80e4`/`4be71632` byte-identicos en 4 builds consecutivos.
- Validaciones en consola realizadas: F1b MIX/FX (protocolo 7 pasos),
  help/audio/undo/redo, F2 help por vista, F3-1..F3-5 chopper/pitch/
  undo-redo/destructivos/mixer/phrase (backups y SHA por tramo en el
  roadmap).
- Protocolo de publicacion: audit verde -> build -> gate diag -> deploy
  con backup -> verificacion SD == build y daemon activo EN LA SD ->
  commit -> push.  Ningun commit se publica sin la verificacion en SD.
- Backups `BACKUPS/LGPT_BEFORE_U2523_*`: 20260813_223545 (F4e),
  225053 (F5), 230223 (F6), 231021 (F7), 235903 (F8), 20260814_002016 (F9).

## 8. Pruebas de regresion (lo que pasa `scripts/audit.sh`)

El gate unico es `AUDIT_CLEAN_MAIN_U2523_OK` (salida 0 de audit.sh):
- 60+ baselines estaticos `tests/test_*.py` (F3.2-F3.5, F4A-F4E, F5, F8,
  F9, input_policy, rc2/rc3/rc4, fx_phase*, u25xx, ui_centered).
- 24+ runners host ASAN/UBSAN: input_policy (165), navigation, help_overlay,
  chop_model, edit_history, pitch_tool, preview_service (740),
  chopper_view (452503), chopper_draw (5222), chopper_controller (128),
  fx_pages (191), mixer_meters (71), mixer_menu (40), fx_navigator (49),
  phrase_grid_edit (116), phrase_undo, audio_driver_modes (68),
  audio_capabilities (55), audio_router (30), audio_backend (48),
  audio_engine (38), storage_policy (55), action_scenarios (342).
- Modelos de audio deterministas: generate_asrc_fir_tables,
  fx_phase0_* (WAVs de validacion regenerables, en .gitignore).
- Sintaxis host de todo el core + `gcc -fsyntax-only` de daemons.

## 9. Recomendaciones para funciones futuras (multitrack, etc.)

1. Multitrack USB (8 outputs): a~adir capabilities y un nuevo daemon
   multicanal SIN tocar el contrato 48k/2ch del camino estable; el nuevo
   backend se registra en AudioBackend/AudioRouter y el resto del sistema
   no cambia (AudioCapabilities ya reserva Stereo/Multitrack).
2. Unificar integral ASRC despues de medir en hardware (mismo header de
   constantes para ambos daemons).
3. Backends nuevos = archivos nuevos en `Application/Audio/Backends/` +
   una fila en AudioDriverModeTable; sin tocar MixerView/Chopper/Help.
4. Storage: cualquier nueva escritura debe declararse en el inventario de
   StoragePolicy (el barrido estatico falla si se omite).
5. El harness F8 (ScenarioCatalog) es la red de seguridad para cualquier
   cambio futuro de input: primero transcribir, despues tocar.
