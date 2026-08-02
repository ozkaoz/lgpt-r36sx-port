#!/usr/bin/env python3
"""Phase 5 model tests: FX persistence + legacy auto-engage.

Faithful Python model of the Fase 5 architecture (PLAN_FX_REDESIGN_ES.md):

- FxEngine starts in legacy bypass (bit-identical to the original LGPT).
  Editing ANY master FX parameter, or raising any per-track send, flips
  legacyMode_ off so the DSP actually runs (auto-engage).  Returning every
  parameter to its legacy default AND clearing all sends re-engages bypass.
- The FxEngine master parameters (delay/reverb/EQ/compressor) persist in the
  MIXER XML block as an FXMASTER element.  Old project files without FXMASTER
  restore to the legacy default state (bypass, FX off).

Acceptance:
- default state is legacy bypass
- editing a master param (delay time, reverb decay, EQ band, comp thr...) engages
- editing a per-track send engages; clearing all sends can re-engage legacy
- returning every master param to default re-engages legacy
- FXMASTER round-trips all 41 params exactly (Q15)
- legacy project without FXMASTER restores to defaults = legacy
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SHIFT = 15
SCALE = 1 << SHIFT


def i2fp(a):
    return a << SHIFT


def fl2fp(f):
    return int(f * SCALE)


def fp2fl(a):
    return a / SCALE


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


# ---------------------------------------------------------------------------
# DSP defaults (mirror the DelayLine/Reverb/ParametricEQ/Compressor ctors and
# the FxEngine ctor's explicit configuration).
# ---------------------------------------------------------------------------
DEFAULTS = {
    "dly_send": 0, "dly_ret": fl2fp(0.5),
    "dly_time": 0, "dly_fb": 0, "dly_mix": i2fp(1), "dly_wid": i2fp(1),
    "dly_pp": False, "dly_sat": False, "dly_byp": False,
    "rvb_send": 0, "rvb_ret": fl2fp(0.5),
    "rvb_pre": 0, "rvb_dec": fl2fp(1.0), "rvb_siz": i2fp(1),
    "rvb_dmp": fl2fp(0.5), "rvb_wid": i2fp(1), "rvb_mode": 1,
    "rvb_mix": i2fp(1), "rvb_byp": False,
    "eq_byp": True,  # FxEngine ctor turns the master EQ off by default
    "eq0_frq": fl2fp(100.0), "eq0_gai": 0, "eq0_q": fl2fp(1.0), "eq0_en": False,
    "eq1_frq": fl2fp(1000.0), "eq1_gai": 0, "eq1_q": fl2fp(1.0), "eq1_en": False,
    "eq2_frq": fl2fp(10000.0), "eq2_gai": 0, "eq2_q": fl2fp(1.0), "eq2_en": False,
    "cmp_byp": True,  # FxEngine ctor turns the master comp off by default
    "cmp_thr": fl2fp(-24.0), "cmp_rat": fl2fp(4.0), "cmp_kne": fl2fp(6.0),
    "cmp_atk": fl2fp(15.0), "cmp_rel": fl2fp(200.0), "cmp_mku": 0,
    "cmp_lnk": True, "cmp_sc": True,
}


class FxEngineModel:
    """Mirrors FxEngine.h/.cpp Fase 5 auto-engage + master params."""

    def __init__(self):
        self.p = dict(DEFAULTS)
        self.legacyMode = True
        self.anyChannelSendActive = False

    def all_params_at_legacy_default(self):
        return self.p == DEFAULTS

    def refresh_legacy(self):
        self.legacyMode = (self.all_params_at_legacy_default()
                           and not self.anyChannelSendActive)

    # Generic setter: edit a master param, then refresh.
    def set_param(self, key, value):
        self.p[key] = value
        self.refresh_legacy()

    def notify_channel_send_active(self, on):
        self.anyChannelSendActive = on
        self.refresh_legacy()

    def to_xml(self):
        attrs = []
        for k, v in sorted(self.p.items()):
            attrs.append('%s="%d"' % (k.upper(), int(v) if not isinstance(v, bool) else (1 if v else 0)))
        return "<FXMASTER %s/>" % " ".join(attrs)

    @classmethod
    def from_xml(cls, xml):
        fx = cls()
        if "<FXMASTER" not in xml:
            return fx  # legacy project: defaults
        body = xml[xml.index("<FXMASTER") + len("<FXMASTER"):]
        body = body[:body.index("/>")]
        for tok in body.split():
            if "=" in tok:
                k, v = tok.split("=")
                key = k.strip().lower()
                val = int(v.strip('"'))
                if key in DEFAULTS:
                    if isinstance(DEFAULTS[key], bool):
                        fx.p[key] = val != 0
                    else:
                        fx.p[key] = val
        fx.refresh_legacy()
        return fx


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------

def check_default_is_legacy():
    fx = FxEngineModel()
    assert fx.legacyMode is True, "default must be legacy bypass"
    assert fx.all_params_at_legacy_default()
    print("default-is-legacy OK")


def check_edit_master_param_engages():
    fx = FxEngineModel()
    fx.set_param("dly_time", fl2fp(250.0))
    assert fx.legacyMode is False, "delay time edit must engage FX"
    fx.set_param("rvb_dec", fl2fp(3.0))
    assert fx.legacyMode is False
    fx.set_param("eq0_en", True)
    assert fx.legacyMode is False
    fx.set_param("cmp_thr", fl2fp(-18.0))
    assert fx.legacyMode is False
    print("edit-master-param-engages OK")


def check_edit_channel_send_engages():
    fx = FxEngineModel()
    fx.notify_channel_send_active(True)
    assert fx.legacyMode is False, "raised send must engage FX"
    fx.notify_channel_send_active(False)
    assert fx.legacyMode is True, "cleared sends re-engage legacy"
    print("edit-channel-send-engages OK")


def check_return_to_default_reengages():
    fx = FxEngineModel()
    fx.set_param("dly_time", fl2fp(250.0))
    assert fx.legacyMode is False
    fx.set_param("dly_time", DEFAULTS["dly_time"])
    assert fx.legacyMode is True, "back to default must re-engage legacy"
    fx.set_param("cmp_thr", fl2fp(-30.0))
    fx.set_param("eq0_gai", fl2fp(3.0))
    fx.set_param("cmp_thr", DEFAULTS["cmp_thr"])
    fx.set_param("eq0_gai", DEFAULTS["eq0_gai"])
    assert fx.legacyMode is True
    print("return-to-default-reengages OK")


def check_roundtrip():
    fx = FxEngineModel()
    # Scatter non-default values across every DSP family.
    fx.set_param("dly_send", fl2fp(0.3))
    fx.set_param("dly_time", fl2fp(280.0))
    fx.set_param("dly_fb", fl2fp(0.42))
    fx.set_param("dly_wid", fl2fp(0.9))
    fx.set_param("dly_pp", True)
    fx.set_param("rvb_send", fl2fp(0.2))
    fx.set_param("rvb_pre", fl2fp(35.0))
    fx.set_param("rvb_dec", fl2fp(2.6))
    fx.set_param("rvb_siz", fl2fp(1.2))
    fx.set_param("rvb_dmp", fl2fp(0.7))
    fx.set_param("rvb_mode", 0)
    fx.set_param("eq_byp", False)
    fx.set_param("eq0_en", True)
    fx.set_param("eq0_frq", fl2fp(140.0))
    fx.set_param("eq0_gai", fl2fp(4.0))
    fx.set_param("eq1_gai", fl2fp(-3.0))
    fx.set_param("eq2_frq", fl2fp(8000.0))
    fx.set_param("cmp_byp", False)
    fx.set_param("cmp_thr", fl2fp(-20.0))
    fx.set_param("cmp_rat", fl2fp(6.0))
    fx.set_param("cmp_atk", fl2fp(12.0))
    fx.set_param("cmp_rel", fl2fp(180.0))
    fx.set_param("cmp_mku", fl2fp(4.0))
    fx.set_param("cmp_lnk", False)
    fx.set_param("cmp_sc", False)
    fx2 = FxEngineModel.from_xml(fx.to_xml())
    for k in DEFAULTS:
        assert fx2.p[k] == fx.p[k], k
    assert fx2.legacyMode is False, "non-default state must stay engaged"
    print("roundtrip OK")


def check_legacy_project_default():
    # Old project files have CHANNEL entries but no FXMASTER element.
    xml = "<MIXER>\n" + "\n".join(
        '  <CHANNEL INDEX="%d" BUS="%d" VOLUME="%d"/>' % (i, i, 100)
        for i in range(8)) + "\n</MIXER>"
    fx = FxEngineModel.from_xml(xml)
    assert fx.p == DEFAULTS, "legacy project must restore defaults"
    assert fx.legacyMode is True, "legacy project must stay in bypass"
    print("legacy-project-default OK")


def check_source_guards():
    hsrc = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.h").read_text()
    for token in ("RefreshLegacy", "AllParamsAtLegacyDefault",
                  "NotifyChannelSendActive", "anyChannelSendActive_"):
        assert token in hsrc, token
    csrc = (ROOT / "source/sources/Application/Audio/FxEngine/FxEngine.cpp").read_text()
    for token in ("RefreshLegacy", "AllParamsAtLegacyDefault",
                  "GetDelayMsTarget", "GetDecayTarget", "GetBandEnabled"):
        assert token in csrc, token
    msrc = (ROOT / "source/sources/Application/Model/Mixer.cpp").read_text()
    for token in ("FXMASTER", "NotifyFxSends", "NotifyChannelSendActive",
                  "DLYSEND", "DLYTIME", "RVBDEC", "EQ%dGAI", "CMPTHR",
                  "GetCompAttackMsFixed"):
        assert token in msrc, token
    # Every public FxEngine setter must re-evaluate the legacy flag, except
    # the explicit SetLegacyMode override itself.
    import re
    setters = re.findall(r"void Set\w+\([^)]*\) \{[^}]*\}", hsrc)
    missing = [s for s in setters
               if "RefreshLegacy()" not in s and "SetLegacyMode" not in s]
    assert not missing, "setters without RefreshLegacy: %s" % missing
    print("source guards OK")


check_default_is_legacy()
check_edit_master_param_engages()
check_edit_channel_send_engages()
check_return_to_default_reengages()
check_roundtrip()
check_legacy_project_default()
check_source_guards()
print("FX_PERSISTENCE_LEGACY_PHASE5_OK")
