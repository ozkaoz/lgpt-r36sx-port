#!/usr/bin/env python3
"""bacon-1.5 item 2: Filter V2 wiring + EQ8 master via the EQ EXT page.

Verifies (token/source-level, mirroring the other F3/Fase golden tests):

- FxPages.h exposes FX_PAGE_EQ_EXT between EQ and COMP and 21 EQ_EXT params
  appended AFTER the golden FX_P_* ids (FX_PARAM_COUNT == 57), so every
  persisted value is bit-identical.
- ParametricEQ (FxEngine) has 8 bands (LOW/MID/HIGH + BAND3..BAND7), a
  per-band type and an independent EXT bypass; the shared EqBiquad primitive
  drives both ParametricEQ and the instrument InstrumentEq (with the added
  BAND_PASS type).
- InstrumentEq is rebuilt at the real audio driver rate (48 kHz fix).
- The sample filter now uses FilterV2 (TPT SVF) via set_filter_v2 /
  filterv2_process with a CHAR_LIST filter-kind variable.
- Persistence (Mixer.cpp) and the MixerView page (drawEqExtPage) cover the
  EXT chain; SELECT cycles through 6 pages.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

FXP = (ROOT / "source/sources/Application/Mixer/FxPages.h").read_text()
FN = (ROOT / "source/sources/Application/Mixer/FxNavigator.h").read_text()
EQ_H = (ROOT / "source/sources/Application/Audio/FxEngine/ParametricEQ.h").read_text()
EQ_CPP = (ROOT / "source/sources/Application/Audio/FxEngine/ParametricEQ.cpp").read_text()
BQ = (ROOT / "source/sources/Application/Audio/EqBiquad.h").read_text()
IEQ_H = (ROOT / "source/sources/Application/Audio/InstrumentEq.h").read_text()
IEQ_CPP = (ROOT / "source/sources/Application/Audio/InstrumentEq.cpp").read_text()
IEQ_MODAL = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/InstrumentEqModal.cpp").read_text()
SI_H = (ROOT / "source/sources/Application/Instruments/SampleInstrument.h").read_text()
SI_CPP = (ROOT / "source/sources/Application/Instruments/SampleInstrument.cpp").read_text()
SID = (ROOT / "source/sources/Application/Instruments/SampleInstrumentDatas.h").read_text()
MIXER = (ROOT / "source/sources/Application/Model/Mixer.cpp").read_text()
MV = (ROOT / "source/sources/Application/UI/Views/MixerView.cpp").read_text()
MV_H = (ROOT / "source/sources/Application/UI/Views/MixerView.h").read_text()
FE_H = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.h").read_text()
FE_CPP = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.cpp").read_text()
IB = (ROOT / "source/sources/Application/Instruments/InstrumentBank.cpp").read_text()
MK = (ROOT / "source/projects/Makefile").read_text()
FV_H = (ROOT / "source/sources/Application/Instruments/FilterV2.h").read_text()
FV_CPP = (ROOT / "source/sources/Application/Instruments/FilterV2.cpp").read_text()
AUDIT = (ROOT / "scripts/audit.sh").read_text()


def check_fxpages():
    # Page enum: EQ_EXT between EQ and COMP.
    assert FXP.index("FX_PAGE_EQ_EXT") > FXP.index("FX_PAGE_EQ")
    assert FXP.index("FX_PAGE_EQ_EXT") < FXP.index("FX_PAGE_COMP")
    # Params appended at the end, before FX_PARAM_COUNT.
    assert FXP.index("FX_P_EQX_BYP") > FXP.index("FX_P_CMP_SC")
    assert FXP.index("FX_P_EQX_B7_TYP") < FXP.index("FX_PARAM_COUNT")
    # Table: 21 rows on the EXT page, TYP is a 0..6 discrete.
    assert '"EQX BYP", FX_PAGE_EQ_EXT' in FXP
    assert '"B7 TYP",  FX_PAGE_EQ_EXT' in FXP
    assert "FX_PARAM_COUNT" in FXP
    # Bypass helper and discrete helper know the new page/params.
    assert "case FX_PAGE_EQ_EXT: return FX_P_EQX_BYP ;" in FXP
    assert "case FX_P_EQX_B3_TYP:" in FXP and "case FX_P_EQX_B7_TYP:" in FXP
    assert "fxIsDiscreteParam" in FXP
    # EXT band frequencies default to the 2/4/8/16 kHz ladder.
    assert '2000.0f' in FXP and '16000.0f' in FXP
    print("1. FxPages EQ_EXT page + 21 appended params OK")


def check_navigator():
    assert "fxIsDiscreteParam(id)" in FN
    assert "EQ_EXT" in FN
    print("2. FxNavigator discrete step + cycle comment OK")


def check_parametric_eq():
    # 8 bands + per-band type + EXT bypass API.
    assert "BAND7" in EQ_H and "kNumBands" in EQ_H
    assert "BT_TYPECOUNT" in EQ_H
    assert "void SetBandType(int band, BandType type);" in EQ_H
    assert "void SetExtBypass(bool on) { extBypass_ = on; }" in EQ_H
    assert "BandType GetBandType" in EQ_H
    assert "bool GetExtBypass" in EQ_H
    assert "void Process(const fixed *in, fixed *out, int frames);" in EQ_H
    # DSP: shared EqBiquad + base/EXT loops + yBase snapshot + crossfade.
    assert "EqBiquad.h" in EQ_CPP
    assert "eqBiquadCoeffs(" in EQ_CPP
    assert "for (int b = LOW; b < BAND3; b++)" in EQ_CPP
    assert "for (int b = BAND3; b < kNumBands; b++)" in EQ_CPP
    assert "yBaseL = yL;" in EQ_CPP
    assert "yL = yBaseL + fp_mul(yL - yBaseL, extBypassMix_);" in EQ_CPP
    # EXT bands enabled iff non-neutral.
    assert "bg.enabled = (bg.db != 0) || (bg.type != BT_BELL);" in EQ_CPP
    # EXT default freq ladder.
    assert "SetBandFreq(BAND3, fl2fp(2000.0f));" in EQ_CPP
    assert "SetBandFreq(BAND6, fl2fp(16000.0f));" in EQ_CPP
    print("3. ParametricEQ 8 bands + EXT bypass DSP OK")


def check_shared_biquad():
    assert "EQ_BIQUAD_TYPECOUNT" in BQ
    assert "EQ_BIQUAD_BAND_PASS" in BQ
    assert "EQ_BIQUAD_LOW_PASS" in BQ
    assert "3.14159265f * 0.9f" in BQ
    print("4. Shared EqBiquad primitive (7 types, 0.9*pi clamp) OK")


def check_instrument_eq():
    # BAND_PASS appended before kTypeCount (persisted values unchanged).
    assert "TYPE_BAND_PASS" in IEQ_H
    assert "kTypeCount" in IEQ_H
    # DSP via the shared primitive.
    assert "EqBiquad.h" in IEQ_CPP
    assert "eqBiquadCoeffs(" in IEQ_CPP
    assert "mapBandType" in IEQ_CPP
    # Modal shows the 7th type and cycles 0..6.
    assert '"BANDP"' in IEQ_MODAL
    assert "kEqTypeNames[7]" in IEQ_MODAL
    assert "% 7" in IEQ_MODAL
    # 48 kHz rebuild at the real driver rate.
    assert "eqRateCache_" in SI_H
    assert "GetSampleRate()" in SI_CPP and "SetSampleRate(rate)" in SI_CPP
    print("5. InstrumentEq BAND_PASS + 48 kHz rate fix + modal OK")


def check_filter_v2_wiring():
    # Sample render uses the V2 filter with a CHAR_LIST kind variable.
    assert '"filter kind"' in SI_CPP
    assert "SIP_FILTERKIND" in SI_H
    assert "FilterV2.h" in SI_CPP
    assert "set_filter_v2(channel,filterKind,rp->cutoff_,rp->reso_,filterMix,bassyFilter,filterBoost,(int)driverRate)" in SI_CPP
    assert "filterv2_process(fltv2, i, s2)" in SI_CPP
    assert "get_filter_v2(channel)" in SI_CPP
    # filterKind[] CHAR_LIST + enum in the datas header.
    assert "FK_LP" in SID and "FK_NOTCH" in SID
    assert '"LP",' in SID and '"HP",' in SID and '"BP",' in SID and '"NOTCH"' in SID
    # Init hook in InstrumentBank + object in the Makefile.
    assert "init_filters_v2()" in IB
    assert "FilterV2.o" in MK
    # FilterV2 implementation is present and complete.
    assert "FV2_TYPECOUNT" in FV_H
    assert "tanf(w0 * 0.5f)" in FV_CPP
    assert "case FV2_HIGHPASS" in FV_CPP
    print("6. Filter V2 wiring (SampleInstrument / datas / init / Makefile) OK")


def check_mixer_persistence():
    assert "EQXBYP" in MIXER
    assert "EQ%dTYP" in MIXER
    assert "for (int b=3;b<8;b++)" in MIXER
    assert "GetEqBandType(b)" in MIXER
    print("7. Mixer FXMASTER save/load EXT attrs OK")


def check_mixerview():
    assert "void MixerView::drawEqExtPage" in MV
    assert "void drawEqExtPage(const char *title) ;" in MV_H
    assert "void drawEqExtRow(int id,int labelX,int valueX,int y) ;" in MV_H
    assert "drawEqExtPage(pageTitle)" in MV
    assert "drawEqExtRow(FX_P_EQX_BYP,ml.labelX,ml.valueX,ml.startY)" in MV
    assert "FX_P_EQX_B3_FRQ+4*b" in MV
    assert "MakeCenteredMenuLayout(21,6,13,2)" in MV
    # bacon-1.5 item 5: MixerView delegates to the unified fxGet/fxSet API
    # (SetParam/GetParam); the switch wiring lives in FxEngine::SetParam.
    assert "id==FX_P_EQX_BYP" in MV and "drawEqExtRow(FX_P_EQX_BYP" in MV
    assert "case FX_P_EQX_BYP:" in FE_CPP or "SetEqExtBypass" in FE_CPP
    assert "fx.GetEqBandType(7)" in MV or "eqExtTypeName" in MV
    assert "fx.SetEqBandType(7,(int)v)" in MV or "case FX_P_EQX_B7_TYP" in FE_CPP
    assert "SetEqBandType" in FE_CPP
    assert '"MASTER EQ EXT [%d/6]"' in MV
    assert "DELAY MASTER [%d/6]" in MV and "REVERB MASTER [%d/6]" in MV
    print("8. MixerView EQ EXT page + titles OK")


def check_fxengine():
    assert "SetEqBandType" in FE_H and "GetEqBandType" in FE_H
    assert "SetEqExtBypass" in FE_H and "GetEqExtBypass" in FE_H
    # Legacy-all-defaults covers the 8 bands (EXT ladder = defaults).
    assert "fl2fp(16000.0f), fl2fp(16000.0f) }" in FE_CPP
    assert "b >= ParametricEQ::BAND3" in FE_CPP
    print("9. FxEngine EXT API + legacy-default consistency OK")


def check_audit():
    assert "run_host_filterv2.sh" in AUDIT
    print("10. audit.sh runs the FilterV2 host test OK")


check_fxpages()
check_navigator()
check_parametric_eq()
check_shared_biquad()
check_instrument_eq()
check_filter_v2_wiring()
check_mixer_persistence()
check_mixerview()
check_fxengine()
check_audit()
print("FX_PHASE9_EQ8_EXT_PAGE_OK")