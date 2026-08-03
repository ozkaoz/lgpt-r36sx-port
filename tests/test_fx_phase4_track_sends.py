#!/usr/bin/env python3
"""Phase 4 model tests: per-track DLY/RVB sends (PLAN_FX_REDESIGN_ES.md).

Faithful Q15 model of the Fase 4 send architecture:

- Mixer holds per-track delay/reverb send levels (0..100), persisted via the
  MIXER XML block (DELAYSEND / REVERBSEND attributes, backward compatible:
  missing attributes restore to the legacy default 0).
- PlayerChannel::Render accumulates each audible track's post-volume buffer
  into the FxEngine send buses with the track's gains, BEFORE the master mix.
- FxEngine::AccumulateChannelSend zeros the buses on the first accumulator of
  the frame and sums the rest; FxEngine::processSendReturns consumes them and
  falls back to the global delaySend_/reverbSend_ when nothing accumulated
  (direct Process() on a mixed buffer).

Acceptance:
- sends are per-track: track with send 100 feeds the delay bus, send 0 does not
- send bus = sum of per-track contributions (stereo accumulation)
- mixer persistence round-trips DELAYSEND/REVERBSEND and defaults to 0 for
  legacy files
- RT safety guards: legacy mode does not accumulate; oversized/negative
  samplecount increments rtViolations and returns
"""
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SHIFT = 15
SCALE = 1 << SHIFT
SONG_CHANNEL_COUNT = 8


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


def clamp(v, lo, hi):
    if v < lo:
        return lo
    if v > hi:
        return hi
    return v


# ---------------------------------------------------------------------------
# Mixer model (mirrors Mixer.cpp Fase 4)
# ---------------------------------------------------------------------------
class MixerModel:
    def __init__(self):
        self.bus = list(range(SONG_CHANNEL_COUNT))
        self.volume = [100] * SONG_CHANNEL_COUNT
        self.delaySend = [0] * SONG_CHANNEL_COUNT
        self.reverbSend = [0] * SONG_CHANNEL_COUNT

    def clamp_channel(self, i):
        return clamp(i, 0, SONG_CHANNEL_COUNT - 1)

    def get_delay_send(self, i):
        return clamp(self.delaySend[self.clamp_channel(i)], 0, 100)

    def set_delay_send(self, i, v):
        self.delaySend[self.clamp_channel(i)] = clamp(v, 0, 100)

    def get_reverb_send(self, i):
        return clamp(self.reverbSend[self.clamp_channel(i)], 0, 100)

    def set_reverb_send(self, i, v):
        self.reverbSend[self.clamp_channel(i)] = clamp(v, 0, 100)

    def to_xml(self):
        lines = ["<MIXER>"]
        for i in range(SONG_CHANNEL_COUNT):
            lines.append('  <CHANNEL INDEX="%d" BUS="%d" VOLUME="%d" '
                         'DELAYSEND="%d" REVERBSEND="%d"/>' % (
                             i, self.bus[i], self.volume[i],
                             self.delaySend[i], self.reverbSend[i]))
        lines.append("</MIXER>")
        return "\n".join(lines)

    @classmethod
    def from_xml(cls, xml):
        m = cls()
        for line in xml.splitlines():
            line = line.strip()
            if not line.startswith("<CHANNEL"):
                continue
            attrs = {}
            for k, v in [tok.split("=") for tok in line[1:-2].split() if "=" in tok]:
                attrs[k] = int(v.strip('"'))
            idx = attrs.get("INDEX", -1)
            if 0 <= idx < SONG_CHANNEL_COUNT:
                m.bus[idx] = attrs.get("BUS", idx)
                m.volume[idx] = attrs.get("VOLUME", 100)
                # Legacy files lack the FX send attributes -> default 0
                m.delaySend[idx] = attrs.get("DELAYSEND", 0)
                m.reverbSend[idx] = attrs.get("REVERBSEND", 0)
        return m


