#!/usr/bin/env python3
"""Phase 7 model tests: compatibility with existing per-track sends
(PLAN_FX_REDESIGN_ES.md, Fase 7).

Mirrors the Fase 7 controlled non-destructive migration strategy:

- Projects saved by the exploratory tag keep per-track DELAYSEND/REVERBSEND in
  the Mixer CHANNEL attributes; instruments may additionally carry DRY/DLY/RVB
  PARAMs (Fase 6).
- On load, an instrument WITHOUT an override (-1) inherits the per-track Mixer
  send, so legacy exploratory songs reproduce exactly as before.
- An instrument WITH an override keeps it (per-instrument sends win), so a
  track using several instruments never loses any instrument's send.
- Saving writes both layers again (Mixer channel attrs + instrument PARAMs);
  no original value is lost.
- The per-track send is never deleted; it is only superseded per-instrument.
- A classic project (no FX, no sends) stays bit-identical (DRY=100, sends 0).

Acceptance:
- classic no-FX project sounds identical (sends 0, DRY 100)
- exploratory project with per-track sends plays identically on load
- instrument override wins over the per-track send for that instrument only
- two instruments on the same track keep distinct effective sends
- save/load round-trip preserves both the track send and the instrument send
- the per-track send is never zeroed by the instrument send (layers coexist)
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
# Models (mirror PlayerChannel.cpp Fase 6/7 + Mixer.cpp DELAYSEND/REVERBSEND)
# ---------------------------------------------------------------------------
class MixerModel:
    def __init__(self):
        # per-track legacy sends (persisted as DELAYSEND/REVERBSEND attrs)
        self.delaySend = [0] * 8
        self.reverbSend = [0] * 8

    def set_channel_delay_send(self, i, v):
        self.delaySend[i] = clamp(v, 0, 100)

    def set_channel_reverb_send(self, i, v):
        self.reverbSend[i] = clamp(v, 0, 100)


class InstrumentModel:
    def __init__(self, dry=100, dly=-1, rvb=-1):
        self.dry = dry
        self.dly = dly          # -1 = inherit
        self.rvb = rvb          # -1 = inherit

    def get_delay_send_override(self):
        return 0xFF if self.dly < 0 else self.dly

    def get_reverb_send_override(self):
        return 0xFF if self.rvb < 0 else self.rvb

    def get_dry(self):
        return clamp(self.dry, 0, 100)


def effective_send(instr, mixer, track):
    """Mirror PlayerChannel::Render: override wins, else inherit the track."""
    d = mixer.delaySend[track]
    r = mixer.reverbSend[track]
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
# Persistence round-trip model (mirror Mixer SaveContent/RestoreContent)
# ---------------------------------------------------------------------------
def save_project(mixer, instruments_by_track):
    """Serialize both layers: per-track sends + per-instrument PARAMs."""
    doc = {"channels": [], "instruments": {}}
    for i in range(8):
        doc["channels"].append(
            {"DELAYSEND": mixer.delaySend[i], "REVERBSEND": mixer.reverbSend[i]})
    for t, instr in instruments_by_track.items():
        doc["instruments"][t] = {
            "DRY": instr.dry, "DLY_SEND": instr.dly, "RVB_SEND": instr.rvb}
    return doc


def load_project(doc):
    mixer = MixerModel()
    instruments = {}
    for i, ch in enumerate(doc["channels"]):
        mixer.set_channel_delay_send(i, ch["DELAYSEND"])
        mixer.set_channel_reverb_send(i, ch["REVERBSEND"])
    for t, p in doc["instruments"].items():
        instruments[t] = InstrumentModel(p["DRY"], p["DLY_SEND"], p["RVB_SEND"])
    return mixer, instruments


def check_classic_project_identical():
    # Classic project: no FX, no sends, DRY 100 -> effective sends 0.
    mixer = MixerModel()
    instr = InstrumentModel()
    dg, rg = effective_send(instr, mixer, 3)
    assert dg == 0 and rg == 0
    print("classic no-FX project identical OK")


def check_exploratory_track_sends_preserved():
    # Exploratory project: per-track sends set, instrument inherits -> the
    # effective send equals the per-track send exactly (bit-identical legacy).
    mixer = MixerModel()
    mixer.set_channel_delay_send(2, 60)
    mixer.set_channel_reverb_send(2, 40)
    instr = InstrumentModel()  # no override -> inherit
    dg, rg = effective_send(instr, mixer, 2)
    assert dg == fl2fp(60 / 100.0), fp2fl(dg)
    assert rg == fl2fp(40 / 100.0), fp2fl(rg)
    print("exploratory per-track sends preserved on load OK")


def check_override_wins_only_that_instrument():
    # Instrument A overrides DLY; the per-track send still feeds instruments
    # without an override on the same track.
    mixer = MixerModel()
    mixer.set_channel_delay_send(0, 30)
    a = InstrumentModel(dly=80)   # override 80
    b = InstrumentModel()         # inherit 30
    da, _ = effective_send(a, mixer, 0)
    db, _ = effective_send(b, mixer, 0)
    assert da == fl2fp(80 / 100.0), fp2fl(da)
    assert db == fl2fp(30 / 100.0), fp2fl(db)
    print("override wins only for that instrument OK")


def check_two_instruments_distinct_sends():
    # Two instruments on the same track produce distinct effective sends.
    mixer = MixerModel()
    mixer.set_channel_delay_send(5, 50)
    i1 = InstrumentModel(dly=100)
    i2 = InstrumentModel(dly=10)
    g1, _ = effective_send(i1, mixer, 5)
    g2, _ = effective_send(i2, mixer, 5)
    assert g1 != g2
    assert g1 == fl2fp(1.0)
    assert g2 == fl2fp(0.1)
    print("two instruments on same track keep distinct sends OK")


def check_save_restore_roundtrip():
    # Save/load must preserve BOTH the per-track send and the instrument send.
    mixer = MixerModel()
    mixer.set_channel_delay_send(1, 70)
    mixer.set_channel_reverb_send(1, 20)
    insts = {1: InstrumentModel(dry=80, dly=40, rvb=60)}
    doc = save_project(mixer, insts)
    m2, i2 = load_project(doc)
    assert m2.delaySend[1] == 70
    assert m2.reverbSend[1] == 20
    assert i2[1].dry == 80 and i2[1].dly == 40 and i2[1].rvb == 60
    # effective send after round-trip is unchanged
    dg, rg = effective_send(i2[1], m2, 1)
    assert dg == fl2fp((40 * 80) / 10000.0), fp2fl(dg)
    assert rg == fl2fp((60 * 80) / 10000.0), fp2fl(rg)
    print("save/restore round-trip preserves both layers OK")


def check_track_send_never_zeroed():
    # The instrument override supersedes but does NOT delete the track send:
    # after saving, the track attribute is still present (both layers coexist).
    mixer = MixerModel()
    mixer.set_channel_delay_send(4, 55)
    insts = {4: InstrumentModel(dly=90)}
    doc = save_project(mixer, insts)
    assert doc["channels"][4]["DELAYSEND"] == 55
    assert doc["instruments"][4]["DLY_SEND"] == 90
    m2, i2 = load_project(doc)
    # instrument override still wins; the track send is untouched
    assert m2.delaySend[4] == 55
    assert i2[4].dly == 90
    print("track send never deleted, layers coexist OK")


def check_legacy_no_fxmaster_default():
    # A project without any FX data loads to legacy defaults (sends 0).
    mixer = MixerModel()
    instr = InstrumentModel()
    dg, rg = effective_send(instr, mixer, 7)
    assert dg == 0 and rg == 0
    print("no-FXMASTER project defaults to legacy OK")


def check_src_guards():
    pc = (ROOT / "source/sources/Application/Player/PlayerChannel.cpp").read_text()
    m = (ROOT / "source/sources/Application/Model/Mixer.cpp").read_text()
    for token in ("DELAYSEND", "REVERBSEND", "GetFxDelaySendOverride",
                  "GetFxReverbSendOverride", "GetFxDry", "0xFF"):
        assert token in pc, token
    assert "DELAYSEND" in m and "REVERBSEND" in m
    assert "Fase 7" in pc or "Fase 7" in m
    print("source guards OK")


check_classic_project_identical()
check_exploratory_track_sends_preserved()
check_override_wins_only_that_instrument()
check_two_instruments_distinct_sends()
check_save_restore_roundtrip()
check_track_send_never_zeroed()
check_legacy_no_fxmaster_default()
check_src_guards()
print("FX_TRACK_SEND_COMPAT_PHASE7_OK")
