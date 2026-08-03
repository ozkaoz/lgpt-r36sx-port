# Changelog

## Release candidate: Bacon 1.1 - FX Dev (RC3 base)

- **Estado**: fase base de la modernización visual integral
  (`docs/PLAN_RC3_MODERNIZACION_VISUAL_ES.md`). No toca el motor FX.
- **Bypass unificado**: en las páginas master DELAY/REVERB/EQ/COMP el
  `BYPASS` es ahora la primera fila visual y lógica (semántica
  `ON = efecto desactivado`). Helpers de fila ordenada (`fxBypassId`,
  `fxIdForRow`, `fxCountOnPage`) compartidos por navegación y dibujo; la
  tabla `kFxParams_` y el enum quedan byte-idénticos (persistencia
  bit-idéntica).
- **Librería UiDraw**: primitivas compartidas `DrawCenteredTitle`
  (título centrado `x=(40-len)/2`), `DrawSectionHeader`, `DrawValueRow`,
  `DrawToggle` (`[ ON ]`/`[ OFF ]`), `DrawSolidBar`, `DrawBipolarBar`,
  `DrawProgressBar`, `DrawTabs`, `DrawModalFrame`, `DrawScrollIndicator`
  y `DrawSeparator`. Todo dibujo se clampa a 40x30.
- **Colores semánticos** (`UiColors.h`): roles `UI_COLOR_*` mapeados a los
  `CD_*` existentes; sin RGB directo por vista.
- **Help centralizado**: `HelpRegistry` (sección por vista) + `HelpOverlay`
  (latch, no propaga input). `SELECT+R1` abre la ayuda contextual desde
  cualquier pantalla; `SELECT+R2` conserva el diálogo de Audio Driver.
- **Auditorías**: `UI_CONTROL_AUDIT.md`, `OBSOLETE_FEATURE_AUDIT.md`,
  `UI_VISUAL_AUDIT.md` y la guía `UI_STYLE_GUIDE.md`.
- **Pruebas**: nuevo `test_rc3_base_unified_bypass_uidraw_help.py`; suite
  completa en verde (`AUDIT_CLEAN_MAIN_U2523_OK`). Core instalado y
  verificado en SD (`VERIFY_U2523_OK`, `ERRORS=0`).

## Release candidate: Bacon 1.1 - FX Dev (RC2)

- **Estado**: release candidate sobre RC1. Añade la **normalización de
  etiquetas FX visibles (tabla T1)** sin tocar los FourCC internos (los
  proyectos siguen siendo bit-identicos): `FBMX->CFM`, `FBTN->CFT`,
  `DLAY->NDL`, `FLTR->FCR`, `FCUT->FCU`, `FRES->FRS`, `CRSH->BCR`,
  `PFIN->PFT`, `DLYS->DSE`, `RVBS->RSE`, `DLYT->DTM`, `DLYF->DFB`,
  `RVDC->RDC`, `RVSZ->RSZ`, `CMPT->CTH`.
- **Selector FX por familias con paginación**: FX 1/2 (INST/FILTER/DELAY/
  REVERB/MASTER) y FX 2/2 (LEGACY COMB, CFM/CFT); los proyectos con
  FBMX/FBTN siguen reproduciéndose y siendo editables.
- **Reverb wet-only**: `RVB MIX` retirado de la UI (inerte en el DSP),
  headroom -3 dB de entrada, suma de combs normalizada (`combNorm_`) y 3er
  allpass por canal en NORMAL. El Delay conserva su dry/wet.
- **Páginas DELAY/REVERB MASTER dedicadas**: menús de dos columnas con
  jerarquía de colores (título `CD_HILITE1`, label `CD_NORMAL`, valor
  `CD_HILITE1`, fila editada invertida `CD_HILITE2`) y unidades
  (`ms`, `s`, `ON/OFF`, `ECO`/`NORMAL`).
- **Barras sólidas en InstrumentView**: EFFECT SENDS con bloque sólido de
  celdas invertidas (estilo MixerView) en vez de `[====----]`; `INH` limpia
  la barra.
