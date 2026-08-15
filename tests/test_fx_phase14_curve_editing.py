#!/usr/bin/env python3
"""Phase 14 model tests: percent-based editing of wide-range params
(FXP_DESCRIPTORS_V1, bacon-1.5 item 1).

Supersedes the Fase 14 semitone/octave curve: continuous params are edited
exclusively in the shared 0..100 % view (fine +1, coarse +10) and mapped to
the natural range through the FxParamDescriptor layer:

- LOG2 params (frequencies and time/ratio: DLY TIM, RVB DEC, EQ FRQ x3,
  EQ Q x3, CMP ATK/REL/RAT) map percent -> dsp = vmin * (vmax/vmin)^(p/100),
  so an equal percent step is an equal ratio (geometric) step: the whole
  range is reachable in <= 100 fine / <= 10 coarse presses.
- LINEAR params (RVB PRE, gains/mixes) map p -> vmin + p/100*(vmax-vmin).
- Values below the floor (e.g. DLY TIM defaults to 0 < vmin) snap to the
  floor on the first upward edit instead of being stuck at 0.
- Switches keep the golden 0/1 stepping (not covered here).

Acceptance:
- every continuous param reaches the top of its range in a bounded number of
  fine presses (<= 100) and coarse presses (<= 10)
- fine steps are proportional (constant ratio = (vmax/vmin)^(1/100)) away
  from the clamps; coarse steps use ratio^(1/10)
- below-floor values are not stuck: first upward edit snaps to vmin
- down-edits clamp at vmin, up-edits clamp at vmax
- source guards: FxPages.h exposes the descriptor helpers (fxPercentToDspId /
  fxDspToPercentId / fxIsPercentParam), FxNavigator::EditValue routes the
  step math through the percent layer, MixerView::fxEditRow uses it
"""
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

MIX = (ROOT / "source/sources/Application/UI/Views/MixerView.cpp").read_text()
MIX_H = (ROOT / "source/sources/Application/UI/Views/MixerView.h").read_text()

# id -> (vmin, vmax, default, linear?) mirror of kFxParams_ + kFxParamMeta_.
CURVE_PARAMS = {
    "EQ LO FRQ": (20.0, 20000.0, 100.0, False),
    "EQ MID FRQ": (20.0, 20000.0, 1000.0, False),
    "EQ HI FRQ": (20.0, 20000.0, 10000.0, False),
    "DLY TIM": (10.0, 2000.0, 0.0, False),
    "RVB DEC": (0.2, 8.0, 1.0, False),
    "CMP ATK": (0.1, 500.0, 15.0, False),
    "CMP REL": (1.0, 2000.0, 200.0, False),
    "CMP RAT": (1.0, 20.0, 4.0, False),
    "RVB PRE": (0.0, 100.0, 0.0, True),
}


def percent_step(v, vmin, vmax, delta, coarse, linear=False):
    """Mirror FxNavigator::EditValue (percent layer, FXP_DESCRIPTORS_V1)."""
    if linear:
        p = round((v - vmin) / (vmax - vmin) * 100) if vmax != vmin else 0
    else:
        p = round(math.log(max(v, vmin) / vmin) / math.log(vmax / vmin) * 100)
    p += (10 if coarse else 1) * delta
    p = max(0, min(100, p))
    if linear:
        return vmin + p / 100.0 * (vmax - vmin)
    return vmin * (vmax / vmin) ** (p / 100.0)


def check_range_reachable():
    for name, (vmin, vmax, vdef, linear) in CURVE_PARAMS.items():
        # Fine: from the default to the top in bounded presses.
        v = vdef
        fine = 0
        while v < vmax and fine < 200:
            v = percent_step(v, vmin, vmax, 1, False, linear)
            fine += 1
        assert v == vmax, (name, v)
        assert fine <= 100, (name, fine)
        # Coarse: from the floor to the top in bounded presses.
        v = vmin
        coarse = 0
        while v < vmax and coarse < 20:
            v = percent_step(v, vmin, vmax, 1, True, linear)
            coarse += 1
        assert v == vmax, (name, v)
        assert coarse <= 10, (name, coarse)
    print("all continuous params reach the top in bounded fine/coarse presses OK")


