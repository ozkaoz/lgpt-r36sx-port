#!/usr/bin/env python3
"""F3-5b baseline: PhraseUndo - historia snapshot/restore de la Phrase pura.

Verifica que:
1. PhraseUndo.h (Application/Phrase) declara la historia golden de la frase:
   kPhraseUndoHistorySize, PhraseUndoSnapshot (layout identico al
   PhraseEdit original: 10 arrays de 16 + currentPhrase), capture, equal
   (dedup V9), push (shift + cap + clear redo + guard de reentrada),
   restore (V8: publica el indice de frase, no el cursor) y el paso
   undo/redo compartido (GlobalUndo/GlobalRedo golden).
2. PhraseUndo.h NO depende de GUI/audio/Player/viewData_/View  (capa pura:
   Types.h, Phrase.h).
3. PhraseView.cpp conserva solo la politica: pushPhraseUndo sigue vivo como
   metodo (llama a PhraseUndoPush con snapshot del estado pre-edit),
   GlobalUndo/GlobalRedo delegan el paso de historia en PhraseUndoStep, y la
   vista aplica el efecto local (phraseCurPos_ = row_, isDirty_) y decide
   cuando capturar (updateCursorValue, paste, cut, interpolate, chop,
   command selector, VM_NEW A, VM_CLONE L+A).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PU = ROOT / "source/sources/Application/Phrase/PhraseUndo.h"
PV_CPP = ROOT / "source/sources/Application/UI/Views/PhraseView.cpp"
PV_H = ROOT / "source/sources/Application/UI/Views/PhraseView.h"

TOKENS = [
    "kPhraseUndoHistorySize", "PhraseUndoSnapshot", "PhraseUndoCapture",
    "PhraseUndoSnapshotEqual", "PhraseUndoPush", "PhraseUndoRestore",
    "PhraseUndoStep", "currentPhrase", "note[16]", "instr[16]", "vol[16]",
    "pitch[16]", "cmd1[16]", "param1[16]", "cmd2[16]", "param2[16]",
    "cmd3[16]", "param3[16]",
]

FORBIDDEN = [
    "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
    "DrawString", "GUITextProperties", "AppWindow", "MixerService",
    "GetInstance", "UIBigHexVarField", "CD_HILITE", "ColorDefinition",
    "EPBM_", "TreeFrogGetFramebuffer", "TREEFROG_LGPT_WIDTH", "viewData_",
    "View::SetNotification", "cmdEditField_", "GUIRect", "CmdEdit",
    "FCC_EDIT", "isDirty_", "phraseCurPos_", "row_",
]

# Mecanica golden que DEBE haber salido de la vista hacia la capa.
MOVED_OUT = [
    "memcpy(e.note, phrase_->note_",
    "memcpy(e.instr, phrase_->instr_",
    "memcpy(e.vol, phrase_->vol_",
    "memcpy(e.pitch, phrase_->pitch_",
    "memcpy(e.cmd1, phrase_->cmd1_",
    "memcpy(e.cmd2, phrase_->cmd2_",
    "memcpy(e.cmd3, phrase_->cmd3_",
    "e.currentPhrase = (uchar)",
    "phraseUndo_[i] = phraseUndo_[i + 1]",
    "phraseRedo_[i] = phraseRedo_[i - 1]",
    "if (*fromCount == 0)",
    "memcmp(a.note, b.note,",
    "phraseUndoRestore(phrase_, e",
]

KEPT_IN_VIEW = [
    "void PhraseView::pushPhraseUndo",
    "bool PhraseView::GlobalUndo",
    "bool PhraseView::GlobalRedo",
    "PhraseUndoPush(",
    "PhraseUndoStep(",
    "viewData_->phraseCurPos_ = row_",
    "isDirty_ = true",
]


def check_layer():
    pu = PU.read_text()
    for token in TOKENS:
        assert token in pu, token
    for token in FORBIDDEN:
        assert token not in pu, token
    print("layer guards OK")


def check_view_delegates():
    pv = PV_CPP.read_text()
    ph = PV_H.read_text()
    assert '#include "Application/Phrase/PhraseUndo.h"' in pv
    assert '"Application/Phrase/PhraseUndo.h"' in ph
    assert "PhraseUndoPush(" in pv
    assert "PhraseUndoStep(" in pv
    assert "PhraseUndoCapture(" in ph or "PhraseUndoSnapshot" in ph
    # el tipo de los arrays de historia es el snapshot puro
    assert "PhraseUndoSnapshot phraseUndo_" in ph
    assert "PhraseUndoSnapshot phraseRedo_" in ph
    assert "typedef PhraseUndoSnapshot PhraseEdit" in ph
    assert "static const int kPhraseHistorySize = kPhraseUndoHistorySize;" in ph
    print("view delegates OK")


def check_math_moved_out():
    pv = PV_CPP.read_text()
    for token in MOVED_OUT:
        assert token not in pv, token
    print("history mechanics moved out of the view OK")


def check_view_kept():
    pv = PV_CPP.read_text()
    for token in KEPT_IN_VIEW:
        assert token in pv, token
    print("view keeps policy + local undo/redo effect OK")


check_layer()
check_view_delegates()
check_math_moved_out()
check_view_kept()
print("F3_5B_BASELINE_OK")