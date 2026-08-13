#!/usr/bin/env python3
"""F3-4b baseline: capa pura MixerMeters del Mixer.

Verifica que:
1. MixerMeters.h (Application/Mixer) contiene el smoothing golden
   (SmoothFrame: ataque instantaneo, release *0.6, piso 0.001, stop->0),
   BarLevel (mixVULevel*vol/100 clamp 0..1), GeometryFor/RowStateFor
   (metrica half-cell: LEVEL_H 3, banda roja 0 dB+ 36/39, fill 2-px) y
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
MV_CPP = ROOT / "source/sources/Application/Views/MixerView.cpp"
MV_H = ROOT / "source/sources/Application/Views/MixerView.h"

TOKENS = [
    "class MixerMeters", "SmoothFrame", "BarLevel", "GeometryFor",
    "RowStateFor", "kLevelHeight", "ZeroDbLevel", "kChannels",
    "0.6f", "0.001f", "0.01f", "levelL", "levelR",
]

FORBIDDEN = [
    "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
    "SoundSource", "DrawString", "GUITextProperties", "AppWindow",
    "GetChannelPeakL", "GetChannelPeakR", "TreeFrogGetFramebuffer",
    "ColorDefinition", "ResolveColor565", "TREEFROG_LGPT_WIDTH",
]


def check_layer():
    mm = MM.read_text()
    for token in TOKENS:
        assert token in mm, token
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
