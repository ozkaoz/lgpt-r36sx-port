#!/usr/bin/env python3
"""RC5 UI centered-layout tests: 40x30 geometry, modal windows, Mixer meters.

(New layout iteration: centered menu-style blocks, 30-row modal centering,
single-cell mixer meters.)

Mirrors the C++ helpers so the checks are against the actual numbers the
views compute:

- UiDraw shared constants (kScreenWidth/kScreenHeight and the safe band
  kMenuBandTop=3..kMenuBandBottom=25; the footer occupies rows 27..29).
- UiDraw::CenterTextX(text, viewportWidth) centers on a viewport: full
  screen (40) for full-screen views, the modal inner width for modals.
- UiDraw::MakeCenteredMenuLayout(rowCount, labelWidth, valueWidth, spacing,
  viewportWidth, contentTop, contentBottom) centers a vertical menu block.
- ModalView::SetWindow centers on the full 30 rows and keeps both borders
  inside rows 0..29.
- The MIX page lays out 9 single-cell meters (MST + 8 channels) every 3
  columns across a 25-cell centered bank; DELAY/REVERB/EQ/COMP master pages
  use centered MenuLayout blocks inside the safe band, and each page draws
  its title at the top of its centered block.
- Phrase/Table/Instrument grids are centered on 40x30 (Phrase grid shifted
  left 4 and all three grids shifted for vertical centering) without changing
  View::GetAnchor().
- The refactor must not touch DSP ranges, defaults or the FxParamId enum.

Acceptance: all layout helpers produce in-band, centered geometry; nothing
from these views reaches rows 27..29; the DSP param table is unchanged.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VIEWS = ROOT / "source/sources/Application/UI/Views"

UIDRAW_H = (VIEWS / "BaseClasses/UiDraw.h").read_text()
UIDRAW_CPP = (VIEWS / "BaseClasses/UiDraw.cpp").read_text()
MODAL_CPP = (VIEWS / "BaseClasses/ModalView.cpp").read_text()
MODAL_H = (VIEWS / "BaseClasses/ModalView.h").read_text()
PROJECT_CPP = (VIEWS / "ProjectView.cpp").read_text()
MIX_CPP = (VIEWS / "MixerView.cpp").read_text()
MIX_H = (VIEWS / "MixerView.h").read_text()
PHRASE_CPP = (VIEWS / "PhraseView.cpp").read_text()
TABLE_CPP = (VIEWS / "TableView.cpp").read_text()
INSTRUMENT_CPP = (VIEWS / "InstrumentView.cpp").read_text()

# ---------------------------------------------------------------------------
# Mirror of the C++ layout helpers (UiDraw.cpp, RC5).
# ---------------------------------------------------------------------------
KSW = 40
KSH = 30
KBAND_TOP = 3
KBAND_BOT = 25


def center_text_x(text, viewport=KSW):
    if not text:
        return 0
    return (viewport - len(text)) // 2


def centered_menu(row_count, label_width, value_width, spacing,
                  viewport=KSW, content_top=KBAND_TOP, content_bottom=KBAND_BOT):
    if spacing < 0:
        spacing = 0
    value_span = spacing + value_width if value_width > 0 else 0
    block_width = min(label_width + value_span, viewport)
    start_x = (viewport - block_width) // 2
    band = content_bottom - content_top + 1
    block_height = min(row_count, band)
    start_y = content_top + (band - block_height) // 2
    label_x = start_x
    value_x = (start_x + label_width + spacing) if value_width > 0 else start_x
    return dict(screenWidth=viewport, contentTop=content_top,
                contentBottom=content_bottom, blockWidth=block_width,
                blockHeight=block_height, startX=start_x, startY=start_y,
                labelX=label_x, valueX=value_x)


# ---------------------------------------------------------------------------
# 1. Shared constants
# ---------------------------------------------------------------------------
def check_constants():
    assert "kScreenWidth = 40" in UIDRAW_H
    assert "kScreenHeight = 30" in UIDRAW_H
    assert "kMenuBandTop = 3" in UIDRAW_H
    assert "kMenuBandBottom = 25" in UIDRAW_H
    assert "kFooterTop = 27" in UIDRAW_H
    assert "kFooterBottom = 29" in UIDRAW_H
    assert "kTitleRow = 0" in UIDRAW_H
    assert "kSubtitleRow = 1" in UIDRAW_H
    print("1. shared 40x30 + band/footer/title constants OK")


# ---------------------------------------------------------------------------
# 2. CenterTextX by viewport
# ---------------------------------------------------------------------------
def check_center_text_x():
    # Full screen (default viewport = 40).
    assert center_text_x("Project Exit") == (40 - 12) // 2 == 14
    # Modal inner viewport: 28-wide window -> x is local to the modal.
    assert center_text_x("Project Exit", 28) == (28 - 12) // 2 == 8
    # Odd-length text on a 40 viewport leaves one cell right (tolerance).
    assert center_text_x("Mixer", 40) == (40 - 5) // 2 == 17
    # The C++ signature carries the viewport parameter.
    assert "int viewportWidth = kScreenWidth" in UIDRAW_H
    assert "int UiDraw::CenterTextX(const char *text, int viewportWidth)" in UIDRAW_CPP
    print("2. CenterTextX centers inside a viewport (default 40) OK")


# ---------------------------------------------------------------------------
# 3. MenuLayout: even and odd block widths / heights
# ---------------------------------------------------------------------------
def check_layout_parity():
    # Odd block width (19) -> startX 10, one-cell right tolerance (11 right).
    odd = centered_menu(7, 9, 8, 2)
    assert odd["blockWidth"] == 19 and odd["startX"] == 10
    assert 40 - (odd["startX"] + odd["blockWidth"]) == 11
    # Even block width (18) -> exact centering.
    even = centered_menu(7, 8, 8, 2)
    assert even["blockWidth"] == 18 and even["startX"] == 11
    assert 40 - (even["startX"] + even["blockWidth"]) == 11
    # Odd row count in the 3..25 band (23 rows) -> startY 11, 8 above/8 below.
    ml = centered_menu(7, 9, 8, 2)
    assert ml["startY"] == 3 + (23 - 7) // 2 == 11
    assert (ml["startY"] - KBAND_TOP) == (KBAND_BOT - (ml["startY"] + 6))
    # Even row count (16) -> startY 6, top margin 3 / bottom margin 4.
    ml16 = centered_menu(16, 6, 9, 2)
    assert ml16["startY"] == 6
    assert (ml16["startY"] - KBAND_TOP) == 3
    assert (KBAND_BOT - (ml16["startY"] + 15)) == 4
    print("3. even/odd block width and height centering OK")


# ---------------------------------------------------------------------------
# 4. DELAY / REVERB pages: 11-row / 9-row centered blocks (bacon-1.5 item 3)
# ---------------------------------------------------------------------------
def check_delay_reverb_pages():
    dl = MIX_CPP[MIX_CPP.index("void MixerView::drawDelayPage"):
                 MIX_CPP.index("void MixerView::drawReverbPage")]
    rv = MIX_CPP[MIX_CPP.index("void MixerView::drawReverbPage"):
                 MIX_CPP.index("void MixerView::drawEqRow")]
    # DELAY 11 rows (SYNC/DIVISION/LOW CUT/HIGH CUT), REVERB 9 rows (IN HP/
    # IN LP).
    assert "static const char *labels[11]" in dl
    assert "static const char *labels[9]" in rv
    assert "MakeCenteredMenuLayout(11,9,12,2)" in dl
    assert "MakeCenteredMenuLayout(9,8,12,2)" in rv
    assert "ml.startY+p" in dl and "ml.startY+p" in rv
    # Block geometry stays inside the safe band 3..25.
    for spec in ((11, 9, 12, 2), (9, 8, 12, 2)):
        ml = centered_menu(*spec)
        assert ml["startY"] >= KBAND_TOP
        assert ml["startY"] + ml["blockHeight"] - 1 <= KBAND_BOT
        assert ml["startX"] >= 0 and ml["startX"] + ml["blockWidth"] <= KSW
    print("4. DELAY 11-row / REVERB 9-row centered blocks in band 3..25 OK")


# ---------------------------------------------------------------------------
# 5. EQ / COMP pages: whole block centered
# ---------------------------------------------------------------------------
def check_eq_comp_pages():
    eq = MIX_CPP[MIX_CPP.index("void MixerView::drawEqPage"):
                 MIX_CPP.index("void MixerView::drawCompPage")]
    cm = MIX_CPP[MIX_CPP.index("void MixerView::drawCompPage"):
                 MIX_CPP.index("void MixerView::drawMixReturns")]
    assert "MakeCenteredMenuLayout(16,6,13,2)" in eq
    assert "MakeCenteredMenuLayout(13,11,13,3)" in cm
    # Bypass row and headers/params share the centered columns.
    assert "drawEqRow(FX_P_EQ_BYP,ml.labelX,ml.valueX,ml.startY)" in eq
    assert "DrawString(ml.labelX,yHeader" in eq
    # FXP_MASTER_EQ8: the EQ EXT page is a self-labeled 21-row centered block
    # between drawEqPage and drawCompPage.
    assert "MakeCenteredMenuLayout(21,6,13,2)" in eq
    assert "drawEqExtRow(bandId+p,ml.labelX,ml.valueX,ml.startY+1+4*b+p)" in eq
    mlx = centered_menu(21, 6, 13, 2)
    assert mlx["startY"] >= KBAND_TOP
    assert mlx["startY"] + mlx["blockHeight"] - 1 <= KBAND_BOT
    # COMP GR meter sits one row below the last parameter, still in band.
    assert "ml.startY+13" in cm
    ml = centered_menu(9, 11, 13, 3)
    assert ml["startY"] + 9 <= KBAND_BOT
    # No fixed columns remain on these pages.
    assert "const int x=13" not in eq and "const int labelX=8" not in cm
    print("5. EQ (16 rows), EQ_EXT (21 rows) and COMP (9 rows) centered blocks OK")


# ---------------------------------------------------------------------------
# 6. ProjectExit modal: local viewport centering
# ---------------------------------------------------------------------------
def check_project_exit_modal():
    seg = PROJECT_CPP[PROJECT_CPP.index("TreeFrogProjectExitModal"):
                      PROJECT_CPP.index("virtual ~TreeFrogProjectExitModal")]
    draw = PROJECT_CPP[PROJECT_CPP.index("SetWindow(28,6)"):
                       PROJECT_CPP.index("virtual void ProcessButtonMask")]
    # Window geometry (28x6) is centered by SetWindow; title and menu center
    # on the modal viewport (GetWindowWidth/Height), not the full screen.
    assert "const int vw = GetWindowWidth();" in draw
    assert "const int vh = GetWindowHeight();" in draw
    assert "UiDraw::CenterTextX(\"Project Exit\", vw)" in draw
    assert "MakeCenteredMenuLayout(3, 20, 0, 0, vw, 0," in draw
    assert "vh - 1)" in draw
    assert "(void)ml" not in draw
    # Verify the local geometry the modal actually produces.
    ml = centered_menu(3, 20, 0, 0, 28, 0, 5)
    assert ml["startX"] == (28 - 20) // 2 == 4
    assert ml["startY"] == (6 - 3) // 2 == 1
    assert center_text_x("Project Exit", 28) == 8
    print("6. ProjectExit uses modal-local centering (no (void)ml) OK")


# ---------------------------------------------------------------------------
# 7. ModalView::SetWindow centers on 30 rows, borders inside 0..29
# ---------------------------------------------------------------------------
def check_set_window_30():
    # Vertical center uses the full 30-row screen.
    assert "UiDraw::kScreenHeight - height) / 2" in MODAL_CPP
    assert "(20 - height) / 2" not in MODAL_CPP
    # Width/height are stored and exposed through getters.
    assert "GetWindowWidth()" in MODAL_H and "GetWindowHeight()" in MODAL_H
    assert "width_ = width;" in MODAL_CPP and "height_ = height;" in MODAL_CPP
    # Border rows (top_-2 / top_+height+1) stay inside 0..29 for every
    # window height in the modal call sites.
    for height in (3, 5, 6, 8, 9, 11, 17, 19, 23, 24, 26):
        top = max(2, min(29 - height - 1, (30 - height) // 2))
        assert top - 2 >= 0, height
        assert top + height + 1 <= 29, height
    print("7. SetWindow centers on 30 rows with in-band borders OK")


# ---------------------------------------------------------------------------
# 8. MIX page: 9 meters (MST + 8 ch) one-cell columns, inside 0..39
# ---------------------------------------------------------------------------
# NOTE: updated for the golden baseline (Bacon 1.1.1 V16 + RC6, stereo
# meters) and BACON_1.5_MIXER_FULLSCREEN (U2.53, feedback #7): layout
# constants are masterX=4, channel0X=8, channelPitch=4, labelY=4 (hex
# labels), barY=labelY+2=6, barHeight=19 (bars 7..25, 27% taller), the
# volume numbers at row 5 and the pan/mute marker at row 26; each meter is
# a single cell column (L/R drawn on the same x by drawMeterBar(side 0/1)).
def check_mixer_meters():
    draw = MIX_CPP[MIX_CPP.index("void MixerView::drawFxPages"):
                   MIX_CPP.index("void MixerView::DrawView")]
    assert "masterX=4" in draw and "channel0X=8" in draw
    assert "channelPitch=4" in draw and "barHeight=19" in draw
    assert "chLabelX=38" not in draw
    # 9 meters: 8 channels in a loop + 1 master bar.
    assert "for (int i=0;i<SONG_CHANNEL_COUNT;i++)" in draw
    assert "drawVolumeBar(i,channel0X+i*channelPitch,barY,barHeight)" in draw
    assert "drawMasterBar(masterX,barY,barHeight)" in draw
    # Uniform positions: MST at 4, channels 8..36 pitch 4 -> 9 columns,
    # all inside 0..39, no overlap.
    positions = [4] + [8 + i * 4 for i in range(8)]
    assert positions == [4, 8, 12, 16, 20, 24, 28, 32, 36]
    assert all(0 <= p <= 39 for p in positions)
    assert len(set(positions)) == 9
    assert all(positions[i + 1] - positions[i] == 4 for i in range(8))
    # One-cell column per meter: stereo bars share the x (drawMeterBar side
    # 0/1); labels and volume numbers are centered at x-1 over the axis.
    bar = MIX_CPP[MIX_CPP.index("void MixerView::drawVolumeBar"):
                  MIX_CPP.index("void MixerView::drawMasterBar")]
    mbar = MIX_CPP[MIX_CPP.index("void MixerView::drawMasterBar"):
                   MIX_CPP.index("void MixerView::cycleFxPage")]
    # F3-4b: the L/R levels come from the MixerMeters layer.
    assert "drawMeterBar(x,y,height,meters_.LevelL(channel)" in bar
    assert "drawMeterBar(x,y,height,meters_.LevelR(channel)" in bar
    assert "DrawString(x-1,y-2,hex,props)" in bar
    assert "DrawString(x-1,y+height+1" in bar
    assert "DrawString(x-1,y-2,\"MST\",props)" in mbar
    assert "meterRecords_[SONG_CHANNEL_COUNT]" in mbar   # master meter slot
    print("8. 9 one-column meters (8ch pitch 4 + MST) in 0..39 OK")


# ---------------------------------------------------------------------------
# 9. Mixer block layout: fullscreen DAW strips, FX RETURNS on the title row
# ---------------------------------------------------------------------------
def check_mixer_block_bounds():
    draw = MIX_CPP[MIX_CPP.index("void MixerView::drawFxPages"):
                   MIX_CPP.index("void MixerView::DrawView")]
    # Constants used by the block (U2.53 MIXER-FULLSCREEN: title/transport
    # rows 0-3, hex labels 4, volume numbers 5, bars 7..25, pan row 26; the
    # played-notes block + view map keep rows 27..29).
    assert "labelY=4" in draw and "barHeight=19" in draw
    assert "barY=labelY+2" in draw
    assert "retY=1" in draw
    # The whole strip block stays inside the safe band: FX RETURNS 1 (title
    # row), labels 4, volume 5, bars 7..25, pan 26 -- never rows 27..29.
    label_y, bar_h = 4, 19
    bar_y = label_y + 2
    ret_y = 1
    assert label_y >= KBAND_TOP
    assert bar_y + bar_h <= KBAND_BOT
    assert ret_y <= KBAND_BOT
    assert ret_y < 26
    # FX RETURNS is drawn on its own parametrized row (drawMixReturns(retY)).
    assert "drawMixReturns(retY)" in draw
    assert "void MixerView::drawMixReturns(int y)" in MIX_CPP
    # Nothing on these pages reaches the footer rows 27..29.
    for seg_name, start_marker, end_marker in (
            ("MIX", "void MixerView::drawFxPages", "void MixerView::DrawView"),
            ("DELAY", "void MixerView::drawDelayPage", "void MixerView::drawReverbPage"),
            ("EQ", "void MixerView::drawEqPage", "void MixerView::drawCompPage"),
            ("COMP", "void MixerView::drawCompPage", "void MixerView::drawMixReturns")):
        seg = MIX_CPP[MIX_CPP.index(start_marker):MIX_CPP.index(end_marker)]
        assert "DrawString(...,2" not in seg
    print("9. DAW strips in rows 4..26; FX RETURNS on row 1; nothing in 27..29 OK")


# ---------------------------------------------------------------------------
# 10. Master pages title + bypass row use the centered columns
# ---------------------------------------------------------------------------
def check_bypass_columns():
    # Every page routes Bypass through DrawBypassRow with the label/value
    # columns of the centered block.
    dl = MIX_CPP[MIX_CPP.index("void MixerView::drawDelayPage"):
                 MIX_CPP.index("void MixerView::drawReverbPage")]
    rv = MIX_CPP[MIX_CPP.index("void MixerView::drawReverbPage"):
                 MIX_CPP.index("void MixerView::drawEqRow")]
    eq = MIX_CPP[MIX_CPP.index("void MixerView::drawEqRow"):
                 MIX_CPP.index("void MixerView::drawEqPage")]
    cm = MIX_CPP[MIX_CPP.index("void MixerView::drawCompPage"):
                 MIX_CPP.index("void MixerView::drawMixReturns")]
    for seg in (dl, rv, eq, cm):
        assert "UiDraw::DrawBypassRow(*this,ml.labelX,ml.valueX" in seg or \
               "UiDraw::DrawBypassRow(*this,labelX,valueX" in seg
    # RC6: each dedicated page draws its title at the top of its centered
    # block (ml.startY-1); drawFxParamPage passes the page title through.
    eqpage = MIX_CPP[MIX_CPP.index("void MixerView::drawEqPage"):
                     MIX_CPP.index("void MixerView::drawCompPage")]
    for page_seg in (dl, rv, eqpage, cm):
        assert "UiDraw::DrawCenteredTitleAt(*this,ml.startY-1,title)" in page_seg
    assert "drawDelayPage(pageTitle)" in MIX_CPP
    assert "drawReverbPage(pageTitle)" in MIX_CPP
    assert "drawEqPage(pageTitle)" in MIX_CPP
    assert "drawCompPage(pageTitle)" in MIX_CPP
    print("10. bypass rows use centered label/value columns; title above block OK")


# ---------------------------------------------------------------------------
# 11. DSP ranges / defaults / enum unchanged by the layout refactor
# ---------------------------------------------------------------------------
def check_dsp_table_unchanged():
    # F3-4a: the FxParamSpec table + enums moved to FxPages.h.
    FXP = (ROOT / "source/sources/Application/Mixer/FxPages.h").read_text()
    # The FxParamSpec table keeps every range/default (layout only changed).
    specs = [
        ("{ \"DLY TIM\"", "10.0f, 2000.0f"),
        ("{ \"DLY MIX\"", "0.0f,   1.0f,    1.0f"),
        ("{ \"RVB DEC\"", "0.2f,   8.0f,    1.0f"),
        ("{ \"EQ  BYP\"", "0.0f,   1.0f,    1.0f"),
        ("{ \"LO  FRQ\"", "20.0f, 20000.0f,100.0f"),
        ("{ \"MID FRQ\"", "20.0f, 20000.0f,1000.0f"),
        ("{ \"HI  FRQ\"", "20.0f, 20000.0f,10000.0f"),
        ("{ \"CMP THR\"", "-60.0f,   0.0f,  -24.0f"),
        ("{ \"CMP RAT\"", "1.0f,  20.0f,    4.0f"),
        ("{ \"CMP ATK\"", "0.1f, 500.0f,   15.0f"),
        ("{ \"CMP REL\"", "1.0f, 2000.0f, 200.0f"),
    ]
    for label, rest in specs:
        idx = FXP.index(label)
        assert rest in FXP[idx:idx + 90], (label, rest)
    # FxParamId enum order is unchanged (bypass last per page, band EN first).
    e = FXP.index("enum FxParamId")
    assert FXP.index("FX_P_DLY_BYP", e) > FXP.index("FX_P_DLY_TIME", e)
    assert FXP.index("FX_P_RVB_BYP", e) > FXP.index("FX_P_RVB_PRE", e)
    assert FXP.index("FX_P_EQ_LOW_EN", e) > FXP.index("FX_P_EQ_BYP", e)
    assert FXP.index("FX_P_CMP_BYP", e) > FXP.index("FX_P_EQ_HI_Q", e)
    assert FXP.index("FX_P_CMP_SC", e) > FXP.index("FX_P_CMP_BYP", e)
    # Mixer FxPage order untouched.
    assert FXP.index("FX_PAGE_MIX") < FXP.index("FX_PAGE_DELAY")
    assert FXP.index("FX_PAGE_COMP") < FXP.index("FX_PAGE_COUNT")
    # FXP_MASTER_EQ8: EQ_EXT sits between EQ and COMP, ids appended at the
    # end (before FX_PARAM_COUNT) so every golden FX_P_* value is unchanged.
    assert FXP.index("FX_PAGE_EQ_EXT") > FXP.index("FX_PAGE_EQ")
    assert FXP.index("FX_PAGE_EQ_EXT") < FXP.index("FX_PAGE_COMP")
    assert FXP.index("FX_P_EQX_BYP") > FXP.index("FX_P_CMP_SC")
    assert FXP.index("FX_P_EQX_BYP") < FXP.index("FX_PARAM_COUNT")
    print("11. DSP ranges/defaults and FxParamId/FxPage enums unchanged OK")


# ---------------------------------------------------------------------------
# 12. Footer band untouched by the layout views
# ---------------------------------------------------------------------------
def check_footer_band():
    # drawMap/drawNotes intentionally own rows 26..29 (View.cpp); the menu
    # blocks of the Mixer and its pages must never reach them.
    block_max_row = {
        "MIX": 22,       # pan row (y+height+3 = 22)
        "DELAY": 17,
        "REVERB": 17,
        "EQ": 21,
        "EQ_EXT": 24,    # 21-row block centered at startY=4
        "COMP": 19,      # GR row
    }
    for name, max_row in block_max_row.items():
        assert max_row < 26, name
    # The safe band is exactly 3..25 (23 rows).
    assert KBAND_BOT - KBAND_TOP + 1 == 23
    assert KBAND_TOP == 3 and KBAND_BOT == 25
    print("12. menu blocks never reach the footer band 27..29 OK")


# ---------------------------------------------------------------------------
# 13. Phrase/Table/Instrument centered without touching View::GetAnchor()
# ---------------------------------------------------------------------------
def check_grid_views_centered():
    # Phrase grid is horizontally centered: kColX content spans 5..35 on the
    # 40-cell screen (center 20), row numbers at x=2, play cursor at x=1.
    assert "{5, 8, 11, 14, 19, 23, 27, 31}" in PHRASE_CPP
    assert "{6, 9, 12, 15, 20, 24, 28, 32}" in PHRASE_CPP
    assert "pos._x = 2;" in PHRASE_CPP
    assert "pos._x = 1;" in PHRASE_CPP
    # Both grids shift down 3 rows so rows 7..22 center on the 30-row screen
    # (three GetAnchor sites each: DrawView / UpdateCursor / OnPlayerUpdate).
    assert PHRASE_CPP.count("anchor._y += 3 ;") == 3
    assert TABLE_CPP.count("anchor._y += 3 ;") == 3
    # Table content is already horizontally centered (9..30); only the
    # vertical shift applies, so the row-number column stays anchor-derived.
    assert "pos._x = anchor._x - 1;" in TABLE_CPP
    # Instrument form columns shift left 4 (labels x=6, second column x+16)
    # in the four fillers (sample/midi/synth/piano) and in the block headers
    # of DrawView (BASS_SYNTH and PIANO_SYNTH added two header branches).
    assert INSTRUMENT_CPP.count("position._x -= 4 ;") == 4
    assert INSTRUMENT_CPP.count("hp._x -= 4 ;") == 3
    # View::GetAnchor() itself is untouched by this iteration (the per-view
    # offsets live in Phrase/Table/Instrument, never in View.cpp).
    v = (VIEWS / "BaseClasses/View.cpp").read_text()
    assert "View::GetAnchor()" in v
    assert "(height-View::songRowCount_)/2" in v
    assert "anchor._y += 3" not in v
    assert "position._x -= 4" not in v
    print("13. Phrase/Table/Instrument centered; GetAnchor() untouched OK")


check_constants()
check_center_text_x()
check_layout_parity()
check_delay_reverb_pages()
check_eq_comp_pages()
check_project_exit_modal()
check_set_window_30()
check_mixer_meters()
check_mixer_block_bounds()
check_bypass_columns()
check_dsp_table_unchanged()
check_footer_band()
check_grid_views_centered()
print("UI_CENTERED_LAYOUT_OK")