# ---------------------------------------------------------------------------
# FxEngine send-bus model (mirrors FxEngine.cpp Fase 4)
# ---------------------------------------------------------------------------
class FxEngineSendModel:
    FX_MAX_CHANNELS = 8
    FX_MAX_FRAMES = 2048

    def __init__(self, legacy_mode=True):
        self.legacyMode = legacy_mode
        self.sendsAccumulated = False
        self.send0 = [0] * (self.FX_MAX_FRAMES * 2)
        self.send1 = [0] * (self.FX_MAX_FRAMES * 2)
        self.rtViolations = 0
        self.delaySend = fl2fp(0.0)
        self.reverbSend = fl2fp(0.0)

    def accumulate_channel_send(self, channel, buf, samplecount, delay_gain,
                                reverb_gain):
        if self.legacyMode:
            return
        if samplecount <= 0 or buf is None:
            self.rtViolations += 1
            return
        if channel < 0 or channel >= self.FX_MAX_CHANNELS:
            self.rtViolations += 1
            return
        if samplecount > self.FX_MAX_FRAMES:
            self.rtViolations += 1
            return
        n = samplecount * 2
        if not self.sendsAccumulated:
            self.send0[:n] = [0] * n
            self.send1[:n] = [0] * n
            self.sendsAccumulated = True
        if delay_gain != 0:
            for i in range(n):
                # Channel buffers are int16<<15 scale; normalize to Q15 before
                # the multiply so the send buses match the delay/reverb DSP
                # range (mirrors FxEngine.cpp).
                self.send0[i] += fp_mul(buf[i] >> SHIFT, delay_gain)
        if reverb_gain != 0:
            for i in range(n):
                self.send1[i] += fp_mul(buf[i] >> SHIFT, reverb_gain)

    def process_send_returns(self, buf, samplecount):
        n = samplecount * 2
        if not self.sendsAccumulated:
            for i in range(n):
                self.send0[i] = fp_mul(buf[i] >> SHIFT, self.delaySend)
                self.send1[i] = fp_mul(buf[i] >> SHIFT, self.reverbSend)
        self.sendsAccumulated = False
        return self.send0[:n], self.send1[:n]


def sine(rate, freq, frames, amp=1.0):
    """int16<<15 scale (as the channel/master buffers really are)."""
    for i in range(frames):
        yield i2fp(int(amp * 32767 * math.sin(2.0 * math.pi * freq * i / rate)))


def stereo_buf(rate, freq, samplecount, amp=1.0):
    """Interleaved stereo buffer of `samplecount` frames (L=R)."""
    s = list(sine(rate, freq, samplecount, amp))
    out = []
    for x in s:
        out += [x, x]
    return out


def check_per_track_isolation():
    m = MixerModel()
    m.set_delay_send(0, 100)
    m.set_reverb_send(0, 60)
    m.set_delay_send(3, 0)   # track 3 has no delay send
    fx = FxEngineSendModel(legacy_mode=False)

    buf3 = stereo_buf(44100, 440, 256, 0.5)
    fx.accumulate_channel_send(3, buf3, 256,
                               fl2fp(m.get_delay_send(3) / 100.0),
                               fl2fp(m.get_reverb_send(3) / 100.0))
    send0, send1 = fx.process_send_returns([0] * (256 * 2), 256)
    assert all(v == 0 for v in send0), "track without delay send leaked"
    assert all(v == 0 for v in send1), "track without reverb send leaked"
    print("per-track isolation OK")


def check_send_level_mapping():
    # Gain 1.0 (send=100) -> send bus equals the track buffer normalized to
    # Q15 (the DSP scale), exactly as FxEngine now does.
    m = MixerModel()
    m.set_delay_send(2, 100)
    fx = FxEngineSendModel(legacy_mode=False)
    buf = stereo_buf(44100, 220, 128, 0.3)
    fx.accumulate_channel_send(2, buf, 128,
                               fl2fp(m.get_delay_send(2) / 100.0),
                               fl2fp(m.get_reverb_send(2) / 100.0))
    send0, _ = fx.process_send_returns([0] * (128 * 2), 128)
    for a, b in zip(send0, buf):
        assert a == (b >> SHIFT), (a, b)
    print("send level mapping OK")


def check_stereo_accumulation():
    # Two tracks (L and R sides of the mix) both feed the delay bus; the bus
    # is the sum of the two post-volume contributions.
    fx = FxEngineSendModel(legacy_mode=False)
    bufL = stereo_buf(44100, 300, 64, 0.4)
    bufR = stereo_buf(44100, 300, 64, 0.2)
    fx.accumulate_channel_send(0, bufL, 64, fl2fp(1.0), 0)
    fx.accumulate_channel_send(1, bufR, 64, fl2fp(1.0), 0)
    send0, _ = fx.process_send_returns([0] * (64 * 2), 64)
    for i in range(64 * 2):
        assert send0[i] == (bufL[i] >> SHIFT) + (bufR[i] >> SHIFT), (i, send0[i])
    print("stereo accumulation OK")


