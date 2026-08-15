#!/usr/bin/env python3
"""Phase 4 command tests: FX column commands (PLAN_FX_REDESIGN_ES.md).

Faithful Q15 model of the Fase 4 command handlers in
SampleInstrument::ProcessCommand (bacon-1.5 item 5: routed through the
UNIFIED API FxEngine::SetParam + fxParamFromByte).  Each command uses a
monotonic 00-FF mapping of the low param byte (value & 0xFF): 0x00 =
minimum, 0xFF = maximum.  The high byte (speed) is reserved and ignored.

Mapping contract (must match HelpLegend.h help strings):
- DLYS  delay send of the track       0..100 %
- RVBS  reverb send of the track      0..100 %
- DLYT  master delay time             10..2000 ms (LOG2 curve, same as UI)
- DLYF  master delay feedback         0..0.98 (linear, same as UI)
- RVDC  master reverb RT60            0.2..8.0 s (LOG2 curve, same as UI)
- RVSZ  master reverb size            0.5..1.5 (linear, same as UI)
- CMPT  master comp threshold         -60..0 dB (linear, same as UI)

Item 5 unifies phrase automation and the Mixer UI: the byte converts to a
percent (0x00=0%, 0xFF=100%) and then to the natural value with the SAME
descriptor curve the UI uses (LOG2 for time/frequency, LINEAR for gains),
so byte 0x80 and UI 50 % produce the same value.

Acceptance:
- handlers must be wired for all 7 commands in SampleInstrument.cpp
- the 7 FourCCs must be declared in CommandList.h and present in the UI list
- HelpLegend.h must carry display names and range help strings
- mapping is monotonic: value 0 -> min, value 255 -> max
- the master commands route through SetParam + fxParamFromByte
"""
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SHIFT = 15
SCALE = 1 << SHIFT


def i2fp(a):
    return a << SHIFT


def fl2fp(f):
    return int(round(f * SCALE))


def fp2fl(a):
    return a / SCALE


# ---------------------------------------------------------------------------
# Handler model (mirrors SampleInstrument.cpp item 5 handlers).  The master
# commands convert byte -> percent -> natural with the UI curve.
# ---------------------------------------------------------------------------
def dlys(value):
    return (value & 0xFF) * 100 // 255


def rvbs(value):
    return (value & 0xFF) * 100 // 255


def byte_to_percent(byte):
    return (byte * 100 + 127) // 255


def percent_to_dsp_linear(lo, hi, pct):
    return lo + (pct / 100.0) * (hi - lo)


def percent_to_dsp_log2(lo, hi, pct):
    return lo * (hi / lo) ** (pct / 100.0)


def dlyt(value):
    pct = byte_to_percent(value & 0xFF)
    return fl2fp(percent_to_dsp_log2(10.0, 2000.0, pct))


def dlyf(value):
    pct = byte_to_percent(value & 0xFF)
    return fl2fp(percent_to_dsp_linear(0.0, 0.98, pct))


def rvdc(value):
    pct = byte_to_percent(value & 0xFF)
    return fl2fp(percent_to_dsp_log2(0.2, 8.0, pct))


def rvsz(value):
    pct = byte_to_percent(value & 0xFF)
    return fl2fp(percent_to_dsp_linear(0.5, 1.5, pct))


def cmpt(value):
    pct = byte_to_percent(value & 0xFF)
    return fl2fp(percent_to_dsp_linear(-60.0, 0.0, pct))


HANDLERS = {
    "I_CMD_DLYS": dlys,
    "I_CMD_RVBS": rvbs,
    "I_CMD_DLYT": dlyt,
    "I_CMD_DLYF": dlyf,
    "I_CMD_RVDC": rvdc,
    "I_CMD_RVSZ": rvsz,
    "I_CMD_CMPT": cmpt,
}


def check_monotonic_ranges():
    # DLYS/RVBS are integer percent sends (0..100); the rest are Q15 fixed.
    fixed_bounds = {
        "DLYT": (10.0, 2000.0),
        "DLYF": (0.0, 0.98),
        "RVDC": (0.2, 8.0),
        "RVSZ": (0.5, 1.5),
        "CMPT": (-60.0, 0.0),
    }
    for name, (lo, hi) in fixed_bounds.items():
        v0 = HANDLERS["I_CMD_" + name](0)
        vf = HANDLERS["I_CMD_" + name](0xFF)
        f0, ff = fp2fl(v0), fp2fl(vf)
        assert abs(f0 - lo) < 0.02, (name, f0, lo)
        assert abs(ff - hi) < 0.02, (name, ff, hi)
        # monotonic across a coarse sweep
        prev = fp2fl(HANDLERS["I_CMD_" + name](0))
        for k in range(1, 256):
            cur = fp2fl(HANDLERS["I_CMD_" + name](k))
            assert cur >= prev - 1e-6, (name, k, prev, cur)
            prev = cur
        # high byte (speed) must be ignored
        assert HANDLERS["I_CMD_" + name](0x12FF) == HANDLERS["I_CMD_" + name](0xFF), name
    print("monotonic range mapping OK")


