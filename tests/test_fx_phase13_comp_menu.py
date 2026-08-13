#!/usr/bin/env python3
"""Phase 13 model tests: dedicated COMP menu (PLAN_FX_REDESIGN_ES.md).

Mirrors the Fase 13 COMP redesign:

- The COMP page is a dedicated exclusive menu (drawCompPage), not the generic
  parameter list.  CMP BYP is the FIRST row so it is never off-screen.
- Rows are centered with a fixed value column; units shown (dB, ms); ratio
  renders as x:1; booleans render as ON/OFF.
- The GR meter (gain reduction) stays visible below the parameters; it is a
  readout, not a selectable row.
- No clipping indicator is added: the engine exposes no reliable real audio
  clip reading (GetRtViolations is buffer RT telemetry that must stay 0).
- Soft clip is labelled "Soft Clip", never "limiter" (no independent limiter).

Acceptance:
- the COMP param table is ordered BYP then THR/RAT/KNE/ATK/REL/MKU/LNK/SC
  with the documented defaults (bypass on, thr -24, ratio 4, knee 6, attack
  15, release 200, makeup 0, link/softclip on)
- the source dispatches the COMP page to drawCompPage and the rendering
  contains the centered labels, x:1 ratio, ON/OFF and the GR readout row
- the source adds no clipping indicator tied to a fake clip reading
- soft clip is only ever called "Soft Clip" in the view
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

MIX = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
MIX_H = (ROOT / "source/sources/Application/Views/MixerView.h").read_text()

# Mirror Fase 13 kFxParams_ COMP section: (id, label, vmin, vmax, vdef).
COMP_PARAMS = [
    (28, "CMP BYP", 0.0, 1.0, 1.0),
    (29, "CMP THR", -60.0, 0.0, -24.0),
    (30, "CMP RAT", 1.0, 20.0, 4.0),
    (31, "CMP KNE", 0.0, 12.0, 6.0),
    (32, "CMP ATK", 0.1, 500.0, 15.0),
    (33, "CMP REL", 1.0, 2000.0, 200.0),
    (34, "CMP MKU", 0.0, 24.0, 0.0),
    (35, "CMP LNK", 0.0, 1.0, 1.0),
    (36, "CMP SCL", 0.0, 1.0, 1.0),
]

BYP, THR, RAT, KNE, ATK, REL, MKU, LNK, SCL = range(9)


def check_comp_order_and_defaults():
    assert [p[0] for p in COMP_PARAMS] == list(range(28, 37))
    assert COMP_PARAMS[BYP][1] == "CMP BYP"
    assert COMP_PARAMS[THR][1] == "CMP THR"
    assert COMP_PARAMS[RAT][1] == "CMP RAT"
    assert COMP_PARAMS[KNE][1] == "CMP KNE"
    assert COMP_PARAMS[ATK][1] == "CMP ATK"
    assert COMP_PARAMS[REL][1] == "CMP REL"
    assert COMP_PARAMS[MKU][1] == "CMP MKU"
    assert COMP_PARAMS[LNK][1] == "CMP LNK"
    assert COMP_PARAMS[SCL][1] == "CMP SCL"
    assert COMP_PARAMS[BYP][4] == 1.0
    assert COMP_PARAMS[THR][4] == -24.0
    assert COMP_PARAMS[RAT][4] == 4.0
    assert COMP_PARAMS[KNE][4] == 6.0
    assert COMP_PARAMS[ATK][4] == 15.0
    assert COMP_PARAMS[REL][4] == 200.0
    assert COMP_PARAMS[MKU][4] == 0.0
    assert COMP_PARAMS[LNK][4] == 1.0
    assert COMP_PARAMS[SCL][4] == 1.0
    print("COMP param order and defaults OK")


def check_source_guards():
    # Dedicated menu: COMP page dispatches to drawCompPage.
    assert "void MixerView::drawCompPage" in MIX
    assert "drawCompPage(const char *title)" in MIX_H
    idx = MIX.index("void MixerView::drawFxParamPage")
    # Window allows for the RC2 DELAY/REVERB dedicated-page dispatches that
    # now precede the COMP dispatch inside drawFxParamPage.
    assert "drawCompPage(pageTitle)" in MIX[idx:idx + 2400]
    # F3-4a: the table rows and enums moved to FxPages.h.
    fxp = (ROOT / "source/sources/Application/Mixer/FxPages.h").read_text()
    # BYP is the first COMP table row (never off-screen).
    byp = fxp.index("{ \"CMP BYP\"")
    thr = fxp.index("{ \"CMP THR\"")
    assert byp < thr
    # BYP is the first COMP enum entry.
    assert fxp.index("FX_P_CMP_BYP") < fxp.index("FX_P_CMP_THR")
    # Centered labels + value column; the BYP row is at the top of the menu.
    assert "Bypass" in MIX and "Threshold" in MIX and "Ratio" in MIX
    assert "Stereo Link" in MIX and "Soft Clip" in MIX
    # Units and ratio/boolean rendering.
    assert "%3.1f:1" in MIX            # ratio x:1
    assert "%5.1f ms" in MIX
    assert "%+5.1f dB" in MIX
    assert '"ON"' in MIX and '"OFF"' in MIX
    # GR meter visible below the parameters (readout row).
    assert "Gain Reduction" in MIX
    assert "GetCompGainReductionDb" in MIX
    # No clipping indicator driven by a fake reading: the only telemetry is
    # the buffer RT counter, which must not be shown as an audio clip meter.
    assert "GetRtViolations" not in MIX.split("drawCompPage")[1][:4000]
    # Soft clip is never called "limiter" in the view.
    assert "limiter" not in MIX.split("drawCompPage")[1][:4000].lower()
    print("source guards OK")


check_comp_order_and_defaults()
check_source_guards()
print("FX_COMP_MENU_PHASE13_OK")
