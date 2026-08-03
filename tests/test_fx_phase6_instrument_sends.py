#!/usr/bin/env python3
"""Phase 6/15 model tests: per-instrument FX sends (PLAN_FX_REDESIGN_ES.md).

Faithful model of the Fase 6 + Fase 15 per-instrument send architecture:

- SampleInstrument owns three persisted variables: DRY (0..100, default 100),
  DLY send override (0..100, default 0), RVB send override (0..100, default 0).
  They are serialised as ordinary instrument PARAMs.  A value of -1 (only
  reachable from projects saved by older exploratory builds) means "inherit the
  per-track Mixer send".
- GetFxDelaySendOverride/GetFxReverbSendOverride return the persisted base when
  set (>=0) else 0xFF; GetFxDry returns the DRY scale.
- TREEFROG_SEND_LIVE_V1 (Fase 15): DLYS/RVBS phrase/table automation writes ONLY
  the live per-channel override.  A new note trigger restores the live value
  from the persisted base.  The persisted base is never clobbered by automation.
- PlayerChannel::Render reads the LIVE override (GetLiveDelaySend /
  GetLiveReverbSend): 0xFF inherits the per-track Mixer send, 0..100 wins.
- Effective gain = (send% * DRY%) / 10000  (both 0..100).

Acceptance:
- a default instrument (base sends 0) has effective sends 0 regardless of track
- a legacy instrument (base -1) inherits the per-track Mixer send exactly
- DRY=100 reproduces the Fase 4 effective gain when the track send is inherited
- DRY scales the effective send linearly
- instrument base override wins over the per-track Mixer send when set
- DLYS/RVBS automation modulates the LIVE value only; the base stays untouched
- a new trigger restores the live value from the base
- source guards for the I_Instrument virtuals and PlayerChannel wiring
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
# Instrument model (mirrors SampleInstrument Fase 6 + 15)
# ---------------------------------------------------------------------------
class InstrumentModel:
    def __init__(self):
        self.dry = 100
        self.dlySend = 0        # persisted base, default 0 (no send)
        self.rvbSend = 0        # persisted base, default 0
        self.liveDly = 0        # live per-channel override after a trigger
        self.liveRvb = 0

    def get_delay_send_override(self):
        return 0xFF if self.dlySend < 0 else self.dlySend

    def get_reverb_send_override(self):
        return 0xFF if self.rvbSend < 0 else self.rvbSend

    def get_live_delay(self):
        return 0xFF if self.liveDly < 0 else self.liveDly

    def get_live_reverb(self):
        return 0xFF if self.liveRvb < 0 else self.liveRvb

    def get_dry(self):
        return clamp(self.dry, 0, 100)

    def set_base_dly(self, send):
        self.dlySend = send         # raw; -1 = legacy inherit, else 0..100

    def set_base_rvb(self, send):
        self.rvbSend = send

    def set_dly(self, send):
        self.liveDly = clamp(send, 0, 100)   # phrase/table DLYS -> live only

    def set_rvb(self, send):
        self.liveRvb = clamp(send, 0, 100)   # phrase/table RVBS -> live only

    def trigger(self):
        # Start(cleanstart=true) restores the live value from the base.
        self.liveDly = self.dlySend if self.dlySend >= 0 else -1
        self.liveRvb = self.rvbSend if self.rvbSend >= 0 else -1


# ---------------------------------------------------------------------------
# PlayerChannel render model (mirrors PlayerChannel.cpp Fase 6 + 15)
# ---------------------------------------------------------------------------
def effective_send(instr, track_dly, track_rvb):
    d = track_dly
    r = track_rvb
    dry = 100
    if instr is not None:
        dv = instr.get_live_delay()
        rv = instr.get_live_reverb()
        if dv != 0xFF:
            d = dv
        if rv != 0xFF:
            r = rv
        dry = instr.get_dry()
    dg = fl2fp((d * dry) / 10000.0)
    rg = fl2fp((r * dry) / 10000.0)
    return dg, rg


# ---------------------------------------------------------------------------
# DLYS/RVBS handler model (mirrors SampleInstrument.cpp Fase 15)
# ---------------------------------------------------------------------------
def dlys(value):
    return (value & 0xFF) * 100 // 255


def check_default_instrument_silent():
    instr = InstrumentModel()
    instr.trigger()
    dg, rg = effective_send(instr, 60, 40)
    # default base sends are 0 -> effective sends 0 regardless of the track
    assert dg == 0, fp2fl(dg)
    assert rg == 0, fp2fl(rg)
    print("default instrument (base 0) is silent regardless of track OK")


def check_legacy_inherits_track_send():
    instr = InstrumentModel()
    instr.set_base_dly(-1)   # legacy exploratory project: inherit
    instr.set_base_rvb(-1)
    instr.trigger()
    dg, rg = effective_send(instr, 60, 40)
    assert dg == fl2fp(60 / 100.0), fp2fl(dg)
    assert rg == fl2fp(40 / 100.0), fp2fl(rg)
    print("legacy instrument inherits per-track send exactly OK")


def check_dry100_matches_fase4():
    # DRY=100 and a legacy inherit instrument must be bit-identical to Fase 4/5.
    for td, tr in [(0, 0), (100, 100), (25, 75), (100, 0)]:
        instr = InstrumentModel()
        instr.set_base_dly(-1)
        instr.set_base_rvb(-1)
        instr.trigger()
        dg, rg = effective_send(instr, td, tr)
        assert dg == fl2fp(td / 100.0), (td, fp2fl(dg))
        assert rg == fl2fp(tr / 100.0), (tr, fp2fl(rg))
    print("DRY=100 matches Fase 4 gains OK")


def check_dry_scales_sends():
    instr = InstrumentModel()
    instr.set_base_dly(-1)
    instr.set_base_rvb(-1)
    instr.trigger()
    instr.dry = 50
    dg, _ = effective_send(instr, 100, 0)
    assert dg == fl2fp(0.5), fp2fl(dg)
    instr.dry = 0
    dg, rg = effective_send(instr, 100, 100)
    assert dg == 0 and rg == 0
    instr.dry = 25
    dg, _ = effective_send(instr, 80, 0)
    assert dg == fl2fp((80 * 25) / 10000.0), fp2fl(dg)
    print("DRY scales effective sends OK")


def check_base_override_wins_over_track():
    track_dly = 10
    track_rvb = 10
    instr = InstrumentModel()
    instr.set_base_dly(90)   # instrument base override
    instr.set_base_rvb(-1)   # RVB keeps inheriting the track
    instr.trigger()
    dg, rg = effective_send(instr, track_dly, track_rvb)
    assert dg == fl2fp(90 / 100.0), fp2fl(dg)   # override wins
    assert rg == fl2fp(10 / 100.0), fp2fl(rg)   # RVB inherits track
    print("instrument base override wins over per-track OK")


def check_override_with_dry():
    instr = InstrumentModel()
    instr.set_base_dly(100)
    instr.set_base_rvb(50)
    instr.dry = 60
    instr.trigger()
    dg, rg = effective_send(instr, 0, 0)  # track sends 0, override still applies
    assert dg == fl2fp((100 * 60) / 10000.0), fp2fl(dg)
    assert rg == fl2fp((50 * 60) / 10000.0), fp2fl(rg)
    print("base override combined with DRY OK")


def check_automation_writes_live_only():
    # DLYS/RVBS from a phrase/table must modulate the live value and never
    # write the persisted base.
    instr = InstrumentModel()
    instr.set_base_dly(90)
    instr.set_base_rvb(40)
    instr.trigger()
    instr.set_dly(50)   # phrase DLYS mid-note
    instr.set_rvb(30)   # phrase RVBS mid-note
    dg, rg = effective_send(instr, 10, 10)
    assert dg == fl2fp(50 / 100.0), fp2fl(dg)
    assert rg == fl2fp(30 / 100.0), fp2fl(rg)
    # the persisted base is untouched by automation
    assert instr.get_delay_send_override() == 90
    assert instr.get_reverb_send_override() == 40
    print("DLYS/RVBS automation writes live only, base untouched OK")


def check_new_trigger_restores_base():
    # After automation sweeps the live value, a new note trigger restores it
    # from the persisted base.
    instr = InstrumentModel()
    instr.set_base_dly(90)
    instr.set_base_rvb(40)
    instr.trigger()
    instr.set_dly(5)    # phrase automation
    instr.set_rvb(95)
    dg, _ = effective_send(instr, 10, 10)
    assert dg == fl2fp(5 / 100.0), fp2fl(dg)
    instr.trigger()     # new note
    dg, rg = effective_send(instr, 10, 10)
    assert dg == fl2fp(90 / 100.0), fp2fl(dg)
    assert rg == fl2fp(40 / 100.0), fp2fl(rg)
    print("new trigger restores live value from base OK")


def check_dlys_rvbs_mapping():
    assert dlys(0) == 0
    assert dlys(255) == 100
    assert dlys(128) == 50
    assert dlys(0x12FF) == 100  # high byte (speed) ignored
    print("DLYS/RVBS byte mapping OK")


def check_src_guards():
    si_h = (ROOT / "source/sources/Application/Instruments/SampleInstrument.h").read_text()
    si_cpp = (ROOT / "source/sources/Application/Instruments/SampleInstrument.cpp").read_text()
    ii_h = (ROOT / "source/sources/Application/Instruments/I_Instrument.h").read_text()
    pc_cpp = (ROOT / "source/sources/Application/Player/PlayerChannel.cpp").read_text()

    for token in ("SIP_DRY", "SIP_DLY_SEND", "SIP_RVB_SEND",
                  "GetFxDelaySendOverride", "GetFxReverbSendOverride",
                  "GetLiveDelaySend", "GetLiveReverbSend",
                  "GetFxDry", "dry_", "dlySend_", "rvbSend_"):
        assert token in si_h, token
    for token in ("new Variable(\"dry\"", "new Variable(\"dly send\"",
                  "new Variable(\"rvb send\"", "GetFxDelaySendOverride",
                  "GetFxReverbSendOverride", "GetLiveDelaySend",
                  "GetLiveReverbSend", "GetFxDry",
                  "renderParams_[channel].dlySend_", "renderParams_[channel].rvbSend_",
                  "new Variable(\"dly send\", SIP_DLY_SEND, 0, 100)",
                  "new Variable(\"rvb send\", SIP_RVB_SEND, 0, 100)"):
        assert token in si_cpp, token
    for token in ("GetFxDelaySendOverride", "GetFxReverbSendOverride",
                  "GetLiveDelaySend", "GetLiveReverbSend",
                  "GetFxDry", "0xFF"):
        assert token in ii_h, token
    for token in ("GetLiveDelaySend", "GetLiveReverbSend",
                  "GetFxDry", "10000.0f", "AccumulateChannelSend"):
        assert token in pc_cpp, token
    print("source guards OK")


check_default_instrument_silent()
check_legacy_inherits_track_send()
check_dry100_matches_fase4()
check_dry_scales_sends()
check_base_override_wins_over_track()
check_override_with_dry()
check_automation_writes_live_only()
check_new_trigger_restores_base()
check_dlys_rvbs_mapping()
check_src_guards()
print("FX_INSTRUMENT_SENDS_PHASE6_OK")
