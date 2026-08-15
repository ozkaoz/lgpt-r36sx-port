#ifndef _FX_PAGES_H_
#define _FX_PAGES_H_

#include <math.h>
#include "Application/Utils/fixed.h"
#include "Application/FX/FxParamDescriptor.h"

// F3-4a (docs/F3_ARCHITECTURE_ES.md): capa pura de las paginas FX
// parametrizadas del Mixer (el "MixerService" del diseno, renombrado a
// FxPages para no colisionar con el servicio de audio DAW existente en
// Application/Mixer/MixerService.h).  Contiene la tabla de parametros
// (kFxParams_), el mapeo de filas por pagina (bypass primero), la edicion
// musical/log en curva y las conversiones VU / returns.  No depende de
// GUI, audio, Player, SamplePool ni de ninguna clase de la aplicacion:
// solo <math.h> y fixed.h (Foundation).  La tabla y los enums son
// byte-identicos a los que vivian en MixerView.{h,cpp} (golden Bacon 1.2.1).

// TREEFROG_FX_PAGES_V3 (PLAN_FX_REDESIGN_ES.md, Fase 9):
// MixerView page system for the master FX engine.  SELECT cycles
// MIX -> DELAY -> REVERB -> EQ -> EQ_EXT -> COMP -> MIX.  MIX keeps the
// per-channel bars plus an FX RETURNS readout (master DLY/RVB return
// levels).  The per-track DLY/RVB send readouts were removed in Fase 9:
// sends are per-instrument now (edited in InstrumentView), and the
// per-track sends survive only as the Fase 7 inheritance/compatibility
// layer.  R2 alone cycles the MIX-page edit target VOL -> DLY RET -> RVB
// RET.  DELAY/REVERB/EQ/EQ_EXT/COMP are parameter pages: UP/DOWN moves the
// row cursor, LEFT/RIGHT edits the value, A+UP/DOWN coarse.  EQ exposes
// the 3-band parametric EQ, EQ_EXT the 5-band EXT chain (bacon-1.5 item 2),
// COMP the compressor (Fase 6 splits the old single MASTER page in two so
// each page fits the 8-line mixer screen without scrolling).
// FXP_MASTER_EQ8 (bacon-1.5, item 2): EQ EXT page inserted between EQ and
// COMP (runtime-only; nothing persists the FxPage number).
enum FxPage {
    FX_PAGE_MIX = 0,
    FX_PAGE_DELAY,
    FX_PAGE_REVERB,
    FX_PAGE_EQ,
    FX_PAGE_EQ_EXT,
    FX_PAGE_COMP,
    FX_PAGE_COUNT
};

