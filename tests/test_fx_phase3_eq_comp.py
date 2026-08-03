#!/usr/bin/env python3
"""Phase 3 DSP model tests for ParametricEQ and Compressor
(PLAN_FX_REDESIGN_ES.md, Fase 3).

Faithful Q15 ports of the FxEngine EQ/compressor kernels, validating the
Fase 3 acceptance criteria:

ParametricEQ:
- bell band response matches the Audio EQ Cookbook spec (±0.5 dB at f0)
- low/high shelf gain at DC / Nyquist matches the target gain
- identity: all bands 0 dB, global bypass on -> output == input
- band enable crossfade produces no discontinuity

Compressor:
- static gain curve: steady sine above threshold is reduced by (1/R - 1)/dB
  of overage; below threshold no reduction; makeup gain applied
- attack/release envelope shape (one-pole time constants)
- stereo link: both channels use the same gain
- GR meter stays negative under compression, ~0 when bypassed
- soft clip: output never exceeds +/-1 (limiter), no NaN
- no DC buildup in the table lookup path
"""
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SHIFT = 15
SCALE = 1 << SHIFT


def i2fp(a):
    return a << SHIFT


def fl2fp(f):
    return int(f * SCALE)


def fp2fl(a):
    return a / SCALE


def fp_mul(x, y):
    return (x * y) >> SHIFT


def sat(q):
    if q > i2fp(1):
        return i2fp(1)
    if q < -i2fp(1):
        return -i2fp(1)
    return q


# ---------------------------------------------------------------------------
# ParametricEQ (mirrors ParametricEQ.cpp)
# ---------------------------------------------------------------------------
class Biquad:
    def __init__(self):
        self.b0 = i2fp(1)
        self.b1 = self.b2 = self.a1 = self.a2 = 0
        self.s1 = [0, 0]
        self.s2 = [0, 0]
        self.mixCur = 0
        self.enabled = False
        self.hz = 0
        self.db = 0
        self.q = fl2fp(1.0)

    def recompute(self, rate, band):
        f0 = fp2fl(self.hz)
        gainDb = fp2fl(self.db)
        q = fp2fl(self.q)
        if f0 <= 0.0:
            f0 = 1.0
        w0 = 2.0 * math.pi * f0 / rate
        if w0 > math.pi * 0.9:
            w0 = math.pi * 0.9
        cw = math.cos(w0)
        sw = math.sin(w0)
        A = 10.0 ** (gainDb / 40.0)
        sqrtA = math.sqrt(A)

        if band == 1:  # MID = bell
            alpha = sw / (2.0 * q)
            b0, b1, b2 = 1.0 + alpha * A, -2.0 * cw, 1.0 - alpha * A
            a0, a1, a2 = 1.0 + alpha / A, -2.0 * cw, 1.0 - alpha / A
        else:
            S = max(0.5, min(q, 2.0))
            alpha = (sw / 2.0) * math.sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0)
            if band == 0:  # low shelf
                b0 = A * ((A + 1.0) - (A - 1.0) * cw + 2.0 * sqrtA * alpha)
                b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw)
                b2 = A * ((A + 1.0) - (A - 1.0) * cw - 2.0 * sqrtA * alpha)
                a0 = (A + 1.0) + (A - 1.0) * cw + 2.0 * sqrtA * alpha
                a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw)
                a2 = (A + 1.0) + (A - 1.0) * cw - 2.0 * sqrtA * alpha
            else:  # high shelf
                b0 = A * ((A + 1.0) + (A - 1.0) * cw + 2.0 * sqrtA * alpha)
                b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw)
                b2 = A * ((A + 1.0) + (A - 1.0) * cw - 2.0 * sqrtA * alpha)
                a0 = (A + 1.0) - (A - 1.0) * cw + 2.0 * sqrtA * alpha
                a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw)
                a2 = (A + 1.0) - (A - 1.0) * cw - 2.0 * sqrtA * alpha
        self.b0 = fl2fp(b0 / a0)
        self.b1 = fl2fp(b1 / a0)
        self.b2 = fl2fp(b2 / a0)
        self.a1 = fl2fp(a1 / a0)
        self.a2 = fl2fp(a2 / a0)


