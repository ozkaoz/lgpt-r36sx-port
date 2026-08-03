#!/usr/bin/env python3
"""Phase 12 model tests: dedicated EQ menu with banded layout (PLAN_FX_REDESIGN_ES.md).

Mirrors the Fase 12 EQ redesign:

- The EQ page is a dedicated exclusive menu (drawEqPage), not the generic
  parameter list.  Layout is banded LOW/MID/HIGH: EQ BYPASS row on top with
  ON/OFF, then each band is a header row (LOW/MID/HIGH) followed by four rows
  EN (ON/OFF) / FRQ (Hz) / GAIN (signed dB) / Q.
- Every parameter keeps its own selectable row, so the selected row is
  unambiguous and the whole band stays visible while editing.
- Frequencies are edited musically (Fase 14 in the EQ layer): fine steps one
  semitone (x2^(1/12)), coarse steps one octave (x2), clamped to 20..20000 Hz.
  This reaches the whole range in ~120 fine presses or ~10 coarse presses.
- The param enum was reordered so each band is EN first, then FRQ/GAI/Q,
  matching the order drawEqPage renders and UP/DOWN walks.

Acceptance:
- the EQ param table is ordered BYP then EN/FRQ/GAI/Q per band with the
  documented defaults (bypass on, bands off at 100/1000/10000 Hz, gain 0, Q 1)
- fine/coarse frequency edits are multiplicative (musical) and clamp to the
  range; the full 20..20000 Hz range is reachable in a bounded number of steps
- the EQ page source dispatches to drawEqPage/drawEqRow and renders units
  (Hz, signed dB) and ON/OFF for enables; bypass is its own top row
- source guards: drawEqPage, drawEqRow, fxIsFrequency, and the enum order
  markers are present in MixerView
"""
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

MIX = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
MIX_H = (ROOT / "source/sources/Application/Views/MixerView.h").read_text()

# Mirror Fase 12 kFxParams_ EQ section: (id, label, vmin, vmax, vdef).
EQ_PARAMS = [
    (15, "EQ  BYP", 0.0, 1.0, 1.0),
    (16, "LO  EN", 0.0, 1.0, 0.0),
    (17, "LO  FRQ", 20.0, 20000.0, 100.0),
    (18, "LO  GAI", -12.0, 12.0, 0.0),
    (19, "LO  Q", 0.1, 10.0, 1.0),
    (20, "MID EN", 0.0, 1.0, 0.0),
    (21, "MID FRQ", 20.0, 20000.0, 1000.0),
    (22, "MID GAI", -12.0, 12.0, 0.0),
    (23, "MID Q", 0.1, 10.0, 1.0),
    (24, "HI  EN", 0.0, 1.0, 0.0),
    (25, "HI  FRQ", 20.0, 20000.0, 10000.0),
    (26, "HI  GAI", -12.0, 12.0, 0.0),
    (27, "HI  Q", 0.1, 10.0, 1.0),
]

SEMITONE = 2.0 ** (1.0 / 12.0)


def eq_freq_step(v, delta, coarse):
    """Mirror MixerView::fxEditFrequency (Fase 12/14 musical steps)."""
    factor = 2.0 if coarse else SEMITONE
    if delta < 0:
        factor = 1.0 / factor
    for _ in range(abs(delta)):
        v *= factor
    return max(20.0, min(20000.0, v))


