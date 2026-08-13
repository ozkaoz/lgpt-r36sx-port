#!/usr/bin/env python3
"""Phase 6 model tests: A+B = restore default navigation (PLAN_FX_REDESIGN_ES.md).

Mirrors the MixerView FxParamSpec table (5 pages, 37 params) and the
InstrumentView A+B field reset.  Acceptance:

- every FX parameter has a documented legacy default matching FxEngine's
  AllParamsAtLegacyDefault state (so one A+B press per row can bring the whole
  page back to "all defaults")
- A+B on a parameter page resets the hovered row to vdef (clamped to vmin/vmax)
- A+B in InstrumentView calls Variable::Reset() on the focused field
- cut-instrument / clear-table moved from B+A to L2+A (A+B is now reset)
- source guards for fxResetRow, vdef, and the InstrumentView handlers
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# (label, page, vmin, vmax, vdef)  -- natural units, mirrors MixerView.cpp
# Fase 6, RC2 (point 3.1): RVB MIX was removed (wet-only reverb), so the
# pages are DELAY(7) REVERB(7) EQ(13) COMP(9) = 36, MIX page has no rows.
PARAMS = [
    # DELAY
    ("DLY TIM", "DELAY", 10.0, 2000.0, 0.0),
    ("DLY FBK", "DELAY", 0.0, 0.98, 0.0),
    ("DLY MIX", "DELAY", 0.0, 1.0, 1.0),
    ("DLY WID", "DELAY", 0.0, 1.0, 1.0),
    ("DLY P/P", "DELAY", 0.0, 1.0, 0.0),
    ("DLY SAT", "DELAY", 0.0, 1.0, 0.0),
    ("DLY BYP", "DELAY", 0.0, 1.0, 0.0),
    # REVERB
    ("RVB PRE", "REVERB", 0.0, 100.0, 0.0),
    ("RVB DEC", "REVERB", 0.2, 8.0, 1.0),
    ("RVB SIZ", "REVERB", 0.5, 1.5, 1.0),
    ("RVB DMP", "REVERB", 0.0, 1.0, 0.5),
    ("RVB WID", "REVERB", 0.0, 1.0, 1.0),
    ("RVB MOD", "REVERB", 0.0, 1.0, 0.0),
    ("RVB BYP", "REVERB", 0.0, 1.0, 0.0),
    # EQ (banded menu Fase 12: bypass + EN/FRQ/GAI/Q per band, EN first)
    ("EQ  BYP", "EQ", 0.0, 1.0, 1.0),
    ("LO  EN", "EQ", 0.0, 1.0, 0.0),
    ("LO  FRQ", "EQ", 20.0, 20000.0, 100.0),
    ("LO  GAI", "EQ", -12.0, 12.0, 0.0),
    ("LO  Q", "EQ", 0.1, 10.0, 1.0),
    ("MID EN", "EQ", 0.0, 1.0, 0.0),
    ("MID FRQ", "EQ", 20.0, 20000.0, 1000.0),
    ("MID GAI", "EQ", -12.0, 12.0, 0.0),
    ("MID Q", "EQ", 0.1, 10.0, 1.0),
    ("HI  EN", "EQ", 0.0, 1.0, 0.0),
    ("HI  FRQ", "EQ", 20.0, 20000.0, 10000.0),
    ("HI  GAI", "EQ", -12.0, 12.0, 0.0),
    ("HI  Q", "EQ", 0.1, 10.0, 1.0),
    # COMP (dedicated menu Fase 13: BYP first, then THR/RAT/KNE/ATK/REL/MKU/LNK/SC)
    ("CMP BYP", "COMP", 0.0, 1.0, 1.0),
    ("CMP THR", "COMP", -60.0, 0.0, -24.0),
    ("CMP RAT", "COMP", 1.0, 20.0, 4.0),
    ("CMP KNE", "COMP", 0.0, 12.0, 6.0),
    ("CMP ATK", "COMP", 0.1, 500.0, 15.0),
    ("CMP REL", "COMP", 1.0, 2000.0, 200.0),
    ("CMP MKU", "COMP", 0.0, 24.0, 0.0),
    ("CMP LNK", "COMP", 0.0, 1.0, 1.0),
    ("CMP SCL", "COMP", 0.0, 1.0, 1.0),
]


def clamp(v, lo, hi):
    return min(max(v, lo), hi)


def reset_row(row, vdef):
    # fxSet clamps to the page range; DLY TIM default 0 clamps up to vmin.
    return clamp(vdef, row[2], row[3])


def check_count_and_pages():
    from collections import Counter
    assert len(PARAMS) == 36, len(PARAMS)
    counts = Counter(p[1] for p in PARAMS)
    assert counts == {"DELAY": 7, "REVERB": 7, "EQ": 13, "COMP": 9}, counts
    print("36 params / 5 pages (DELAY 7, REVERB 7, EQ 13, COMP 9) OK")


def check_legacy_defaults():
    # The vdef column must reproduce FxEngine's AllParamsAtLegacyDefault state:
    # delay time 0/fbk 0/mix 1/wid 1/flags 0/bypass 0
    assert reset_row(PARAMS[0], PARAMS[0][4]) == 10.0  # page floor clamps 0
    assert PARAMS[1][4] == 0.0
    assert PARAMS[2][4] == 1.0
    assert PARAMS[3][4] == 1.0
    assert PARAMS[4][4] == 0.0
    assert PARAMS[5][4] == 0.0
    assert PARAMS[6][4] == 0.0
    # reverb pre 0/dec 1/siz 1/dmp 0.5/wid 1/mode 0/bypass 0 (RC2: no MIX row)
    assert PARAMS[7][4] == 0.0
    assert PARAMS[8][4] == 1.0
    assert PARAMS[9][4] == 1.0
    assert PARAMS[10][4] == 0.5
    assert PARAMS[11][4] == 1.0
    assert PARAMS[12][4] == 0.0
    assert PARAMS[13][4] == 0.0
    # EQ bypass on (dry) + bands off at 100/1000/10000, gain 0, Q 1
    assert PARAMS[14][4] == 1.0
    for i in range(15, 27):
        assert PARAMS[i][4] in (0.0, 1.0, 100.0, 1000.0, 10000.0)
    # comp (Fase 13 order, BYP first): bypass on, thr -24 ratio 4 knee 6
    # attack 15 release 200 make-up 0, link+softclip on
    assert PARAMS[27][4] == 1.0   # CMP BYP
    assert PARAMS[28][4] == -24.0  # CMP THR
    assert PARAMS[29][4] == 4.0   # CMP RAT
    assert PARAMS[30][4] == 6.0   # CMP KNE
    assert PARAMS[31][4] == 15.0  # CMP ATK
    assert PARAMS[32][4] == 200.0  # CMP REL
    assert PARAMS[33][4] == 0.0   # CMP MKU
    assert PARAMS[34][4] == 1.0   # CMP LNK
    assert PARAMS[35][4] == 1.0   # CMP SCL
    print("legacy defaults match AllParamsAtLegacyDefault OK")


def check_reset_is_idempotent():
    # Reset of an already-default row is a no-op (value unchanged after clamp).
    for row in PARAMS:
        first = reset_row(row, row[4])
        second = reset_row(row, first)
        assert second == first, row
    print("A+B reset idempotent across all rows OK")


def check_vdef_in_range_semantics():
    # Every vdef is either inside [vmin,vmax] or equal to the legacy default
    # that the page floor clamps up to (DLY TIM only).
    for row in PARAMS:
        vdef = row[4]
        if vdef < row[2]:
            assert row[0] == "DLY TIM", row  # only page-floor clamp allowed
    print("vdef range semantics OK")


def check_src_guards():
    mv = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
    mh = (ROOT / "source/sources/Application/Views/MixerView.h").read_text()
    iv = (ROOT / "source/sources/Application/Views/InstrumentView.cpp").read_text()
    for token in ("fxResetRow", "vdef", "FX_PAGE_EQ", "FX_PAGE_COMP"):
        assert token in mv, token
    # F3-4a: the table rows moved to FxPages.h.
    fxp = (ROOT / "source/sources/Application/Mixer/FxPages.h").read_text()
    assert "CMP BYP" in fxp
    assert "fxResetRow" in mh
    for token in ("TREEFROG_FX_NAV_A_B_DEFAULT_V1",
                  "v.Reset()", "cutInstrument", "EPBM_L2"):
        assert token in iv, token
    # The old B+A cut block must be gone; L2+A cut must exist.
    assert "Allow cut instrument" not in iv
    print("source guards OK")


check_count_and_pages()
check_legacy_defaults()
check_reset_is_idempotent()
check_vdef_in_range_semantics()
check_src_guards()
print("FX_NAV_AB_DEFAULT_PHASE6_OK")