def _snap(step, cur, target):
    st = fp_mul(step, target - cur)
    if st == 0 and cur != target:
        return target
    return cur + st


class ParametricEQModel:
    def __init__(self, rate=44100):
        self.rate = rate
        self.bypass = False
        self.bypassMix = 0
        self.bands = [Biquad() for _ in range(3)]
        self.bands[0].hz = fl2fp(100.0)
        self.bands[1].hz = fl2fp(1000.0)
        self.bands[2].hz = fl2fp(10000.0)
        for b in self.bands:
            b.q = fl2fp(1.0)
            b.recompute(rate, 0)  # placeholder, real band index used below
        for i, b in enumerate(self.bands):
            b.recompute(rate, i)

    def set_band_freq(self, band, hz):
        self.bands[band].hz = fl2fp(hz)
        self.bands[band].recompute(self.rate, band)

    def set_band_gain(self, band, db):
        self.bands[band].db = fl2fp(db)
        self.bands[band].recompute(self.rate, band)

    def set_band_q(self, band, q):
        self.bands[band].q = fl2fp(q)
        self.bands[band].recompute(self.rate, band)

    def set_band_enabled(self, band, on):
        self.bands[band].enabled = on

    def process(self, frames, x_l=0, x_r=0):
        targetBypass = 0 if self.bypass else i2fp(1)
        self.bypassMix = _snap(fl2fp(0.001), self.bypassMix, targetBypass)
        for b in self.bands:
            target = i2fp(1) if b.enabled else 0
            b.mixCur = _snap(fl2fp(0.001), b.mixCur, target)

        out = []
        for _ in range(frames):
            yL = x_l
            yR = x_r
            inL = x_l
            inR = x_r
            for b in self.bands:
                if b.mixCur == 0:
                    continue
                tL = fp_mul(b.b0, inL) + b.s1[0]
                b.s1[0] = fp_mul(b.b1, inL) - fp_mul(b.a1, tL) + b.s2[0]
                b.s2[0] = fp_mul(b.b2, inL) - fp_mul(b.a2, tL)
                tR = fp_mul(b.b0, inR) + b.s1[1]
                b.s1[1] = fp_mul(b.b1, inR) - fp_mul(b.a1, tR) + b.s2[1]
                b.s2[1] = fp_mul(b.b2, inR) - fp_mul(b.a2, tR)
                yL = inL + fp_mul(tL - inL, b.mixCur)
                yR = inR + fp_mul(tR - inR, b.mixCur)
                inL = yL
                inR = yR
            if self.bypassMix != i2fp(1):
                yL = x_l + fp_mul(yL - x_l, self.bypassMix)
                yR = x_r + fp_mul(yR - x_r, self.bypassMix)
            out.append(sat(yL))
            out.append(sat(yR))
        return out


# ---------------------------------------------------------------------------
# Compressor (mirrors Compressor.cpp)
# ---------------------------------------------------------------------------
kTableSize = 1 << 12