- **`UIBigHexVarField::SetHexMode` corregido**: conserva el cursor de nibble
  (clamp al rango) y clampa/envuelve el valor al nuevo `[min,max]`.
- **Pruebas**: suite completa en verde (`AUDIT_CLEAN_MAIN_U2523_OK`).
  Nuevo `test_fx_rc2_master_pages_solid_bars_hexmode.py`; actualizados
  `test_fx_phase10_wetonly_audit`, `test_fx_phase4_ui`,
  `test_fx_phase6_nav_ab_default`, `test_fx_phase12_eq_menu`,
  `test_fx_phase13_comp_menu` y `test_fx_phase8_instrument_blocks`.
- **Binario**: `BUILD/U2523/lgpt_r36sx_u2523.so`
  (SHA-256 `c114863b8c43d6ae1300dd672edc5b6980970f59ec08bc29ceb658b58126bc20`),
  daemon ABI7 golden inalterado. Detalles en `docs/RELEASE_BACON_1.1_FX_DEV_ES.md`.

## Release candidate: Bacon 1.1 - FX Dev (RC1)

- **Estado**: release candidate (no estable) sobre la línea Bacon. Integra el
  rediseño FX completo (Fases 0-18 de `docs/PLAN_FX_REDESIGN_ES.md`):
  sends por instrumento DRY/DLY/RVB (live por canal, defaults `100/0/0`),
  automatización live `DLYS`/`RVBS`, retornos globales `DLYRET`/`RVBRET`,
  páginas dedicadas Delay/Reverb/EQ/Comp y branding `LGPT R36SX - Bacon 1.1`.
- **Pruebas**: suite completa en verde (`AUDIT_CLEAN_MAIN_U2523_OK`, 176
  checks OK, 27/27 grupos de audit). Tests preexistentes corregidos:
  `test_u2520_transactional_record_source` (ruta temp `/tmp/r36sx_lgpt_record`),
  `test_u2521_browser_rename`, `test_u2522_nested_rename_frame_forwarding` y
  `test_u2523_rename_caret_alignment` (verifican la implementación actual con
  `TreeFrogTextEditor`; el modal `ImportBrowserRenameModal` fue eliminado en H38.x).
- **Binario**: `BUILD/U2523/lgpt_r36sx_u2523.so`
  (SHA-256 `fddc4b042742da0745edf4f24edeee66543e67b900ecb03b87196bc41f8764ec`),
  daemon ABI7 golden inalterado. Detalles en `docs/RELEASE_BACON_1.1_FX_DEV_ES.md`.

## FX redesign Fase 14 (general fine/coarse curve editing)

- The Fase 12 musical curve edit is generalized: `fxUsesCurve()` + `fxEditCurve()` apply semitone fine (L/R) / octave coarse (A+UP/DOWN) proportional steps to every wide-range time/ratio param — EQ frequencies plus `DLY TIM`, `RVB PRE`, `RVB DEC`, `CMP ATK`, `CMP REL`, `CMP RAT`. Relative error is constant, so the full range is reachable in a bounded number of presses (e.g. CMP REL 1..2000 ms in ~132 fine / ~11 coarse).
- Below-floor values never get stuck: values below vmin snap to the floor on the first upward edit (e.g. DLY TIM default 0, vmin 10); when the floor is 0 (RVB PRE) the first edit starts at 1% of the range. Clamped to [vmin, vmax] both ways. Other (linear) params keep fine=1 / coarse=10.
- New test `test_fx_phase14_curve_editing.py` -> `FX_EDIT_CURVE_PHASE14_OK`; MixerView compiles, full FX suite green, `HOST_SYNTAX_CHECK_U2523_OK`.

## FX redesign Fase 13 (dedicated COMP menu with BYP first and visible GR)

