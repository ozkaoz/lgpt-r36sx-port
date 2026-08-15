#!/usr/bin/env python3
"""Phase 8 model tests: InstrumentView block reorganization
(PLAN_FX_REDESIGN_ES.md, Fase 8).

Mirrors the reorganized fillSampleParameters() layout:

- The sample instrument view is split into vertical blocks with headers
  (INSTRUMENT, FILTER, BITCRUSHER, PLAYBACK, EFFECT SENDS, AUTOMATION).
- Block headers are drawn by DrawView(), NOT inserted as UIStaticField, so
  T_SimpleList<UIField>::GetFirst() stays the sample field and GetLast() stays
  the table field (L2+A cut/clear depend on both).
- BITCRUSHER is labeled "bit depth", never "compressor".
- EFFECT SENDS rows use the percent-bar rendering (SetBar DRY/DELAY/REVERB).
  RC2 (point 5) renders the bar as a solid block of inverted cells (MixerView
  style): "LABEL [solid fill]  85%" for 0..100 and "LABEL: INH" for -1.
- Legacy COMB parameters (fb tune/fb mix) are retired from editing while
  their variables keep existing (load/playback/IDs preserved).
- Offline render FX (print fx/wet/pad) stay behind #ifdef FFMPEG_ENABLED and
  therefore do not exist in the R36SX build.
- The whole field stack stays on-screen (rows 5..26 within the 22 visible
  rows anchored at y=4; the bottom bar/map starts at y=27).

Acceptance:
- headers and their screen rows match the field layout
- every field row is within the visible field area (<=26)
- sample is the first inserted field, table is the last inserted field
- bitcrusher row is labeled "bit depth" and "compressor" never labels it
- bar rendering math matches UIIntVarField::Draw (INH for -1, full at 100)
- fb tune/fb mix are gone from the editor but still exist in SampleInstrument
- OFFLINE RENDER FX block is behind #ifdef FFMPEG_ENABLED
- source guards for SetBar / block headers
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

IV_CPP = (ROOT / "source/sources/Application/UI/Views/InstrumentView.cpp").read_text()
UIH_CPP = (ROOT / "source/sources/Application/UI/Views/BaseClasses/UIIntVarField.cpp").read_text()
UIH_H = (ROOT / "source/sources/Application/UI/Views/BaseClasses/UIIntVarField.h").read_text()
SI_H = (ROOT / "source/sources/Application/Instruments/SampleInstrument.h").read_text()
SI_CPP = (ROOT / "source/sources/Application/Instruments/SampleInstrument.cpp").read_text()

ANCHOR_Y = 4  # GetAnchor() => (10,4)
MAP_TOP = 27  # drawMap x=1-4, drawNotes x=10-33 both at y=27-29

# (header label, screen row).  Must match DrawView() draws AND fillSampleParameters.
HEADERS = [
    ("INSTRUMENT", 4),
    ("FILTER", 8),
    ("BITCRUSHER", 12),
    ("PLAYBACK", 15),
    ("EFFECT SENDS", 21),
    ("AUTOMATION", 25),
]

# (field label, screen row).  Mirrors fillSampleParameters block layout.
FIELDS = [
    ("sample", 5),
    ("volume", 6), ("pan", 6),
    ("root note", 7), ("detune", 7),
    ("type", 9), ("mode", 9),
    ("cutoff", 10), ("reso", 10),
    ("attenuate", 11), ("filt", 11),
    ("bit depth", 13), ("drive", 13),
    ("downsample", 14),
    ("interpolation", 16), ("loop mode", 16),
    ("slices", 17),
    ("start", 18), ("loop start", 19), ("loop end", 20),
    ("DRY", 22), ("DELAY", 23), ("REVERB", 24),
    ("table auto", 26), ("table", 26),
]

# Map label -> tokens expected in the source for that field.
LABEL_TOKENS = {
    "DRY": ("SetBar(\"DRY\",", "SIP_DRY"),
    "DELAY": ("SetBar(\"DELAY\",", "SIP_DLY_SEND"),
    "REVERB": ("SetBar(\"REVERB\",", "SIP_RVB_SEND"),
}


def check_layout_rows():
    max_field_row = max(row for _, row in FIELDS)
    # every field must stay above the bottom map/notes band (y>=27)
    assert max_field_row < MAP_TOP, max_field_row
    for _, row in HEADERS:
        assert row < MAP_TOP, row
    header_rows = [r for _, r in HEADERS]
    field_rows = {r for _, r in FIELDS}
    for hrow in header_rows:
        assert hrow not in field_rows, hrow  # headers own their row
    print("layout rows stay above the map/notes band (y=27) OK")


def check_headers_in_drawview():
    for label, row in HEADERS:
        # RC3 (fase completa): DrawView draws them with UiDraw section
        # headers at hp._y + offset; anchor y == ANCHOR_Y.
        offset = row - ANCHOR_Y
        assert 'DrawSectionHeader(*this, hp._x' in IV_CPP, label
        assert '"%s"' % label in IV_CPP, label
        if offset > 0:
            assert 'hp._y + %d' % offset in IV_CPP, (label, offset)
    # block headers are NOT inserted as UIStaticField fields
    for sf in IV_CPP.split("new UIStaticField(")[1:]:
        for label, _ in HEADERS:
            assert ('"%s"' % label) not in sf.split(")")[0], label
    print("block headers drawn in DrawView via UiDraw section headers (not as fields) OK")


def check_first_last_fields():
    fill = IV_CPP.split("fillSampleParameters() {", 1)[1].split("fillMidiParameters", 1)[0]
    # first inserted field is the sample field
    first_insert = fill.find("T_SimpleList<UIField>::Insert")
    assert first_insert != -1
    assert fill.find('"sample: %s"') < first_insert + 400
    # last inserted field is the table field
    last_insert = fill.rfind("T_SimpleList<UIField>::Insert")
    assert last_insert != -1
    assert '"table: %2.2X"' in fill[max(0, last_insert - 300):last_insert], \
        "table must remain the last field (L2+A clear table)"
    print("sample first / table last preserved OK")


def check_bitcrusher_label():
    fill = IV_CPP.split("fillSampleParameters() {", 1)[1].split("fillMidiParameters", 1)[0]
    no_comments = "\n".join(
        line.split("//")[0] for line in fill.splitlines())
    assert '"bit depth: %d"' in no_comments
    assert "compressor" not in no_comments.lower(), \
        "bitcrusher must never be labeled compressor"
    print("BITCRUSHER labeled bit depth (never compressor) OK")


def bar_render(value, width):
    """Mirror UIIntVarField::Draw() bar branch (RC2 solid-bar rendering).

    Returns (filled, percent) where filled is the number of inverted cells
    (the solid fill) and percent the value displayed after the bar; -1 -> INH.
    """
    if value < 0:
        return "INH"
    v = max(0, min(100, value))
    filled = (width * v) // 100
    return (filled, v)


def check_bar_render():
    # Solid-bar math: filled cells = width*v/100, always in [0,width].
    assert bar_render(-1, 14) == "INH"
    assert bar_render(0, 14)[0] == 0
    assert bar_render(100, 14)[0] == 14
    assert bar_render(50, 14)[0] == 7
    assert bar_render(85, 14)[0] == 11  # 14*85//100
    assert bar_render(7, 14)[1] == 7    # percent shows the raw clamped value
    for label, (tok1, tok2) in LABEL_TOKENS.items():
        assert tok1 in IV_CPP, (label, tok1)
        assert tok2 in IV_CPP, (label, tok2)
    assert "barLabel_" in UIH_CPP and "barWidth_" in UIH_CPP
    assert "barLabel_" in UIH_H and "barWidth_" in UIH_H
    assert "SetBar" in UIH_H
    # RC2 solid-bar source guards: inverted-cell fill + INH clear path.
    assert "props.invert_=true" in UIH_CPP
    assert "props.invert_=false" in UIH_CPP
    assert 'sprintf(buffer,"%s: INH",barLabel_)' in UIH_CPP
    assert "CD_HILITE1" in UIH_CPP
    print("send bars (SetBar DRY/DELAY/REVERB, solid inverted cells, INH) OK")


def check_filter_kind_field():
    # FXP_FILTER_V2 (bacon-1.5, item 2): the filter-kind row (right cell of
    # the attenuate row, y=11) is a CHAR_LIST field fed by SIP_FILTERKIND.
    fill = IV_CPP.split("fillSampleParameters() {", 1)[1].split("fillMidiParameters", 1)[0]
    assert '"filt: %s"' in fill
    assert "SIP_FILTERKIND" in fill
    assert "SIP_FILTERKIND" in SI_H
    assert "filter kind" in SI_CPP
    print("filter-kind field (filt) wired to SIP_FILTERKIND OK")


def check_fb_retired():
    # retired from editing: SIP_FBTUNE/SIP_FBMIX not referenced in the view
    assert "SIP_FBTUNE" not in IV_CPP
    assert "SIP_FBMIX" not in IV_CPP
    # but the variables still exist for load/playback/IDs
    assert "SIP_FBTUNE" in SI_H and "SIP_FBMIX" in SI_H
    assert "feedback tune" in SI_CPP and "feedback mix" in SI_CPP
    print("fb tune/mix retired from editing, variables kept OK")


def check_offline_fx_guarded():
    assert "#ifdef FFMPEG_ENABLED" in IV_CPP
    assert "OFFLINE RENDER FX" in IV_CPP
    assert "#endif" in IV_CPP
    print("OFFLINE RENDER FX behind FFMPEG_ENABLED guard OK")


check_layout_rows()
check_headers_in_drawview()
check_first_last_fields()
check_bitcrusher_label()
check_bar_render()
check_filter_kind_field()
check_fb_retired()
check_offline_fx_guarded()
print("FX_INSTRUMENT_BLOCKS_PHASE8_OK")