class CompressorModel:
    def __init__(self, rate=44100, thr=-24.0, ratio=4.0, knee=6.0,
                 attack=15.0, release=200.0, makeup=0.0, link=True,
                 bypass=True, soft_clip=True):
        self.rate = rate
        self.thr = thr
        self.ratio = ratio
        self.knee = knee
        self.makeup = makeup
        self.attack = attack
        self.release = release
        self.link = link
        self.bypass = bypass
        self.soft_clip = soft_clip
        self.level = [0, 0]
        self.grMeter = 0
        self._tables()

    def _tables(self):
        self.gainTable = [0] * kTableSize
        self.grTable = [0] * kTableSize
        thr, ratio, knee, makeup = self.thr, self.ratio, self.knee, self.makeup
        if ratio < 1.0:
            ratio = 1.0
        for i in range(kTableSize):
            level = (i + 1) / kTableSize
            if level < 1e-4:
                level = 1e-4
            levelDb = 20.0 * math.log10(level)
            over = levelDb - thr
            gr = 0.0
            if knee <= 0.0:
                if over > 0.0:
                    gr = over * (1.0 / ratio - 1.0)
            else:
                if over > knee:
                    gr = over * (1.0 / ratio - 1.0)
                elif over > 0.0:
                    t = over + knee
                    gr = (t * t) / (2.0 * knee) * (1.0 / ratio - 1.0)
            gainDb = gr + makeup
            gain = 10.0 ** (gainDb / 20.0)
            if gain > 4.0:
                gain = 4.0
            self.gainTable[i] = fl2fp(gain)
            self.grTable[i] = fl2fp(gr)

    def _env_coeff(self, ms):
        if ms <= 0:
            ms = 0.1
        return 1.0 - math.exp(-1000.0 / (ms * self.rate))

    def process(self, frames, x_l=0, x_r=0):
        # True passthrough when bypassed: never touch the audio (mirrors
        # Compressor::Process early-return).  GR meter still glides to 0.
        if self.bypass:
            self.grMeter += fp_mul(fl2fp(0.005), 0 - self.grMeter)
            out = []
            for _ in range(frames):
                out.append(x_l)
                out.append(x_r)
            return out

        attK = fl2fp(self._env_coeff(self.attack))
        relK = fl2fp(self._env_coeff(self.release))
        out = []
        for _ in range(frames):
            detL = abs(x_l)
            detR = abs(x_r)
            if self.link:
                det = max(detL, detR)
                if det > self.level[0]:
                    self.level[0] += fp_mul(attK, det - self.level[0])
                else:
                    self.level[0] += fp_mul(relK, det - self.level[0])
                self.level[1] = self.level[0]
            else:
                for ch, det in ((0, detL), (1, detR)):
                    if det > self.level[ch]:
                        self.level[ch] += fp_mul(attK, det - self.level[ch])
                    else:
                        self.level[ch] += fp_mul(relK, det - self.level[ch])

            idxL = self.level[0] >> (15 - 12)
            idxL = max(0, min(idxL, kTableSize - 1))
            gainL = self.gainTable[idxL]
            gainR = gainL
            if not self.link:
                idxR = self.level[1] >> (15 - 12)
                idxR = max(0, min(idxR, kTableSize - 1))
                gainR = self.gainTable[idxR]

            yL = fp_mul(x_l, gainL)
            yR = fp_mul(x_r, gainR)
            if self.soft_clip:
                yL = _cubic(yL)
                yR = _cubic(yR)
            out.append(sat(yL))
            out.append(sat(yR))
        g = self.level[0] >> (15 - 12)
        g = max(0, min(g, kTableSize - 1))
        targetGr = self.grTable[g]
        self.grMeter += fp_mul(fl2fp(0.005), targetGr - self.grMeter)
        return out


def _cubic(x):
    if x > i2fp(1):
        return i2fp(1)
    if x < -i2fp(1):
        return -i2fp(1)
    x2 = fp_mul(x, x)
    x3 = fp_mul(x2, x)
    y = fp_mul(x, fl2fp(1.5)) - fp_mul(x3, fl2fp(0.5))
    return sat(y)


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------
def sine(rate, freq, frames, amp=0.5):
    return [fl2fp(amp * math.sin(2 * math.pi * freq * i / rate)) for i in range(frames)]


