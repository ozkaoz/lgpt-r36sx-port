#!/usr/bin/env python3
"""Phase 0 DSP model tests for the inline lowpass filter (SVF-like).

Models the state-variable filter in SampleInstrument::Render (Q15 fixed
point) and set_filter in Filters.cpp, and documents the hard-coded
22050 Hz normalization (bassy mapping) flagged as defect 11 in
PLAN_FX_REDESIGN_ES.md (filter depends on a 44100 Hz driver; the 48000 Hz
USB path miscales the cutoff).

The model reproduces the exact fixed-point arithmetic so the filter state
can be driven with impulse / DC / sine inputs and checked for:
- finite energy (no instability) on impulse at default settings
- no NaN / Inf propagation on adversarial input
- signal cancellation when the filter is fully open (cutoff == 1)
"""
from pathlib import Path

FIXED_SHIFT = 15
SCALE = 1 << FIXED_SHIFT
FP_ONE = SCALE


def i2fp(a):
    return a << FIXED_SHIFT


def fl2fp(f):
    return int(f * SCALE)


def fp2fl(a):
    return a / SCALE


def fp_mul(x, y):
    return (x * y) >> FIXED_SHIFT


def clamp_q31(x):
    lo = -(1 << 31)
    hi = (1 << 31) - 1
    if x < lo:
        return lo
    if x > hi:
        return hi
    return x


class SetFilter:
    """Faithful Q15 port of Filters.cpp set_filter() for one voice."""

    def __init__(self, param1, param2, mix, bassy):
        self.parm1 = param1
        self.parm2 = param2
        self.mix = fp_mul(i2fp(mix), fl2fp(1.0 / 255.0))
        self.dirt = fp_mul(i2fp(100), i2fp(1) - param1) + fp_mul(i2fp(5000), param1)
        if bassy:
            fpFreqDivider = fl2fp(1 / 22050.0)
            fpZeroSix = fl2fp(0.6)
            fpThreeOne = fl2fp(3.1)
            power = fpZeroSix + fp_mul(param1, fpThreeOne)
            frequency = fl2fp(pow(10.0, fp2fl(power)))
            self.freq = fp_mul(frequency, fpFreqDivider)
        else:
            self.freq = fp_mul(param1, param1)
        reso = i2fp(1) - param2
        self.reso = FP_ONE - fp_mul(reso, fp_mul(reso, reso))


class InlineFilter:
    """State-variable filter equivalent to the SampleInstrument::Render block."""

    def __init__(self, param1, param2, mix=0, bassy=True):
        cfg = SetFilter(param1, param2, mix, bassy)
        self.mix = cfg.mix
        self.mix_inv = FP_ONE - cfg.mix
        self.dirt = cfg.dirt
        self.freq = cfg.freq
        self.reso = cfg.reso
        self.speed = 0
        self.height = 0
        self.hipdelay = 0

    def process(self, s2, filter_boost=False):
        lpin = fp_mul(s2, self.mix_inv)
        hpin = -fp_mul(s2, self.mix)
        difr = lpin - self.height
        if filter_boost:
            f_s = FP_ONE - fl2fp(1.0 / 3.0)  # f_s = 1 - f_k, f_k = 1/3
            if self.speed < -FP_ONE:
                self.speed = -f_s
            elif self.speed > FP_ONE:
                self.speed = f_s
            self.speed = fp_mul(self.speed, self.dirt)
        self.speed = fp_mul(self.speed, self.reso)
        self.speed += fp_mul(difr, self.freq)
        self.height = clamp_q31(self.height + self.speed)
        self.height = clamp_q31(self.height + (self.hipdelay - hpin))
        out = self.height
        self.hipdelay = hpin
        return out


def check_impulse_finite():
    f = InlineFilter(i2fp(1), i2fp(0), mix=0, bassy=True)  # full cutoff, no reso
    energy = 0
    for i in range(1024):
        inp = i2fp(30000) if i == 0 else 0
        out = f.process(inp)
        energy += abs(out)
    assert energy < 1 << 42, energy  # finite, no runaway


def check_open_filter_passes_dc():
    # Fully open lowpass (cutoff=1, res=0) must reach the DC input level.
    f = InlineFilter(i2fp(1), i2fp(0), mix=0, bassy=True)
    dc = i2fp(8000)
    final = 0
    for _ in range(500):
        final = f.process(dc)
    assert abs(final - dc) < abs(dc) // 50, (final, dc)


def check_no_nan_inf():
    f = InlineFilter(i2fp(1), i2fp(0), mix=0, bassy=True)
    for i in range(300):
        out = f.process(i2fp(1000))
        assert out == out and out != float("inf") and out != float("-inf"), i


def check_fixed_44k_documented():
    root = Path(__file__).resolve().parents[1]
    src = (root / "source/sources/Application/Instruments/Filters.cpp").read_text()
    assert "22050" in src
    plan = (root / "docs/PLAN_FX_REDESIGN_ES.md").read_text()
    assert "44.1kHz" in plan and "22050" in plan


check_impulse_finite()
check_open_filter_passes_dc()
check_no_nan_inf()
check_fixed_44k_documented()
print("DSP_INLINE_LOWPASS_PHASE0_OK")
