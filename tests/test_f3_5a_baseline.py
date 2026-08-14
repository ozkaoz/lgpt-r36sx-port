#!/usr/bin/env python3
"""F3-5a baseline: PhraseGridEdit - logica de grid/edicion de la Phrase pura.

Verifica que:
1. PhraseGridEdit.h (Application/Phrase) declara la geometria de columnas
   (kPhraseColCount/kPhraseCol*), la matematica golden del paso de valores
   (PhraseClampWrap, PhraseStepCell con limites/wrap/scale-snap/auto-fill,
   PasteLast/PasteLastCommand), la seleccion (PhraseNormalizeRect,
   PhraseExtendSelection) y el portapapeles/interpolacion (PhraseClipboard,
   PhraseFillClipboard, PhraseCutSelectionCells, PhrasePasteClipboard,
   PhraseInterpolateSelection).
2. PhraseGridEdit.h NO depende de GUI/audio/Player/viewData_/Song/
   framebuffer (capa pura: Types.h, Phrase.h, Scale.h, CommandList.h).
3. PhraseView.cpp ya no contiene la matematica inline de pasos (offsets_,
   updateData, scale-snap, auto-fill) ni el bucle de clipboard; delega en la
   capa.  El dibujo (DrawView), el engine (Player), el hex editor
   (cmdEditField_), el command selector y el chop logic siguen en la vista.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PGE = ROOT / "source/sources/Application/Phrase/PhraseGridEdit.h"
PV_CPP = ROOT / "source/sources/Application/UI/Views/PhraseView.cpp"
PV_H = ROOT / "source/sources/Application/UI/Views/PhraseView.h"

TOKENS = [
    "kPhraseColCount", "kPhraseColNote", "kPhraseColVol", "kPhraseColPitch",
    "kPhraseColInstr", "kPhraseColCmd1", "kPhraseColParam1", "kPhraseColCmd2",
    "kPhraseColParam2", "kPhraseNoteLimit", "kPhraseVolLimit",
    "kPhrasePitchLimit", "kPhraseInstrLimit", "kPhraseVolFull",
    "kPhraseStepOffsets", "PhraseClampWrap", "PhraseStepCell",
    "PhraseLimitFor", "PhraseWrapFor", "PhrasePasteLast",
    "PhrasePasteLastCommand", "PhraseNormalizeRect", "PhraseExtendSelection",
    "PhraseClipboard", "PhraseFillClipboard", "PhraseCutSelectionCells",
    "PhrasePasteClipboard", "PhraseInterpolateSelection",
]

FORBIDDEN = [
    "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
    "DrawString", "GUITextProperties", "AppWindow", "MixerService",
    "GetInstance", "UIBigHexVarField", "CD_HILITE", "ColorDefinition",
    "EPBM_", "TreeFrogGetFramebuffer", "TREEFROG_LGPT_WIDTH",
    "viewData_", "View::SetNotification", "cmdEditField_", "GUIRect",
    "CmdEdit", "FCC_EDIT",
]

# Matematica golden que DEBE haber salido de la vista hacia la capa.
MOVED_OUT = [
    "updateData(c, offset, limit, wrap)",
    "offsets_[editCol",
    "clipboard_.width_ = selRect.Width() + 1",
    "clipboard_.height_ = selRect.Height() + 1",
    "clipboard_.row_ = selRect.Top()",
    "scaleSteps[scale][", "noteWasEmpty",
]

KEPT_IN_VIEW = [
    "void PhraseView::DrawView()",
    "void PhraseView::OnPlayerUpdate",
    "Player *player = Player::GetInstance()",
    "cmdEditField_",
    "enterCommandSelector",
    "updateChopNoteValueForRow",
    "pasteDefaultChopForRow",
    "void PhraseView::ProcessButtonMask",
    "void PhraseView::processNormalButtonMask",
    "void PhraseView::processSelectionButtonMask",
    "void PhraseView::pushPhraseUndo",
    "bool PhraseView::GlobalUndo",
    "bool PhraseView::GlobalRedo",
]


def check_layer():
    pge = PGE.read_text()
    for token in TOKENS:
        assert token in pge, token
    for token in FORBIDDEN:
        assert token not in pge, token
    print("layer guards OK")


def check_view_delegates():
    pv = PV_CPP.read_text()
    ph = PV_H.read_text()
    # PhraseGridEdit.h consumido por la vista
    assert '#include "Application/Phrase/PhraseGridEdit.h"' in pv
    assert '"Application/Phrase/PhraseGridEdit.h"' in ph
    # la vista delega el paso de valor de datos a la capa
    assert "PhraseStepCell(" in pv
    assert "PhrasePasteLast(" in pv
    assert "PhrasePasteLastCommand(" in pv
    assert "PhraseFillClipboard(" in pv
    assert "PhraseCutSelectionCells(" in pv
    assert "PhrasePasteClipboard(" in pv
    assert "PhraseInterpolateSelection(" in pv
    assert "PhraseExtendSelection(" in pv
    # el portapapeles de la vista es el tipo puro
    assert "PhraseClipboard clipboard_;" in ph
    print("view delegates OK")


def check_math_moved_out():
    pv = PV_CPP.read_text()
    for token in MOVED_OUT:
        assert token not in pv, token
    print("math moved out of the view OK")


def check_view_kept():
    pv = PV_CPP.read_text()
    for token in KEPT_IN_VIEW:
        assert token in pv, token
    print("view keeps drawing/audio/hex/chop/undo OK")


check_layer()
check_view_delegates()
check_math_moved_out()
check_view_kept()
print("F3_5A_BASELINE_OK")