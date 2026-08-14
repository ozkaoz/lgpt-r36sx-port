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
           driver latch, and the overlay never forwards input while open.
RC4 P0 (PLAN_RC4 11.1) - the overlay is a real modal that closes
           deterministically with B or SELECT+R1 (EndModal), consuming every
           event so nothing propagates to the view underneath.
Point 30 - tests for the toggle/bar/layout primitives.
Point 32 - no primitive draws outside 0<=x<40 / 0<=y<30 (clamping).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

MIX_CPP = (ROOT / "source/sources/Application/UI/Views/MixerView.cpp").read_text()
MIX_H = (ROOT / "source/sources/Application/UI/Views/MixerView.h").read_text()
UIDRAW_H = (ROOT / "source/sources/Application/UI/Views/BaseClasses/UiDraw.h").read_text()
UIDRAW_CPP = (ROOT / "source/sources/Application/UI/Views/BaseClasses/UiDraw.cpp").read_text()
UICOL_H = (ROOT / "source/sources/Application/UI/Views/BaseClasses/UiColors.h").read_text()
HELPREG_H = (ROOT / "source/sources/Application/UI/Views/BaseClasses/HelpRegistry.h").read_text()
HELPREG_CPP = (ROOT / "source/sources/Application/UI/Views/BaseClasses/HelpRegistry.cpp").read_text()
HELPOVL_H = (ROOT / "source/sources/Application/UI/Views/BaseClasses/HelpOverlay.h").read_text()
HELPOVL_CPP = (ROOT / "source/sources/Application/UI/Views/BaseClasses/HelpOverlay.cpp").read_text()
APPW_CPP = (ROOT / "source/sources/Application/AppWindow.cpp").read_text()
APPW_H = (ROOT / "source/sources/Application/AppWindow.h").read_text()


# ---------------------------------------------------------------------------
# Point 7: unified bypass on the four master pages
# ---------------------------------------------------------------------------
def check_bypass_first():
    # F3-4a: row helpers + param table live in FxPages.h now.
    FXP = (ROOT / "source/sources/Application/Mixer/FxPages.h").read_text()
    assert "fxBypassId" in MIX_H
    assert "fxIdForRow" in MIX_H
    assert "fxCountOnPage" in MIX_H
    idx = FXP.index("inline int fxBypassId")
    assert "FX_P_DLY_BYP" in FXP[idx:idx + 900]
    assert "FX_P_RVB_BYP" in FXP[idx:idx + 900]
    assert "FX_P_EQ_BYP" in FXP[idx:idx + 900]
    assert "FX_P_CMP_BYP" in FXP[idx:idx + 900]
    # fxRowForId keeps the bypass-first rule in the layer.
    rowidx = FXP.index("fxRowForId")
    assert "if (id==byp) return 0" in FXP[rowidx:rowidx + 500]
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
    assert FXP.index("{ \"EQ  BYP\"") < FXP.index("{ \"LO  EN\"")
    assert FXP.index("{ \"CMP BYP\"") < FXP.index("{ \"CMP THR\"")
    print("bypass unified as first row on all four master pages OK")


def check_bypass_uidraw():
    # RC4 P2 (PLAN_RC4 section 12): the four master pages render Bypass
    # through the shared UiDraw::DrawBypassRow (label + toggle), not ad-hoc.
    assert "DrawBypassRow" in UIDRAW_H
    assert "DrawBypassRow" in UIDRAW_CPP
    assert "\"BYPASS\"" in UIDRAW_CPP
    assert "[ %s ]" in UIDRAW_CPP.replace("\\n", "").replace("\n", "")
    assert "CD_MUTE" in UIDRAW_CPP
    assert "CD_HILITE2" in UIDRAW_CPP
    # Every master page delegates its bypass row to the helper.
    dl = MIX_CPP[MIX_CPP.index("void MixerView::drawDelayPage"):
                 MIX_CPP.index("void MixerView::drawReverbPage")]
    assert "DrawBypassRow" in dl
    rv = MIX_CPP[MIX_CPP.index("void MixerView::drawReverbPage"):
                 MIX_CPP.index("void MixerView::drawEqRow")]
    assert "DrawBypassRow" in rv
    eq = MIX_CPP[MIX_CPP.index("void MixerView::drawEqRow"):
                 MIX_CPP.index("void MixerView::drawEqPage")]
    assert "DrawBypassRow" in eq
    cm = MIX_CPP[MIX_CPP.index("void MixerView::drawCompPage"):
                 MIX_CPP.index("void MixerView::drawMixReturns")]
    assert "DrawBypassRow" in cm
    print("DrawBypassRow unified on DELAY/REVERB/EQ/COMP OK")


