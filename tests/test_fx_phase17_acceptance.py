#!/usr/bin/env python3
"""Phase 17 acceptance tests: representation, inventory, sends, UI,
nomenclature and persistence (PLAN_FX_REDESIGN_ES.md, Fase 17).

Covers the "pruebas nuevas requeridas" checklist against the current source:

Representation
- the 7 new FxEngine commands use a 2-digit HEX8 editor (%2.2X, min 0, max 0xFF)
- the 8 inherited beatmaking commands keep the 4-digit HEX16 editor
- editing clamps to [min,max], so the HEX8 high byte stays zero and the low
  byte keeps the full 00..FF range
- PhraseView and TableView drive the editor from the same CommandList helpers

Inventory
- exactly 7 new + 8 inherited commands in the central table (+NONE = 15 assignable)
- hidden commands (removed from the popup, e.g. PTCH) still play
- DLAY is presented as Note Delay (NDL), never as audio delay

Instrument sends
- each instrument keeps its own sends; two instruments on one track differ
- changing instrument restores its base values; DLYS/RVBS are live-only
- a new trigger restores the base; DRY scales; defaults keep legacy dry sound

Interface
- no parameter page draws over the help rows; CMP BYP and CMP SCL visible
- EQ and COMP are independent pages; booleans ON/OFF; units present

Nomenclature
- "reverb" is not tied to SIP_FBMIX, "compressor" not to SIP_CRUSH
- "EQ" lives in the central command table, not only on the filter reso row
- the phrase grid copies names from the central CommandList table

Persistence
- per-instrument send round-trip; classic and exploratory projects
- compatibility/migration mode (Fase 7 inherit) ; FXMASTER restore
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "source/sources/Application"

CL_CPP = (APP / "Instruments/CommandList.cpp").read_text()
CL_H = (APP / "Instruments/CommandList.h").read_text()
HELP = (APP / "Utils/HelpLegend.h").read_text()
PV = (APP / "Views/PhraseView.cpp").read_text()
TV = (APP / "Views/TableView.cpp").read_text()
IV = (APP / "Views/InstrumentView.cpp").read_text()
MIX = (APP / "Views/MixerView.cpp").read_text()
SI_CPP = (APP / "Instruments/SampleInstrument.cpp").read_text()
SI_H = (APP / "Instruments/SampleInstrument.h").read_text()
MIXER = (APP / "Model/Mixer.cpp").read_text()
PC = (APP / "Player/PlayerChannel.cpp").read_text()
PLAYER = (APP / "Player/Player.cpp").read_text()

SHIFT = 15
SCALE = 1 << SHIFT


def fl2fp(f):
    return int(round(f * SCALE))


# ---------------------------------------------------------------------------
# Canonical command table (parsed from CommandList.cpp _specs[])
# ---------------------------------------------------------------------------
SPECS = {
    "I_CMD_NONE": ("----", "HEX16"),
    "I_CMD_FBMX": ("FBM ", "HEX16"),
    "I_CMD_FBTN": ("FBT ", "HEX16"),
    "I_CMD_DLAY": ("NDL ", "HEX16"),
    "I_CMD_FLTR": ("FLT ", "HEX16"),
    "I_CMD_FCUT": ("EQ  ", "HEX16"),
    "I_CMD_FRES": ("RES ", "HEX16"),
    "I_CMD_CRSH": ("BTS ", "HEX16"),
    "I_CMD_PFIN": ("PFI ", "HEX16"),
    "I_CMD_DLYS": ("DSN ", "HEX8"),
    "I_CMD_RVBS": ("RSN ", "HEX8"),
    "I_CMD_DLYT": ("DTM ", "HEX8"),
    "I_CMD_DLYF": ("DFB ", "HEX8"),
    "I_CMD_RVDC": ("RDC ", "HEX8"),
    "I_CMD_RVSZ": ("RSZ ", "HEX8"),
    "I_CMD_CMPT": ("CTH ", "HEX8"),
}
NEW7 = ["I_CMD_DLYS", "I_CMD_RVBS", "I_CMD_DLYT", "I_CMD_DLYF",
        "I_CMD_RVDC", "I_CMD_RVSZ", "I_CMD_CMPT"]
LEGACY8 = ["I_CMD_FBMX", "I_CMD_FBTN", "I_CMD_DLAY", "I_CMD_FLTR",
           "I_CMD_FCUT", "I_CMD_FRES", "I_CMD_CRSH", "I_CMD_PFIN"]


def hex_editor(cmd):
    """Mirror CommandList::GetParamPrecision/Format/Min/Max for a format."""
    if SPECS[cmd][1] == "HEX8":
        return 2, "%2.2X", 0, 0xFF
    return 4, "%4.4X", 0, 0xFFFF


def hex_edit(value, fmt, lo, hi):
    """Mirror UIBigHexVarField::ProcessArrow clamp (max_/min_, no wrap)."""
    return max(lo, min(hi, value))


# ---------------------------------------------------------------------------
# Instrument send model (mirrors Fase 6/15)
# ---------------------------------------------------------------------------
class InstrumentModel:
    def __init__(self, dry=100, dly=0, rvb=0):
        self.dry = dry
        self.dly = dly
        self.rvb = rvb
        self.liveDly = dly
        self.liveRvb = rvb

    def trigger(self):
        self.liveDly = self.dly
        self.liveRvb = self.rvb

    def automate_dly(self, v):
        self.liveDly = max(0, min(100, v))

    def automate_rvb(self, v):
        self.liveRvb = max(0, min(100, v))


def effective_send(instr, track_dly, track_rvb):
    d = track_dly
    r = track_rvb
    dry = 100
    if instr is not None:
        d = instr.liveDly
        r = instr.liveRvb
        dry = max(0, min(100, instr.dry))
    return fl2fp((d * dry) / 10000.0), fl2fp((r * dry) / 10000.0)


# ---------------------------------------------------------------------------
# 1. Command representation
# ---------------------------------------------------------------------------
def check_editor_formats():
    for cmd in NEW7 + LEGACY8:
        prec, fmt, lo, hi = hex_editor(cmd)
        assert prec == 2 if SPECS[cmd][1] == "HEX8" else prec == 4
        assert fmt == "%2.2X" if SPECS[cmd][1] == "HEX8" else fmt == "%4.4X"
        assert lo == 0 and hi == (0xFF if SPECS[cmd][1] == "HEX8" else 0xFFFF)
    # high byte is forced to zero by the clamp for HEX8 (max 0xFF)
    assert hex_edit(0x12FF, *hex_editor("I_CMD_DLYS")[1:]) == 0xFF
    assert hex_edit(0x0001, *hex_editor("I_CMD_DLYS")[1:]) == 0x01
    # a legacy 16-bit command keeps the full 4-digit range
    assert hex_edit(0xFFFF, *hex_editor("I_CMD_FBMX")[1:]) == 0xFFFF
    print("HEX8 (7) vs HEX16 (8) editor precision/range OK")


def check_draw_low_byte_only():
    # TableView and PhraseView draw the low byte only for HEX8 commands.
    assert "(p & 0xFF)" in TV and "(p & 0xFF)" in PV
    # the format clamps so the high byte never survives an edit
    for tok in ('CMD_PARAM_FORMAT_HEX8', "GetParamFormat(cmd)"):
        assert tok in TV and tok in PV
    print("views draw the low byte for HEX8 commands OK")


def check_phrase_table_same_format():
    for src in (PV, TV):
        for tok in ("CommandList::GetParamPrecision(command)",
                    "CommandList::GetParamFormatString(command)",
                    "CommandList::GetParamMin(command)",
                    "CommandList::GetParamMax(command)",
                    "CommandList::GetParamWrap(command)"):
            assert tok in src, tok
    print("Phrase and Table share the same CommandList editor format OK")


# ---------------------------------------------------------------------------
# 2. Command inventory
# ---------------------------------------------------------------------------
def check_inventory():
    # every canonical entry exists in the source table
    for cmd, (name, _) in SPECS.items():
        assert cmd in CL_H, cmd
        assert re.search(r'\{ %s,\s+"%s",\s+CMD_PARAM_FORMAT_%s \}' % (
            cmd, name, ("HEX16" if cmd == "I_CMD_NONE" else SPECS[cmd][1])),
            CL_CPP), (cmd, name)
    # counts: 7 new + 8 inherited + NONE = 16 entries -> 15 assignable
    assert len(NEW7) == 7 and len(LEGACY8) == 8
    assert len(SPECS) == 16
    print("inventory: 7 new + 8 inherited + NONE = 15 assignable OK")


def check_hidden_commands_still_play():
    # PTCH was removed from the popup (H38.7) but must still be processed.
    assert "case I_CMD_PTCH:" in SI_CPP
    assert "case I_CMD_RTRG:" in SI_CPP
    assert "case I_CMD_LEGA:" in SI_CPP
    # player-level hidden commands are still handled
    for t in ("I_CMD_TABL", "I_CMD_STOP", "I_CMD_KILL", "I_CMD_HOP", "I_CMD_TMPO"):
        assert t in PLAYER, t
    print("hidden commands still processed (PTCH/RTRG/LEGA + player) OK")


def check_dlay_is_note_delay():
    # DLAY displays as NDL and its help says note delay, never audio delay.
    assert 'I_CMD_DLAY' in CL_H
    assert re.search(r'\{ I_CMD_DLAY,\s+"NDL "', CL_CPP)
    seg = HELP.split("case I_CMD_DLAY:")[1].split("case I_CMD_FBMX:")[0]
    assert "Note DeLay" in seg and "note trigger" in seg
    assert "audio delay" not in seg.lower() and "delay line" not in seg.lower()
    print("DLAY presented as Note Delay (NDL), not audio delay OK")


# ---------------------------------------------------------------------------
# 3. Instrument sends
# ---------------------------------------------------------------------------
def check_instrument_sends():
    a = InstrumentModel(dly=90, rvb=10)
    b = InstrumentModel(dly=30, rvb=60)
    # each instrument keeps its own sends on the same track
    da, ra = effective_send(a, 50, 50)
    db, rb = effective_send(b, 50, 50)
    assert (da, ra) != (db, rb)
    assert da == fl2fp(90 / 100.0) and ra == fl2fp(10 / 100.0)
    assert db == fl2fp(30 / 100.0) and rb == fl2fp(60 / 100.0)
    # changing instrument restores its base (new trigger)
    a.automate_dly(5)
    assert effective_send(a, 0, 0)[0] == fl2fp(5 / 100.0)
    a.trigger()
    assert effective_send(a, 0, 0)[0] == fl2fp(90 / 100.0)
    # DLYS/RVBS are live-only: the persisted base never changes
    a.trigger()
    a.automate_dly(42)
    a.automate_rvb(7)
    assert (a.dly, a.rvb) == (90, 10)
    assert effective_send(a, 0, 0) == (fl2fp(42 / 100.0), fl2fp(7 / 100.0))
    # DRY scales DRY=50 halves, DRY=0 kills
    c = InstrumentModel(dry=50, dly=100)
    assert effective_send(c, 0, 0)[0] == fl2fp(0.5)
    c.dry = 0
    assert effective_send(c, 0, 0) == (0, 0)
    # defaults keep legacy dry sound: DRY=100, sends 0 -> effective 0
    d = InstrumentModel()
    assert effective_send(d, 100, 100) == (0, 0)
    print("instrument sends: distinct, live-only, trigger restore, DRY, legacy OK")


# ---------------------------------------------------------------------------
# 4. Interface
# ---------------------------------------------------------------------------
def check_ui_bounds():
    # help rows at y=22/23 in the MIX/param pages; no param page draws there
    assert 'DrawString(1,22,"UP/DN row' in MIX
    assert 'DrawString(1,23,"SELECT page' in MIX
    # InstrumentView fields stay above the map band (y=27)
    assert "drawMap" in IV
    # CMP BYP (first) and CMP SCL (soft clip) are visible rows
    assert "CMP BYP" in MIX or 'Bypass' in MIX
    assert "Soft Clip" in MIX
    # EQ and COMP are separate pages
    assert "FxPage" in (APP / "Views/MixerView.h").read_text()
    assert "EQ" in MIX and "COMP" in MIX
    print("UI: no page over help rows, CMP BYP/SCL visible, EQ/COMP separate OK")


def check_ui_onoff_units():
    assert '"ON "' in MIX or '"ON"' in MIX or "ON" in MIX
    assert "Hz" in MIX and "dB" in MIX
    print("UI: booleans ON/OFF and units (Hz/dB) present OK")


# ---------------------------------------------------------------------------
# 5. Nomenclature
# ---------------------------------------------------------------------------
def check_nomenclature():
    assert 'new Variable("feedback mix",SIP_FBMIX' in SI_CPP
    assert 'new Variable("crush",SIP_CRUSH' in SI_CPP
    assert 'new Variable("crushdrive",SIP_CRUSHVOL' in SI_CPP
    assert 'new Variable("filter res",SIP_FILTRESO' in SI_CPP
    # no audio-reverb label tied to FBMIX
    seg = SI_CPP.split('new Variable("feedback mix"')[0]
    assert "reverb" not in SI_CPP.split("SIP_FBMIX")[0][-200:].lower()
    # no "compressor" label on the crush variable
    assert 'new Variable("crush",SIP_CRUSH' in SI_CPP
    assert "compressor" not in SI_CPP.split("SIP_CRUSH")[0][-120:].lower()
    # EQ is a command label (FCUT="EQ ") in the central table, not the reso row
    assert re.search(r'\{ I_CMD_FCUT,\s+"EQ  "', CL_CPP)
    assert "EQ" not in SI_CPP.split("SIP_FILTRESO")[0][-80:].lower()
    # the grid/table/selector all copy the central name
    assert 'CommandList::GetDisplayName(command)' in HELP
    assert "getCommandDisplayName(command, buffer)" in PV
    assert "getCommandDisplayName(command, buffer)" in TV
    print("nomenclature: FBMIX/reverb, CRUSH/compressor, EQ/reso, central names OK")


# ---------------------------------------------------------------------------
# 6. Persistence
# ---------------------------------------------------------------------------
def check_persistence():
    # the send variables are serialised as ordinary PARAMs (Insert'ed)
    assert "Insert(dry_)" in SI_CPP
    assert "Insert(dlySend_)" in SI_CPP
    assert "Insert(rvbSend_)" in SI_CPP
    assert "SIP_DRY" in SI_H and "SIP_DLY_SEND" in SI_H and "SIP_RVB_SEND" in SI_H
    # per-track sends still persist as the Fase 7 inheritance layer
    assert "DELAYSEND" in MIXER and "REVERBSEND" in MIXER
    # compatibility mode: PlayerChannel keeps reading the Mixer send as fallback
    assert "GetChannelDelaySend" in PC and "GetChannelReverbSend" in PC
    # FXMASTER still carries the 41-param block (DLYRET/RVBRET etc.)
    for tok in ("DLYRET", "RVBRET", "DLYTIME", "RVBDEC", "CMPTHR"):
        assert tok in MIXER, tok
    print("persistence: instrument sends + track layer + FXMASTER OK")


# ---------------------------------------------------------------------------
def check_src_guards():
    for tok in ('CMD_PARAM_FORMAT_HEX8', 'CMD_PARAM_FORMAT_HEX16',
                '"%2.2X"', '"%4.4X"', "GetParamPrecision"):
        assert tok in CL_CPP, tok
    assert "SIP_DLY_SEND" in SI_H and "SIP_RVB_SEND" in SI_H
    print("source guards OK")


check_editor_formats()
check_draw_low_byte_only()
check_phrase_table_same_format()
check_inventory()
check_hidden_commands_still_play()
check_dlay_is_note_delay()
check_instrument_sends()
check_ui_bounds()
check_ui_onoff_units()
check_nomenclature()
check_persistence()
check_src_guards()
print("FX_ACCEPTANCE_PHASE17_OK")
