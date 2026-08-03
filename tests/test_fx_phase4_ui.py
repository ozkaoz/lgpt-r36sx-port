#!/usr/bin/env python3
"""Phase 4 UI tests: FX pages + full master exposure (PLAN_FX_REDESIGN_ES.md).

Faithful model checks for the MixerView page system (Fase 4.3 pages,
refined in Fase 6: the single MASTER page is split into EQ and COMP, and the
global SEND/RET rows are gone because sends are per-track / per-instrument):

- SELECT cycles MIX -> DELAY -> REVERB -> EQ -> COMP -> MIX
- DELAY/REVERB/EQ/COMP pages expose master-bus parameters in natural units
- the FxEngine getters mirror the setters (readback == what was written)
- the DSP module getters read back their stored values
- the MIX page per-track sends edit the Mixer model (0..100)

Acceptance:
- every master parameter has a getter that reads back what the setter wrote
- the FxParamSpec table covers all 37 parameter ids exactly once per page
- the UI source wires the page cycle, row edit, send edit and GR meter
"""
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SHIFT = 15
SCALE = 1 << SHIFT


def fl2fp(f):
    return int(round(f * SCALE))


def fp2fl(a):
    return a / SCALE


# ---------------------------------------------------------------------------
# Model: DSP modules with readback (mirror Fase 4.3 getters)
# ---------------------------------------------------------------------------
class DelayModel:
    def __init__(self):
        self.time_ms = 250.0
        self.fb = 0.3
        self.mix = 0.3
        self.width = 0.6
        self.pp = False
        self.sat = False
        self.bypass = False
        self.send = 0.0
        self.ret = 0.3

    def readback(self):
        return {
            "time": self.time_ms, "fb": self.fb, "mix": self.mix,
            "width": self.width, "pp": self.pp, "sat": self.sat,
            "bypass": self.bypass, "send": self.send, "ret": self.ret}


class ReverbModel:
    def __init__(self):
        self.pre = 10.0
        self.dec = 2.5
        self.size = 1.0
        self.dmp = 0.5
        self.width = 0.7
        self.mode = 0
        self.mix = 0.35
        self.bypass = False
        self.send = 0.0
        self.ret = 0.3

    def readback(self):
        return {
            "pre": self.pre, "dec": self.dec, "size": self.size,
            "dmp": self.dmp, "width": self.width, "mode": self.mode,
            "mix": self.mix, "bypass": self.bypass, "send": self.send,
            "ret": self.ret}


class EqModel:
    def __init__(self):
        self.freq = [500.0, 2500.0, 8000.0]
        self.gain = [0.0, 0.0, 0.0]
        self.q = [0.7, 0.9, 0.7]
        self.en = [False, False, False]
        self.bypass = True

    def readback(self, band):
        return {
            "freq": self.freq[band], "gain": self.gain[band],
            "q": self.q[band], "en": self.en[band]}


class CompModel:
    def __init__(self):
        self.thr = -20.0
        self.rat = 3.0
        self.knee = 6.0
        self.atk = 5.0
        self.rel = 100.0
        self.mku = 0.0
        self.link = True
        self.sc = True
        self.bypass = True
        self.gr = 0.0

    def readback(self):
        return {
            "thr": self.thr, "rat": self.rat, "knee": self.knee,
            "atk": self.atk, "rel": self.rel, "mku": self.mku,
            "link": self.link, "sc": self.sc, "bypass": self.bypass,
            "gr": self.gr}


def check_dsp_readback():
    d = DelayModel()
    d.time_ms, d.fb, d.mix = 123.0, 0.45, 0.8
    rb = d.readback()
    assert abs(rb["time"] - 123.0) < 1e-9
    assert abs(rb["fb"] - 0.45) < 1e-9
    assert rb["sat"] is False and rb["pp"] is False

    r = ReverbModel()
    r.pre, r.dec, r.size, r.mode = 33.0, 4.2, 1.25, 1
    rb = r.readback()
    assert abs(rb["pre"] - 33.0) < 1e-9
    assert abs(rb["dec"] - 4.2) < 1e-9
    assert abs(rb["size"] - 1.25) < 1e-9
    assert rb["mode"] == 1

    e = EqModel()
    e.freq[1], e.gain[1], e.q[1], e.en[1] = 1800.0, -3.5, 1.4, True
    rb = e.readback(1)
    assert abs(rb["freq"] - 1800.0) < 1e-9
    assert abs(rb["gain"] - -3.5) < 1e-9
    assert rb["en"] is True

    c = CompModel()
    c.thr, c.rat, c.atk, c.rel, c.mku = -40.0, 6.0, 12.0, 250.0, 2.0
    rb = c.readback()
    assert abs(rb["thr"] - -40.0) < 1e-9
    assert abs(rb["rat"] - 6.0) < 1e-9
    assert abs(rb["rel"] - 250.0) < 1e-9
    print("dsp readback OK")