- The COMP page is now a dedicated, exclusive menu (`drawCompPage()`); CMP BYP is the first row so it is never off-screen. Centered rows with a fixed value column: `Bypass`, `Threshold -24.0 dB`, `Ratio 4.0:1` (rendered as x:1), `Knee 6.0 dB`, `Attack 15.0 ms`, `Release 200.0 ms`, `Makeup 0.0 dB`, `Stereo Link ON`, `Soft Clip ON` (booleans as ON/OFF, units shown).
- `Gain Reduction -00.0 dB` GR meter stays visible below the parameters (readout, not selectable); navigation hints remain outside the parameter area (rows 22-23); no row overlap (params 2..10, GR at 12).
- No clipping indicator: the engine has no reliable real audio clip reading (`GetRtViolations` is buffer RT telemetry that must stay 0), so none is added. Soft clip is labelled "Soft Clip" only (never "limiter").
- The COMP param enum was reordered to BYP first (then THR/RAT/KNE/ATK/REL/MKU/LNK/SC), same IDs 28..36; Fase 4/6 test models updated.
- New test `test_fx_phase13_comp_menu.py` -> `FX_COMP_MENU_PHASE13_OK`; `MixerView.cpp` compiles, full FX suite green, `HOST_SYNTAX_CHECK_U2523_OK`.

## FX redesign Fase 12 (dedicated EQ menu with banded layout)

- The EQ page is now a dedicated, exclusive menu (no longer the generic parameter list): `drawEqPage()`/`drawEqRow()` render `EQ BYPASS [ ON ]` on top and LOW/MID/HIGH band blocks (header + EN ON/OFF, FRQ in Hz, GAIN with signed dB, Q). Every parameter is its own selectable row, so selection is unambiguous and the whole band stays visible while editing.
- The EQ param enum was reordered so each band is EN first, then FRQ/GAI/Q, matching the visual order UP/DOWN walks; Fase 4/6 test models updated (IDs 15..27 and per-page counts unchanged).
- Frequency editing is musical: fine (L/R) steps one semitone, coarse (A+UP/DOWN) one octave, clamped to 20..20000 Hz; the full range is reachable in ~120 fine or ~10 coarse presses.
- New test `test_fx_phase12_eq_menu.py` -> `FX_EQ_MENU_PHASE12_OK`; `MixerView.cpp` compiles (`MIXERVIEW_SYNTAX_OK`), full FX suite green, `HOST_SYNTAX_CHECK_U2523_OK`.

## FX redesign Fase 9-11 (MIX FX RETURNS + wet-only audit + [n/5])

- Fase 9: the MIX page per-track D/R send readouts are gone (sends are per-instrument since Fase 6/7 and are edited in InstrumentView; the per-track sends remain only as the Fase 7 inheritance layer). The MIX page now shows an editable FX RETURNS readout for the master delay/reverb return levels (`RET D:50% R:50%`), edited by cycling the R2 target VOL -> DLY RET -> RVB RET. Returns are global master levels, persisted as `DLYRET`/`RVBRET`.
- Fase 10: audit confirmed the global SEND/RET rows are gone from the FX pages and that DLY MIX / RVB MIX default to 1.0 (full wet) so the default return is wet-only (no dry leak); lowering MIX is the documented dry/wet crossfade, not a regression.
- Fase 11: every FX page title shows its position `[n/5]` (`DELAY MASTER [2/5]`, etc.); the MIX page hint shows `SELECT [1/5]`.
- New tests: `test_fx_phase9_mixer_returns.py` -> `FX_MIXER_RETURNS_PHASE9_OK`, `test_fx_phase10_wetonly_audit.py` -> `FX_WETONLY_AUDIT_PHASE10_OK`; `test_fx_phase4_ui.py` updated -> `FX_UI_PHASE43_OK`.

## FX redesign Fase 7-8 (track-send compat + InstrumentView blocks)