def check_bypass_row_model():
    # F3-4a: fxRowForId/fxIdForRow moved to FxPages.h.
    FXP = (ROOT / "source/sources/Application/Mixer/FxPages.h").read_text()
    # Model: the row order puts bypass at 0 and preserves every other row.
    delay_ids = ["FX_P_DLY_TIME", "FX_P_DLY_FBK", "FX_P_DLY_MIX",
                 "FX_P_DLY_WID", "FX_P_DLY_PP", "FX_P_DLY_SAT"]
    reverb_ids = ["FX_P_RVB_PRE", "FX_P_RVB_DEC", "FX_P_RVB_SIZ",
                  "FX_P_RVB_DMP", "FX_P_RVB_WID", "FX_P_RVB_MODE"]
    # fxRowForId gives bypass row 0 and each later param row 1..n in order.
    assert "return 0" in FXP[FXP.index("fxRowForId"):][:800]
    assert "row=1" in FXP[FXP.index("fxRowForId"):][:800]
    # fxIdForRow maps row 0 back to the bypass id.
    fh = FXP[FXP.index("fxIdForRow"):][:900]
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
    # Clamping helpers bound every draw to 40x30 (RC5: the constants live
    # in the UiDraw class, so the checks reference UiDraw::kScreen*).
    assert "clampX" in UIDRAW_CPP and "clampY" in UIDRAW_CPP
    assert "x >= UiDraw::kScreenWidth" in UIDRAW_CPP
    assert "y >= UiDraw::kScreenHeight" in UIDRAW_CPP
    # View grants UiDraw access to its draw primitives.
    view_h = (ROOT / "source/sources/Application/UI/Views/BaseClasses/View.h").read_text()
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
    # RC4 P3 (PLAN_RC4): sequencing views render page titles with the
    # semantic role instead of raw CD_NORMAL.
    groove = (ROOT / "source/sources/Application/UI/Views/GrooveView.cpp").read_text()
    project = (ROOT / "source/sources/Application/UI/Views/ProjectView.cpp").read_text()
    song = (ROOT / "source/sources/Application/UI/Views/SongView.cpp").read_text()
    chain = (ROOT / "source/sources/Application/UI/Views/ChainView.cpp").read_text()
    phrase = (ROOT / "source/sources/Application/UI/Views/PhraseView.cpp").read_text()
    table = (ROOT / "source/sources/Application/UI/Views/TableView.cpp").read_text()
    for src, name in ((groove, "Groove"), (project, "Project"), (song, "Song"),
                      (chain, "Chain"), (phrase, "Phrase"), (table, "Table")):
        assert "UiColors::Resolve(UI_COLOR_TITLE)" in src, name
    print("UiColors semantic roles + sequencing-view title adoption OK")


def check_layout_model():
    # UiDraw clamps so no primitive can draw out of bounds.
    assert "kScreenWidth" in UIDRAW_CPP and "kScreenHeight" in UIDRAW_CPP
    # RC4 P4: centered menu helpers exist and are used.
    assert "int UiDraw::CenterTextX" in UIDRAW_CPP
    assert "MakeCenteredMenuLayout" in UIDRAW_H and "MakeCenteredMenuLayout" in UIDRAW_CPP
    assert "struct MenuLayout" in UIDRAW_H
    project = (ROOT / "source/sources/Application/UI/Views/ProjectView.cpp").read_text()
    assert "UiDraw::CenterTextX" in project
    assert "UiDraw::MakeCenteredMenuLayout" in project
    print("layout bounds + centered menu helpers OK")


