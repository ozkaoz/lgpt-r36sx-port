#!/usr/bin/env python3
"""Phase 15 model tests: live per-channel FX send layer
(PLAN_FX_REDESIGN_ES.md, Fase 15 = TREEFROG_SEND_LIVE_V1).

Mirrors the Fase 15 engine layer that decouples automation from persistence:

- SampleInstrument keeps the persisted base DRY/DLY/RVB (default 100/0/0) plus,
  per channel, a LIVE delay/reverb send override stored in renderParams
  (dlySend_/rvbSend_, -1 = inherit).
- A new note trigger (Start(cleanstart=true)) restores the live value from the
  persisted base; legato does NOT reset it.
- Phrase/table DLYS/RVBS handlers write ONLY renderParams_[channel].dlySend_ /
  rvbSend_; they never write dlySend_/rvbSend_ variables nor the Mixer per-track
  sends, so saved values are never clobbered by automation.
- PlayerChannel::Render reads the live accessors (GetLiveDelaySend /
  GetLiveReverbSend): 0xFF inherits the per-track Mixer send, 0..100 wins.
- Switching instruments on a track changes the effective sends (the new
  instrument resets its live layer on trigger).

Acceptance:
- defaults are DRY=100 / DLY=0 / RVB=0 (new instruments are dry, sends off)
- a default instrument is silent regardless of the per-track sends
- legacy -1 instruments still inherit the per-track Mixer send exactly
- DLYS/RVBS write the live value only; the persisted base never changes
- a new trigger restores the live value from the base
- legato does not reset the live value
- per-channel live values are independent (channel 0 vs channel 3)
- switching instruments on a track yields the new instrument's base sends
- source guards for renderParams fields, accessors, handler writes, defaults
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SHIFT = 15
SCALE = 1 << SHIFT


def fl2fp(f):
    return int(round(f * SCALE))


def fp2fl(a):
    return a / SCALE


def clamp(v, lo, hi):
    if v < lo:
        return lo
    if v > hi:
        return hi
    return v


# ---------------------------------------------------------------------------
# Models (mirror SampleInstrument Fase 15 + PlayerChannel.cpp)
# ---------------------------------------------------------------------------
class InstrumentModel:
    """One instrument.  Base values are persisted; live values per channel."""

    def __init__(self, dry=100, dly=0, rvb=0):
        self.dry = dry
        self.dly = dly            # persisted base
        self.rvb = rvb            # persisted base
        self.live = {}            # channel -> (liveDly, liveRvb), -1 = inherit

    def get_delay_send_override(self):
        return 0xFF if self.dly < 0 else self.dly

    def get_reverb_send_override(self):
        return 0xFF if self.rvb < 0 else self.rvb

    def get_dry(self):
        return clamp(self.dry, 0, 100)

    def get_live_delay(self, channel):
        v = self.live.get(channel, (-1, -1))[0]
        return 0xFF if v < 0 else v

    def get_live_reverb(self, channel):
        v = self.live.get(channel, (-1, -1))[1]
        return 0xFF if v < 0 else v

    def trigger(self, channel, cleanstart=True):
        # Start(cleanstart=true): live = base.  cleanstart=false: unchanged.
        if cleanstart:
            self.live[channel] = (self.dly if self.dly >= 0 else -1,
                                  self.rvb if self.rvb >= 0 else -1)

    def automate_dly(self, channel, send):
        # DLYS handler: live only.
        d, r = self.live.get(channel, (-1, -1))
        self.live[channel] = (clamp(send, 0, 100), r)

    def automate_rvb(self, channel, send):
        # RVBS handler: live only.
        d, r = self.live.get(channel, (-1, -1))
        self.live[channel] = (d, clamp(send, 0, 100))


def effective_send(instr, track_dly, track_rvb, channel):
    """Mirror PlayerChannel::Render for one channel."""
    d = track_dly
    r = track_rvb
    dry = 100
    if instr is not None:
        dv = instr.get_live_delay(channel)
        rv = instr.get_live_reverb(channel)
        if dv != 0xFF:
            d = dv
        if rv != 0xFF:
            r = rv
        dry = instr.get_dry()
    dg = fl2fp((d * dry) / 10000.0)
    rg = fl2fp((r * dry) / 10000.0)
    return dg, rg


# ---------------------------------------------------------------------------
# Acceptance checks
# ---------------------------------------------------------------------------
def check_defaults():
    instr = InstrumentModel()
    assert instr.get_dry() == 100
    assert instr.get_delay_send_override() == 0
    assert instr.get_reverb_send_override() == 0
    print("defaults DRY=100 / DLY=0 / RVB=0 OK")


def check_default_silent():
    instr = InstrumentModel()
    instr.trigger(0)
    dg, rg = effective_send(instr, 80, 80, 0)
    assert dg == 0 and rg == 0
    print("default instrument is silent regardless of track sends OK")


def check_legacy_inherit():
    instr = InstrumentModel(dly=-1, rvb=-1)
    instr.trigger(2)
    dg, rg = effective_send(instr, 60, 40, 2)
    assert dg == fl2fp(60 / 100.0), fp2fl(dg)
    assert rg == fl2fp(40 / 100.0), fp2fl(rg)
    print("legacy -1 instrument inherits the per-track sends exactly OK")


def check_automation_live_only():
    instr = InstrumentModel(dly=70, rvb=30)
    instr.trigger(0)
    instr.automate_dly(0, 15)   # phrase DLYS
    instr.automate_rvb(0, 85)   # phrase RVBS
    dg, rg = effective_send(instr, 10, 10, 0)
    assert dg == fl2fp(15 / 100.0), fp2fl(dg)
    assert rg == fl2fp(85 / 100.0), fp2fl(rg)
    # persisted base untouched by automation
    assert instr.get_delay_send_override() == 70
    assert instr.get_reverb_send_override() == 30
    print("automation writes live only, persisted base untouched OK")


def check_trigger_restores_base():
    instr = InstrumentModel(dly=70, rvb=30)
    instr.trigger(0)
    instr.automate_dly(0, 5)
    instr.automate_rvb(0, 95)
    assert effective_send(instr, 0, 0, 0)[0] == fl2fp(5 / 100.0)
    instr.trigger(0)   # new note
    dg, rg = effective_send(instr, 0, 0, 0)
    assert dg == fl2fp(70 / 100.0), fp2fl(dg)
    assert rg == fl2fp(30 / 100.0), fp2fl(rg)
    print("new trigger restores live value from base OK")


def check_legato_keeps_live():
    instr = InstrumentModel(dly=70, rvb=30)
    instr.trigger(0)
    instr.automate_dly(0, 20)
    instr.trigger(0, cleanstart=False)   # legato must not reset the live value
    dg, _ = effective_send(instr, 0, 0, 0)
    assert dg == fl2fp(20 / 100.0), fp2fl(dg)
    print("legato does not reset the live value OK")


def check_channels_independent():
    instr = InstrumentModel(dly=50, rvb=50)
    instr.trigger(0)
    instr.trigger(3)
    instr.automate_dly(0, 100)
    instr.automate_rvb(0, 0)
    # channel 0 automated, channel 3 untouched -> still base on its own channel
    dg0, rg0 = effective_send(instr, 0, 0, 0)
    dg3, rg3 = effective_send(instr, 0, 0, 3)
    assert dg0 == fl2fp(1.0) and rg0 == 0
    assert dg3 == fl2fp(0.5) and rg3 == fl2fp(0.5)
    print("per-channel live values are independent OK")


def check_instrument_switch():
    # Track has a running note with instrument A (send 10); phrase moves the
    # note to instrument B (base 90).  A new trigger on B yields B's base.
    a = InstrumentModel(dly=10, rvb=10)
    b = InstrumentModel(dly=90, rvb=60)
    a.trigger(1)
    a.automate_dly(1, 30)
    b.trigger(1)   # instrument switch = new note on B
    dg, rg = effective_send(b, 50, 50, 1)
    assert dg == fl2fp(90 / 100.0), fp2fl(dg)
    assert rg == fl2fp(60 / 100.0), fp2fl(rg)
    print("instrument switch resets to the new instrument base OK")


def check_src_guards():
    sp = (ROOT / "source/sources/Application/Instruments/SampleRenderingParams.h").read_text()
    si_h = (ROOT / "source/sources/Application/Instruments/SampleInstrument.h").read_text()
    si_cpp = (ROOT / "source/sources/Application/Instruments/SampleInstrument.cpp").read_text()
    ii_h = (ROOT / "source/sources/Application/Instruments/I_Instrument.h").read_text()
    pc_cpp = (ROOT / "source/sources/Application/Player/PlayerChannel.cpp").read_text()

    assert "int dlySend_" in sp and "int rvbSend_" in sp
    assert "TREEFROG_SEND_LIVE_V1" in sp
    for token in ("GetLiveDelaySend", "GetLiveReverbSend"):
        assert token in si_h, token
        assert token in ii_h, token
        assert token in si_cpp, token
        assert token in pc_cpp, token
    for token in ("new Variable(\"dry\", SIP_DRY, 100, 100)",
                  "new Variable(\"dly send\", SIP_DLY_SEND, 0, 100)",
                  "new Variable(\"rvb send\", SIP_RVB_SEND, 0, 100)",
                  "renderParams_[channel].dlySend_=send",
                  "renderParams_[channel].rvbSend_=send",
                  "rp->dlySend_=GetFxDelaySendOverride()",
                  "rp->rvbSend_=GetFxReverbSendOverride()"):
        assert token in si_cpp, token
    print("source guards OK")


check_defaults()
check_default_silent()
check_legacy_inherit()
check_automation_live_only()
check_trigger_restores_base()
check_legato_keeps_live()
check_channels_independent()
check_instrument_switch()
check_src_guards()
print("FX_LIVE_SENDS_PHASE15_OK")
