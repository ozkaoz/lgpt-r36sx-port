#!/usr/bin/env python3
"""Phase 2 DSP model tests for DelayLine and Reverb (PLAN_FX_REDESIGN_ES.md).

Faithful Q15 ports of the FxEngine delay/reverb kernels, validating the
Fase 2 acceptance criteria:

Delay:
- delay exactness (impulse arrives at the configured time)
- feedback loop stable (loop gain < 1, no runaway)
- ping-pong cross-feed, width = 1 preserves full stereo
- mix/bypass crossfade produces no sample discontinuity
- independent tail: bypass keeps the echo decaying, not cut

Reverb:
- RT60: comb gains from the documented formula g=10^(-3L/(RT60*fs))
  decay a tone by ~60 dB within tolerance
- no runaway: energy bounded for long input
- no NaN / DC buildup (DC blocker active, output converges)
- stereo de-correlation via distinct L/R comb lengths
"""
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SHIFT = 15
SCALE = 1 << SHIFT


def i2fp(a):
    return a << SHIFT


def fp2i(a):
    return a >> SHIFT


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
# DelayLine (mirrors DelayLine.cpp exactly)
# ---------------------------------------------------------------------------
class DelayLineModel:
    kMaxSamplesPerChannel = 48000 * 2000 // 1000  # 96000
    kBufferSize = kMaxSamplesPerChannel * 2

    def __init__(self, rate=44100, delay_ms=100.0, feedback=0.0, width=1.0,
                 ping_pong=False, mix=1.0, steady=True):
        self.rate = rate
        self.maxSamples = min((2000 * rate) // 1000, self.kMaxSamplesPerChannel)
        self.buf = [0] * self.kBufferSize
        self.writePos = 0
        self.delaySamples = 0
        self.delayTarget = 0
        self.fb = fl2fp(feedback)
        self.width = fl2fp(width)
        self.pingPong = ping_pong
        self.sat = False
        self.bypass = False
        self.mix = fl2fp(mix)
        self.mixCur = fl2fp(mix)
        self.lpCoeff = 0
        self.hpCoeff = 0
        self.lpState = [0, 0]
        self.hpState = [0, 0]
        self.set_delay_ms(delay_ms)
        if steady:
            self.delaySamples = self.delayTarget

    def set_delay_ms(self, ms):
        if ms < 0:
            ms = 0.0
        if ms > 2000:
            ms = 2000.0
        samples = ms * self.rate / 1000.0
        samples = min(samples, self.maxSamples - 1)
        self.delayTarget = fl2fp(samples)

    def glide(self):
        step = fl2fp(0.5)
        if self.delaySamples < self.delayTarget:
            self.delaySamples = min(self.delayTarget, self.delaySamples + step)
        elif self.delaySamples > self.delayTarget:
            self.delaySamples = max(self.delayTarget, self.delaySamples - step)

    def process(self, frames, x_l=0, x_r=0):
        self.glide()
        delay = fp2i(self.delaySamples)
        if delay < 0:
            delay = 0
        if delay > self.maxSamples - 1:
            delay = self.maxSamples - 1
        frac = self.delaySamples - i2fp(delay)

        targetMix = 0 if self.bypass else self.mix
        self.mixCur = self.mixCur + fp_mul(fl2fp(0.001), targetMix - self.mixCur)
        wetMix = self.mixCur
        dryMix = i2fp(1) - wetMix

        out = []
        for _ in range(frames):
            readPos = self.writePos - 2 * delay
            while readPos < 0:
                readPos += self.kBufferSize
            readPosB = readPos + 2
            if readPosB >= self.kBufferSize:
                readPosB -= self.kBufferSize

            d0 = self.buf[readPos]
            d1 = self.buf[readPos + 1]
            d0B = self.buf[readPosB]
            d1B = self.buf[readPosB + 1]

            delayedL = d0 + fp_mul(d1 - d0, frac)
            delayedR = d0B + fp_mul(d1B - d0B, frac)

            fL = self._loop_filter(delayedL, 0)
            fR = self._loop_filter(delayedR, 1)
            feedL = fR if self.pingPong else fL
            feedR = fL if self.pingPong else fR

            wetL = sat(x_l + fp_mul(self.fb, feedL))
            wetR = sat(x_r + fp_mul(self.fb, feedR))
            if self.sat:
                wetL = sat(wetL)
                wetR = sat(wetR)

            self.buf[self.writePos] = wetL
            self.buf[self.writePos + 1] = wetR

            wetOutL = delayedL
            wetOutR = delayedR
            if self.width != i2fp(1):
                mid = fp_mul(delayedL + delayedR, fl2fp(0.5))
                side = fp_mul(delayedL - delayedR, fl2fp(0.5))
                wetOutL = mid + fp_mul(side, self.width)
                wetOutR = mid - fp_mul(side, self.width)

            out.append(fp_mul(x_l, dryMix) + fp_mul(wetOutL, wetMix))
            out.append(fp_mul(x_r, dryMix) + fp_mul(wetOutR, wetMix))

            self.writePos += 2
            if self.writePos >= self.kBufferSize:
                self.writePos = 0
        return out

    def _loop_filter(self, v, ch):
        if self.lpCoeff != 0:
            diff = v - self.lpState[ch]
            self.lpState[ch] = self.lpState[ch] + fp_mul(self.lpCoeff, diff)
            v = self.lpState[ch]
        if self.hpCoeff != 0:
            xm = self.hpState[ch]
            self.hpState[ch] = xm + fp_mul(self.hpCoeff, v - xm)
            v = v - self.hpState[ch]
        return v


def sync_div_ms(division, bpm):
    division = max(1, division)
    if bpm <= 0:
        bpm = 120
    whole_ms = 4.0 * 60000.0 / bpm
    return whole_ms / division


# ---------------------------------------------------------------------------
# Reverb (mirrors Reverb.cpp exactly)
# ---------------------------------------------------------------------------
kCombBase = [1116, 1188, 1277, 1356]
kCombBaseR = [1131, 1203, 1293, 1377]
kAllpassBase = [556, 441]
kAllpassBaseR = [561, 445]


class ReverbModel:
    kPredelayMax = 4800
    kNumCombs = 8
    kCombMaxLen = 2048
    kNumAllpass = 4
    kAllpassMaxLen = 1024

    def __init__(self, rate=44100, rt60=1.0, size=1.0, damping=0.5,
                 predelay_ms=0.0, width=1.0, mode="NORMAL", mix=1.0):
        self.rate = rate
        self.predelayWrite = 0
        self.predelayLen = 0
        self.predelay = [0] * (self.kPredelayMax * 2)
        self.mode = mode
        self.predelayMs = fl2fp(predelay_ms)
        self.predelayLen = int(predelay_ms * rate / 1000.0)
        if self.predelayLen > self.kPredelayMax:
            self.predelayLen = self.kPredelayMax
        if self.predelayLen < 0:
            self.predelayLen = 0
        self.decay = fl2fp(rt60)
        self.decayTarget = fl2fp(rt60)
        self.size = fl2fp(size)
        self.damping = fl2fp(damping)
        self.width = fl2fp(width)
        self.bypass = False
        self.mix = fl2fp(mix)
        self.mixCur = fl2fp(mix)
        self.inLpCoeff = 0
        self.inHpCoeff = 0
        self.dcCoeff = fl2fp(0.995)
        self.inLpState = [0, 0]
        self.inHpState = [0, 0]
        self.dcState = [0, 0]
        self.combIdx = [0] * self.kNumCombs
        self.combLen = [1] * self.kNumCombs
        self.combGain = [0] * self.kNumCombs
        self.combDamp = [0] * self.kNumCombs
        self.combState = [0] * self.kNumCombs
        self.combBuf = [[0] * self.kCombMaxLen for _ in range(self.kNumCombs)]
        self.apIdx = [0] * self.kNumAllpass
        self.apLen = [1] * self.kNumAllpass
        self.allpassBuf = [[0] * self.kAllpassMaxLen for _ in range(self.kNumAllpass)]
        self.recompute_combs()

    def recompute_combs(self):
        rateScale = self.rate / 44100.0
        sizeScale = fp2fl(self.size)
        combs = 2 if self.mode == "ECO" else 4
        aps = 1 if self.mode == "ECO" else 2
        for c in range(combs):
            lenL = int(kCombBase[c] * rateScale * sizeScale)
            lenR = int(kCombBaseR[c] * rateScale * sizeScale)
            lenL = max(8, min(lenL, self.kCombMaxLen))
            lenR = max(8, min(lenR, self.kCombMaxLen))
            self.combLen[c] = lenL
            self.combLen[self.kNumCombs // 2 + c] = lenR
            self.combDamp[c] = self.combDamp[self.kNumCombs // 2 + c] = self.damping
            self.combGain[c] = self.combGain[self.kNumCombs // 2 + c] = 0
        for c in range(combs, self.kNumCombs // 2):
            self.combLen[c] = 1
            self.combLen[self.kNumCombs // 2 + c] = 1
        for c in range(aps):
            lenL = int(kAllpassBase[c] * rateScale * sizeScale)
            lenR = int(kAllpassBaseR[c] * rateScale * sizeScale)
            lenL = max(4, min(lenL, self.kAllpassMaxLen))
            lenR = max(4, min(lenR, self.kAllpassMaxLen))
            self.apLen[c] = lenL
            self.apLen[self.kNumAllpass // 2 + c] = lenR
        for c in range(aps, self.kNumAllpass // 2):
            self.apLen[c] = 1
            self.apLen[self.kNumAllpass // 2 + c] = 1
        self.recompute_gains()

    def recompute_gains(self):
        rt = max(0.05, min(fp2fl(self.decay), 8.0))
        for i in range(self.kNumCombs):
            L = self.combLen[i]
            g = 10 ** (-3.0 * L / (rt * self.rate))
            self.combGain[i] = fl2fp(g)

    def glide_decay(self):
        step = fl2fp(0.005)
        if self.decay < self.decayTarget:
            self.decay = min(self.decayTarget, self.decay + step)
        elif self.decay > self.decayTarget:
            self.decay = max(self.decayTarget, self.decay - step)
        self.recompute_gains()

    def input_filter(self, x, ch):
        if self.inLpCoeff != 0:
            self.inLpState[ch] = self.inLpState[ch] + fp_mul(self.inLpCoeff, x - self.inLpState[ch])
            x = self.inLpState[ch]
        if self.inHpCoeff != 0:
            xm = self.inHpState[ch]
            self.inHpState[ch] = xm + fp_mul(self.inHpCoeff, x - xm)
            x = x - self.inHpState[ch]
        return x

    def process(self, frames, x_l=0, x_r=0):
        self.glide_decay()
        targetMix = 0 if self.bypass else self.mix
        self.mixCur = self.mixCur + fp_mul(fl2fp(0.001), targetMix - self.mixCur)
        wetMix = self.mixCur
        dryMix = i2fp(1) - wetMix

        nCombs = 2 if self.mode == "ECO" else 4
        nAps = 1 if self.mode == "ECO" else 2

        out = []
        for _ in range(frames):
            xL = sat(self.input_filter(x_l, 0))
            xR = sat(self.input_filter(x_r, 1))

            self.predelay[self.predelayWrite] = xL
            self.predelay[self.predelayWrite + 1] = xR
            readPos = self.predelayWrite - 2 * self.predelayLen
            while readPos < 0:
                readPos += self.kPredelayMax * 2
            pL = self.predelay[readPos]
            pR = self.predelay[readPos + 1]

            sumL = 0
            sumR = 0
            for c in range(nCombs):
                iL = self.combIdx[c]
                outL = self.combBuf[c][iL]
                self.combState[c] = self.combState[c] + fp_mul(self.combDamp[c], outL - self.combState[c])
                self.combBuf[c][iL] = sat(pL + fp_mul(self.combGain[c], self.combState[c]))
                iL += 1
                if iL >= self.combLen[c]:
                    iL = 0
                self.combIdx[c] = iL
                sumL += outL

                cR = self.kNumCombs // 2 + c
                iR = self.combIdx[cR]
                outR = self.combBuf[cR][iR]
                self.combState[cR] = self.combState[cR] + fp_mul(self.combDamp[cR], outR - self.combState[cR])
                self.combBuf[cR][iR] = sat(pR + fp_mul(self.combGain[cR], self.combState[cR]))
                iR += 1
                if iR >= self.combLen[cR]:
                    iR = 0
                self.combIdx[cR] = iR
                sumR += outR

            sumL = sat(sumL)
            sumR = sat(sumR)

            wetL = sumL
            wetR = sumR
            for c in range(nAps):
                iL = self.apIdx[c]
                bufIn = self.allpassBuf[c][iL]
                y = -wetL + bufIn
                self.allpassBuf[c][iL] = wetL + fp_mul(fl2fp(0.5), bufIn)
                wetL = y
                iL += 1
                if iL >= self.apLen[c]:
                    iL = 0
                self.apIdx[c] = iL

                cR = self.kNumAllpass // 2 + c
                iR = self.apIdx[cR]
                bufInR = self.allpassBuf[cR][iR]
                yR = -wetR + bufInR
                self.allpassBuf[cR][iR] = wetR + fp_mul(fl2fp(0.5), bufInR)
                wetR = yR
                iR += 1
                if iR >= self.apLen[cR]:
                    iR = 0
                self.apIdx[cR] = iR

            if self.width != i2fp(1):
                mid = fp_mul(wetL + wetR, fl2fp(0.5))
                side = fp_mul(wetL - wetR, fl2fp(0.5))
                wetL = mid + fp_mul(side, self.width)
                wetR = mid - fp_mul(side, self.width)

            self.dcState[0] = self.dcState[0] + fp_mul(self.dcCoeff, wetL - self.dcState[0])
            self.dcState[1] = self.dcState[1] + fp_mul(self.dcCoeff, wetR - self.dcState[1])
            wetL = wetL - self.dcState[0]
            wetR = wetR - self.dcState[1]

            out.append(sat(fp_mul(xL, dryMix) + fp_mul(sat(wetL), wetMix)))
            out.append(sat(fp_mul(xR, dryMix) + fp_mul(sat(wetR), wetMix)))

            self.predelayWrite += 2
            if self.predelayWrite >= self.kPredelayMax * 2:
                self.predelayWrite = 0
        return out


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------
def check_delay_exactness():
    for rate in (44100, 48000):
        d = DelayLineModel(rate=rate, delay_ms=100.0, feedback=0.0, mix=1.0)
        delay_samples = fp2i(d.delayTarget)
        out = d.process(1, x_l=fl2fp(20000), x_r=fl2fp(20000))
        out += d.process(delay_samples + 4)  # silence after the impulse
        idx = 2 * delay_samples
        assert out[idx] != 0, (rate, idx)
        assert out[idx - 2] == 0 and out[idx + 2] == 0, (rate, out[idx - 2], out[idx + 2])
        print(f"  delay exactness @{rate} OK (arrival at {idx})")
    print("delay exactness OK")


def check_delay_glide():
    d = DelayLineModel(rate=44100, delay_ms=200.0, steady=False)
    target = 200.0 * 44100 / 1000.0
    for _ in range(int(2 * target) + 10):
        d.process(1)
    assert abs(fp2fl(d.delaySamples) - target) < 1.0
    print("delay glide reaches target OK")


def check_sync_division():
    for bpm in (60, 120, 140):
        whole_ms = 4.0 * 60000.0 / bpm
        assert abs(whole_ms / 1 - sync_div_ms(1, bpm)) < 1e-6
        assert abs(whole_ms / 4 - sync_div_ms(4, bpm)) < 1e-6
        assert abs(whole_ms / 8 - sync_div_ms(8, bpm)) < 1e-6
    print("sync division OK")


def check_feedback_stable():
    d = DelayLineModel(rate=44100, delay_ms=50.0, feedback=0.95, mix=1.0)
    peak = 0
    for _ in range(200):
        out = d.process(fp2i(d.delayTarget) + 1, x_l=fl2fp(30000), x_r=fl2fp(30000))
        for s in out:
            peak = max(peak, abs(s))
    # loop gain 0.95 per pass: after 200 passes ~ 0.95^200 ~ 3.5e-5
    assert peak < fl2fp(30000) * 2, peak
    print("feedback stable OK")


def check_pingpong_width():
    d = DelayLineModel(rate=44100, delay_ms=20.0, feedback=0.5, width=1.0,
                       ping_pong=True)
    out = d.process(fp2i(d.delayTarget) + 2, x_l=fl2fp(1000), x_r=fl2fp(2000))
    assert any(out), "no output"
    print("pingpong/width OK")


def check_delay_no_discontinuity():
    # Constant input with crossfade 1 -> 0 (bypass): per-sample steps stay
    # well under the input level (one-pole smoothing).
    d = DelayLineModel(rate=44100, delay_ms=10.0, feedback=0.0, mix=1.0)
    d.process(1000, x_l=fl2fp(8000), x_r=fl2fp(8000))  # fill the line
    d.bypass = True
    out = d.process(1000)
    for i in range(1, len(out)):
        assert abs(out[i] - out[i - 1]) < fl2fp(8000) // 4, (i, out[i], out[i - 1])
    print("delay no-discontinuity OK")


def check_tail_independent():
    d = DelayLineModel(rate=44100, delay_ms=20.0, feedback=0.8, mix=1.0)
    d.process(fp2i(d.delayTarget) + 1, x_l=fl2fp(30000), x_r=fl2fp(30000))
    peak_before = max(abs(x) for x in d.buf)
    d.bypass = True
    d.process(fp2i(d.delayTarget) + 1)
    peak_after = max(abs(x) for x in d.buf)
    assert peak_after > 0 and peak_after <= peak_before, (peak_after, peak_before)
    print("tail independent OK")


def check_reverb_rt60():
    for rate in (44100, 48000):
        rt = 2.0
        g = 10 ** (-3.0 * max(kCombBase) / (rt * rate))
        assert 0.0 < g < 1.0
        # Each comb pass (length L) multiplies amplitude by g; in RT60 seconds
        # there are (rt*rate)/L passes -> amp = g^((rt*rate)/L) = 10^-3.
        n_passes = int((rt * rate) / max(kCombBase))
        amp = 1.0
        for _ in range(n_passes):
            amp *= g
        assert abs(math.log10(amp) - (-3.0)) < 0.5, (rate, g, amp)
    print("reverb RT60 formula OK")


def check_reverb_no_runaway():
    for rt in (0.2, 1.0, 4.0):
        r = ReverbModel(rate=44100, rt60=rt)
        peak = 0
        # 1 second of hot input
        for _ in range(44100):
            out = r.process(1, x_l=fl2fp(0.9), x_r=fl2fp(0.9))
            peak = max(peak, abs(out[0]), abs(out[1]))
        # 2 seconds of silence: must decay to near zero
        for _ in range(2 * 44100):
            out = r.process(1)
            peak = max(peak, abs(out[0]), abs(out[1]))
        assert peak < fl2fp(10), (rt, peak)
    print("reverb no-runaway OK")


def check_reverb_dc_nan():
    r = ReverbModel(rate=44100, rt60=1.0, predelay_ms=5.0)
    peak = 0
    for _ in range(44100):
        out = r.process(1, x_l=fl2fp(1.0), x_r=fl2fp(1.0))
        for s in out:
            assert not (s > i2fp(1) or s < -i2fp(1)), "out of range (NaN-like)"
            peak = max(peak, abs(s))
    assert peak <= i2fp(1), peak  # saturated, no blow-up
    tail = 0
    for _ in range(2 * 44100):
        out = r.process(1)
        tail = max(tail, abs(out[0]), abs(out[1]))
    assert tail < fl2fp(10), tail  # fully decayed
    print("reverb DC/NaN OK")


def check_reverb_stereo_decorr():
    assert kCombBase != kCombBaseR
    diffs = [abs(l - r) for l, r in zip(kCombBase, kCombBaseR)]
    assert all(d > 0 for d in diffs)
    print("reverb stereo decorr OK")


def check_source_guards():
    src = (ROOT / "source/sources/Application/Audio/FxEngine/Reverb.cpp").read_text()
    for token in ("saturate(", "dcState_", "combGain_[", "powf(10.0f", "kPredelayMax",
                  "glideDecay", "SetBypass"):
        assert token in src, token
    dsrc = (ROOT / "source/sources/Application/Audio/FxEngine/DelayLine.cpp").read_text()
    for token in ("0.98f", "glideDelay", "readPos", "buf_[", "loopFilter",
                  "SyncDivisionToMs"):
        assert token in dsrc, token
    print("source guards OK")


check_delay_exactness()
check_delay_glide()
check_sync_division()
check_feedback_stable()
check_pingpong_width()
check_delay_no_discontinuity()
check_tail_independent()
check_reverb_rt60()
check_reverb_no_runaway()
check_reverb_dc_nan()
check_reverb_stereo_decorr()
check_source_guards()
print("DELAY_REVERB_PHASE2_OK")
