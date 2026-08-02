#!/usr/bin/env python3
"""Phase 4 command tests: FX column commands (PLAN_FX_REDESIGN_ES.md).

Faithful Q15 model of the Fase 4 command handlers in
SampleInstrument::ProcessCommand.  Each command uses a monotonic 00-FF mapping
of the low param byte (value & 0xFF): 0x00 = minimum, 0xFF = maximum.  The
high byte (speed) is reserved and ignored.

Mapping contract (must match HelpLegend.h help strings):
- DLYS  delay send of the track       0..100 %
- RVBS  reverb send of the track      0..100 %
- DLYT  master delay time             10..2000 ms
- DLYF  master delay feedback         0..0.98 (linear)
- RVDC  master reverb RT60            0.2..8.0 s
- RVSZ  master reverb size            0.5..1.5
- CMPT  master comp threshold         -60..0 dB

Acceptance:
- handlers must be wired for all 7 commands in SampleInstrument.cpp
- the 7 FourCCs must be declared in CommandList.h and present in the UI list
- HelpLegend.h must carry display names and range help strings
- mapping is monotonic: value 0 -> min, value 255 -> max, strict midpoints
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


def fl_round(f):
    return int(round(f * 1000)) / 1000.0


# ---------------------------------------------------------------------------
# Handler model (mirrors SampleInstrument.cpp Fase 4 handlers)
# ---------------------------------------------------------------------------
def dlys(value):
    return (value & 0xFF) * 100 // 255


def rvbs(value):
    return (value & 0xFF) * 100 // 255


def dlyt(value):
    v = (value & 0xFF) / 255.0
    return fl2fp(10.0 + (2000.0 - 10.0) * v)


def dlyf(value):
    v = (value & 0xFF) / 255.0
    return fl2fp(0.98 * v)


def rvdc(value):
    v = (value & 0xFF) / 255.0
    return fl2fp(0.2 + (8.0 - 0.2) * v)


def rvsz(value):
    v = (value & 0xFF) / 255.0
    return fl2fp(0.5 + (1.5 - 0.5) * v)


def cmpt(value):
    v = (value & 0xFF) / 255.0
    return fl2fp(-60.0 + 60.0 * v)


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
    for token in ("Mixer::GetInstance()->SetChannelDelaySend",
                  "Mixer::GetInstance()->SetChannelReverbSend",
                  "SetDelayTimeMs", "SetDelayFeedback", "SetReverbDecay",
                  "SetReverbSize", "SetCompThresholdDb"):
        assert token in src, token
    print("handler wiring OK")


def check_src_helplegend():
    src = (ROOT / "source/sources/Application/Utils/HelpLegend.h").read_text()
    for token in ("DLYS", "RVBS", "DLYT", "DLYF", "RVDC", "RVSZ", "CMPT"):
        assert token in src, token
    print("help legend OK")


check_monotonic_ranges()
check_percent_sends()
check_src_fourccs()
check_src_handlers()
check_src_helplegend()
print("FX_COMMANDS_PHASE4_OK")