def check_proportional_and_clamps():
    for name, (vmin, vmax, vdef, linear) in CURVE_PARAMS.items():
        if linear:
            continue
        ratio_fine = (vmax / vmin) ** 0.01
        ratio_coarse = (vmax / vmin) ** 0.1
        for start in (vmin, vmax * 0.25, vmax * 0.75):
            # Percent editing quantizes on the 0..100 grid: proportionality
            # is exact between grid points, so snap the start to its grid
            # value first (that is also what an edit round-trips to).
            p0 = round(math.log(start / vmin) / math.log(vmax / vmin) * 100)
            v0 = vmin * (vmax / vmin) ** (p0 / 100.0)
            if p0 < 100:
                rel = percent_step(v0, vmin, vmax, 1, False) / v0
                assert abs(rel - ratio_fine) < 1e-4, (name, v0, rel)
            if p0 < 90:
                rel = percent_step(v0, vmin, vmax, 1, True) / v0
                assert abs(rel - ratio_coarse) < 1e-4, (name, v0, rel)
        # Up clamps at vmax, down clamps at vmin.
        assert percent_step(vmax, vmin, vmax, 1, False) == vmax, name
        assert percent_step(vmax, vmin, vmax, 5, True) == vmax, name
        assert percent_step(vmin, vmin, vmax, -1, False) == vmin, name
        assert percent_step(vmin, vmin, vmax, -3, True) == vmin, name
    print("proportional steps and clamp behaviour OK")


def check_zero_floor_not_stuck():
    # DLY TIM defaults to 0 below its vmin: the first upward edit snaps to
    # the floor (p0 -> p1 = vmin * ratio^0.01) and then stays proportional.
    v = 0.0
    v = percent_step(v, 10.0, 2000.0, 1, False)
    assert v == 10.0 * 200.0 ** 0.01, v
    v2 = percent_step(v, 10.0, 2000.0, 1, False)
    assert abs(v2 / v - 200.0 ** 0.01) < 1e-4, v2
    # RVB PRE has vmin == 0 and is LINEAR: the first upward edit is exactly
    # 1% of the range (1 ms) instead of being stuck at 0.
    v = percent_step(0.0, 0.0, 100.0, 1, False, True)
    assert abs(v - 1.0) < 1e-6, v
    v = percent_step(50.0, 0.0, 100.0, 1, False, True)
    assert abs(v - 51.0) < 1e-6, v
    print("below-floor values snap instead of getting stuck OK")


def check_source_guards():
    assert "void MixerView::fxEditCurve" in MIX
    assert "fxEditCurve" in MIX_H
    # FXP_DESCRIPTORS_V1: the percent layer lives in FxPages.h, the step math
    # is routed through the pure FxNavigator::EditValue, not inline in view.
    fxp = (ROOT / "source/sources/Application/Mixer/FxPages.h").read_text()
    assert "bool fxUsesCurve" in fxp
    assert "fxPercentToDspId" in fxp
    assert "fxDspToPercentId" in fxp
    assert "bool fxIsPercentParam" in fxp
    nav = (ROOT / "source/sources/Application/Mixer/FxNavigator.h").read_text()
    assert "static float EditValue" in nav
    assert "fxIsPercentParam(id)" in nav
    assert "fxPercentToDspId(id" in nav
    idx = MIX.index("void MixerView::fxEditRow")
    block = MIX[idx:idx + 1200]
    assert "FxNavigator::EditValue(targetId" in block
    # The percent layer covers frequencies + wide-range time/ratio params.
    for tok in ("FX_P_DLY_TIME", "FX_P_RVB_PRE", "FX_P_RVB_DEC",
                "FX_P_CMP_ATK", "FX_P_CMP_REL", "FX_P_CMP_RAT"):
        assert tok in MIX
    print("source guards OK")


check_range_reachable()
check_proportional_and_clamps()
check_zero_floor_not_stuck()
check_source_guards()
print("FX_EDIT_CURVE_PHASE14_OK")