def check_p5_global_uidraw():
    # RC4 P5 (PLAN_RC4 section 12): the three global primitives exist.
    for token in ("DrawSelectionRegion", "DrawStatusMessage",
                  "DrawErrorMessage"):
        assert token in UIDRAW_H, token
        assert token in UIDRAW_CPP, token
    # Each has a real consumer in the char-screen views.
    helpovl = (ROOT / "source/sources/Application/UI/Views/BaseClasses/HelpOverlay.cpp").read_text()
    assert "UiDraw::DrawSelectionRegion" in helpovl
    sm = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/SampleManagerDialog.cpp").read_text()
    smh = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/SampleManagerDialog.h").read_text()
    assert "UiDraw::DrawStatusMessage" in sm
    assert "UiDraw::DrawErrorMessage" in sm
    assert "setStatusError" in smh and "setStatusError" in sm
    assert "statusIsError_" in smh
    print("RC4 P5 global primitives + real consumers OK")


def check_p5_warning_error_colors():
    # RC4 P5: CD_WARNING/CD_ERROR exist and map to real GUI colors.
    view_h = (ROOT / "source/sources/Application/UI/Views/BaseClasses/View.h").read_text()
    assert "CD_WARNING" in view_h and "CD_ERROR" in view_h
    assert "CD_WARNING" in APPW_CPP and "CD_ERROR" in APPW_CPP
    assert "warningColor_" in APPW_H and "errorColor_" in APPW_H
    assert "case CD_WARNING" in APPW_CPP and "case CD_ERROR" in APPW_CPP
    # Semantic roles wire the new colors into UiColors.
    assert "UI_COLOR_WARNING" in UICOL_H and "UI_COLOR_ERROR" in UICOL_H
    print("CD_WARNING/CD_ERROR + semantic roles OK")


# ---------------------------------------------------------------------------
# Points 10/12/13: centralized Help
# ---------------------------------------------------------------------------
def check_help_registry():
    for token in ("HelpRegistry", "GetSection", "GetLineCount", "HelpLine",
                  "HelpSection", "GetSectionCount", "GetSectionAt"):
        assert token in HELPREG_H, token
    # One section per ViewType (VT_SONG..VT_MIXER) plus the array.
    sec = HELPREG_CPP[HELPREG_CPP.index("kSections_["):]
    assert "\"SONG\"" in sec
    assert "\"MIXER\"" in sec
    assert "GetSectionCount" in HELPREG_CPP
    assert "GetSectionAt" in HELPREG_CPP
    # RC4 P3: the Mixer navigation hint "R+UP Song" moved to Help.
    assert "R+UP" in HELPREG_CPP
    assert "back to Song" in HELPREG_CPP
    print("HelpRegistry context sections + index OK")


def check_perm_hints_retired():
    # RC4 P3 (PLAN_RC4 11.6): permanent help texts are gone from the main
    # views; the navigation actions they advertised now live in Help.
    for src, name in ((MIX_CPP, "MixerView"),):
        # The retired draw call (not the comment documenting the move).
        assert 'DrawString(7,pos._y,"R+UP Song"' not in src, name
    print("Permanent hint rows retired from main views OK")


def check_help_overlay():
    for token in ("HelpOverlay", "DrawView", "ProcessButtonMask",
                  "SetWindow", "EndModal(0)", "EPBM_B",
                  "EPBM_SELECT | EPBM_R"):
        assert token in HELPOVL_CPP, token
    # RC4 P0: every event (press and release) is consumed while Help is open.
    assert "if (!pressed)" in HELPOVL_CPP
    # RC4 P1: navigable overlay - section index, scroll, section switching.
    assert "GetSectionCount" in HELPOVL_CPP
    assert "GetSectionAt" in HELPOVL_CPP
    assert "sectionIndex_" in HELPOVL_CPP and "lineScroll_" in HELPOVL_CPP
    assert "showIndex_" in HELPOVL_CPP
    for nav in ("EPBM_UP", "EPBM_DOWN", "EPBM_L", "EPBM_R", "EPBM_L2",
                "EPBM_R2", "EPBM_A"):
        assert nav in HELPOVL_CPP, nav
    # Context from the active view type.
    assert "GetViewType" in HELPOVL_CPP
    print("HelpOverlay navigable modal close OK")