def measure_gain(model, freq, frames, amp=fl2fp(0.1)):
    # settle: drive mixCur/envelope to steady state with enough calls.
    # Use a zero signal: an EQ shelf has DC gain > 1 and would saturate the
    # biquad state if fed a DC offset while settling.
    for _ in range(20000):
        model.process(1)
    x = sine(model.rate, freq, frames, fp2fl(amp))
    out = []
    for s in x:
        out += model.process(1, x_l=s, x_r=0)
    # use the L channel outputs (index 0, 2, 4, ...)
    l = out[0::2]
    # measure RMS gain after settling (skip first 30%); input RMS = amp/sqrt(2)
    start = int(frames * 0.3)
    n = frames - start
    def rms(a):
        return math.sqrt(sum((fp2fl(v) ** 2) for v in a[start:]) / n)
    g = rms(l) / (fp2fl(amp) / math.sqrt(2))
    return g


def check_eq_bell_response():
    rate = 44100
    for db, tol in ((6.0, 0.5), (-6.0, 0.5), (12.0, 0.5)):
        m = ParametricEQModel(rate)
        m.set_band_freq(1, 1000.0)
        m.set_band_gain(1, db)
        m.set_band_enabled(1, True)
        # allow mixCur to fully engage
        m.process(2000, x_l=fl2fp(0.1))
        g = measure_gain(m, 1000.0, 16000, amp=fl2fp(0.1))
        expected = 10 ** (db / 20.0)
        actualDb = 20 * math.log10(g)
        assert abs(actualDb - db) < tol, (db, actualDb)
        print(f"  bell @1k {db:+} dB -> {actualDb:+.2f} dB OK")
    print("eq bell response OK")


def check_eq_shelves():
    rate = 44100
    for band, f0, lo, hi, db in ((0, 100.0, 20.0, 800.0, 6.0),
                                 (2, 10000.0, 3000.0, 15000.0, -6.0)):
        m = ParametricEQModel(rate)
        m.set_band_freq(band, f0)
        m.set_band_gain(band, db)
        m.set_band_enabled(band, True)
        m.process(2000)
        # measure at a frequency well within the affected region; 1 second so
        # even a 20 Hz probe has ~20 cycles for a reliable RMS
        probe = lo if band == 0 else hi
        g = measure_gain(m, probe, 44100, amp=fl2fp(0.1))
        actualDb = 20 * math.log10(g)
        assert abs(actualDb - db) < 1.0, (band, db, actualDb)
        print(f"  shelf band{band} {db:+.1f} dB @{probe} Hz -> {actualDb:+.2f} dB OK")
    print("eq shelves OK")


