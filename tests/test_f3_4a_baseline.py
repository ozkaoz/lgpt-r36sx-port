#!/usr/bin/env python3
"""F3-4a baseline: capa pura FxPages del Mixer.

Verifica que:
1. FxPages.h (Application/Mixer) contiene la tabla kFxParams_ completa
   (36 filas, etiquetas golden), los enums FxPage/FxParamId y los helpers
   puros (fxBypassId/fxCountOnPage/fxRowForId/fxIdForRow/fxIdOnPage/
   fxUsesCurve/fxEditCurveValue/mixVULevel/fxReturnPercent/
   fxReturnFromPercent).
2. FxPages.h NO depende de GUI/audio/Player/SamplePool (capa pura).
3. MixerView.cpp ya no define la tabla ni los helpers (delega a FxPages),
   pero mantiene su superficie publica como delegados one-line.
4. MixerView.h incluye FxPages.h y ya no define los enums.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FXP = ROOT / "source/sources/Application/Mixer/FxPages.h"
MV_CPP = ROOT / "source/sources/Application/Views/MixerView.cpp"
MV_H = ROOT / "source/sources/Application/Views/MixerView.h"

LABELS = [
    # DELAY (7)
    "DLY TIM", "DLY FBK", "DLY MIX", "DLY WID", "DLY P/P", "DLY SAT",
    "DLY BYP",
    # REVERB (7)
    "RVB PRE", "RVB DEC", "RVB SIZ", "RVB DMP", "RVB WID", "RVB MOD",
    "RVB BYP",
    # EQ (13)
    "EQ  BYP", "LO  EN", "LO  FRQ", "LO  GAI", "LO  Q", "MID EN",
    "MID FRQ", "MID GAI", "MID Q", "HI  EN", "HI  FRQ", "HI  GAI",
    "HI  Q",
    # COMP (9)
    "CMP BYP", "CMP THR", "CMP RAT", "CMP KNE", "CMP ATK", "CMP REL",
    "CMP MKU", "CMP LNK", "CMP SCL",
]

FORBIDDEN = [
    "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
    "SoundSource", "DrawString", "GUITextProperties", "AppWindow",
    "FxEngine::GetInstance", "SetDelayReturn", "GetChannelPeakL",
    "GetChannelPeakR", "GUIWindow",
]

HELPERS = [
    "fxBypassId", "fxCountOnPage", "fxRowForId", "fxIdForRow",
    "fxIdOnPage", "fxUsesCurve", "fxEditCurveValue", "mixVULevel",
    "fxReturnPercent", "fxReturnFromPercent", "FxParamSpec",
    "kFxParams_", "FX_PARAM_COUNT",
]


def check_layer():
    fxp = FXP.read_text()
    for label in LABELS:
        assert f'"{label}"' in fxp, f"label {label} in FxPages.h"
    assert 'enum FxPage' in fxp
    assert 'enum FxParamId' in fxp
    for helper in HELPERS:
        assert helper in fxp, helper
    for token in FORBIDDEN:
        assert token not in fxp, token
    # tabla y enums no duplicados en el cpp/h
    assert "struct FxParamSpec" not in MV_CPP.read_text()
    assert "enum FxPage" not in MV_CPP.read_text()
    assert "enum FxPage" not in MV_H.read_text()
    assert "static const FxParamSpec kFxParams_" not in MV_CPP.read_text()
    print("layer guards OK")


def check_view_delegates():
    mv = MV_CPP.read_text()
    mh = MV_H.read_text()
    # la vista mantiene la superficie publica como delegados one-line
    for member in ("::fxIdOnPage", "::fxBypassId", "::fxCountOnPage",
                   "::fxRowForId", "::fxIdForRow"):
        assert member in mv, member
    assert "fxEditCurveValue" in mv
    # el header incluye la capa y ya no define los enums
    assert '#include "Application/Mixer/FxPages.h"' in mh
    # los usos de la tabla en la vista (setter clamp, defaults, dibujo)
    assert "kFxParams_" in mv
    assert "mixVULevel" in mv
    assert "fxReturnPercent" in mv
    print("view delegates OK")


check_layer()
check_view_delegates()
print("F3_4A_BASELINE_OK")