- Fase 7: controlled non-destructive per-track send compatibility. An instrument without an override (-1) inherits the per-track Mixer send; an instrument override wins per-instrument. Both layers are persisted (Mixer CHANNEL `DELAYSEND`/`REVERBSEND` + instrument PARAMs `DRY`/`DLY_SEND`/`RVB_SEND`); the per-track send is never deleted.
- Fase 8: the sample InstrumentView is reorganized into vertical blocks (INSTRUMENT / FILTER / BITCRUSHER / PLAYBACK / EFFECT SENDS / AUTOMATION) with two-column field rows. Block headers are drawn in DrawView (not as UIStaticField) so L2+A cut/clear still see the sample field first and the table field last.
- New `UIIntVarField::SetBar(label, width)`: percent-bar rendering for the EFFECT SENDS rows (`DRY`/`DELAY`/`REVERB`, with `INH` when a send inherits the per-track value). Existing field rendering is unchanged by default.
- BITCRUSHER block is labeled "bit depth" (never "compressor"); fb tune/fb mix are retired from editing but their variables stay for load/playback; print fx/wet/pad moved behind `#ifdef FFMPEG_ENABLED` (absent from the R36SX build).
- The whole sample-instrument layout now fits above the bottom map/notes band (max row 26; map/notes at y=27-29).
- New test `tests/test_fx_phase8_instrument_blocks.py` -> `FX_INSTRUMENT_BLOCKS_PHASE8_OK`.

## FX redesign Fase 6 (per-instrument sends + 5-page mixer + A+B=default)

- Per-instrument FX sends: each sample instrument has DRY, DLY send and RVB send variables. DRY scales the effective send (`gain = send*DRY/10000`); the instrument override wins over the per-track Mixer send, and DRY=100 is bit-identical to the previous behaviour.
- DLYS/RVBS phrase commands now write both the instrument override and the per-track Mixer send.
- Mixer FX pages redesigned: MIX / DELAY / REVERB / EQ / COMP (5 pages, 37 parameters). The global SEND/RET rows were removed (sends are per-track/per-instrument, returns stay fixed).
- The Instrument FX modal is gone: R2+A in the Mixer jumps straight to the Instrument view for the hovered channel.
- New FX navigation: A+B restores the focused parameter to its default, on both the Mixer FX pages and the Instrument view fields.
- The legacy "B+A cut instrument / clear table" action moved to L2+A (A+B now means "restore default").
- The Instrument view shows a live `fx sends: dry/dly/rvb` readout.

## H38.6

- New dedicated Pitch column in the phrase grid (`N V P I FX1 P1 FX2 P2`): each step can be transposed -24..+24 semitones, edited with L/R (+-1) and A+UP/DOWN (+-10). Persisted in the project (new `PITCHES` buffer) and applied per note at playback. Chop rows (S01..S99) are protected so the pitch never selects a different chop.
- PTCH command removed from the FX command list (old projects with PTCH in FX are ignored on playback; the pitch is now a per-step column).
- Phrase volume fix: every value 0-100 now maps linearly (100 = full, 1 = ~2%, 0/0xFF = silent), no clipping or distortion at any level; new notes default to volume 100.
- Mixer VU meters now refresh in real time even while the player is stopped (same frame cadence as the USB-C Record meter).
- Rename Project moved from the in-project menu (crash when re-entering TreeFrogUI) to the startup menu: R1+A on a selected project.
- Unified text editor input everywhere (USB-C Record, project rename, new project, sample rename): X+UP/DOWN fast, L1+X case, A confirms, B erases, R1+LEFT cancels.
- New project and Save As use the same Record-style text editor (QWERTY keyboard removed).

## H38.5

- Removed FX3 from phrase editing and playback (Phrase: `00 N V I FX1 P FX2 P`; Table: `F1 P1 F2 P2`).
- Centered phrase and table grids with per-column headers.
- Volume and FX intensity scale now treat 1 as 100% (higher values attenuate).
- Dense mixer VU meters in Record style.
- Rename Project action in the Project menu.

## U2.52.3

- Stable bidirectional USB-C OTG audio at 48 kHz.
- Rewritten Record workflow with transactional Preview, Save and Discard.
- Input monitor restricted to Record.
- Chord-aware input handling and Chopper Undo/Redo.
- Safe sample rename and deferred sample deletion.
- Fixed nested rename input forwarding and caret alignment.
- Consolidated source, scripts, tests and bilingual documentation.

Earlier experimental iterations were consolidated into this release and are available through Git history, not as duplicate files in the current tree.
