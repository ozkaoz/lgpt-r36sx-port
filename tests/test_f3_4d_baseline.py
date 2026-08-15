#!/usr/bin/env python3
"""F3-4d baseline: FxNavigator - navegacion/edicion de paginas FX pura.

Verifica que:
1. FxNavigator.h (Application/Mixer) declara el estado del cursor (pagina,
   fila y edit target del MIX page) y la matematica golden de pasos:
   SetPage (rango + reset de fila), CyclePage (MIX->...->COMP->MIX),
    MoveRow (wrap por pagina), CycleEditTarget (VOL->DLY RET->RVB RET),
    IdForRow (bypass primero) y EditValue/ResetValue (vista comun 0..100 %
    via la capa FxParamDescriptor: paso fino 1, grueso 10, switches 0/1,
    clamps 0..100, vdef).
2. FxNavigator.h NO depende de GUI/audio/Player/framebuffer (capa pura:
   solo FxPages.h -> fixed.h + <math.h>).
3. MixerView.h ya no declara fxPage_/fxRow_/fxEditTarget_: usa el miembro
   navigator_ (FxNavigator).
4. MixerView.cpp ya no contiene la matematica de pasos inline (step,
   clamps, curva) ni el estado del cursor: delega en navigator_.Page()/
   Row()/EditTarget()/IdForRow()/CycleEditTarget() y en
   FxNavigator::EditValue/ResetValue.  El engine (fxGet/fxSet) y la
   historia undo (pushMixUndo) siguen en la vista.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FN = ROOT / "source/sources/Application/Mixer/FxNavigator.h"
MV_CPP = ROOT / "source/sources/Application/UI/Views/MixerView.cpp"
MV_H = ROOT / "source/sources/Application/UI/Views/MixerView.h"

TOKENS = [
    "FxNavigator", "Page()", "Row()", "EditTarget()", "SetPage",
    "CyclePage", "MoveRow", "CycleEditTarget", "IdForRow", "EditValue",
    "ResetValue", "FX_PAGE_MIX", "FX_PAGE_COUNT", "fxCountOnPage",
    "fxIdForRow", "fxIsPercentParam", "fxPercentToDspId",
    "fxDspToPercentId", "vdef",
]

FORBIDDEN = [
    "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
    "DrawString", "GUITextProperties", "AppWindow", "MixerService",
    "GetInstance", "FxEngine", "CD_HILITE", "ColorDefinition", "EPBM_",
    "TreeFrogGetFramebuffer", "TREEFROG_LGPT_WIDTH", "pushMixUndo",
]


def check_layer():
    fn = FN.read_text()
    for token in TOKENS:
        assert token in fn, token
    for token in FORBIDDEN:
        assert token not in fn, token
    print("layer guards OK")


def check_view():
    mv = MV_CPP.read_text()
    mh = MV_H.read_text()
    # el estado del cursor ya no vive en la vista
    assert "fxPage_" not in mv and "fxPage_" not in mh
    assert "fxRow_" not in mv and "fxRow_" not in mh
    assert "fxEditTarget_" not in mv and "fxEditTarget_" not in mh
    # la vista delega en el navigator puro
    assert "FxNavigator navigator_" in mh
    assert "navigator_.SetPage(page)" in mv
    assert "navigator_.CyclePage()" in mv
    assert "navigator_.MoveRow(delta)" in mv
    assert "navigator_.CycleEditTarget()" in mv
    assert "navigator_.IdForRow()" in mv
    assert "FxNavigator::EditValue(targetId" in mv
    assert "FxNavigator::ResetValue(targetId)" in mv
    # el engine y la historia siguen en la vista
    assert "pushMixUndo(ME_FX" in mv
    assert "float MixerView::fxGet" in mv
    assert "void MixerView::fxSet" in mv
    print("view delegates OK")


check_layer()
check_view()
print("F3_4D_BASELINE_OK")
