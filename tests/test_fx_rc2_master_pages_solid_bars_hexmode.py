#!/usr/bin/env python3
"""RC2 tests: DELAY/REVERB MASTER pages + solid send bars + SetHexMode fix
(PLAN_FX_REDESIGN_ES.md, RC2 points 4/5/6; RELEASE_BACON_1.1_FX_DEV_ES.md).

Covers the three UI/DSP-C++ changes that have no dedicated phase test yet:

Point 4 - dedicated DELAY MASTER / REVERB MASTER pages:
  - drawDelayPage/drawReverbPage/drawMasterFxRow exist and are dispatched by
    drawFxParamPage (the generic row loop no longer renders those two pages)
  - each page is a 7-row two-column menu (label / value) with the RC2 label
    set (no RVB MIX row)
  - hierarchy colors: title in CD_HILITE1, row label CD_NORMAL, value
    CD_HILITE1, edited row inverted CD_HILITE2
  - value formatting: TIME in ms, DECAY in s, MODE ECO/NORMAL, toggles ON/OFF

Point 5 - solid effect-send bars in InstrumentView:
  - the UIIntVarField::Draw bar branch renders filled cells as inverted
    (solid) cells and empty cells as CD_HILITE1, and clears stale cells on INH

Point 6 - UIBigHexVarField::SetHexMode no longer forces position_=0:
  - the nibble cursor keeps its digit (clamped into the new range) instead of
    jumping to the least significant nibble on a command-mode switch
  - the whole stored value is clamped (or wrapped when wrap_ is set) into the
    new [min,max] range so the variable never stays out of range

Acceptance:
- source guards for the three changes
- SetHexMode model: cursor preserved+clamped, value clamped/wrapped correctly
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

MV_CPP = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
MV_H = (ROOT / "source/sources/Application/Views/MixerView.h").read_text()
UIF_CPP = (ROOT / "source/sources/Application/Views/BaseClasses/UIIntVarField.cpp").read_text()
UIF_H = (ROOT / "source/sources/Application/Views/BaseClasses/UIIntVarField.h").read_text()
BIGHEX_CPP = (ROOT / "source/sources/Application/Views/BaseClasses/UIBigHexVarField.cpp").read_text()


# ---------------------------------------------------------------------------
# Point 4: DELAY/REVERB MASTER dedicated pages
# ---------------------------------------------------------------------------
def check_master_pages_dispatched():
    # drawFxParamPage dispatches DELAY/REVERB to the dedicated page renderers.
    assert "drawDelayPage()" in MV_CPP and "drawReverbPage()" in MV_CPP
    assert "drawDelayPage()" in MV_H and "drawReverbPage()" in MV_H
    assert "drawMasterFxRow" in MV_H
    idx = MV_CPP.index("void MixerView::drawFxParamPage")
    assert "drawDelayPage()" in MV_CPP[idx:idx + 1200]
    assert "drawReverbPage()" in MV_CPP[idx:idx + 1600]
    # Titles carry the SELECT page position [n/5].
    assert '"DELAY MASTER [%d/5]"' in MV_CPP
    assert '"REVERB MASTER [%d/5]"' in MV_CPP
    print("DELAY/REVERB MASTER pages dispatched by drawFxParamPage OK")


def check_master_hierarchy_colors():
    # RC3: the FX page title is drawn centered via UiDraw::DrawCenteredTitleAt,
    # which internally applies CD_HILITE1 (verified in UiDraw.cpp).  Rows keep
    # the hierarchy: label CD_NORMAL, value CD_HILITE1, edited row CD_HILITE2.
    fn = MV_CPP.index("void MixerView::drawFxParamPage")
    fn_end = MV_CPP.index("void MixerView::drawMasterFxRow")
    fn_seg = MV_CPP[fn:fn_end]
    # Centered title call in drawFxParamPage using the RC3 UiDraw API.
    title_pos = fn_seg.index("UiDraw::DrawCenteredTitleAt(*this,1,pageTitle)")
    assert "sprintf(pageTitle" in fn_seg[:title_pos]
    # The dedicated pages no longer draw the old (1,1) title or hint rows.
    assert "DrawString(1,1,pageTitle" not in fn_seg
    assert "UP/DN row" not in fn_seg
    assert "SELECT page" not in fn_seg
    # drawMasterFxRow: label CD_NORMAL, value CD_HILITE1, edited row inverted.
    row = MV_CPP.index("void MixerView::drawMasterFxRow")
    seg = MV_CPP[row:row + 700]
    assert "SetColor(CD_NORMAL)" in seg           # label
    assert "selected?CD_HILITE2:CD_HILITE1" in seg  # value + edited row
    assert "props.invert_=selected" in seg
    print("hierarchy colors (centered UiDraw title, NORMAL label, HILITE1 value, "
          "HILITE2 invert) OK")


def check_master_page_rows():
    # DELAY: TIME/FEEDBACK/MIX/WIDTH/PING/PONG/SATURATE/BYPASS (7 rows).
    # REVERB: PREDELAY/DECAY/SIZE/DAMPING/WIDTH/MODE/BYPASS (7 rows, no MIX).
    dl = MV_CPP[MV_CPP.index("void MixerView::drawDelayPage"):
               MV_CPP.index("void MixerView::drawReverbPage")]
    rv = MV_CPP[MV_CPP.index("void MixerView::drawReverbPage"):
               MV_CPP.index("void MixerView::drawEqPage")]
    dlabels = ['"TIME"', '"FEEDBACK"', '"MIX"', '"WIDTH"',
               '"PING/PONG"', '"SATURATE"', '"BYPASS"']
    rlabels = ['"PREDELAY"', '"DECAY"', '"SIZE"', '"DAMPING"',
               '"WIDTH"', '"MODE"', '"BYPASS"']
    for lbl in dlabels:
        assert lbl in dl, lbl
    for lbl in rlabels:
        assert lbl in rv, lbl
    # no RVB MIX anywhere on the reverb page
    assert "MIX" not in rv.replace('"MODE"', '').replace('"DAMPING"', '')
    # 7 rows each: the labels/ids arrays are size 7.
    assert "[7]=" in dl.split("labels")[1] or "static const char *labels[7]" in dl
    assert "[7]=" in rv.split("labels")[1] or "static const char *labels[7]" in rv
    print("DELAY/REVERB page rows (7 each, reverb without MIX) OK")


def check_master_page_formats():
    dl = MV_CPP[MV_CPP.index("void MixerView::drawDelayPage"):
               MV_CPP.index("void MixerView::drawReverbPage")]
    rv = MV_CPP[MV_CPP.index("void MixerView::drawReverbPage"):
               MV_CPP.index("void MixerView::drawEqPage")]
    assert '"%4.0f ms"' in dl                       # delay time ms
    assert '"%s",v>=0.5f?"ON":"OFF"' in dl          # ping-pong/sat
    assert '"%4.0f ms"' in rv                       # predelay ms
    assert '"%.2f s"' in rv                         # decay seconds
    assert 'v>=0.5f?"NORMAL":"ECO"' in rv           # reverb mode
    # RC3: reverb bypass renders through the unified UiDraw toggle
    # (UI_STYLE_GUIDE point 4) instead of a raw ON/OFF sprintf.
    assert "UiDraw::DrawToggle" in rv               # reverb bypass toggle
    print("master page value formats (ms, s, ON/OFF, ECO/NORMAL) OK")


# ---------------------------------------------------------------------------
# Point 5: solid effect-send bars (UIIntVarField::Draw)
# ---------------------------------------------------------------------------
def check_solid_bar_rendering():
    bar = UIF_CPP[UIF_CPP.index("if (barLabel_)"):UIF_CPP.index("Variable::Type type")]
    # Solid fill: per-cell inverted spaces, empty cells CD_HILITE1.
    assert "props.invert_=true" in bar
    assert "CD_HILITE1" in bar
    assert 'w.DrawString(" ",barPos,props)' in bar
    # INH clears the stale bar + percent area.
    assert 'sprintf(buffer,"%s: INH",barLabel_)' in bar
    assert "clearPos" in bar
    # Percent readout kept after the bar.
    assert '" %3d%%"' in bar
    # SetBar signature unchanged (other UIIntVarField users unaffected).
    assert "void SetBar(const char *label, int width)" in UIF_H
    print("solid send-bar rendering (inverted cells + INH clear) OK")


# ---------------------------------------------------------------------------
# Point 6: SetHexMode preserves the nibble cursor and clamps the value
# ---------------------------------------------------------------------------
class HexModeModel:
    """Mirror UIBigHexVarField::SetHexMode (RC2 fix)."""

    def __init__(self, precision, value, pos, minv, maxv, wrap):
        self.precision = max(precision - 1, 0)
        self.value = value
        self.pos = pos
        self.minv = minv
        self.maxv = maxv
        self.wrap = wrap

    def set_hex_mode(self, precision, minv, maxv, wrap):
        self.precision = max(precision - 1, 0)
        if self.pos > self.precision:
            self.pos = self.precision
        span = maxv - minv + 1
        if wrap and span > 0:
            rel = (self.value - minv) % span
            self.value = minv + rel
        else:
            if self.value > maxv:
                self.value = maxv
            if self.value < minv:
                self.value = minv
        return self.value, self.pos


def check_hexmode_fix_source():
    # The old forced position_=0 is gone; the new code clamps the cursor and
    # the whole value.
    seg = BIGHEX_CPP[BIGHEX_CPP.index("void UIBigHexVarField::SetHexMode"):
                    BIGHEX_CPP.index("void UIBigHexVarField::Draw")]
    assert "position_=0" not in seg, "must not force the nibble cursor to 0"
    assert "position_>precision_" in seg
    assert "src_.SetInt(value)" in seg
    assert "span" in seg and "rel" in seg
    print("SetHexMode source no longer forces position_=0 OK")


def check_hexmode_cursor_preserved():
    # Editing the most significant nibble of a 4-digit field, then switching
    # command mode with the same precision: the cursor must NOT jump to 0.
    f = HexModeModel(precision=4, value=0x0003, pos=3, minv=0, maxv=0xFFFF, wrap=False)
    v, pos = f.set_hex_mode(precision=4, minv=0, maxv=0xFFFF, wrap=False)
    assert pos == 3, pos
    assert v == 0x0003
    # Switching to a smaller precision clamps the cursor into range.
    f = HexModeModel(precision=4, value=0x45, pos=3, minv=0, maxv=0xFF, wrap=False)
    v, pos = f.set_hex_mode(precision=2, minv=0, maxv=0x17, wrap=False)
    assert pos == 1, pos
    print("SetHexMode preserves/clamps the nibble cursor OK")


def check_hexmode_value_clamp():
    # Clamp (no wrap): an out-of-range legacy param is clamped to the new max.
    f = HexModeModel(precision=4, value=0xFF, pos=0, minv=0, maxv=0xFF, wrap=False)
    v, _ = f.set_hex_mode(precision=2, minv=0, maxv=0x17, wrap=False)
    assert v == 0x17, hex(v)
    # Clamp low.
    f = HexModeModel(precision=4, value=0, pos=0, minv=0, maxv=0xFF, wrap=False)
    v, _ = f.set_hex_mode(precision=2, minv=1, maxv=0x17, wrap=False)
    assert v == 1, v
    # Wrap: a far-out value folds into [min,max] preserving its residue.
    f = HexModeModel(precision=4, value=80, pos=0, minv=0, maxv=29, wrap=True)
    v, _ = f.set_hex_mode(precision=2, minv=0, maxv=29, wrap=True)
    assert v == 80 % 30 == 20, v
    print("SetHexMode clamps/wraps the full value into range OK")


check_master_pages_dispatched()
check_master_hierarchy_colors()
check_master_page_rows()
check_master_page_formats()
check_solid_bar_rendering()
check_hexmode_fix_source()
check_hexmode_cursor_preserved()
check_hexmode_value_clamp()
print("FX_RC2_MASTER_PAGES_SOLID_BARS_HEXMODE_OK")
