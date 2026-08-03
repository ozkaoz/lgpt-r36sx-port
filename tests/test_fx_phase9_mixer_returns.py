#!/usr/bin/env python3
"""Phase 9 model tests: MixerView MIX page FX RETURNS (PLAN_FX_REDESIGN_ES.md).

Mirrors the Fase 9 MIX page cleanup:

- The per-track D/R send readouts are gone from the MIX page (drawMixSends is
  removed and NudgeChannelDelaySend/NudgeChannelReverbSend are no longer wired
  into the view).  Sends are per-instrument now (Fase 6/15, edited in
  InstrumentView); the per-track sends survive only as the Fase 7
  inheritance/compatibility layer (still persisted).  Since Fase 15, DLYS/RVBS
  automation writes only the live per-channel instrument override, never the
  per-track Mixer send.
- The MIX page edit target cycles VOL -> DLY RET -> RVB RET.  VOL edits the
  hovered channel volume; DLY RET / RVB RET edit the MASTER return levels of
  the delay/reverb into the master bus (FxEngine SetDelayReturn/SetReverbReturn).
- Returns are fixed (Q15) 0..1 levels shown/edited as integer percent 0..100;
  the percent helpers clamp so every value round-trips.
- The returns are persisted as FXMASTER DLYRET/RVBRET attributes.

Acceptance:
- return percent <-> fixed round-trip is lossless for every 0..100 value
- the target cycle VOL -> DLY RET -> RVB RET wraps
- returns are global (not per-channel): editing from any channel changes the
  same master level
- source guards: drawMixSends gone, drawMixReturns + nudge helpers present,
  per-track send nudges no longer wired in the view
- persistence guards: DLYRET/RVBRET saved and restored
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

MIX = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
MIX_H = (ROOT / "source/sources/Application/Views/MixerView.h").read_text()
MIXER = (ROOT / "source/sources/Application/Model/Mixer.cpp").read_text()

SHIFT = 15
SCALE = 1 << SHIFT


def fl2fp(f):
    return int(round(f * SCALE))


def fp2fl(a):
    return a / SCALE


def percent(ret_fp):
    f = max(0.0, min(1.0, fp2fl(ret_fp)))
    return int(round(f * 100.0))


def from_percent(p):
    p = max(0, min(100, p))
    return fl2fp(p * 0.01)


class MixReturnsModel:
    """Mirror MixerView: one global DLY/RVB return edited by UP/DOWN."""

    def __init__(self):
        self.dly_ret = fl2fp(0.5)
        self.rvb_ret = fl2fp(0.5)
        self.target = 0  # 0=VOL 1=DLY RET 2=RVB RET

    def cycle(self):
        self.target = (self.target + 1) % 3

    def edit(self, delta):
        if self.target == 1:
            self.dly_ret = from_percent(percent(self.dly_ret) + delta)
        elif self.target == 2:
            self.rvb_ret = from_percent(percent(self.rvb_ret) + delta)


def check_percent_roundtrip():
    for p in range(0, 101):
        assert percent(from_percent(p)) == p, p
    # default return is the Fase 6 helper 0.5 = 50%
    assert percent(fl2fp(0.5)) == 50
    print("return percent <-> fixed round-trip OK")


def check_target_cycle():
    m = MixReturnsModel()
    seen = []
    for _ in range(6):
        m.cycle()
        seen.append(m.target)
    assert seen == [1, 2, 0, 1, 2, 0]
    print("edit target cycles VOL -> DLY RET -> RVB RET OK")


def check_global_semantics():
    m = MixReturnsModel()
    # edit from "any channel": target 1 then edit changes the single global DLY RET
    m.cycle()          # -> DLY RET
    m.edit(30)         # 50 -> 80
    assert percent(m.dly_ret) == 80
    m.cycle()          # -> RVB RET
    m.edit(-20)        # 50 -> 30
    assert percent(m.rvb_ret) == 30
    m.edit(200)        # clamps to 100
    assert percent(m.rvb_ret) == 100
    m.cycle()          # -> VOL
    assert percent(m.dly_ret) == 80  # unchanged while editing VOL
    assert percent(m.rvb_ret) == 100
    print("returns are global master levels (not per-channel) OK")


def check_src_guards():
    assert "drawMixReturns" in MIX
    assert "drawMixSends" not in MIX
    assert "NudgeChannelDelaySend" not in MIX
    assert "NudgeChannelReverbSend" not in MIX
    assert "nudgeDelayReturn" in MIX and "nudgeDelayReturn" in MIX_H
    assert "nudgeReverbReturn" in MIX and "nudgeReverbReturn" in MIX_H
    assert "fxReturnPercent" in MIX and "fxReturnFromPercent" in MIX
    assert "GetDelayReturn" in MIX and "SetDelayReturn" in MIX
    assert "GetReverbReturn" in MIX and "SetReverbReturn" in MIX
    # the FxEngine setters/getters still exist on the DSP side
    fxe = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.h").read_text()
    for token in ("GetDelayReturn", "SetDelayReturn",
                  "GetReverbReturn", "SetReverbReturn"):
        assert token in fxe, token
    # per-track sends are still the Fase 7 inheritance layer (not deleted)
    assert "DELAYSEND" in MIXER and "REVERBSEND" in MIXER
    print("MIX page source guards OK")


def check_persistence_guards():
    assert "DLYRET" in MIXER
    assert "RVBRET" in MIXER
    # the attributes are both saved and restored
    assert 'SetAttribute("DLYRET"' in MIXER and 'Attribute("DLYRET"' in MIXER
    assert 'SetAttribute("RVBRET"' in MIXER and 'Attribute("RVBRET"' in MIXER
    print("returns persistence guards OK")


check_percent_roundtrip()
check_target_cycle()
check_global_semantics()
check_src_guards()
check_persistence_guards()
print("FX_MIXER_RETURNS_PHASE9_OK")