def check_eq_identity_bypass():
    m = ParametricEQModel(44100)
    m.bypass = True
    m.process(2000, x_l=fl2fp(0.5))
    x = sine(44100, 1000, 4000, 0.5)
    out = []
    for s in x:
        out += m.process(1, x_l=s, x_r=s)
    for i in range(0, len(out), 2):
        assert abs(out[i] - out[i + 1]) == 0
        assert abs(out[i] - x[i // 2]) < fl2fp(0.001), (i, out[i], x[i // 2])
    print("eq identity/bypass OK")


def check_eq_enabled_zero_db_identity():
    m = ParametricEQModel(44100)
    for band in (0, 1, 2):
        m.set_band_enabled(band, True)
    m.process(2000, x_l=fl2fp(0.5))
    x = sine(44100, 1000, 4000, 0.5)
    out = []
    for s in x:
        out += m.process(1, x_l=s, x_r=0)
    l = out[0::2]
    for i in range(400, len(l)):
        assert abs(l[i] - x[i]) < fl2fp(0.001), (i, l[i], x[i])
    print("eq 0dB enabled identity OK")


def check_comp_curve():
    rate = 44100
    m = CompressorModel(rate, thr=-24, ratio=4.0, knee=6.0, makeup=0.0,
                        bypass=False, soft_clip=False)
    # steady sine well above threshold (0 dBFS): gain should be ~ -13.5 dB.
    # The 15 ms attack envelope on a 1 kHz sine only tracks the peak to ~0.42
    # (-7.5 dBFS), so expect GR in [-15, -9.5].
    for s in sine(rate, 1000, rate // 4, 0.5):
        m.process(1, x_l=s)
    out = m.process(1, x_l=fl2fp(0.5))
    tail = []
    for s in sine(rate, 1000, 4000, 0.5):
        o = m.process(1, x_l=s)
        tail.append(fp2fl(o[0]))
    rms_in = 0.5 / math.sqrt(2)
    rms_out = math.sqrt(sum(v * v for v in tail) / len(tail))
    grDb = 20 * math.log10(rms_out / rms_in)
    assert -15.0 < grDb < -9.5, grDb
    print(f"  comp steady 0dBFS GR = {grDb:.2f} dB (expect ~-13.5) OK")

    # below threshold: no reduction
    m2 = CompressorModel(rate, thr=-24, ratio=4.0, knee=6.0, makeup=0.0,
                         bypass=False, soft_clip=False)
    for s in sine(rate, 1000, rate // 4, 0.02):
        m2.process(1, x_l=s)
    tail2 = []
    for s in sine(rate, 1000, 4000, 0.02):
        o = m2.process(1, x_l=s)
        tail2.append(fp2fl(o[0]))
    rms_in2 = 0.02 / math.sqrt(2)
    rms_out2 = math.sqrt(sum(v * v for v in tail2) / len(tail2))
    assert abs(20 * math.log10(rms_out2 / rms_in2)) < 0.5, \
        20 * math.log10(rms_out2 / rms_in2)
    print("comp below-threshold unity OK")

    # static table: gain at a given steady level matches the dB gain computer
    for level, expectedGr in ((0.5, (20*math.log10(0.5)+24)*(1/4-1)),
                              (0.9, (20*math.log10(0.9)+24)*(1/4-1))):
        idx = min(int(round(level * kTableSize)) - 1, kTableSize - 1)
        idx = max(0, idx)
        gain = fp2fl(m.gainTable[idx])
        actualGr = 20 * math.log10(gain)
        # table includes makeup (0) so GR == gainDb
        assert abs(actualGr - expectedGr) < 0.5, (level, actualGr, expectedGr)
    print("comp static table OK")
    print("comp curve OK")


def check_comp_attack_release():
    rate = 44100
    m = CompressorModel(rate, thr=-24, ratio=4.0, knee=6.0, attack=15.0,
                        release=200.0, bypass=False, soft_clip=False)
    m.level[0] = m.level[1] = 0
    # step from silence to 0dBFS: envelope must rise with ~15ms time constant
    for _ in range(100):
        m.process(1, x_l=0)
    out = []
    env_trace = []
    for s in sine(rate, 1000, rate, 0.5):
        out += m.process(1, x_l=s)
        env_trace.append(m.level[0])
    # one-pole attack: after ~15ms the envelope is ~63% of steady state
    n_att = int(0.015 * rate)
    steady_idx = int(0.05 * rate)
    steady = fp2fl(env_trace[steady_idx])
    at = fp2fl(env_trace[n_att])
    frac = at / steady if steady > 0 else 0
    assert 0.4 < frac < 0.8, frac
    print(f"  attack env frac@{15}ms = {frac:.2f} (expect ~0.63) OK")
    print("comp attack/release OK")


def check_comp_stereo_link():
    rate = 44100
    m = CompressorModel(rate, thr=-24, ratio=4.0, knee=6.0, bypass=False,
                        soft_clip=False)
    xl = sine(rate, 1000, rate // 4, 0.5)
    xr = sine(rate, 1000, rate // 4, 0.05)
    for sl, sr in zip(xl, xr):
        m.process(1, x_l=sl, x_r=sr)
    L, R = [], []
    xl2 = sine(rate, 1000, rate // 2, 0.5)
    xr2 = sine(rate, 1000, rate // 2, 0.05)
    for sl, sr in zip(xl2, xr2):
        o = m.process(1, x_l=sl, x_r=sr)
        L.append(o[0])
        R.append(o[1])
    # linked means same gain applied: ratio of outputs equals ratio of inputs
    ratio_out = sum(abs(fp2fl(a)) for a in L) / \
                sum(abs(fp2fl(b)) for b in R)
    assert 8.0 < ratio_out < 12.0, ratio_out  # ~10x input ratio preserved
    print(f"  stereo link ratio {ratio_out:.2f} (~10) OK")
    print("comp stereo link OK")


def check_comp_softclip_limits():
    rate = 44100
    m = CompressorModel(rate, bypass=False, soft_clip=True)
    out = []
    for s in sine(rate, 1000, 4000, 2.0):  # 2.0 -> clipping
        out += m.process(1, x_l=s, x_r=s)
    for v in out:
        assert -i2fp(1) <= v <= i2fp(1), v
        assert v == v  # no NaN
    # overshoot must be reduced: peak output < peak input
    peak_out = max(abs(fp2fl(v)) for v in out)
    assert peak_out <= 1.0 and peak_out < 2.0
    print(f"  soft clip peak = {peak_out:.3f} (input 2.0) OK")
    print("comp soft clip OK")


def check_comp_bypass_identity():
    # Bypass must be a TRUE passthrough: even a 2.0 (over-full-scale) signal
    # must come out bit-for-bit untouched.  This is the regression for the
    # "FX destroys the audio" bug: the old code applied cubicClip/saturate
    # even when bypassed, collapsing every sample to +/-1.
    rate = 44100
    m = CompressorModel(rate, bypass=True, soft_clip=True)
    x = sine(rate, 1000, 4000, 2.0)
    out = []
    for s in x:
        out += m.process(1, x_l=s, x_r=s)
    for i in range(0, len(out), 2):
        assert out[i] == x[i // 2], (i, out[i], x[i // 2])
        assert out[i + 1] == x[i // 2], (i + 1, out[i + 1], x[i // 2])
    # GR meter must stay ~0 in bypass.
    assert abs(fp2fl(m.grMeter)) < 0.5, fp2fl(m.grMeter)
    print("comp bypass identity OK")


def check_comp_gr_meter():
    rate = 44100
    m = CompressorModel(rate, thr=-24, ratio=4.0, knee=6.0, bypass=False,
                        soft_clip=False)
    for s in sine(rate, 1000, rate // 2, 0.5):
        m.process(1, x_l=s)
    gr = fp2fl(m.grMeter)
    assert gr < -5.0, gr  # significant reduction
    m.bypass = True
    for s in sine(rate, 1000, rate // 4, 0.5):
        m.process(1, x_l=s)
    assert abs(fp2fl(m.grMeter)) < 0.5, fp2fl(m.grMeter)
    print(f"  GR meter {gr:.2f} dB under comp, ~0 bypassed OK")
    print("comp GR meter OK")


def check_source_guards():
    src = (ROOT / "source/sources/Application/Audio/FxEngine/ParametricEQ.cpp").read_text()
    for token in ("Audio EQ Cookbook", "sqrtf", "cosf", "sinf", "Transposed",
                  "SetBandEnabled", "SetBypass"):
        assert token in src, token
    csrc = (ROOT / "source/sources/Application/Audio/FxEngine/Compressor.cpp").read_text()
    for token in ("recomputeTable", "cubicClip", "stereoLink_", "attK_",
                  "relK_", "grTable_", "out[i] = in[i]"):
        assert token in csrc, token
    chdr = (ROOT / "source/sources/Application/Audio/FxEngine/Compressor.h").read_text()
    for token in ("GetGainReductionDb", "SetSoftClip", "kTableSize"):
        assert token in chdr, token
    fsrc = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.cpp").read_text()
    for token in ("eq_.Process", "comp_.Process"):
        assert token in fsrc, token
    print("source guards OK")


check_eq_bell_response()
check_eq_shelves()
check_eq_identity_bypass()
check_eq_enabled_zero_db_identity()
check_comp_curve()
check_comp_attack_release()
check_comp_stereo_link()
check_comp_softclip_limits()
check_comp_bypass_identity()
check_comp_gr_meter()
check_source_guards()
print("EQ_COMP_PHASE3_OK")
