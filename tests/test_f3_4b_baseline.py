#!/usr/bin/env python3
"""F3-4b baseline: capa pura MixerMeters del Mixer.

Verifica que:
1. MixerMeters.h (Application/Mixer) contiene el smoothing golden
   (SmoothFrame: ataque instantaneo, release *0.6, piso 0.001, stop->0),
   BarLevel (escala dB BACON_1.5_VU_DB_SCALE: (20*log10(pico)+24)/24 sin
   factor de volumen -- el pico escaneado ya incluye su fader; clamp 0..1;
   BACON_1.5_VU_TOP0DB: 0 dBFS = tope de la barra),
   GeometryFor/RowStateFor
   (metrica half-cell: LEVEL_H 3, banda roja = celda tope 0 dBFS, fill 2-px) y
   kChannels == 8.
2. MixerMeters.h NO depende de GUI/audio/Player/framebuffer (capa pura).
3. MixerView.cpp ya no contiene el smoothing inline (vuDisplayL_/R_),
   delega en MixerMeters (meters_.SmoothFrame / MixerMeters::BarLevel /
   GeometryFor / RowStateFor).
4. MixerView.h tiene el miembro MixerMeters meters_ y ya no declara los
   arrays vuDisplayL_/vuDisplayR_.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MM = ROOT / "source/sources/Application/Mixer/MixerMeters.h"
FP = ROOT / "source/sources/Application/Mixer/FxPages.h"
MV_CPP = ROOT / "source/sources/Application/UI/Views/MixerView.cpp"
MV_H = ROOT / "source/sources/Application/UI/Views/MixerView.h"

TOKENS = [
    "class MixerMeters", "SmoothFrame", "BarLevel", "GeometryFor",
    "RowStateFor", "kLevelHeight", "ZeroDbLevel", "kChannels",
    "0.6f", "0.001f", "levelL", "levelR",
]

# BACON_1.5_VU_TOP0DB (U2.59): la escala dB (-24..0 dBFS, 0 dBFS = tope de
# la barra) vive en mixVULevel (FxPages.h): (20*log10(p)+24)/24.
FP_TOKENS = ["(db + 24.0f) / 24.0f"]

FORBIDDEN = [
    "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
    "SoundSource", "DrawString", "GUITextProperties", "AppWindow",
    "GetChannelPeakL", "GetChannelPeakR", "TreeFrogGetFramebuffer",
    "ColorDefinition", "ResolveColor565", "TREEFROG_LGPT_WIDTH",
]


def check_layer():
    mm = MM.read_text()
    fp = FP.read_text()
    for token in TOKENS:
        assert token in mm, token
    for token in FP_TOKENS:
        assert token in fp, token
    for token in FORBIDDEN:
        assert token not in mm, token
    print("layer guards OK")


def check_view_delegates():
    mv = MV_CPP.read_text()
    mh = MV_H.read_text()
    # smoothing inline fuera de la vista; delegacion a la capa
    assert "meters_.SmoothFrame" in mv
    assert "vuDisplayL_" not in mv and "vuDisplayR_" not in mv
    assert "MixerMeters::BarLevel" in mv
    assert "MixerMeters::GeometryFor" in mv
    assert "MixerMeters::RowStateFor" in mv
    assert "vuDisplay_" not in mv
    # el header incluye la capa y tiene el miembro meters_
    assert '#include "Application/Mixer/MixerMeters.h"' in mh
    assert "MixerMeters meters_" in mh
    assert "vuDisplayL_" not in mh and "vuDisplayR_" not in mh
    print("view delegates OK")


check_layer()
check_view_delegates()
print("F3_4B_BASELINE_OK")
