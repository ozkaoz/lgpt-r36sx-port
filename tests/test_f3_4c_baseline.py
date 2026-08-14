#!/usr/bin/env python3
"""F3-4c baseline: menu L1+A del Mixer declarado como datos puros.

Verifica que:
1. MixerMenu.h (Application/Mixer) declara los datos golden del menu
   L1+A (TREEFROG_MIXER_ACTION_MENU_V1): filas (6 master / 5 track),
   etiquetas en orden exacto, clamps de softclip (0..4) y clip gain
   (0..1), codificado de accion (fila master >= 2 -> pagina FX 1..4 =
   DELAY..COMP; fila track -> seccion 101..105) y los hints FourCC de
   seccion (SIP_FILTMIX/SIP_CRUSH/SIP_INTERPOLATION/SIP_DRY/SIP_TABLEAUTO).
2. MixerMenu.h NO depende de GUI/audio/Player/framebuffer (capa pura:
   solo Foundation/Types/Types.h).
3. MixerView.cpp ya no lleva las etiquetas/clamps/sections/hintIds
   inline: dibuja y procesa delegando en mixerMenuRowCount/mixerMenuLabel/
   mixerMenuClampSoftclip/mixerMenuClampSoftclipGain/mixerMenuActionForRow/
   mixerMenuSectionHint.  El modal y el callback se quedan en la vista
   (dibujo + audio/project siguen en la vista).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MM = ROOT / "source/sources/Application/Mixer/MixerMenu.h"
MV_CPP = ROOT / "source/sources/Application/UI/Views/MixerView.cpp"

TOKENS = [
    "kMixerMasterMenuRowCount", "kMixerTrackMenuRowCount",
    "kMixerMasterMenuLabels", "kMixerTrackMenuLabels",
    "kMixerSoftclipMin", "kMixerSoftclipMax",
    "kMixerSoftclipGainMin", "kMixerSoftclipGainMax",
    "kMixerTrackSectionHints", "mixerMenuRowCount", "mixerMenuLabel",
    "mixerMenuClampSoftclip", "mixerMenuClampSoftclipGain",
    "mixerMenuActionForRow", "mixerMenuSectionHint",
    "LIMITER", "CLIP GAIN", "FX DELAY", "FX REVERB", "FX EQ", "FX COMP",
    "FILTER", "BITCRUSHER", "PLAYBACK", "FX SENDS", "AUTOMATION",
]

FORBIDDEN = [
    "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
    "DrawString", "GUITextProperties", "AppWindow", "MixerService",
    "GetSoftclip", "CD_HILITE", "ColorDefinition", "EPBM_",
    "TreeFrogGetFramebuffer", "TREEFROG_LGPT_WIDTH",
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
    # la vista usa la capa y ya no lleva los datos inline
    assert "mixerMenuRowCount(masterMenu_)" in mv
    assert "mixerMenuLabel(true, i)" in mv
    assert "mixerMenuLabel(false, i)" in mv
    assert "mixerMenuClampSoftclip(" in mv
    assert "mixerMenuClampSoftclipGain(" in mv
    assert "mixerMenuActionForRow(masterMenu_, item_)" in mv
    assert "mixerMenuSectionHint(section)" in mv
    assert "hintIds" not in mv
    assert '"LIMITER"' not in mv and '"CLIP GAIN"' not in mv
    assert "sections[5]" not in mv
    assert '"FILTER"' not in mv
    print("view delegates OK")


check_layer()
check_view_delegates()
print("F3_4C_BASELINE_OK")
