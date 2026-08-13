#!/usr/bin/env python3
"""Phase 14 model tests: general fine/coarse curve editing (PLAN_FX_REDESIGN_ES.md).

Mirrors the Fase 14 generalization of the Fase 12 frequency curve:

- Wide-range proportional parameters are edited on a musical/log curve, never
  linear 1/10 steps: fine (L/R) steps by one semitone (x2^(1/12)), coarse
  (A+UP/DOWN) by one octave (x2).  Relative error is constant so the whole
  range is reachable in a bounded number of presses.
- Applied to: EQ frequencies (LO/MID/HI FRQ), delay time, reverb pre-delay and
  decay, compressor attack/release/ratio.
- Values that sit below the floor (e.g. DLY TIM / RVB PRE default to 0 while
  their vmin is above it) snap to the floor on the first upward edit instead of
  being stuck at 0; the value is clamped to [vmin, vmax].

Acceptance:
- every curve param reaches the top of its range in a bounded number of fine
  presses (<= ~160) and coarse presses (<= ~16)
- steps are proportional (relative error constant) away from the clamps
- zero-floored params are not stuck: first upward edit snaps to vmin
- down-edits clamp at vmin, up-edits clamp at vmax
- source guards: fxUsesCurve / fxEditCurve present, fxEditRow uses the curve
  path, the curve covers the expected parameter ids
"""
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

MIX = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
MIX_H = (ROOT / "source/sources/Application/Views/MixerView.h").read_text()

SEMITONE = 2.0 ** (1.0 / 12.0)

# id -> (vmin, vmax, default) mirror of kFxParams_ for the curve params.
CURVE_PARAMS = {
    "EQ LO FRQ": (20.0, 20000.0, 100.0),
    "EQ MID FRQ": (20.0, 20000.0, 1000.0),
    "EQ HI FRQ": (20.0, 20000.0, 10000.0),
    "DLY TIM": (10.0, 2000.0, 0.0),
    "RVB PRE": (0.0, 100.0, 0.0),
    "RVB DEC": (0.2, 8.0, 1.0),
    "CMP ATK": (0.1, 500.0, 15.0),
    "CMP REL": (1.0, 2000.0, 200.0),
    "CMP RAT": (1.0, 20.0, 4.0),
}


def curve_step(v, vmin, vmax, delta, coarse):
    """Mirror MixerView::fxEditCurve (Fase 14)."""
    if delta > 0:
        if v < vmin:
            v = vmin                      # below-floor snap (e.g. DLY TIM)
        elif v <= 0.0:
            v = (vmax - vmin) * 0.01      # floor at 0 starts at 1% of range
    if delta < 0 and v > vmax:
        v = vmax
    factor = 2.0 if coarse else SEMITONE
    if delta < 0:
        factor = 1.0 / factor
    for _ in range(abs(delta)):
        v *= factor
    return max(vmin, min(vmax, v))


def check_range_reachable():
    for name, (vmin, vmax, vdef) in CURVE_PARAMS.items():
        # Fine: from the default to the top in bounded presses.
        v = vdef
        fine = 0
        while v < vmax and fine < 200:
            v = curve_step(v, vmin, vmax, 1, False)
            fine += 1
        assert v == vmax, (name, v)
        assert fine <= 160, (name, fine)
        # Coarse: from the floor to the top in bounded presses.
        v = vmin
        coarse = 0
        while v < vmax and coarse < 20:
            v = curve_step(v, vmin, vmax, 1, True)
            coarse += 1
        assert v == vmax, (name, v)
        assert coarse <= 16, (name, coarse)
    print("all curve params reach the top in bounded fine/coarse presses OK")


def check_proportional_and_clamps():
    for name, (vmin, vmax, vdef) in CURVE_PARAMS.items():
        # Proportional fine step away from the clamps (start must be > 0;
        # the 0-floor base step is covered by check_zero_floor_not_stuck).
        for start in (vmin, max(vmin, vmax * 0.25), vmax * 0.75):
            if start > 0.0 and start * SEMITONE <= vmax:
                rel = curve_step(start, vmin, vmax, 1, False) / start
                assert abs(rel - SEMITONE) < 1e-4, (name, start, rel)
        # Up clamps at vmax, down clamps at vmin.
        assert curve_step(vmax, vmin, vmax, 1, False) == vmax, name
        assert curve_step(vmax, vmin, vmax, 5, True) == vmax, name
        assert curve_step(vmin, vmin, vmax, -1, False) == vmin, name
        assert curve_step(vmin, vmin, vmax, -3, True) == vmin, name
    print("proportional steps and clamp behaviour OK")


def check_zero_floor_not_stuck():
    # DLY TIM / RVB PRE default to 0 below their vmin; the first upward edit
    # snaps to the floor and then steps proportionally (never stuck at 0).
    v = 0.0
    v = curve_step(v, 10.0, 2000.0, 1, False)
    assert v > 10.0, v                                  # snapped off the floor
    assert abs(v / 10.0 - SEMITONE) < 1e-4, v           # then a semitone step
    v2 = curve_step(v, 10.0, 2000.0, 1, False)
    assert abs(v2 / v - SEMITONE) < 1e-4, v2            # stays proportional
    # RVB PRE has vmin == 0: the first upward edit starts at 1% of the range
    # (1 ms) and then steps proportionally instead of being stuck at 0.
    v = curve_step(0.0, 0.0, 100.0, 1, False)
    assert abs(v / 1.0 - SEMITONE) < 1e-4, v
    v = curve_step(1.0, 0.0, 100.0, 1, False)
    assert abs(v / 1.0 - SEMITONE) < 1e-4, v
    print("below-floor values snap instead of getting stuck OK")


def check_source_guards():
    assert "void MixerView::fxEditCurve" in MIX
    assert "fxEditCurve" in MIX_H
    # F3-4a: fxUsesCurve/fxEditCurveValue moved to FxPages.h.
    fxp = (ROOT / "source/sources/Application/Mixer/FxPages.h").read_text()
    assert "bool fxUsesCurve" in fxp
    assert "fxEditCurveValue" in fxp
    # fxEditRow routes curve params through the curve editor.
    idx = MIX.index("void MixerView::fxEditRow")
    block = MIX[idx:idx + 1200]
    assert "fxUsesCurve(targetId)" in block
    assert "fxEditCurve(targetId,delta,coarse)" in block
    # The curve covers frequencies + wide-range time/ratio params.
    for tok in ("FX_P_DLY_TIME", "FX_P_RVB_PRE", "FX_P_RVB_DEC",
                "FX_P_CMP_ATK", "FX_P_CMP_REL", "FX_P_CMP_RAT"):
        assert tok in MIX
    print("source guards OK")


check_range_reachable()
check_proportional_and_clamps()
check_zero_floor_not_stuck()
check_source_guards()
print("FX_EDIT_CURVE_PHASE14_OK")