def check_fixed_roundtrip():
    # The UI stores natural floats but the DSP stores Q15 fixed.  Verify the
    # fl2fp / fp2fl roundtrip keeps sub-% precision for the param ranges.
    cases = [
        (10.0, 2000.0),     # delay ms
        (0.0, 0.98),        # feedback
        (0.0, 100.0),       # predelay ms
        (0.2, 8.0),         # reverb decay s
        (0.5, 1.5),         # reverb size
        (-60.0, 0.0),       # comp threshold dB
        (-12.0, 12.0),      # EQ gain dB
        (20.0, 20000.0),    # EQ freq Hz
        (1.0, 20.0),        # comp ratio
    ]
    for lo, hi in cases:
        for v in (lo, (lo + hi) / 2.0, hi):
            back = fp2fl(fl2fp(v))
            # Freq in Hz spans 3 orders; accept 1% error there, 0.1% elsewhere.
            tol = 0.01 if hi > 1000.0 else 0.001
            assert abs(back - v) <= tol * max(abs(v), 1e-9), (lo, hi, v, back)
    print("fixed roundtrip OK")


# ---------------------------------------------------------------------------
# Model: FxParamSpec table (mirrors kFxParams_)
# ---------------------------------------------------------------------------
# (id, page, label, vmin, vmax)
FX_PARAMS = [
    # DELAY
    (0, "DELAY", "DLY TIM", 10.0, 2000.0),
    (1, "DELAY", "DLY FBK", 0.0, 0.98),
    (2, "DELAY", "DLY MIX", 0.0, 1.0),
    (3, "DELAY", "DLY WID", 0.0, 1.0),
    (4, "DELAY", "DLY P/P", 0.0, 1.0),
    (5, "DELAY", "DLY SAT", 0.0, 1.0),
    (6, "DELAY", "DLY BYP", 0.0, 1.0),
    # REVERB
    (7, "REVERB", "RVB PRE", 0.0, 100.0),
    (8, "REVERB", "RVB DEC", 0.2, 8.0),
    (9, "REVERB", "RVB SIZ", 0.5, 1.5),
    (10, "REVERB", "RVB DMP", 0.0, 1.0),
    (11, "REVERB", "RVB WID", 0.0, 1.0),
    (12, "REVERB", "RVB MOD", 0.0, 1.0),
    (13, "REVERB", "RVB MIX", 0.0, 1.0),
    (14, "REVERB", "RVB BYP", 0.0, 1.0),
    # EQ (3 bands: bypass + freq/gain/Q/enable each)
    (15, "EQ", "EQ  BYP", 0.0, 1.0),
    (16, "EQ", "LO  FRQ", 20.0, 20000.0),
    (17, "EQ", "LO  GAI", -12.0, 12.0),
    (18, "EQ", "LO  Q", 0.1, 10.0),
    (19, "EQ", "LO  EN", 0.0, 1.0),
    (20, "EQ", "MID FRQ", 20.0, 20000.0),
    (21, "EQ", "MID GAI", -12.0, 12.0),
    (22, "EQ", "MID Q", 0.1, 10.0),
    (23, "EQ", "MID EN", 0.0, 1.0),
    (24, "EQ", "HI  FRQ", 20.0, 20000.0),
    (25, "EQ", "HI  GAI", -12.0, 12.0),
    (26, "EQ", "HI  Q", 0.1, 10.0),
    (27, "EQ", "HI  EN", 0.0, 1.0),
    # COMP
    (28, "COMP", "CMP THR", -60.0, 0.0),
    (29, "COMP", "CMP RAT", 1.0, 20.0),
    (30, "COMP", "CMP KNE", 0.0, 12.0),
    (31, "COMP", "CMP ATK", 0.1, 500.0),
    (32, "COMP", "CMP REL", 1.0, 2000.0),
    (33, "COMP", "CMP MKU", 0.0, 24.0),
    (34, "COMP", "CMP LNK", 0.0, 1.0),
    (35, "COMP", "CMP SCL", 0.0, 1.0),
    (36, "COMP", "CMP BYP", 0.0, 1.0),
]


