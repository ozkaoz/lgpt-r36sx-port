#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RC3 full-phase static tests (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, section C).

Validates the modernizations shipped in the "fase completa":
  * MixerView MASTER pages: centered UiDraw title, DrawToggle bypass,
    permanent hint rows (y=22/23) and MIX page legend rows migrated to Help.
  * InstrumentView: block headers via UiDraw::DrawSectionHeader, USB-REC
    hint removed from row 0 and migrated to HelpRegistry.
  * HelpRegistry: Mixer/Instrument sections carry the migrated controls.
  * No undocumented ASCII widgets remain in the view layer (only the
    documented Chopper frame and `--` value placeholders).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "source/sources/Application"
MV = (SRC / "Views/MixerView.cpp").read_text()
IV = (SRC / "Views/InstrumentView.cpp").read_text()
HR = (SRC / "Views/BaseClasses/HelpRegistry.cpp").read_text()
CHO = (SRC / "Views/ModalDialogs/SampleChopperModal.cpp").read_text()


def check_mixerview_master():
    # Centered page title on the FX pages (DrawCenteredTitleAt at row 1).
    assert "UiDraw::DrawCenteredTitleAt(*this,1,pageTitle)" in MV
    # Old left title and permanent hint rows are gone.
    assert "DrawString(1,1,pageTitle" not in MV
    assert "UP/DN row" not in MV
    assert "SELECT page" not in MV
    # RC4 P2: Bypass on DELAY/REVERB renders through the unified bypass row
    # helper (DrawBypassRow), replacing the old x+6 toggle.
    assert "UiDraw::DrawBypassRow(*this,x,2+p," in MV
    assert "DrawToggle(*this,x+6,2+p," not in MV
    # MIX page legend rows migrated to Help.
    assert "A+UP/DN x10" not in MV
    assert "L/R ch  L->MST" not in MV
    assert "R1+A solo" not in MV
    assert "R2 edit VOL/RET" not in MV
    print("MixerView MASTER: centered title, DrawToggle bypass, legends migrated OK")


def check_instrument():
    # Section headers drawn through UiDraw, not raw DrawString.
    assert "UiDraw::DrawSectionHeader(*this, hp._x, hp._y, \"INSTRUMENT\")" in IV
    assert "UiDraw::DrawSectionHeader(*this, hp._x, hp._y + 4, \"FILTER\")" in IV
    assert "R1+RIGHT USB REC" not in IV
    print("InstrumentView: UiDraw section headers, USB-REC hint migrated OK")


def check_help_registry():
    # Mixer section documents the master-page row/edit controls.
    assert '{"UP/DN", "row"},' in HR
    assert '{"L/R", "edit"},' in HR
    assert '{"A", "coarse"},' in HR
    assert '{"START", "play"},' in HR
    # Instrument section documents the USB record shortcut.
    assert '{"R1+RIGHT", "USB record"},' in HR
    print("HelpRegistry: Mixer master controls + Instrument USB record OK")


def check_ascii_widget_allowlist():
    # The only ASCII boxes left are the documented Chopper boxes (frame,
    # inverted operation overlay, inverted Pitch/Env panel) and `--`/`----`
    # value placeholders.  3 boxes x 2 border lines = 6 `+----+` lines.
    frames = CHO.count("+------")
    assert frames == 6, frames
    assert "----------- no sample loaded -----------" in CHO  # placeholder text
    for view in (MV, IV):
        assert "=====" not in view and "+-----" not in view
        assert not __import__("re").search(r'"[^"]*\*{3}[^"]*"', view)
    print("ASCII widget allowlist: 3 Chopper boxes documented, no others OK")


check_mixerview_master()
check_instrument()
check_help_registry()
check_ascii_widget_allowlist()
print("RC3_FULL_VIEWS_MODERNIZATION_OK")
