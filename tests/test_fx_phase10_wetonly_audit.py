#!/usr/bin/env python3
"""Phase 10 audit tests: FX pages clean + wet-only semantics
(PLAN_FX_REDESIGN_ES.md, Fase 10, updated for RC2 point 3).

Two-part audit:

1) "retirar DLY SND/RET + RVB SND/RET de páginas FX" (Fase 6):
   - the master param table has no DLY SND / DLY RET / RVB SND / RVB RET rows;
   - sends are per-track (Mixer CHANNEL attrs) and per-instrument (PARAMs);
   - returns are fixed helpers now surfaced by the MIX page FX RETURNS (Fase 9).

2) wet-only semantics:
   - DELAY keeps the documented dry/wet crossfade (DLY MIX): default 1.0 (full
     wet) so at the default the delay return is wet-only (dryMix = 1 - mix = 0),
     which is the correct send/return behaviour (the dry signal already lives in
     the master bus).  Lowering MIX below 1.0 introduces a dry component into the
     return: that is the documented crossfade the Phase 2 tests verify.
   - REVERB is RC2 wet-only (point 3.1): no internal dry*dryMix term at all.
     RVB MIX is gone from the UI; the audible level is the instrument send + the
     Mixer REVERB RETURN.  Reverb::Process delivers only the processed (wet)
     signal; mixCur_ is a smoothed wet gain (1.0 full wet, 0 while bypassed)
     and the tail keeps running.  The comb sum is normalized (combNorm_) and
     the input gets -3 dB headroom before the diffusers (point 3.2).

Acceptance:
- the param table has no global SEND/RET rows
- DLY MIX defaults to 1.0 in the table and in the engine (delay crossfade)
- RVB MIX is no longer a row in the param table
- the delay crossfade at mix=1.0 leaks no dry into the output and is smooth
- the reverb source is wet-only: no dryMix term, wet gain = mixCur_,
  combNorm_ normalization and input headroom present
- FxEngine routes the reverb wet return back with the master return gain
- source guards: engine sets full-wet on construction, mix setter clamps
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SHIFT = 15
SCALE = 1 << SHIFT


def fl2fp(f):
    return int(round(f * SCALE))


def i2fp(i):
    return i << SHIFT


def fp_mul(a, b):
    return (a * b) >> SHIFT


def fp2fl(a):
    return a / SCALE


# ---------------------------------------------------------------------------
# Delay dry/wet crossfade model (mirror DelayLine.cpp wet mix)
# ---------------------------------------------------------------------------
class DelayCrossfadeModel:
    def __init__(self, mix=1.0, bypass=False, smooth=0.001):
        self.mix = fl2fp(mix)
        self.mixCur = fl2fp(mix)
        self.bypass = bypass
        self.smooth = fl2fp(smooth)

    def target(self):
        return 0 if self.bypass else self.mix

    def process(self, input_samples, processed):
        out = []
        for x, w in zip(input_samples, processed):
            self.mixCur = self.mixCur + fp_mul(self.smooth, self.target() - self.mixCur)
            wetMix = self.mixCur
            dryMix = i2fp(1) - wetMix
            out.append(fp_mul(x, dryMix) + fp_mul(w, wetMix))
        return out


def check_no_send_ret_rows():
    src = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
    for token in ("\"DLY SND\"", "\"DLY RET\"", "\"RVB SND\"", "\"RVB RET\""):
        assert token not in src, token
    # returns are master controls now, surfaced on the MIX page (Fase 9)
    assert "FX RETURNS" in src
    assert "drawMixReturns" in src
    print("no global SEND/RET rows on the FX pages OK")


def check_delay_mix_default_full_wet():
    # engine constructor sets full wet
    fxe_cpp = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.cpp").read_text()
    assert "SetMix(i2fp(1))" in fxe_cpp
    # FxParamSpec table: DLY MIX row belongs to DELAY and defaults to 1.0
    # (the third float column is vdef).
    import re
    mix_cpp = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
    m = re.search(
        r'\{\s*"DLY MIX",\s*FX_PAGE_DELAY,\s*(-?\d+\.\d+f),\s*(-?\d+\.\d+f),\s*(-?\d+\.\d+f)',
        mix_cpp)
    assert m, "DLY MIX"
    vdef = m.group(3)
    assert float(vdef[:-1]) == 1.0, vdef
    print("DLY MIX default to 1.0 (full wet crossfade) OK")


def check_rvb_mix_row_gone():
    # RC2 (point 3.1): RVB MIX was removed from the param table and the page.
    mix_cpp = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
    mix_h = (ROOT / "source/sources/Application/Views/MixerView.h").read_text()
    assert "\"RVB MIX\"" not in mix_cpp
    assert "FX_P_RVB_MIX" not in mix_h
    # the reverb page still exposes the full wet tail + mode + bypass
    for token in ("FX_P_RVB_PRE", "FX_P_RVB_DEC", "FX_P_RVB_SIZ",
                  "FX_P_RVB_DMP", "FX_P_RVB_WID", "FX_P_RVB_MODE",
                  "FX_P_RVB_BYP"):
        assert token in mix_h, token
    print("RVB MIX row removed from the param table / page OK")


def check_delay_full_wet_leaks_no_dry():
    m = DelayCrossfadeModel(mix=1.0)
    inp = [fl2fp(x) for x in (0.2, -0.4, 0.9, -0.8)]
    wet = [fl2fp(x * 0.5) for x in (1.0, 1.0, 1.0, 1.0)]  # arbitrary processed
    out = m.process(inp, wet)
    # at steady full-wet the dry term (fp_mul(inp, 0)) contributes nothing
    for o, w in zip(out, wet):
        assert o == w, (fp2fl(o), fp2fl(w))
    print("delay mix=1.0 leaks no dry into the return OK")


def check_delay_crossfade_smooth():
    m = DelayCrossfadeModel(mix=0.5)
    inp = [fl2fp(0.5)] * 4000
    wet = [fl2fp(0.1)] * 4000
    out = m.process(inp, wet)
    # a step is not possible: the one-pole limits the per-sample delta
    prev = out[0]
    max_step = 0
    for o in out[1:]:
        step = abs(o - prev)
        if step > max_step:
            max_step = step
        prev = o
    assert max_step <= i2fp(1) // 1000, fp2fl(max_step)
    print("delay dry/wet crossfade is smooth (no step) OK")


def check_src_guards():
    dl = (ROOT / "source/sources/Application/Audio/FxEngine/DelayLine.cpp").read_text()
    rv = (ROOT / "source/sources/Application/Audio/FxEngine/Reverb.cpp").read_text()
    # Delay keeps the documented dry/wet crossfade.
    assert "targetMix = bypass_ ? 0 : mix_" in dl
    assert "dryMix = i2fp(1) - wetMix" in dl
    # Reverb is RC2 wet-only: no dry/wet mix variable anywhere (only the
    # explanatory comment "no dry*dryMix term" is allowed).
    assert "fixed dryMix" not in rv
    assert "wetMix" not in rv
    assert "targetMix" not in rv
    # wet-only output: smoothed wet gain, normalized comb sum, -3 dB headroom.
    assert "fixed wetGain = mixCur_" in rv
    assert "targetGain = bypass_ ? 0 : i2fp(1)" in rv
    assert "combNorm_" in rv
    assert "FX_REVERB_INPUT_HEADROOM" in rv
    assert "fp_mul(saturate(wetL), wetGain)" in rv
    # the return path adds the effect wet bus back with the master return gain
    fxe = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.cpp").read_text()
    assert "delayReturn_" in fxe and "reverbReturn_" in fxe
    assert "fp_mul(buses_.returnReverb_[i], reverbReturn_)" in fxe
    print("wet-only audit source guards OK")


check_no_send_ret_rows()
check_delay_mix_default_full_wet()
check_rvb_mix_row_gone()
check_delay_full_wet_leaks_no_dry()
check_delay_crossfade_smooth()
check_src_guards()
print("FX_WETONLY_AUDIT_PHASE10_OK")
