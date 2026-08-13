#!/usr/bin/env python3
"""F3-2 baseline (docs/F3_ARCHITECTURE_ES.md): SampleEditHistory +
PitchEnvelopeTool extraidos de SampleChopperModal.

Este tramo mueve las pilas undo/redo (LogicalHistoryState) y los
parametros pitch/env + DSP puro a capas puras, SIN cambiar los mensajes
de estado, la API publica de la vista ni los algoritmos golden.  Verifica:

1. La API publica de SampleChopperModal sigue intacta (lista F3-1 +
   metodos pitch/historia que el resto del proyecto invoca).
2. Los mensajes de estado golden (pitch/historia) siguen literalmente en
   el .cpp de la vista.
3. El header de la vista YA NO declara:
   - pitchSemitones_ / pitchEditParam_ / pitchAttackMs_ /
     pitchSustainPercent_ / pitchReleaseMs_ / pitchScope_ (raw);
   - undoHistory_[...] / redoHistory_[...] / undoHistoryCount_ /
     redoHistoryCount_.
4. SampleChopperModal.h declara SampleEditHistory y PitchEnvelopeTool
   (por valor, en la seccion privada) via include de los headers puros.
5. SampleEditHistory.h: kMaxEntries == MAX_LOGICAL_HISTORY (24),
   metodos Push/Undo/Redo/Clear/PeekUndo/PeekRedo, operaciones sobre
   Estado con copy plana.
6. PitchEnvelopeTool.h: clamps golden (12, 5000, 150), metodos de
   reset/params/param name/DSP (BuildPitchedRange/ApplyEnvelope).
7. Los runners host y los tests estaticos del tramo existen y estan en
   audit.sh.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SCM_H = (ROOT / "source/sources/Application/Views/ModalDialogs/SampleChopperModal.h").read_text()
SCM_CPP = (ROOT / "source/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp").read_text()
HIST_H = (ROOT / "source/sources/Application/Views/ModalDialogs/SampleEditHistory.h").read_text()
PET_H = (ROOT / "source/sources/Application/Views/ModalDialogs/PitchEnvelopeTool.h").read_text()
CC_H = (ROOT / "source/sources/Application/Views/ModalDialogs/ChopperController.h").read_text()
AUDIT = (ROOT / "scripts/audit.sh").read_text()


# ---------------------------------------------------------------------------
# 1. API publica intacta (F3-1 + F3-2)
# ---------------------------------------------------------------------------
def check_public_api():
    for m in [
        "void DrawView();",
        "virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);",
        "virtual void OnFocus();",
        "virtual void ProcessButtonMask(unsigned short mask, bool pressed);",
        "virtual ViewType GetViewType() const",
        "virtual void OnSuspend()",
        "virtual void OnRestore()",
        "bool undoLastChopperEdit();",
        "bool redoLastChopperEdit();",
        "void togglePitchMode();",
        "void addChopAtCursor();",
        "void deleteSelectedChop();",
        "void splitSampleIntoEqualParts(int parts);",
        "void snapSelectedBoundaryToZeroCross(bool isStart);",
        "void selectChop(int delta);",
        "void nudgeSelectedStart(int deltaFrames);",
        "void nudgeSelectedEnd(int deltaFrames);",
        "void toggleTrimMode();",
        "void exportChopsToPhrase();",
        "void assignSelectedChopToPhrase();",
        "bool destructiveCropToSelectedRange();",
        "bool destructivePitchSample(int semitones);",
        "void clearAllChops();",
        "void previewPitchSetting();",
        "bool buildPitchedBuffer(int semitones, short **outSamples, int *outFrames, int *outChannels, int *outRate);",
        "bool preparePitchEnvelopePreviewBuffer(short **outSamples, int *outFrames, int *outChannels, int *outRate);",
    ]:
        assert m in SCM_H, f"API publica perdida o privada cambiada: {m}"


# ---------------------------------------------------------------------------
# 2. Mensajes de estado golden intactos (historia + pitch)
#    F3-3c: los de edicion (chop/split/delete) viven en ChopperController.
# ---------------------------------------------------------------------------
def check_status_strings():
    for s in [
        '"Undo: %.46s"',
        '"Redo: %.46s"',
        '"Undo history does not match sample"',
        '"Redo history does not match sample"',
        '"Pitch/Env sample"',
        '"Pitch off"',
        '"Pitch %+d st"',
        '"Attack %d ms"',
        '"Sustain %d%%"',
        '"Release %d ms"',
        '"Scope %s"',
        '"%s selected"',
        '"Pitch target sample %02X"',
        '"Pitch preview fail"',
        '"Pitch preview write fail"',
        '"Use PITCH/ENV first"',
        '"No WAV to pitch"',
        '"Pitch WAV only"',
        '"Pitch/env unchanged"',
    ]:
        assert s in SCM_CPP, f"Mensaje golden perdido: {s}"
    for s in [
        '"Cannot chop at edge"',
        '"Chop already exists"',
        '"Max 100 chops reached"',
        '"No sample to chop"',
        '"Deleted cut"',
        '"Merge cuts"',
        '"No chop to delete"',
        '"Cannot delete edge"',
        '"Selected chop %02d"',
        '"Move cut start"',
        '"Move cut end"',
        '"Adjusted chop start"',
        '"Adjusted chop end"',
        '"Split sample in %d parts"',
        '"No cuts (L1+B to split again)"',
        '"Zero-cross %s %d"',
        '"Already at zero-cross"',
        '"Live chop %02d at %d"',
        '"Chop %02d at %d"',
        '"No user chops"',
    ]:
        assert s in CC_H, f"Mensaje golden de edicion perdido de la capa: {s}"


# ---------------------------------------------------------------------------
# 3. Members raw extraidos del header
# ---------------------------------------------------------------------------
def check_raw_removed():
    for raw in [
        "pitchSemitones_",
        "pitchEditParam_",
        "pitchAttackMs_",
        "pitchSustainPercent_",
        "pitchReleaseMs_",
        "pitchScope_",
        "undoHistory_",
        "redoHistory_",
        "undoHistoryCount_",
        "redoHistoryCount_",
        "LogicalHistoryState undoHistory_",
        "LogicalHistoryState redoHistory_",
    ]:
        assert raw not in SCM_H, f"member raw todavia en header: {raw}"


def check_pure_members_declared():
    assert "SampleEditHistory<LogicalHistoryState>" in SCM_H, (
        "header no declara SampleEditHistory por valor")
    assert "PitchEnvelopeTool pitchEnvTool_;" in SCM_H, (
        "header no declara PitchEnvelopeTool por valor")


# ---------------------------------------------------------------------------
# 4. Capa SampleEditHistory
# ---------------------------------------------------------------------------
def check_history_layer():
    assert "class SampleEditHistory" in HIST_H
    assert "LGPT_HISTORY_MAX_ENTRIES 24" in HIST_H
    for m in [
        "void Push(const State &state)",
        "bool Undo(const State &redoState)",
        "bool Redo(const State &undoState)",
        "void Clear()",
        "bool PeekUndo(State &out) const",
        "bool PeekRedo(State &out) const",
        "int UndoCount() const",
        "int RedoCount() const",
    ]:
        assert m in HIST_H, f"SampleEditHistory sin {m}"
    assert "undo_[LGPT_HISTORY_MAX_ENTRIES]" in HIST_H
    assert "redo_[LGPT_HISTORY_MAX_ENTRIES]" in HIST_H
    assert "undoCount_ = 0" in HIST_H


# ---------------------------------------------------------------------------
# 5. Capa PitchEnvelopeTool
# ---------------------------------------------------------------------------
def check_pitch_layer():
    assert "class PitchEnvelopeTool" in PET_H
    for c in [
        "LGPT_PITCH_MIN_SEMITONES (-12)",
        "LGPT_PITCH_MAX_SEMITONES (12)",
        "LGPT_PITCH_MAX_ATTACK_MS (5000)",
        "LGPT_PITCH_MAX_SUSTAIN_PERCENT (150)",
        "LGPT_PITCH_MAX_RELEASE_MS (5000)",
        "LGPT_PITCH_PARAM_COUNT (6)",
    ]:
        assert c in PET_H, f"constante golden falta: {c}"
    for m in [
        "void Reset()",
        "bool HasChange() const",
        "int NudgeSemitones(int delta)",
        "int NudgeAttackMs(int delta)",
        "int NudgeSustainPercent(int delta)",
        "int NudgeReleaseMs(int delta)",
        "void ToggleScope()",
        "static bool ApplyEnvelope(",
        "static bool BuildPitchedRange(",
        "static const char *ParamName(int param)",
    ]:
        assert m in PET_H, f"PitchEnvelopeTool sin {m}"
    for name in ['return "Attack";', 'return "Sustain";',
                 'return "Release";', 'return "Scope";',
                 'return "Sample";', 'return "Pitch";']:
        assert name in PET_H, f"ParamName golden falta: {name}"


# ---------------------------------------------------------------------------
# 6. Runners y audit
# ---------------------------------------------------------------------------
def check_runners():
    for p in ["tests/run_host_edit_history.sh",
              "tests/run_host_pitch_tool.sh"]:
        assert (ROOT / p).exists(), f"runner falta: {p}"
    for run in ["run_host_edit_history.sh", "run_host_pitch_tool.sh"]:
        assert run in AUDIT, f"{run} no esta en audit.sh"


def main():
    check_public_api()
    check_status_strings()
    check_raw_removed()
    check_pure_members_declared()
    check_history_layer()
    check_pitch_layer()
    check_runners()
    print("test_f3_2_baseline: ALL OK")


if __name__ == "__main__":
    main()