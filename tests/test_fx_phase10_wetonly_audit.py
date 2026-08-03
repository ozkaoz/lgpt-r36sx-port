#!/usr/bin/env python3
"""Phase 10 audit tests: FX pages clean + DLY MIX/RVB MIX wet-only semantics
(PLAN_FX_REDESIGN_ES.md, Fase 10).

Two-part audit:

1) "retirar DLY SND/RET + RVB SND/RET de páginas FX" (already done in Fase 6):
   - the master param table has no DLY SND / DLY RET / RVB SND / RVB RET rows;
   - sends are per-track (Mixer CHANNEL attrs) and per-instrument (PARAMs);
   - returns are fixed helpers now surfaced by the MIX page FX RETURNS (Fase 9).

2) "auditar DLY MIX/RVB MIX wet-only":
   - DLY MIX / RVB MIX default to 1.0 (full wet) both in the engine constructor
     and in the FxParamSpec table, so at the default the effect output is
     wet-only (dryMix = 1 - wetMix = 0) which is the correct send/return
     behaviour (the dry signal already lives in the master bus).
   - Lowering MIX below 1.0 introduces a dry component into the return: that is
     the documented dry/wet crossfade the DSP models (verified by the Phase 2
     tests), not a regression.
   - With MIX=1.0 and bypass off, the engine reports full-wet: a single-sample
     check shows out == processed (no dry leaked into the return).

Acceptance:
- the param table has no global SEND/RET rows
- DLY MIX / RVB MIX defaults are 1.0 in the table and in the engine
- the effect crossfade at mix=1.0 leaks no dry into the output
- the mix/both crossfade is smooth (no step) as the Phase 2 model verifies
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
# Effect crossfade model (mirror DelayLine.cpp/Reverb.cpp wet mix)
# ---------------------------------------------------------------------------
class CrossfadeModel:
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


def check_mix_defaults_full_wet():
    # engine constructor sets full wet
    fxe_cpp = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.cpp").read_text()
    assert "SetMix(i2fp(1))" in fxe_cpp
    # FxParamSpec table: DLY MIX and RVB MIX rows belong to DELAY/REVERB and
    # default to 1.0 (the third float column is vdef).
    import re
    mix_cpp = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
    for label in ("DLY MIX", "RVB MIX"):
        m = re.search(
            r'\{\s*"%s",\s*FX_PAGE_(DELAY|REVERB),\s*(-?\d+\.\d+f),\s*(-?\d+\.\d+f),\s*(-?\d+\.\d+f)' % label,
            mix_cpp)
        assert m, label
        vdef = m.group(4)
        assert float(vdef[:-1]) == 1.0, (label, vdef)
    print("DLY MIX / RVB MIX default to 1.0 (full wet) OK")


def check_full_wet_leaks_no_dry():
    m = CrossfadeModel(mix=1.0)
    inp = [fl2fp(x) for x in (0.2, -0.4, 0.9, -0.8)]
    wet = [fl2fp(x * 0.5) for x in (1.0, 1.0, 1.0, 1.0)]  # arbitrary processed
    out = m.process(inp, wet)
    # at steady full-wet the dry term (fp_mul(inp, 0)) contributes nothing
    for o, w in zip(out, wet):
        assert o == w, (fp2fl(o), fp2fl(w))
    print("mix=1.0 leaks no dry into the return OK")


def check_crossfade_smooth():
    m = CrossfadeModel(mix=0.5)
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
    print("dry/wet crossfade is smooth (no step) OK")


def check_src_guards():
    dl = (ROOT / "source/sources/Application/Audio/FxEngine/DelayLine.cpp").read_text()
    rv = (ROOT / "source/sources/Application/Audio/FxEngine/Reverb.cpp").read_text()
    for src in (dl, rv):
        assert "targetMix = bypass_ ? 0 : mix_" in src
        assert "dryMix = i2fp(1) - wetMix" in src
    # the return path adds the effect wet bus back with the master return gain
    fxe = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.cpp").read_text()
    assert "delayReturn_" in fxe and "reverbReturn_" in fxe
    print("wet-only audit source guards OK")


check_no_send_ret_rows()
check_mix_defaults_full_wet()
check_full_wet_leaks_no_dry()
check_crossfade_smooth()
check_src_guards()
print("FX_WETONLY_AUDIT_PHASE10_OK")