// Parameter rows available on the DELAY/REVERB/EQ/COMP pages (see
// FxPages.h kFxParams_ and MixerView fxGet/fxSet).  Also used to size the
// cursor.  Fase 6: the global SEND/RET rows were removed from the
// DELAY/REVERB pages (sends are now per-track / per-instrument; returns
// are fixed 0.5 helpers).
enum FxParamId {
    // DELAY
    FX_P_DLY_TIME = 0,
    FX_P_DLY_FBK,
    FX_P_DLY_MIX,
    FX_P_DLY_WID,
    FX_P_DLY_PP,
    FX_P_DLY_SAT,
    FX_P_DLY_BYP,
    // REVERB
    // RC2 (point 3.1): the legacy RVB MIX row was removed from the UI.  The
    // reverb is a true wet-only send/return processor now: RVB MIX no longer
    // acts as a dry/wet control (the engine keeps reading/persisting it but it
    // is inert), and the audible level is set by the instrument send + the
    // Mixer REVERB RETURN.  The page shows PRE/DEC/SIZ/DMP/WID/MODE/BYP.
    FX_P_RVB_PRE,
    FX_P_RVB_DEC,
    FX_P_RVB_SIZ,
    FX_P_RVB_DMP,
    FX_P_RVB_WID,
    FX_P_RVB_MODE,
    FX_P_RVB_BYP,
    // EQ (3 bands, dedicated banded menu - Fase 12: bypass + enable/freq/
    // gain/Q each; EN is first so UP/DOWN walks the band in the same visual
    // order the EQ menu draws)
    FX_P_EQ_BYP,
    FX_P_EQ_LOW_EN,
    FX_P_EQ_LOW_FRQ,
    FX_P_EQ_LOW_GAI,
    FX_P_EQ_LOW_Q,
    FX_P_EQ_MID_EN,
    FX_P_EQ_MID_FRQ,
    FX_P_EQ_MID_GAI,
    FX_P_EQ_MID_Q,
    FX_P_EQ_HI_EN,
    FX_P_EQ_HI_FRQ,
    FX_P_EQ_HI_GAI,
    FX_P_EQ_HI_Q,
    // COMP
    // COMP (dedicated menu - Fase 13: BYP first so it is never off-screen;
    // THR/RAT/KNE/ATK/REL/MKU/LNK/SC follow in the same order the COMP menu
    // draws them, with the GR meter below)
    FX_P_CMP_BYP,
    FX_P_CMP_THR,
    FX_P_CMP_RAT,
    FX_P_CMP_KNE,
    FX_P_CMP_ATK,
    FX_P_CMP_REL,
    FX_P_CMP_MKU,
    FX_P_CMP_LINK,
    FX_P_CMP_SC,
    // EQ EXT (FX_PAGE_EQ_EXT, bacon-1.5 item 2): bypass + 5 EXT bands
    // (BAND3..BAND7, no EN: enabled derived from GAI!=0 || TYP!=BELL).
    // Appended AFTER the golden ids so every existing FX_P_* value is
    // unchanged (bit-identical persistence).
    FX_P_EQX_BYP,
    FX_P_EQX_B3_FRQ,
    FX_P_EQX_B3_GAI,
    FX_P_EQX_B3_Q,
    FX_P_EQX_B3_TYP,
    FX_P_EQX_B4_FRQ,
    FX_P_EQX_B4_GAI,
    FX_P_EQX_B4_Q,
    FX_P_EQX_B4_TYP,
    FX_P_EQX_B5_FRQ,
    FX_P_EQX_B5_GAI,
    FX_P_EQX_B5_Q,
    FX_P_EQX_B5_TYP,
    FX_P_EQX_B6_FRQ,
    FX_P_EQX_B6_GAI,
    FX_P_EQX_B6_Q,
    FX_P_EQX_B6_TYP,
    FX_P_EQX_B7_FRQ,
    FX_P_EQX_B7_GAI,
    FX_P_EQX_B7_Q,
    FX_P_EQX_B7_TYP,
    FX_PARAM_COUNT
};

// TREEFROG_FX_PAGES_PARAMS_V2 (PLAN_FX_REDESIGN_ES.md, Fase 6):
// Parameter table for the DELAY / REVERB / EQ / COMP pages.  Each row
// exposes a master-bus parameter as a float in natural units (ms, %, dB,
// Hz, s, ratio).  The UI edits the float and the setter clamps to the
// documented range; the DSP modules clamp again, so the page is always
// consistent.  Fase 6: the global SEND/RET rows were removed (sends are
// per-track / per-instrument; returns are fixed 0.5 helpers), and the old
// MASTER page was split into EQ and COMP so each fits the 8-line mixer
// screen.
struct FxParamSpec {
    const char *label ;        // short name shown on the page
    FxPage page ;              // which page owns this row
    float vmin ;               // minimum (natural units)
    float vmax ;               // maximum (natural units)
    float vdef ;               // legacy default (natural units), A+B restores
    const char *fmt ;          // printf format for the value
} ;

