#!/usr/bin/env python3
"""Phase 6 model tests: per-instrument FX sends (PLAN_FX_REDESIGN_ES.md).

Faithful Q15 model of the Fase 6 per-instrument send architecture:

- SampleInstrument owns three persisted variables: DRY (0..100, default 100),
  DLY send override (0..100, default -1 = inherit), RVB send override (0..100,
  default -1 = inherit).  They are serialised as ordinary instrument PARAMs.
- GetFxDelaySendOverride/GetFxReverbSendOverride return the value when set
  (>=0) else 0xFF ("inherit the per-track Mixer send"); GetFxDry returns the
  DRY scale.
- DLYS/RVBS phrase commands write BOTH the instrument override and the
  per-track Mixer send (legacy UI + persistence stay consistent).
- PlayerChannel::Render computes the effective send gains:
      gain = (send% * DRY%) / 10000     (both 0..100)
  with send% = instrument override when set, else the per-track Mixer send.
  DRY=100 is bit-identical to the Fase 4/5 behaviour (no DRY).

Acceptance:
- default instruments inherit the per-track Mixer send exactly
- DRY=100 reproduces the Fase 4 effective gain (send/100)
- DRY scales the effective send linearly (DRY=50 halves it, DRY=0 kills it)
- instrument override wins over the per-track Mixer send when set
- overriding then "unsetting" is not representable (-1 only via persistence);
  the override stays until explicitly changed (documented)
- DLYS/RVBS still touch the per-track Mixer send (source guard)
- new variables exist in the source and are inserted in the constructor
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
# Instrument model (mirrors SampleInstrument Fase 6)
# ---------------------------------------------------------------------------
class InstrumentModel:
    def __init__(self):
        self.dry = 100
        self.dlySend = -1   # -1 = inherit
        self.rvbSend = -1   # -1 = inherit

    def get_delay_send_override(self):
        return 0xFF if self.dlySend < 0 else self.dlySend

    def get_reverb_send_override(self):
        return 0xFF if self.rvbSend < 0 else self.rvbSend

    def get_dry(self):
        return clamp(self.dry, 0, 100)

    def set_dly(self, send):
        self.dlySend = clamp(send, 0, 100)

    def set_rvb(self, send):
        self.rvbSend = clamp(send, 0, 100)


# ---------------------------------------------------------------------------
# PlayerChannel render model (mirrors PlayerChannel.cpp Fase 6)
# ---------------------------------------------------------------------------
def effective_send(instr, track_dly, track_rvb):
    d = track_dly
    r = track_rvb
    dry = 100
    if instr is not None:
        dv = instr.get_delay_send_override()
        rv = instr.get_reverb_send_override()
        if dv != 0xFF:
            d = dv
        if rv != 0xFF:
            r = rv
        dry = instr.get_dry()
    dg = fl2fp((d * dry) / 10000.0)
    rg = fl2fp((r * dry) / 10000.0)
    return dg, rg


# ---------------------------------------------------------------------------
# DLYS/RVBS handler model (mirrors SampleInstrument.cpp Fase 6)
# ---------------------------------------------------------------------------
def dlys(value):
    return (value & 0xFF) * 100 // 255


def check_default_inherits_track_send():
    instr = InstrumentModel()  # never touched by DLYS/RVBS
    dg, rg = effective_send(instr, 60, 40)
    # DRY=100, no override -> exactly the per-track send / 100
    assert dg == fl2fp(60 / 100.0), fp2fl(dg)
    assert rg == fl2fp(40 / 100.0), fp2fl(rg)
    print("default inherits per-track send OK")


def check_dry100_matches_fase4():
    # DRY=100 and no override must be bit-identical to Fase 4/5
    for td, tr in [(0, 0), (100, 100), (25, 75), (100, 0)]:
        instr = InstrumentModel()
        dg, rg = effective_send(instr, td, tr)
        assert dg == fl2fp(td / 100.0), (td, fp2fl(dg))
        assert rg == fl2fp(tr / 100.0), (tr, fp2fl(rg))
    print("DRY=100 matches Fase 4 gains OK")


def check_dry_scales_sends():
    instr = InstrumentModel()
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


def check_override_wins_over_track():
    track_dly = 10
    track_rvb = 10
    instr = InstrumentModel()
    instr.set_dly(90)   # DLYS on this instrument
    dg, rg = effective_send(instr, track_dly, track_rvb)
    assert dg == fl2fp(90 / 100.0), fp2fl(dg)   # instrument override wins
    assert rg == fl2fp(10 / 100.0), fp2fl(rg)   # RVB still inherits track
    print("instrument override wins over per-track OK")


def check_override_with_dry():
    instr = InstrumentModel()
    instr.set_dly(100)
    instr.set_rvb(50)
    instr.dry = 60
    dg, rg = effective_send(instr, 0, 0)  # track sends 0, override still applies
    assert dg == fl2fp((100 * 60) / 10000.0), fp2fl(dg)
    assert rg == fl2fp((50 * 60) / 10000.0), fp2fl(rg)
    print("override combined with DRY OK")


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
                  "GetFxDry", "dry_", "dlySend_", "rvbSend_"):
        assert token in si_h, token
    for token in ("new Variable(\"dry\"", "new Variable(\"dly send\"",
                  "new Variable(\"rvb send\"", "GetFxDelaySendOverride",
                  "GetFxReverbSendOverride", "GetFxDry",
                  "dlySend_->SetInt(send)", "rvbSend_->SetInt(send)",
                  "Mixer::GetInstance()->SetChannelDelaySend",
                  "Mixer::GetInstance()->SetChannelReverbSend"):
        assert token in si_cpp, token
    for token in ("GetFxDelaySendOverride", "GetFxReverbSendOverride",
                  "GetFxDry", "0xFF"):
        assert token in ii_h, token
    for token in ("GetFxDelaySendOverride", "GetFxReverbSendOverride",
                  "GetFxDry", "10000.0f", "AccumulateChannelSend"):
        assert token in pc_cpp, token
    print("source guards OK")


check_default_inherits_track_send()
check_dry100_matches_fase4()
check_dry_scales_sends()
check_override_wins_over_track()
check_override_with_dry()
check_dlys_rvbs_mapping()
check_src_guards()
print("FX_INSTRUMENT_SENDS_PHASE6_OK")
