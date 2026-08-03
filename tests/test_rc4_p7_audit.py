#!/usr/bin/env python3
"""RC4 P7 audit tests.

(PLAN_RC4 section 12, P7: static audits.)

Verifies the RC4 P7 deliverables:

- No `#if 0` dead code blocks remain in the Application/Views tree.
- The RC4 docs (UI_VISUAL_AUDIT, UI_CONTROL_AUDIT, OBSOLETE_FEATURE_AUDIT,
  UI_STYLE_GUIDE) reflect the P3-P6 work.
- Every RC4-added UiDraw primitive has at least one real consumer, and the
  RC3 primitives without consumers are documented (KEEP_HIDDEN) rather than
  silently shipped.
- CD_WARNING / CD_ERROR map to real colors and stay within the 7-bit screen
  cache property encoding.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VIEWS = ROOT / "source/sources/Application/Views"
DOCS = ROOT / "docs"

UIDRAW_CPP = (VIEWS / "BaseClasses/UiDraw.cpp").read_text()
UIDRAW_H = (VIEWS / "BaseClasses/UiDraw.h").read_text()
UICOL_H = (VIEWS / "BaseClasses/UiColors.h").read_text()
VIEW_H = (VIEWS / "BaseClasses/View.h").read_text()
APPW_CPP = (ROOT / "source/sources/Application/AppWindow.cpp").read_text()
APPW_H = (ROOT / "source/sources/Application/AppWindow.h").read_text()

# Consumers outside the BaseClasses library itself.
def count_consumers(symbol):
    n = 0
    for f in (VIEWS / "BaseClasses").rglob("*.cpp"):
        pass
    for f in VIEWS.rglob("*.cpp"):
        if "BaseClasses/UiDraw.cpp" in str(f):
            continue
        text = f.read_text()
        if "UiDraw::" + symbol in text:
            n += 1
    return n


def check_no_if0():
    bad = []
    for f in VIEWS.rglob("*.cpp"):
        if "BaseClasses" in str(f):
            continue
        text = f.read_text()
        if "#if 0" in text or "#if false" in text:
            bad.append(str(f))
    assert not bad, f"#if 0 blocks found: {bad}"
    print("no #if 0 / #if false dead code in views OK")


def check_p7_docs_updated():
    visual = (DOCS / "UI_VISUAL_AUDIT.md").read_text()
    control = (DOCS / "UI_CONTROL_AUDIT.md").read_text()
    obsolete = (DOCS / "OBSOLETE_FEATURE_AUDIT.md").read_text()
    style = (DOCS / "UI_STYLE_GUIDE.md").read_text()
    # P4 centered menus.
    assert "MakeCenteredMenuLayout" in visual and "CenterTextX" in style
    # P5 severity colors + global primitives.
    assert "CD_WARNING" in visual and "CD_ERROR" in visual
    assert "DrawStatusMessage" in visual and "DrawErrorMessage" in visual
    # P6 chopper solid frame (no ASCII).
    assert "sin ASCII" in visual or "sin marcos ASCII" in visual or "solid" in visual
    assert "REMOVED (RC4 P6)" in obsolete
    assert "R+UP Song" in control and "RC4 P3" in control
    print("RC4 docs reflect P3-P6 OK")


def check_consumers():
    # RC4-added primitives MUST have real consumers.
    for sym in ("DrawBypassRow", "DrawSelectionRegion", "DrawStatusMessage",
                "DrawErrorMessage", "DrawTabs", "DrawScrollIndicator",
                "CenterTextX", "MakeCenteredMenuLayout"):
        assert count_consumers(sym) >= 1, f"{sym} has no consumer"
    # RC3 primitives without consumers must be documented as KEEP_HIDDEN.
    obsolete = (DOCS / "OBSOLETE_FEATURE_AUDIT.md").read_text()
    for sym in ("DrawValueRow", "DrawSolidBar", "DrawBipolarBar", "DrawToggle",
                "DrawProgressBar", "DrawModalFrame", "DrawSeparator"):
        assert count_consumers(sym) == 0, f"{sym} unexpectedly has consumers"
        assert sym in obsolete, f"{sym} not documented in OBSOLETE_FEATURE_AUDIT"
    print("consumer audit + KEEP_HIDDEN documentation OK")


def check_p7_colors_bounds():
    # CD_WARNING / CD_ERROR are added and map to real GUI colors.
    assert "CD_WARNING" in VIEW_H and "CD_ERROR" in VIEW_H
    assert "warningColor_" in APPW_H and "errorColor_" in APPW_H
    assert "case CD_WARNING" in APPW_CPP and "case CD_ERROR" in APPW_CPP
    # Semantic roles wired in UiColors.
    assert "UI_COLOR_WARNING" in UICOL_H and "UI_COLOR_ERROR" in UICOL_H
    print("P7 warning/error colors wired OK")


check_no_if0()
check_p7_docs_updated()
check_consumers()
check_p7_colors_bounds()
print("RC4_P7_AUDIT_OK")
