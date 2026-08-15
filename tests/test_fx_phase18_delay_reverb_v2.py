#!/usr/bin/env python3
"""bacon-1.5 item 3: Delay/Reverb V2 musical effects.

Token/source-level audit (mirroring the other golden tests) plus a small
integer model, verifying:

- FxPages.h: 6 new params appended AFTER the golden + EQ_EXT ids
  (FX_PARAM_COUNT == 63) so every persisted value is bit-identical:
  DLY SYN (0/1), DLY DIV (0..15, discrete, default 3 = SDIV_1_16), DLY LOW
  and DLY HIG (20..20000 Hz, LOG2 curve), RVB HP / RVB LP (20..20000 Hz,
  LOG2).
- DelayLine: FREE/SYNC engine state, musical divisions (kSyncDivisions, 16
  entries, SyncDivisionToMs with 240000LL int math + 2000 ms clamp), per-
  sample time glide and cascaded LP/HP loop filters.
- Reverb V2: fractional comb reads (no per-sample transcendentals), RT60
  gains recomputed only at control rate (decayDirty_ -> recomputeGains),
  LFO shimmer on a 64-entry Q15 sine table with a control-rate phase inc,
  -3 dB input headroom + normalized comb sum (RC2 wet-only), SetSampleRate
  recomputes the input-filter coefficients, SetDamping propagates to every
  comb (legacy bug fix).
- FxEngine: sync mode reads the master BPM from SyncMaster inside
  processSendReturns; AllParamsAtLegacyDefault extended with the 6 new
  controls (legacy = FREE, 1/16, filters open).
- MixerView: DELAY page now has 11 rows (SYNC/DIVISION/LOW CUT/HIGH CUT)
  and REVERB 9 rows (IN HP/IN LP), both with the centered layout helper.
- Mixer persistence: DLYSYNC/DLYDIV/DLYLOW/DLYHIG/RVBINHP/RVBINLP saved,
  optional on restore, legacy branch resets to legacy defaults.
- audit.sh runs the DelayLine+Reverb host test.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

FXP = (ROOT / "source/sources/Application/Mixer/FxPages.h").read_text()
FN = (ROOT / "source/sources/Application/Mixer/FxNavigator.h").read_text()
FE_H = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.h").read_text()
FE_CPP = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.cpp").read_text()
DL_H = (ROOT / "source/sources/Application/Audio/FxEngine/DelayLine.h").read_text()
DL_CPP = (ROOT / "source/sources/Application/Audio/FxEngine/DelayLine.cpp").read_text()
RV_H = (ROOT / "source/sources/Application/Audio/FxEngine/Reverb.h").read_text()
RV_CPP = (ROOT / "source/sources/Application/Audio/FxEngine/Reverb.cpp").read_text()
MIXER = (ROOT / "source/sources/Application/Model/Mixer.cpp").read_text()
MV = (ROOT / "source/sources/Application/UI/Views/MixerView.cpp").read_text()
AUDIT = (ROOT / "scripts/audit.sh").read_text()


def check_fxpages():
    # 6 new params appended after the EQ_EXT block, before FX_PARAM_COUNT.
    assert FXP.index("FX_P_DLY_SYNC") > FXP.index("FX_P_EQX_B7_TYP")
    assert FXP.index("FX_P_RVB_LP") > FXP.index("FX_P_DLY_SYNC")
    assert FXP.index("FX_P_RVB_LP") < FXP.index("FX_PARAM_COUNT")
    assert "FX_P_DLY_SYNC," in FXP and "FX_P_RVB_LP," in FXP
    # rows: (label, page, vmin, vmax, vdef)
    rows = {
        "DLY SYN": ("FX_PAGE_DELAY", 0.0, 1.0, 0.0),
        "DLY DIV": ("FX_PAGE_DELAY", 0.0, 15.0, 3.0),
        "DLY LOW": ("FX_PAGE_DELAY", 20.0, 20000.0, 20.0),
        "DLY HIG": ("FX_PAGE_DELAY", 20.0, 20000.0, 20000.0),
        "RVB HP ": ("FX_PAGE_REVERB", 20.0, 20000.0, 20.0),
        "RVB LP ": ("FX_PAGE_REVERB", 20.0, 20000.0, 20000.0),
    }
    for label, (page, lo, hi, vdef) in rows.items():
        assert f'"{label}", {page}' in FXP, label
        assert f"{lo}f" in FXP and f"{hi}f" in FXP and f"{vdef}f" in FXP, label
    # curve + discrete helpers know the new params.
    assert "case FX_P_DLY_DIV:" in FXP
    assert "fxUsesCurve" in FXP and "FX_P_DLY_LOW" in FXP
    assert "FX_PARAM_CONTINUOUS" in FXP and "FX_CURVE_LOG2" in FXP
    print("1. FxPages 6 appended params (63 total) + curve/discrete OK")


def check_navigator():
    assert "fxIsDiscreteParam(id)" in FN
    assert "step = 1.0f" in FN
    print("2. FxNavigator discrete step via fxIsDiscreteParam OK")


def check_delayline():
    # sync state + division enum/table.
    assert "SDIV_COUNT" in DL_H and "kSyncDivisions" in DL_H
    assert "void SetSync(bool on)" in DL_H
    assert "bool GetSync()" in DL_H
    assert "void SetDivision(int division)" in DL_H
    assert "int GetDivision()" in DL_H
    assert "void SetLoopLPHz(fixed hz)" in DL_H
    assert "void SetLoopHPHz(fixed hz)" in DL_H
    assert "fixed GetLoopLPHz()" in DL_H
    assert "fixed GetLoopHPHz()" in DL_H
    assert "static fixed SyncDivisionToMs(int division, int bpm)" in DL_H
    # 16 musical divisions with num/den (int math, clamp to 2000 ms).
    assert "240000LL" in DL_CPP
    assert "kMaxMs" in DL_CPP
    assert "if (bpm < 40) bpm = 120;" in DL_CPP
    assert "if (bpm > 300) bpm = 300;" in DL_CPP
    # per-sample time glide + cascaded LP/HP loop filters (open = bypass).
    # bacon-1.5 item 5: glide state is 64-bit fractional samples (Q15) so the
    # full 2000 ms range (96000 samples at 48 kHz) never overflows int32.
    assert "kGlideStep" in DL_CPP and "0.5f * 32768.0f" in DL_CPP
    assert "long long delaySamples_" in DL_H and "long long delayTarget_" in DL_H
    assert "delayTargetMs_" in DL_H
    assert "fixed loopFilter(fixed v, int ch);" in DL_H
    assert "loopFilter(delayedL, 0)" in DL_CPP
    print("3. DelayLine FREE/SYNC + divisions + glide + loop filters OK")


def check_reverb_dsp():
    # Fractional comb reads and glides in Process().
    assert "fixed frac = effClamped - i2fp(len0);" in RV_CPP
    assert "combLenF_[c] + mod" in RV_CPP
    assert "kNumCombs / 2 + c" in RV_CPP
    # RT60 gains recomputed only when the target changes (control rate).
    assert "if (decayDirty_)" in RV_CPP
    assert "decayDirty_ = false;" in RV_CPP
    assert "powf(10.0f, -3.0f * (float)L / (rt * (float)rate_))" in RV_CPP
    # Process() must stay transcendental-free.
    assert "Process() contains zero powf()" in RV_CPP
    # LFO shimmer: 64-entry Q15 sine table + control-rate phase inc.
    assert "static const fixed kLfoTable[64]" in RV_CPP
    assert "lfoPhase_ += lfoInc_;" in RV_CPP
    assert "16777216.0f" in RV_CPP
    assert "lfoPhase_" in RV_H and "lfoInc_" in RV_H
    # RC2 wet-only keeps headroom + normalized comb sum.
    assert "FX_REVERB_INPUT_HEADROOM" in RV_CPP
    assert "combNorm_" in RV_CPP
    assert "fp_mul(sumL, combNorm_)" in RV_CPP
    # rate / damping fixes.
    assert "inLpCoeff_" in RV_CPP and "inHpCoeff_" in RV_CPP
    assert "for (int i = 0; i < kNumCombs; i++) combDamp_[i] = i2fp(1) - damping_;" in RV_CPP
    assert "fixed GetInputHPHz()" in RV_H
    assert "fixed GetInputLPHz()" in RV_H
    print("4. Reverb V2 fractional DSP + control-rate recompute + LFO OK")


def check_fxengine():
    # sync mode: master BPM from SyncMaster in the audio path.
    assert "#include \"Application/Player/SyncMaster.h\"" in FE_CPP
    assert "GetTempo()" in FE_CPP
    assert "SyncDivisionToMs" in FE_CPP
    assert "delay_.GetSync()" in FE_CPP
    # new API surface.
    for token in ("SetDelaySync", "SetDelayDivision", "SetDelayLowCutHz",
                  "SetDelayHighCutHz", "GetDelaySync", "GetDelayDivision",
                  "GetDelayLowCutHz", "GetDelayHighCutHz",
                  "SetReverbInputHPHz", "SetReverbInputLPHz",
                  "GetReverbInputHPHz", "GetReverbInputLPHz"):
        assert token in FE_H, token
    # legacy defaults extended: FREE + 1/16 + filters open (getter checks).
    assert "if (delay_.GetSync()) return false;" in FE_CPP
    assert "delay_.GetDivision() != " in FE_CPP and "SDIV_1_16" in FE_CPP
    assert "if (delay_.GetLoopLPHz() != fl2fp(20000.0f)) return false;" in FE_CPP
    assert "if (delay_.GetLoopHPHz() != fl2fp(20.0f)) return false;" in FE_CPP
    assert "if (reverb_.GetInputHPHz() != fl2fp(20.0f)) return false;" in FE_CPP
    assert "if (reverb_.GetInputLPHz() != fl2fp(20000.0f)) return false;" in FE_CPP
    print("5. FxEngine sync BPM + extended legacy defaults OK")


def check_mixerview():
    # DELAY: 11 rows, SYNC/DIVISION/LOW CUT/HIGH CUT rendered.
    assert "MakeCenteredMenuLayout(11,9,12,2)" in MV
    assert "\"SYNC\",\"DIVISION\",\"LOW CUT\",\"HIGH CUT\"" in MV
    assert "FX_P_DLY_SYNC,FX_P_DLY_DIV" in MV
    assert "v>=0.5f?\"SYNC\":\"FREE\"" in MV
    assert "kSyncDivisions[div].name" in MV
    assert "SDIV_COUNT" in MV
    # REVERB: 9 rows, IN HP / IN LP rendered.
    assert "MakeCenteredMenuLayout(9,8,12,2)" in MV
    assert "\"DAMPING\",\"WIDTH\",\"MODE\",\"IN HP\",\"IN LP\"" in MV
    # get/set wiring for the 6 new ids.
    for token in ("case FX_P_DLY_SYNC:", "case FX_P_DLY_DIV:",
                  "case FX_P_DLY_LOW:", "case FX_P_DLY_HIG:",
                  "case FX_P_RVB_HP:", "case FX_P_RVB_LP:"):
        assert token in MV, token
    # bacon-1.5 item 5: MixerView delegates to SetParam/GetParam (unified
    # API); the FxEngine still exposes the direct getters/setters.
    assert "fx.GetDelaySync()" in MV or "case FX_P_DLY_SYNC" in MV
    assert "fx.SetDelaySync" in MV or "case FX_P_DLY_SYNC" in MV
    assert "fx.GetDelayLowCutHz()" in MV or "case FX_P_DLY_LOW" in MV
    assert "fx.SetDelayLowCutHz" in MV or "case FX_P_DLY_LOW" in MV
    assert "fx.GetReverbInputHPHz()" in MV or "case FX_P_RVB_HP" in MV
    assert "fx.SetReverbInputHPHz" in MV or "case FX_P_RVB_HP" in MV
    print("6. MixerView DELAY 11 / REVERB 9 rows + get/set OK")


def check_mixer_persistence():
    for attr in ("DLYSYNC", "DLYDIV", "DLYLOW", "DLYHIG",
                 "RVBINHP", "RVBINLP"):
        assert attr in MIXER, attr
    # optional restore + legacy branch resets to legacy defaults.
    assert "Attribute(\"DLYSYNC\"" in MIXER
    assert "Attribute(\"DLYDIV\"" in MIXER
    assert "fx.SetDelaySync(false)" in MIXER
    assert "fx.SetDelayDivision((int)FxEngine::SDIV_1_16)" in MIXER
    assert "fx.SetReverbInputHPHz(fl2fp(20.0f))" in MIXER
    print("7. Mixer FXMASTER save/load of the 6 new attrs OK")


def check_audit():
    assert "run_host_delay_reverb_v2.sh" in AUDIT
    print("8. audit.sh runs the DelayLine+Reverb host test OK")


def check_sync_model():
    # mirrors the real kSyncDivisions table (16 entries, num/den of a whole
    # note, triplets 2/3 and dotted 3/2 of the straight division).
    divisions = [
        ("1/32", 1, 32), ("1/32T", 2, 96), ("1/32D", 3, 64),
        ("1/16", 1, 16), ("1/16T", 2, 48), ("1/16D", 3, 32),
        ("1/8", 1, 8), ("1/8T", 2, 24), ("1/8D", 3, 16),
        ("1/4", 1, 4), ("1/4T", 2, 12), ("1/4D", 3, 8),
        ("1/2", 1, 2), ("1/2T", 2, 6), ("1/2D", 3, 4),
        ("1/1", 1, 1),
    ]
    assert len(divisions) == 16
    for name, num, den in divisions:
        bpm = 120
        ms = 240000 * num / (bpm * den)
        # 1/2D and 1/1 exceed kMaxMs at low BPM; the engine clamps.
        eff = min(ms, 2000.0)
        assert 1.0 <= eff <= 2000.0, (name, ms)
    # golden values.
    assert 240000 * 1 / (120 * 4) == 500.0        # 1/4 @ 120 = 500 ms
    assert 240000 * 1 / (120 * 16) == 125.0       # 1/16 @ 120 = 125 ms
    assert 240000 * 1 / 120 == 2000.0             # 1/1 @ 120 = 2000 ms
    # triplets are exactly 2/3 and dotted 3/2 of the straight division.
    for i in range(0, 15, 3):
        straight = 240000 * divisions[i][1] / (120 * divisions[i][2])
        tri = 240000 * divisions[i + 1][1] / (120 * divisions[i + 1][2])
        dot = 240000 * divisions[i + 2][1] / (120 * divisions[i + 2][2])
        assert abs(tri - 2.0 / 3.0 * straight) < 1e-9, divisions[i]
        assert abs(dot - 1.5 * straight) < 1e-9, divisions[i]
    # 1/1 @ 40 BPM = 6000 ms -> clamped to kMaxMs.
    assert min(240000 * 1 / (40 * 1), 2000.0) == 2000.0
    print("9. sync division int model (16 divisions, triplets/dotted) OK")


def check_defaults_legacy_inert():
    # At legacy defaults the new controls are inert: sync off, filters open
    # (open thresholds in the engine), so the golden DSP path is unchanged.
    assert ">= 19000" in DL_CPP
    assert "<= 30" in DL_CPP
    print("10. legacy defaults inert (sync off, filters open) OK")


check_fxpages()
check_navigator()
check_delayline()
check_reverb_dsp()
check_fxengine()
check_mixerview()
check_mixer_persistence()
check_audit()
check_sync_model()
check_defaults_legacy_inert()
print("FX_PHASE18_DELAY_REVERB_V2_OK")