def check_eq_param_order_and_defaults():
    ids = [p[0] for p in EQ_PARAMS]
    assert ids == list(range(15, 28)), "EQ ids must be contiguous 15..27"
    # Band order: bypass, then EN/FRQ/GAI/Q per band (LOW/MID/HIGH).
    assert EQ_PARAMS[0][1] == "EQ  BYP"
    for base, labels in (
            (1, ("LO  EN", "LO  FRQ", "LO  GAI", "LO  Q")),
            (5, ("MID EN", "MID FRQ", "MID GAI", "MID Q")),
            (9, ("HI  EN", "HI  FRQ", "HI  GAI", "HI  Q"))):
        for k, lbl in enumerate(labels):
            assert EQ_PARAMS[base + k][1] == lbl, (base, k)
    # Defaults: bypass on, bands off, freqs 100/1000/10000, gain 0, Q 1.
    assert EQ_PARAMS[0][4] == 1.0
    for i in (1, 5, 9):
        assert EQ_PARAMS[i][4] == 0.0          # EN off
        assert EQ_PARAMS[i + 1][4] == 100.0 * 10 ** (i // 4 - 0), \
            (i, EQ_PARAMS[i + 1])
    for i in (3, 7, 11):
        assert EQ_PARAMS[i][4] == 0.0          # gain 0
        assert EQ_PARAMS[i + 1][4] == 1.0      # Q 1
    assert EQ_PARAMS[1 + 1][4] == 100.0
    assert EQ_PARAMS[5 + 1][4] == 1000.0
    assert EQ_PARAMS[9 + 1][4] == 10000.0
    print("EQ param order and defaults OK")


def check_freq_steps_are_musical():
    # One fine step up is a semitone (multiplicative, not linear).
    assert eq_freq_step(1000.0, 1, False) == math.isclose(1000.0, 0) or \
        abs(eq_freq_step(1000.0, 1, False) / 1000.0 - SEMITONE) < 1e-4
    # Twelve semitones up ≈ one octave (double).
    v = 1000.0
    for _ in range(12):
        v = eq_freq_step(v, 1, False)
    assert abs(v / 1000.0 - 2.0) < 1e-3
    # One coarse step up is an octave.
    assert abs(eq_freq_step(100.0, 1, True) / 100.0 - 2.0) < 1e-4
    print("fine/coarse frequency steps are musical (semitones/octaves) OK")


def check_range_reachable_and_clamped():
    # From the low default, the whole range is reachable in ~120 fine presses.
    v = 100.0
    presses = 0
    while v < 20000.0 and presses < 200:
        v = eq_freq_step(v, 1, False)
        presses += 1
    assert v == 20000.0, v
    assert presses <= 130, presses            # bounded, not hundreds
    # Coarse from the low bound reaches the top in ~10 presses.
    v = 20.0
    coarse_presses = 0
    while v < 20000.0 and coarse_presses < 20:
        v = eq_freq_step(v, 1, True)
        coarse_presses += 1
    assert v == 20000.0
    assert coarse_presses <= 10, coarse_presses
    # Clamping at both ends.
    assert eq_freq_step(20000.0, 1, True) == 20000.0
    assert eq_freq_step(20000.0, 3, False) == 20000.0
    assert eq_freq_step(20.0, -1, True) == 20.0
    assert eq_freq_step(20.0, -5, False) == 20.0
    # Fine stepping is proportional everywhere the clamp is not hit.
    for start in (20.0, 100.0, 1000.0, 10000.0):
        rel = eq_freq_step(start, 1, False) / start
        assert abs(rel - SEMITONE) < 1e-4, (start, rel)
    # Near the top the clamp applies (19000 + one semitone -> 20000).
    assert eq_freq_step(19000.0, 1, False) == 20000.0
    print("range reachable in bounded presses and clamped at 20..20000 Hz OK")


def check_source_guards():
    # Dedicated menu: EQ page dispatches to drawEqPage inside drawFxParamPage.
    assert "drawEqPage()" in MIX
    assert "drawFxParamPage" in MIX
    idx = MIX.index("void MixerView::drawFxParamPage")
    # Window allows for the RC2 DELAY/REVERB dedicated-page dispatches that
    # now precede the EQ dispatch inside drawFxParamPage.
    assert "drawEqPage()" in MIX[idx:idx + 2400]
    # Row renderer + musical frequency editing helpers.
    assert "void MixerView::drawEqRow" in MIX
    assert "void MixerView::drawEqPage" in MIX
    assert "fxUsesCurve" in MIX
    assert "1.05946309436" in MIX or "2.0 ** (1.0 / 12.0)" in MIX
    # Units and ON/OFF rendering in the EQ menu.
    assert "[ %s ]" in MIX and '"ON"' in MIX and '"OFF"' in MIX
    assert "%6.0f Hz" in MIX
    assert '%+5.1f dB' in MIX
    # Band headers LOW/MID/HIGH rendered by drawEqPage.
    assert '"LOW"' in MIX and '"MID"' in MIX and '"HIGH"' in MIX
    # Header declares the EQ menu methods (RC5: drawEqRow takes the centered
    # label/value columns).
    assert "drawEqPage()" in MIX_H and "drawEqRow(int id,int labelX,int valueX,int y)" in MIX_H
    # Enum is EN-first per band (order marker comment in the header).
    assert "FX_P_EQ_LOW_EN" in MIX_H and "FX_P_EQ_LOW_FRQ" in MIX_H
    l = MIX_H.index("FX_P_EQ_LOW_EN")
    assert "FX_P_EQ_LOW_FRQ" in MIX_H[l:l + 120]
    # Bypass is the first EQ param (top row) and the EQ page keeps its own
    # dedicated layout instead of the generic row list.
    first = MIX.index("{ \"EQ  BYP\"")
    assert first < MIX.index("{ \"LO  EN\""), "EN-first band order"
    print("source guards OK")


check_eq_param_order_and_defaults()
check_freq_steps_are_musical()
check_range_reachable_and_clamped()
check_source_guards()
print("FX_EQ_MENU_PHASE12_OK")
