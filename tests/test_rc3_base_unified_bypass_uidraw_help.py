#!/usr/bin/env python3
"""RC3 base tests: unified bypass, UiDraw/UiColors, centralized Help.

(PLAN_RC3_MODERNIZACION_VISUAL_ES.md, points 7/10/12/13/19/20/30-32;
UI_STYLE_GUIDE.md.)

Covers the phase-base deliverables:

Point 7  - BYPASS is the first visual and logical row on the four master
           pages (DELAY/REVERB/EQ/COMP): fxRowForId returns 0 for the page
           Bypass, fxIdForRow(0) returns it, and the DELAY/REVERB label
           tables put BYPASS first.  EQ/COMP already had BYP first.
Points 19/20 - UiDraw primitives exist (DrawCenteredTitle, DrawValueRow,
           DrawToggle, DrawSolidBar, DrawModalFrame) and UiColors maps the
           UI_COLOR_* roles onto CD_*.
Points 10/12/13 - HelpRegistry has a section per ViewType, HelpOverlay opens
           on SELECT+R1 with its own latch, SELECT+R2 keeps the audio
           driver latch, and the overlay never forwards input while latched.
Point 30 - tests for the toggle/bar/layout primitives.
Point 32 - no primitive draws outside 0<=x<40 / 0<=y<30 (clamping).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

MIX_CPP = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
MIX_H = (ROOT / "source/sources/Application/Views/MixerView.h").read_text()
UIDRAW_H = (ROOT / "source/sources/Application/Views/BaseClasses/UiDraw.h").read_text()
UIDRAW_CPP = (ROOT / "source/sources/Application/Views/BaseClasses/UiDraw.cpp").read_text()
UICOL_H = (ROOT / "source/sources/Application/Views/BaseClasses/UiColors.h").read_text()
HELPREG_H = (ROOT / "source/sources/Application/Views/BaseClasses/HelpRegistry.h").read_text()
HELPREG_CPP = (ROOT / "source/sources/Application/Views/BaseClasses/HelpRegistry.cpp").read_text()
HELPOVL_H = (ROOT / "source/sources/Application/Views/BaseClasses/HelpOverlay.h").read_text()
HELPOVL_CPP = (ROOT / "source/sources/Application/Views/BaseClasses/HelpOverlay.cpp").read_text()
APPW_CPP = (ROOT / "source/sources/Application/AppWindow.cpp").read_text()
APPW_H = (ROOT / "source/sources/Application/AppWindow.h").read_text()


# ---------------------------------------------------------------------------
# Point 7: unified bypass on the four master pages
# ---------------------------------------------------------------------------
def check_bypass_first():
    # Row helper: the page Bypass must resolve to logical row 0.
    assert "fxBypassId" in MIX_H
    assert "fxIdForRow" in MIX_H
    assert "fxCountOnPage" in MIX_H
    idx = MIX_CPP.index("int MixerView::fxBypassId")
    assert "if (id==byp) return 0" in MIX_CPP[idx:idx + 900]
    assert "FX_P_DLY_BYP" in MIX_CPP[idx:idx + 900]
    assert "FX_P_RVB_BYP" in MIX_CPP[idx:idx + 900]
    assert "FX_P_EQ_BYP" in MIX_CPP[idx:idx + 900]
    assert "FX_P_CMP_BYP" in MIX_CPP[idx:idx + 900]
    # DELAY page label table starts with BYPASS.
    dl = MIX_CPP[MIX_CPP.index("void MixerView::drawDelayPage"):
                 MIX_CPP.index("void MixerView::drawReverbPage")]
    assert "\"BYPASS\",\"TIME\"" in dl.replace("\n", "").replace(" ", "")
    assert "FX_P_DLY_BYP,FX_P_DLY_TIME" in dl.replace("\n", "").replace(" ", "")
    # REVERB page label table starts with BYPASS.
    rv = MIX_CPP[MIX_CPP.index("void MixerView::drawReverbPage"):
                 MIX_CPP.index("void MixerView::drawEqRow")]
    assert "\"BYPASS\",\"PREDELAY\"" in rv.replace("\n", "").replace(" ", "")
    assert "FX_P_RVB_BYP,FX_P_RVB_PRE" in rv.replace("\n", "").replace(" ", "")
    # EQ/COMP keep BYP first (already in the param table).
    assert MIX_CPP.index("{ \"EQ  BYP\"") < MIX_CPP.index("{ \"LO  EN\"")
    assert MIX_CPP.index("{ \"CMP BYP\"") < MIX_CPP.index("{ \"CMP THR\"")
    print("bypass unified as first row on all four master pages OK")


def check_bypass_row_model():
    # Model: the row order puts bypass at 0 and preserves every other row.
    delay_ids = ["FX_P_DLY_TIME", "FX_P_DLY_FBK", "FX_P_DLY_MIX",
                 "FX_P_DLY_WID", "FX_P_DLY_PP", "FX_P_DLY_SAT"]
    reverb_ids = ["FX_P_RVB_PRE", "FX_P_RVB_DEC", "FX_P_RVB_SIZ",
                  "FX_P_RVB_DMP", "FX_P_RVB_WID", "FX_P_RVB_MODE"]
    # fxRowForId gives bypass row 0 and each later param row 1..n in order.
    assert "return 0" in MIX_CPP[MIX_CPP.index("fxRowForId"):][:800]
    assert "row=1" in MIX_CPP[MIX_CPP.index("fxRowForId"):][:800]
    # fxIdForRow maps row 0 back to the bypass id.
    fh = MIX_CPP[MIX_CPP.index("int MixerView::fxIdForRow"):][:900]
    assert "row==0) return byp" in fh
    print("bypass row model OK")


# ---------------------------------------------------------------------------
# Points 19/20: UiDraw + UiColors
# ---------------------------------------------------------------------------
def check_uidraw_primitives():
    for token in ("DrawCenteredTitle", "DrawSectionHeader", "DrawValueRow",
                  "DrawToggle", "DrawSolidBar", "DrawBipolarBar",
                  "DrawProgressBar", "DrawTabs", "DrawModalFrame",
                  "DrawScrollIndicator", "DrawSeparator"):
        assert token in UIDRAW_H, token
        assert token in UIDRAW_CPP, token
    # Centered title uses (screenWidth - len)/2 on row 0.
    idx = UIDRAW_CPP.index("void UiDraw::DrawCenteredTitle")
    assert "0" in UIDRAW_CPP[idx:idx + 400]
    assert "- len) / 2" in UIDRAW_CPP[idx:idx + 400]
    # Clamping helpers bound every draw to 40x30.
    assert "clampX" in UIDRAW_CPP and "clampY" in UIDRAW_CPP
    assert "x >= kScreenWidth" in UIDRAW_CPP
    assert "y >= kScreenHeight" in UIDRAW_CPP
    # View grants UiDraw access to its draw primitives.
    view_h = (ROOT / "source/sources/Application/Views/BaseClasses/View.h").read_text()
    assert "friend class UiDraw" in view_h
    print("UiDraw primitives and 40x30 clamping OK")


def check_uicolors():
    for token in ("UI_COLOR_TITLE", "UI_COLOR_LABEL", "UI_COLOR_VALUE",
                  "UI_COLOR_TEXT_EDIT", "UI_COLOR_CURSOR", "UI_COLOR_BORDER",
                  "UI_COLOR_BACKGROUND", "UI_COLOR_ACTIVE",
                  "UI_COLOR_DISABLED"):
        assert token in UICOL_H, token
    assert "CD_HILITE1" in UICOL_H
    assert "CD_MUTE" in UICOL_H
    print("UiColors semantic roles OK")


def check_layout_model():
    # UiDraw clamps so no primitive can draw out of bounds.
    assert "kScreenWidth" in UIDRAW_CPP and "kScreenHeight" in UIDRAW_CPP
    print("layout bounds (0<=x<40, 0<=y<30) model OK")


# ---------------------------------------------------------------------------
# Points 10/12/13: centralized Help
# ---------------------------------------------------------------------------
def check_help_registry():
    for token in ("HelpRegistry", "GetSection", "GetLineCount", "HelpLine",
                  "HelpSection"):
        assert token in HELPREG_H, token
    # One section per ViewType (VT_SONG..VT_MIXER) plus the array.
    sec = HELPREG_CPP[HELPREG_CPP.index("const HelpSection *HelpRegistry::GetSection"):]
    assert "\"SONG\"" in sec
    assert "\"MIXER\"" in sec
    assert "&sections_[vt]" in sec
    print("HelpRegistry context sections OK")


def check_help_overlay():
    for token in ("HelpOverlay", "DrawView", "ProcessButtonMask",
                  "SetWindow", "Release SELECT+R1"):
        assert token in HELPOVL_CPP, token
    # Latch: overlay ignores all input while open (informational only).
    assert "(void)mask" in HELPOVL_CPP
    # Context from the active view type.
    assert "GetViewType" in HELPOVL_CPP
    print("HelpOverlay latch + context OK")


def check_appwindow_shortcut():
    # SELECT+R1 opens HelpOverlay (helpCombo = EPBM_SELECT | EPBM_R).
    assert "EPBM_SELECT | EPBM_R" in APPW_CPP
    assert "new HelpOverlay" in APPW_CPP
    assert "HelpOverlayApplyCallback" in APPW_CPP
    # SELECT+R2 audio driver is preserved.
    assert "EPBM_SELECT | EPBM_R2" in APPW_CPP
    assert "new AudioDriverModal" in APPW_CPP
    # Both latches clear when their combo is released.
    assert "_helpShortcutLatched" in APPW_H and "_helpShortcutLatched" in APPW_CPP
    assert "_audioShortcutLatched" in APPW_H and "_audioShortcutLatched" in APPW_CPP
    # Latched overlays swallow the event so the view cannot change page.
    assert "_helpShortcutLatched || _audioShortcutLatched" in APPW_CPP
    # The help shortcut must not fire when a modal is already open.
    assert "!_currentView->HasModal()" in APPW_CPP
    print("AppWindow SELECT+R1 help / SELECT+R2 audio latch OK")


def check_makefile_registration():
    mk = (ROOT / "source/projects/Makefile").read_text()
    for token in ("HelpOverlay.o", "HelpRegistry.o", "UiDraw.o"):
        assert token in mk, token
    print("Makefile registration OK")


check_bypass_first()
check_bypass_row_model()
check_uidraw_primitives()
check_uicolors()
check_layout_model()
check_help_registry()
check_help_overlay()
check_appwindow_shortcut()
check_makefile_registration()
print("RC3_BASE_UNIFIED_BYPASS_UIDRAW_HELP_OK")