def check_ui_equivalence():
    # Item 5: byte 0x80 == UI 50 % for the LOG2 params (equal octaves).
    half = fp2fl(HANDLERS["I_CMD_DLYT"](0x80))
    assert abs(half - 10.0 * math.sqrt(2000.0 / 10.0)) < 0.5, half
    half = fp2fl(HANDLERS["I_CMD_RVDC"](0x80))
    assert abs(half - 0.2 * math.sqrt(8.0 / 0.2)) < 0.02, half
    # LOG2 params are NOT at the linear midpoint at 0x80.
    assert abs(fp2fl(HANDLERS["I_CMD_DLYT"](0x80)) - 1005.0) > 100.0
    # Linear params stay linear at 0x80.
    assert abs(fp2fl(HANDLERS["I_CMD_CMPT"](0x80)) - (-30.0)) < 0.05
    assert abs(fp2fl(HANDLERS["I_CMD_DLYF"](0x80)) - 0.49) < 0.005
    assert abs(fp2fl(HANDLERS["I_CMD_RVSZ"](0x80)) - 1.0) < 0.005
    print("UI equivalence (byte 0x80 == percent 50) OK")


def check_percent_sends():
    # DLYS/RVBS: integer 0..100, linear in the low byte.
    assert dlys(0) == 0
    assert dlys(255) == 100
    assert rvbs(128) == 50
    assert dlys(0x0001) == 0      # 1/255 * 100 -> 0 (truncation, still safe)
    assert 0 <= dlys(0x80) <= 100
    print("percent send mapping OK")


def check_src_fourccs():
    h = (ROOT / "source/sources/Application/Instruments/CommandList.h").read_text()
    cpp = (ROOT / "source/sources/Application/Instruments/CommandList.cpp").read_text()
    for cmd in ("I_CMD_DLYS", "I_CMD_RVBS", "I_CMD_DLYT", "I_CMD_DLYF",
                "I_CMD_RVDC", "I_CMD_RVSZ", "I_CMD_CMPT"):
        assert cmd in h, cmd
    for fourcc in ("DLYS", "RVBS", "DLYT", "DLYF", "RVDC", "RVSZ", "CMPT"):
        assert fourcc in cpp, fourcc
    print("commandlist fourccs OK")


def check_src_handlers():
    src = (ROOT / "source/sources/Application/Instruments/SampleInstrument.cpp").read_text()
    for cmd in ("I_CMD_DLYS", "I_CMD_RVBS", "I_CMD_DLYT", "I_CMD_DLYF",
                "I_CMD_RVDC", "I_CMD_RVSZ", "I_CMD_CMPT"):
        assert ("case %s:" % cmd) in src, cmd
    # TREEFROG_SEND_LIVE_V1 (Fase 15): DLYS/RVBS write the live per-channel
    # override (renderParams_[channel].*).
    for token in ("renderParams_[channel].dlySend_",
                  "renderParams_[channel].rvbSend_"):
        assert token in src, token
    # bacon-1.5 item 5: the five master commands route through the UNIFIED
    # API (FxEngine::SetParam + fxParamFromByte), never hardcoded maps.
    assert src.count("fxParamFromByte(") >= 5, "5 master commands use fxParamFromByte"
    for token in ("SetParam(\n\t\t\t\t    FX_P_DLY_TIME, fxParamFromByte(FX_P_DLY_TIME",
                  "SetParam(\n\t\t\t\t    FX_P_DLY_FBK, fxParamFromByte(FX_P_DLY_FBK",
                  "SetParam(\n\t\t\t\t    FX_P_RVB_DEC, fxParamFromByte(FX_P_RVB_DEC",
                  "SetParam(\n\t\t\t\t    FX_P_RVB_SIZ, fxParamFromByte(FX_P_RVB_SIZ",
                  "SetParam(\n\t\t\t\t    FX_P_CMP_THR, fxParamFromByte(FX_P_CMP_THR"):
        assert token in src, token
    # No hardcoded linear maps remain in the master cases.
    assert "SetDelayTimeMs(" not in src.split("I_CMD_DLYT:")[1].split("break ;")[0]
    print("handler wiring OK")


def check_src_helplegend():
    src = (ROOT / "source/sources/Application/Utils/HelpLegend.h").read_text()
    for token in ("DLYS", "RVBS", "DLYT", "DLYF", "RVDC", "RVSZ", "CMPT"):
        assert token in src, token
    assert "log curve" in src
    print("help legend OK")


def check_src_unified_api():
    # The unified API exists in FxEngine and MixerView delegates to it.
    fe = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.cpp").read_text()
    assert "void FxEngine::SetParam(int id, float v)" in fe
    assert "float FxEngine::GetParam(int id) const" in fe
    # MixerView::fxGet/fxSet are thin delegates (single conversion surface).
    mv = (ROOT / "source/sources/Application/UI/Views/MixerView.cpp").read_text()
    fxget = mv[mv.index("float MixerView::fxGet"):mv.index("void MixerView::fxSet")]
    assert "GetInstance().GetParam(id)" in fxget
    fxset = mv[mv.index("void MixerView::fxSet"):mv.index("void MixerView::drawFxParamRow")]
    assert "GetInstance().SetParam(id,v)" in fxset
    # fxParamFromByte lives in the pure page layer.
    fxp = (ROOT / "source/sources/Application/Mixer/FxPages.h").read_text()
    assert "inline float fxParamFromByte(int id, int byte)" in fxp
    print("unified API wiring OK")


check_monotonic_ranges()
check_ui_equivalence()
check_percent_sends()
check_src_fourccs()
check_src_handlers()
check_src_helplegend()
check_src_unified_api()
print("FX_COMMANDS_PHASE4_OK")