static const FxParamSpec kFxParams_[FX_PARAM_COUNT] = {
    // DELAY page
    { "DLY TIM", FX_PAGE_DELAY,   10.0f, 2000.0f,   0.0f,   "%5.0f" },  // FX_P_DLY_TIME (ms)
    { "DLY FBK", FX_PAGE_DELAY,    0.0f,   0.98f,   0.0f,   "%5.2f" },  // FX_P_DLY_FBK
    { "DLY MIX", FX_PAGE_DELAY,    0.0f,   1.0f,    1.0f,   "%5.2f" },  // FX_P_DLY_MIX
    { "DLY WID", FX_PAGE_DELAY,    0.0f,   1.0f,    1.0f,   "%5.2f" },  // FX_P_DLY_WID
    { "DLY P/P", FX_PAGE_DELAY,    0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_DLY_PP (0/1)
    { "DLY SAT", FX_PAGE_DELAY,    0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_DLY_SAT (0/1)
    { "DLY BYP", FX_PAGE_DELAY,    0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_DLY_BYP (0/1)
    // REVERB page
    // RC2 (point 3.1): the RVB MIX row was removed.  The reverb is wet-only
    // (no dry/wet crossfade); the audible level is the instrument send + the
    // Mixer REVERB RETURN (MIX page FX RETURNS), not an internal mix.
    { "RVB PRE", FX_PAGE_REVERB,   0.0f, 100.0f,    0.0f,   "%5.0f" },  // FX_P_RVB_PRE (ms)
    { "RVB DEC", FX_PAGE_REVERB,   0.2f,   8.0f,    1.0f,   "%5.2f" },  // FX_P_RVB_DEC (s)
    { "RVB SIZ", FX_PAGE_REVERB,   0.5f,   1.5f,    1.0f,   "%5.2f" },  // FX_P_RVB_SIZ
    { "RVB DMP", FX_PAGE_REVERB,   0.0f,   1.0f,    0.5f,   "%5.2f" },  // FX_P_RVB_DMP
    { "RVB WID", FX_PAGE_REVERB,   0.0f,   1.0f,    1.0f,   "%5.2f" },  // FX_P_RVB_WID
    { "RVB MOD", FX_PAGE_REVERB,   0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_RVB_MODE (0/1)
    { "RVB BYP", FX_PAGE_REVERB,   0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_RVB_BYP (0/1)
    // EQ page (3 bands, dedicated banded menu - Fase 12).  Per band the rows
    // are EN / FRQ / GAI / Q so UP/DOWN walks the band in the same visual
    // order drawEqPage() renders.  Frequencies default to 100/1000/10000 Hz.
    { "EQ  BYP", FX_PAGE_EQ,       0.0f,   1.0f,    1.0f,   "%5.0f" },  // FX_P_EQ_BYP
    { "LO  EN",  FX_PAGE_EQ,       0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_EQ_LOW_EN
    { "LO  FRQ", FX_PAGE_EQ,      20.0f, 20000.0f,100.0f,  "%5.0f" },  // FX_P_EQ_LOW_FRQ
    { "LO  GAI", FX_PAGE_EQ,      -24.0f,  24.0f,   0.0f,   "%5.1f" },  // FX_P_EQ_LOW_GAI
    { "LO  Q",   FX_PAGE_EQ,       0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQ_LOW_Q
    { "MID EN",  FX_PAGE_EQ,       0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_EQ_MID_EN
    { "MID FRQ", FX_PAGE_EQ,      20.0f, 20000.0f,1000.0f, "%5.0f" },  // FX_P_EQ_MID_FRQ
    { "MID GAI", FX_PAGE_EQ,      -24.0f,  24.0f,   0.0f,   "%5.1f" },  // FX_P_EQ_MID_GAI
    { "MID Q",   FX_PAGE_EQ,       0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQ_MID_Q
    { "HI  EN",  FX_PAGE_EQ,       0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_EQ_HI_EN
    { "HI  FRQ", FX_PAGE_EQ,      20.0f, 20000.0f,10000.0f,"%5.0f" },  // FX_P_EQ_HI_FRQ
    { "HI  GAI", FX_PAGE_EQ,      -24.0f,  24.0f,   0.0f,   "%5.1f" },  // FX_P_EQ_HI_GAI
    { "HI  Q",   FX_PAGE_EQ,       0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQ_HI_Q
    // COMP page (dedicated menu - Fase 13: BYP first so it is never
    // off-screen, then THR/RAT/KNE/ATK/REL/MKU/LNK/SC in the order
    // drawCompPage() renders; the GR meter sits below the parameters).
    { "CMP BYP", FX_PAGE_COMP,     0.0f,   1.0f,    1.0f,   "%5.0f" },  // FX_P_CMP_BYP
    { "CMP THR", FX_PAGE_COMP,   -60.0f,   0.0f,  -24.0f,  "%5.1f" },  // FX_P_CMP_THR (dB)
    { "CMP RAT", FX_PAGE_COMP,     1.0f,  20.0f,    4.0f,   "%5.1f" },  // FX_P_CMP_RAT
    { "CMP KNE", FX_PAGE_COMP,     0.0f,  12.0f,    6.0f,   "%5.1f" },  // FX_P_CMP_KNE (dB)
    { "CMP ATK", FX_PAGE_COMP,     0.1f, 500.0f,   15.0f,   "%5.1f" },  // FX_P_CMP_ATK (ms)
    { "CMP REL", FX_PAGE_COMP,     1.0f, 2000.0f, 200.0f,   "%5.0f" },  // FX_P_CMP_REL (ms)
    { "CMP MKU", FX_PAGE_COMP,     0.0f,  24.0f,    0.0f,   "%5.1f" },  // FX_P_CMP_MKU (dB)
    { "CMP LNK", FX_PAGE_COMP,     0.0f,   1.0f,    1.0f,   "%5.0f" },  // FX_P_CMP_LINK
    { "CMP SCL", FX_PAGE_COMP,     0.0f,   1.0f,    1.0f,   "%5.0f" },  // FX_P_CMP_SC (softclip)
    // EQ EXT page (bacon-1.5 item 2): BYP + 5 bands x (FRQ/GAI/Q/TYP), the
    // ParametricEQ BAND3..BAND7.  Frequencies default to the 2/4/8/16 kHz
    // ladder; TYP defaults BELL so the bands are implicitly disabled until
    // gain or type changes.
    { "EQX BYP", FX_PAGE_EQ_EXT,    0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_EQX_BYP
    { "B3 FRQ",  FX_PAGE_EQ_EXT,   20.0f, 20000.0f,2000.0f, "%5.0f" },  // FX_P_EQX_B3_FRQ
    { "B3 GAI",  FX_PAGE_EQ_EXT,  -24.0f,  24.0f,   0.0f,   "%5.1f" },  // FX_P_EQX_B3_GAI
    { "B3 Q",    FX_PAGE_EQ_EXT,    0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQX_B3_Q
    { "B3 TYP",  FX_PAGE_EQ_EXT,    0.0f,   6.0f,    1.0f,   "%5.0f" },  // FX_P_EQX_B3_TYP (BELL)
    { "B4 FRQ",  FX_PAGE_EQ_EXT,   20.0f, 20000.0f,4000.0f, "%5.0f" },  // FX_P_EQX_B4_FRQ
    { "B4 GAI",  FX_PAGE_EQ_EXT,  -24.0f,  24.0f,   0.0f,   "%5.1f" },  // FX_P_EQX_B4_GAI
    { "B4 Q",    FX_PAGE_EQ_EXT,    0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQX_B4_Q
    { "B4 TYP",  FX_PAGE_EQ_EXT,    0.0f,   6.0f,    1.0f,   "%5.0f" },  // FX_P_EQX_B4_TYP
    { "B5 FRQ",  FX_PAGE_EQ_EXT,   20.0f, 20000.0f,8000.0f, "%5.0f" },  // FX_P_EQX_B5_FRQ
    { "B5 GAI",  FX_PAGE_EQ_EXT,  -24.0f,  24.0f,   0.0f,   "%5.1f" },  // FX_P_EQX_B5_GAI
    { "B5 Q",    FX_PAGE_EQ_EXT,    0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQX_B5_Q
    { "B5 TYP",  FX_PAGE_EQ_EXT,    0.0f,   6.0f,    1.0f,   "%5.0f" },  // FX_P_EQX_B5_TYP
    { "B6 FRQ",  FX_PAGE_EQ_EXT,   20.0f, 20000.0f,16000.0f,"%5.0f" },  // FX_P_EQX_B6_FRQ
    { "B6 GAI",  FX_PAGE_EQ_EXT,  -24.0f,  24.0f,   0.0f,   "%5.1f" },  // FX_P_EQX_B6_GAI
    { "B6 Q",    FX_PAGE_EQ_EXT,    0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQX_B6_Q
    { "B6 TYP",  FX_PAGE_EQ_EXT,    0.0f,   6.0f,    1.0f,   "%5.0f" },  // FX_P_EQX_B6_TYP
    { "B7 FRQ",  FX_PAGE_EQ_EXT,   20.0f, 20000.0f,16000.0f,"%5.0f" },  // FX_P_EQX_B7_FRQ
    { "B7 GAI",  FX_PAGE_EQ_EXT,  -24.0f,  24.0f,   0.0f,   "%5.1f" },  // FX_P_EQX_B7_GAI
    { "B7 Q",    FX_PAGE_EQ_EXT,    0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQX_B7_Q
    { "B7 TYP",  FX_PAGE_EQ_EXT,    0.0f,   6.0f,    1.0f,   "%5.0f" },  // FX_P_EQX_B7_TYP
} ;

// FXP_DESCRIPTORS_V1 (bacon-1.5, item 1): metadatos de la capa comun de
// parametros para cada fila master.  El rango natural sigue viviendo en
// kFxParams_ (fuente unica de verdad); aqui solo se declaran el tipo
// (continuo/signed/switch), la curva perceptiva (LOG2 para
// frecuencia/tiempo, LINEAR para ganancias/mezclas) y la unidad para el
// display secundario.  El orden coincide 1:1 con kFxParams_.
struct FxParamMeta {
    FxParamKind kind_ ;
    FxParamCurve curve_ ;
    const char *unit_ ;
} ;

static const FxParamMeta kFxParamMeta_[FX_PARAM_COUNT] = {
    // DELAY
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "ms" },  // DLY TIM
    { FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, ""   },  // DLY FBK
    { FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, ""   },  // DLY MIX
    { FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, ""   },  // DLY WID
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // DLY P/P
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // DLY SAT
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // DLY BYP
    // REVERB
    { FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, "ms" },  // RVB PRE
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "s"  },  // RVB DEC
    { FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, ""   },  // RVB SIZ
    { FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, ""   },  // RVB DMP
    { FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, ""   },  // RVB WID
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // RVB MOD
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // RVB BYP
    // EQ (3 bands)
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // EQ BYP
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // LO EN
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Hz" },  // LO FRQ
    { FX_PARAM_SIGNED,     FX_CURVE_LINEAR, "dB" },  // LO GAI
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Q"  },  // LO Q
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // MID EN
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Hz" },  // MID FRQ
    { FX_PARAM_SIGNED,     FX_CURVE_LINEAR, "dB" },  // MID GAI
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Q"  },  // MID Q
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // HI EN
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Hz" },  // HI FRQ
    { FX_PARAM_SIGNED,     FX_CURVE_LINEAR, "dB" },  // HI GAI
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Q"  },  // HI Q
    // COMP
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // CMP BYP
    { FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, "dB" },  // CMP THR
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   ""   },  // CMP RAT
    { FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, "dB" },  // CMP KNE
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "ms" },  // CMP ATK
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "ms" },  // CMP REL
    { FX_PARAM_CONTINUOUS, FX_CURVE_LINEAR, "dB" },  // CMP MKU
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // CMP LNK
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // CMP SC
    // EQ EXT (bacon-1.5 item 2)
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // EQX BYP
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Hz" },  // B3 FRQ
    { FX_PARAM_SIGNED,     FX_CURVE_LINEAR, "dB" },  // B3 GAI
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Q"  },  // B3 Q
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // B3 TYP (discrete 0..6)
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Hz" },  // B4 FRQ
    { FX_PARAM_SIGNED,     FX_CURVE_LINEAR, "dB" },  // B4 GAI
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Q"  },  // B4 Q
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // B4 TYP
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Hz" },  // B5 FRQ
    { FX_PARAM_SIGNED,     FX_CURVE_LINEAR, "dB" },  // B5 GAI
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Q"  },  // B5 Q
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // B5 TYP
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Hz" },  // B6 FRQ
    { FX_PARAM_SIGNED,     FX_CURVE_LINEAR, "dB" },  // B6 GAI
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Q"  },  // B6 Q
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // B6 TYP
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Hz" },  // B7 FRQ
    { FX_PARAM_SIGNED,     FX_CURVE_LINEAR, "dB" },  // B7 GAI
    { FX_PARAM_CONTINUOUS, FX_CURVE_LOG2,   "Q"  },  // B7 Q
    { FX_PARAM_SWITCH,     FX_CURVE_LINEAR, ""   },  // B7 TYP
} ;

// Descriptor completo (range natural de kFxParams_ + meta de la capa
// comun) para una fila master.  Uso en UI (display percent) y en la
// edicion pura (FxNavigator::EditValue).
inline FxParamDescriptor fxDescForId(int id) {
	FxParamDescriptor d ;
	d.label_=kFxParams_[id].label ;
	d.kind_=kFxParamMeta_[id].kind_ ;
	d.curve_=kFxParamMeta_[id].curve_ ;
	d.rawMin_=0 ;
	d.rawMax_=0 ;
	d.rawDefault_=0 ;
	d.dspMin_=kFxParams_[id].vmin ;
	d.dspMax_=kFxParams_[id].vmax ;
	d.dspDefault_=kFxParams_[id].vdef ;
	d.unit_=kFxParamMeta_[id].unit_ ;
	return d ;
}

inline float fxPercentToDspId(int id,int p) {
	return fxPercentToDsp(fxDescForId(id),p) ;
}

inline int fxDspToPercentId(int id,float v) {
	return fxDspToPercent(fxDescForId(id),v) ;
}

// True si el parametro es continuo (editable en %); false para switches.
inline bool fxIsPercentParam(int id) {
	return kFxParamMeta_[id].kind_!=FX_PARAM_SWITCH ;
}

// FXP_MASTER_EQ8 (bacon-1.5, item 2): discrete params (switch AND multi-value
// discrete like the EQ EXT band TYPE 0..6) always step by 1, never by a
// percent step.
inline bool fxIsDiscreteParam(int id) {
	if (kFxParamMeta_[id].kind_==FX_PARAM_SWITCH) return true ;
	switch (id) {
	case FX_P_EQX_B3_TYP:
	case FX_P_EQX_B4_TYP:
	case FX_P_EQX_B5_TYP:
	case FX_P_EQX_B6_TYP:
	case FX_P_EQX_B7_TYP:
		return true ;
	default:
		return false ;
	}
}

// TREEFROG_MIXER_VU_DB_SCALE_V5 (Bacon 1.1.1):
// DAW/VU-style rebased scale.  The displayed 0 dB row corresponds to the
// typical loud output at volume 100 (measured ~-12 dBFS real peaks for loud
// material on the normalized 0..1 peaks), so at volume 100 the bar genuinely
// reaches the 0 dB row and strong material pushes into the red +3 dB zone
// above it -- 0 dB is reachable, red means over 0 dB.  Displayed dB = real
// dB + 12 on a -36..+3 span (39 dB): level = (db+36)/39 maps 0 dB to cell
// 11 of 12 and +3 dB to the top cell, which is exactly where the fill turns
// red (filledCells >= totalCells).  The V3 -50..0 scale was honest but made
// red unreachable: loud material read 9/12 cells at volume 100 and the +3
// zone did not exist.
inline float mixVULevel(float peak) {
	if (peak <= 0.0f) return 0.0f ;
	float db = 20.0f * log10f(peak) + 12.0f ;
	float level = (db + 36.0f) / 39.0f ;
	if (level < 0.0f) level = 0.0f ;
	if (level > 1.0f) level = 1.0f ;
	return level ;
}

// TREEFROG_FX_PAGES_V3 (PLAN_FX_REDESIGN_ES.md, Fase 9):
// Master FX returns are stored as fixed (Q15) 0..1 levels in FxEngine.
// These helpers convert to/from the integer percent (0..100) the MIX page
// edits, clamped so the value always round-trips.
inline int fxReturnPercent(fixed ret) {
	float f=fp2fl(ret) ;
	if (f<0.0f) f=0.0f ;
	if (f>1.0f) f=1.0f ;
	return (int)(f*100.0f+0.5f) ;
}
inline fixed fxReturnFromPercent(int p) {
	if (p<0) p=0 ;
	if (p>100) p=100 ;
	return fl2fp((float)p*0.01f) ;
}

// TREEFROG_MASTER_BYPASS_FIRST_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md,
// point 7): On DELAY/REVERB/EQ/COMP the BYPASS parameter is the FIRST
// visual and logical row.  These helpers put the page Bypass at row 0 for
// navigation and drawing.  The underlying kFxParams_ table and FxParam
// enum stay byte-identical (bit-identical persistence); only the row order
// changes.
inline int fxBypassId(FxPage page) {
	switch (page) {
	case FX_PAGE_DELAY:  return FX_P_DLY_BYP ;
	case FX_PAGE_REVERB: return FX_P_RVB_BYP ;
	case FX_PAGE_EQ:     return FX_P_EQ_BYP ;
	case FX_PAGE_EQ_EXT: return FX_P_EQX_BYP ;
	case FX_PAGE_COMP:   return FX_P_CMP_BYP ;
	default:             return -1 ;
	}
}

inline int fxCountOnPage(FxPage page) {
	int count=0 ;
	for (int i=0;i<FX_PARAM_COUNT;i++) {
		if (kFxParams_[i].page==page) count++ ;
	}
	return count ;
}

// Ordered row position of a param on its page, with the page Bypass first.
inline int fxRowForId(int id) {
	FxPage page=kFxParams_[id].page ;
	int byp=fxBypassId(page) ;
	if (id==byp) return 0 ;
	int row=1 ;
	for (int i=0;i<id;i++) {
		if (kFxParams_[i].page==page && i!=byp) row++ ;
	}
	return row ;
}

// Inverse of fxRowForId: given a logical row (0 = bypass), return the param
// id.  F3-4a: the pure form takes the page explicitly (the MixerView member
// passes its current fxPage_).
inline int fxIdForRow(FxPage page,int row) {
	int byp=fxBypassId(page) ;
	if (byp>=0 && row==0) return byp ;
	int seen=1 ;
	for (int i=0;i<FX_PARAM_COUNT;i++) {
		if (kFxParams_[i].page!=page || i==byp) continue ;
		if (seen==row) return i ;
		seen++ ;
	}
	return -1 ;
}

inline bool fxIdOnPage(int id,FxPage page) {
	return kFxParams_[id].page==page ;
}

// TREEFROG_FX_EDIT_CURVE_V1 (PLAN_FX_REDESIGN_ES.md, Fase 12 + Fase 14):
// Wide-range proportional parameters are edited on a musical/log curve,
// never with a linear 1/10 step: fine (L/R) steps by one semitone
// (x2^(1/12)), coarse (A+UP/DOWN) by one octave (x2).  The relative error
// is constant, so the whole range is traversable in a bounded number of
// presses and editing stays musically meaningful.  Applies to EQ
// frequencies and to every other wide-range time/ratio parameter (delay
// time, reverb pre-delay/decay, compressor attack/release/ratio).
inline bool fxUsesCurve(int id) {
	switch (id) {
	case FX_P_EQ_LOW_FRQ:
	case FX_P_EQ_MID_FRQ:
	case FX_P_EQ_HI_FRQ:
	case FX_P_EQX_B3_FRQ:
	case FX_P_EQX_B4_FRQ:
	case FX_P_EQX_B5_FRQ:
	case FX_P_EQX_B6_FRQ:
	case FX_P_EQX_B7_FRQ:
	case FX_P_DLY_TIME:
	case FX_P_RVB_PRE:
	case FX_P_RVB_DEC:
	case FX_P_CMP_ATK:
	case FX_P_CMP_REL:
	case FX_P_CMP_RAT:
		return true ;
	default:
		return false ;
	}
}

// Pure edit math: returns the new value for one curve edit of the current
// value v (the MixerView member fxEditCurve reads via fxGet and applies via
// fxSet around this).
inline float fxEditCurveValue(const FxParamSpec &spec,float v,int delta,bool coarse) {
	// Values below the floor snap to it so proportional editing never
	// multiplies zero (e.g. DLY TIM defaults to 0 while vmin is 10).  If the
	// floor itself is 0 (e.g. RVB PRE), the first upward edit starts from 1%
	// of the range instead of being stuck at 0.
	if (delta>0) {
		if (v<spec.vmin) v=spec.vmin ;
		else if (v<=0.0f) v=(spec.vmax-spec.vmin)*0.01f ;
	} else if (delta<0 && v>spec.vmax) {
		v=spec.vmax ;
	}
	float factor=coarse?2.0f:1.05946309436f ;  // octave / semitone
	if (delta<0) factor=1.0f/factor ;
	int steps=delta<0?-delta:delta ;
	for (int s=0;s<steps;s++) v*=factor ;
	if (v<spec.vmin) v=spec.vmin ;
	if (v>spec.vmax) v=spec.vmax ;
	return v ;
}

#endif