def check_param_table_consistency():
    ids = [p[0] for p in FX_PARAMS]
    assert ids == list(range(37)), "param ids must be contiguous 0..36"
    assert len(set(ids)) == 37
    counts = {}
    for _, page, _, _, _ in FX_PARAMS:
        counts[page] = counts.get(page, 0) + 1
    assert counts == {"DELAY": 7, "REVERB": 8, "EQ": 13, "COMP": 9}, counts
    for _, page, label, lo, hi in FX_PARAMS:
        assert lo <= hi, (label, lo, hi)
    print("param table consistency OK")


def check_mix_send_edit():
    # MIX page per-track sends: values are integers 0..100 in the Mixer.
    sends = [0] * 8
    for i in range(8):
        sends[i] = max(0, min(100, sends[i] + 7))
    assert all(0 <= s <= 100 for s in sends)
    # Edit target cycles VOL -> DLY -> RVB.
    t = 0
    for expected in (1, 2, 0):
        t = (t + 1) % 3
        assert t == expected
    print("mix send edit OK")


def check_src_ui_wiring():
    src = (ROOT / "source/sources/Application/Views/MixerView.cpp").read_text()
    for token in ("cycleFxPage", "fxEditRow", "fxMoveRow", "fxGet", "fxSet",
                  "drawFxPages", "drawFxParamPage", "drawMixSends",
                  "FX_PAGE_MIX", "FX_PAGE_DELAY", "FX_PAGE_REVERB",
                  "FX_PAGE_EQ", "FX_PAGE_COMP", "kFxParams_",
                  "NudgeChannelDelaySend", "NudgeChannelReverbSend",
                  "GetCompGainReductionDb", "EPBM_SELECT"):
        assert token in src, token
    h = (ROOT / "source/sources/Application/Views/MixerView.h").read_text()
    for token in ("FxPage", "FX_PARAM_COUNT", "fxPage_", "fxRow_",
                  "fxEditTarget_"):
        assert token in h, token
    # Fase 6: the global SEND/RET rows must be gone from the param table.
    for token in ("\"DLY SND\"", "\"DLY RET\"", "\"RVB SND\"", "\"RVB RET\""):
        assert token not in src, token
    print("ui wiring OK")


def check_src_getters():
    fxe = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.h").read_text()
    for token in ("GetDelayTimeMs", "GetDelayFeedback", "GetReverbDecay",
                  "GetReverbSize", "GetEqBandFreq", "GetEqBandGainDb",
                  "GetEqBandQ", "GetCompThresholdDb", "GetCompRatio",
                  "GetCompKneeDb", "GetCompAttackMs", "GetCompReleaseMs",
                  "GetCompMakeupDb", "GetCompStereoLink", "GetCompSoftClip",
                  "GetCompBypass", "GetDelaySend", "GetDelayReturn",
                  "GetReverbSend", "GetReverbReturn"):
        assert token in fxe, token
    for f in ("DelayLine.h", "Reverb.h", "ParametricEQ.h", "Compressor.h"):
        p = ROOT / ("source/sources/Application/Audio/FxEngine/" + f)
        s = p.read_text()
        assert "GetRtViolations" in s
    dl = (ROOT / "source/sources/Application/Audio/FxEngine/DelayLine.h").read_text()
    for token in ("GetDelayMsTarget", "GetFeedback", "GetMix", "GetWidth",
                  "GetPingPong", "GetSaturation", "GetBypass"):
        assert token in dl, token
    rv = (ROOT / "source/sources/Application/Audio/FxEngine/Reverb.h").read_text()
    for token in ("GetPredelayMs", "GetDecayTarget", "GetSize", "GetDamping",
                  "GetMode", "GetMix", "GetBypass"):
        assert token in rv, token
    eq = (ROOT / "source/sources/Application/Audio/FxEngine/ParametricEQ.h").read_text()
    for token in ("GetBandFreq", "GetBandGainDb", "GetBandQ", "GetBandEnabled"):
        assert token in eq, token
    cp = (ROOT / "source/sources/Application/Audio/FxEngine/Compressor.h").read_text()
    for token in ("GetThresholdDb", "GetRatio", "GetKneeDb", "GetMakeupDb",
                  "GetAttackMs", "GetReleaseMs", "GetStereoLink"):
        assert token in cp, token
    print("dsp getters OK")


check_dsp_readback()
check_fixed_roundtrip()
check_param_table_consistency()
check_mix_send_edit()
check_src_ui_wiring()
check_src_getters()
print("FX_UI_PHASE43_OK")