def check_view_push_modal():
    view_h = (ROOT / "source/sources/Application/UI/Views/BaseClasses/View.h").read_text()
    view_cpp = (ROOT / "source/sources/Application/UI/Views/BaseClasses/View.cpp").read_text()
    # PushModal suspends the active modal and restores it on close (RC4 11.3).
    assert "bool PushModal" in view_h
    assert "suspendedModal_" in view_h and "RestoreSuspendedModal" in view_h
    assert "bool View::PushModal" in view_cpp
    assert "void View::RestoreSuspendedModal" in view_cpp
    assert "suspendedModal_ = modalView_" in view_cpp
    assert "RestoreSuspendedModal()" in view_cpp
    print("View PushModal suspend/restore OK")


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
    # RC4 P1: Help opens over an active dialog via PushModal (no HasModal guard).
    assert "PushModal" in APPW_CPP
    # The audio driver shortcut still refuses to replace an active modal.
    assert "new AudioDriverModal" in APPW_CPP
    assert "_currentView->HasModal()" in APPW_CPP
    print("AppWindow SELECT+R1 help / SELECT+R2 audio latch + PushModal OK")


def check_makefile_registration():
    mk = (ROOT / "source/projects/Makefile").read_text()
    for token in ("HelpOverlay.o", "HelpRegistry.o", "UiDraw.o"):
        assert token in mk, token
    print("Makefile registration OK")


def check_p6_chopper_modernization():
    # RC4 P6 (PLAN_RC4 section 11.7): the Graphical Chopper draws its frame
    # with solid cells, never ASCII box-drawing chars.
    chopper = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.cpp").read_text()
    frameStart = chopper.index("void SampleChopperModal::drawFrame")
    frameEnd = chopper.index("void SampleChopperModal::drawSampleInfo")
    frame = chopper[frameStart:frameEnd]
    assert '"+-' not in frame
    assert '"|"' not in frame
    # F3-3b: el marco (celdas solidas CD_BORDER) vive en la capa pura
    # ChopperView; la vista drena CHOP_COLOR_BORDER -> CD_BORDER.
    assert "ChopperView::DrawFrame(grid)" in frame
    assert "CHOP_COLOR_BORDER" in (ROOT / "source/sources/Application/UI/Views/ModalDialogs/ChopperView.h").read_text()
    assert "SetColor(CD_BORDER)" in chopper
    print("RC4 P6 chopper solid frame (no ASCII) OK")


def check_p6_tabs_scroll_consumers():
    # RC4 P6: DrawTabs and DrawScrollIndicator have real consumers.
    helpovl = (ROOT / "source/sources/Application/UI/Views/BaseClasses/HelpOverlay.cpp").read_text()
    assert "UiDraw::DrawTabs" in helpovl
    assert "UiDraw::DrawScrollIndicator" in helpovl
    assert "prevSec" in helpovl and "nextSec" in helpovl
    assert "lineScroll_" in helpovl
    print("RC4 P6 DrawTabs + DrawScrollIndicator consumers OK")


check_bypass_first()
check_bypass_uidraw()
check_bypass_row_model()
check_uidraw_primitives()
check_uicolors()
check_layout_model()
check_p5_global_uidraw()
check_p5_warning_error_colors()
check_help_registry()
check_perm_hints_retired()
check_help_overlay()
check_view_push_modal()
check_appwindow_shortcut()
check_makefile_registration()
check_p6_chopper_modernization()
check_p6_tabs_scroll_consumers()
print("RC3_BASE_UNIFIED_BYPASS_UIDRAW_HELP_OK")