def check_fallback_global_sends():
    # No channel accumulated this frame: Process() uses the global sends,
    # preserving the Fase 2/3 single-buffer behaviour (input normalized to Q15).
    fx = FxEngineSendModel(legacy_mode=False)
    fx.delaySend = fl2fp(0.25)
    buf = stereo_buf(44100, 440, 128, 0.5)
    send0, send1 = fx.process_send_returns(buf, 128)
    for i in range(128 * 2):
        assert send0[i] == fp_mul(buf[i] >> SHIFT, fx.delaySend), i
        assert send1[i] == 0, i
    print("global send fallback OK")


def check_legacy_no_accumulate():
    fx = FxEngineSendModel(legacy_mode=True)
    buf = stereo_buf(44100, 440, 128, 0.5)
    fx.accumulate_channel_send(0, buf, 128, fl2fp(1.0), fl2fp(1.0))
    assert fx.rtViolations == 0
    assert fx.sendsAccumulated is False, "legacy mode must not accumulate"
    print("legacy no-accumulate OK")


def check_rt_guards():
    fx = FxEngineSendModel(legacy_mode=False)
    fx.accumulate_channel_send(-1, [0] * 4, 2, fl2fp(1.0), 0)
    fx.accumulate_channel_send(0, None, 2, fl2fp(1.0), 0)
    fx.accumulate_channel_send(0, [0] * 4, 0, fl2fp(1.0), 0)
    fx.accumulate_channel_send(0, [0] * 4, 4096, fl2fp(1.0), 0)
    assert fx.rtViolations == 4, fx.rtViolations
    assert fx.sendsAccumulated is False
    print("RT guards OK")


def check_persistence_roundtrip():
    m = MixerModel()
    for i in range(SONG_CHANNEL_COUNT):
        m.set_delay_send(i, (i * 13) % 101)
        m.set_reverb_send(i, (i * 7) % 101)
    m2 = MixerModel.from_xml(m.to_xml())
    for i in range(SONG_CHANNEL_COUNT):
        assert m2.get_delay_send(i) == m.get_delay_send(i), i
        assert m2.get_reverb_send(i) == m.get_reverb_send(i), i
    print("persistence round-trip OK")


def check_persistence_legacy_default():
    # Old project files have no DELAYSEND/REVERBSEND attributes.
    legacy = "\n".join(
        '  <CHANNEL INDEX="%d" BUS="%d" VOLUME="%d"/>' % (i, i, 100)
        for i in range(SONG_CHANNEL_COUNT))
    m = MixerModel.from_xml("<MIXER>\n%s\n</MIXER>" % legacy)
    for i in range(SONG_CHANNEL_COUNT):
        assert m.get_delay_send(i) == 0, i
        assert m.get_reverb_send(i) == 0, i
    print("legacy persistence default OK")


def check_source_guards():
    src = (ROOT / "source/sources/Application/Model/Mixer.cpp").read_text()
    for token in ("DELAYSEND", "REVERBSEND", "channelDelaySend_",
                  "channelReverbSend_", "clampSend"):
        assert token in src, token
    fsrc = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.cpp").read_text()
    for token in ("AccumulateChannelSend", "sendsAccumulated_",
                  "FX_ENGINE_MAX_CHANNELS", "processSendReturns",
                  "FX_ENGINE_MAX_FRAMES", "FIXED_SHIFT",
                  "buffer[i] >> FIXED_SHIFT"):
        assert token in fsrc, token
    pcsrc = (ROOT / "source/sources/Application/Player/PlayerChannel.cpp").read_text()
    for token in ("AccumulateChannelSend", "GetChannelDelaySend",
                  "GetChannelReverbSend"):
        assert token in pcsrc, token
    print("source guards OK")


check_per_track_isolation()
check_send_level_mapping()
check_stereo_accumulation()
check_fallback_global_sends()
check_legacy_no_accumulate()
check_rt_guards()
check_persistence_roundtrip()
check_persistence_legacy_default()
check_source_guards()
print("FX_TRACK_SENDS_PHASE4_OK")
