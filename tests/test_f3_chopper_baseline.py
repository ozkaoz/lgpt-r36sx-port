#!/usr/bin/env python3
"""F3-1 baseline (docs/F3_ARCHITECTURE_ES.md): extraccion de ChopModel.

El tramo F3-1 movio el estado de cortes de SampleChopperModal a
ChopModel SIN cambiar los mensajes de estado, la API publica de la vista
ni los algoritmos golden.  Este test verifica:

1. La API publica de SampleChopperModal sigue intacta (los metodos que
   la consola y AppWindow invocan existen en el header).
2. Los mensajes de estado verificados en consola Bacon 1.2.1 siguen
   literalmente en el .cpp de la vista.
3. El header de la vista ya NO declara los miembros privados raw
   (boundaries_/boundaryCount_/selectedChop_) y declara chopModel_.
4. ChopModel.h existe con los metodos golden y kMaxBoundaries ==
   MAX_CHOP_BOUNDARIES.
5. El test host de equivalencia golden existe y esta en audit (se
   ejecuta como parte del mismo audit; aqui se comprueba su existencia).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SCM_H = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.h").read_text()
SCM_CPP = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.cpp").read_text()
CHOP_H = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/ChopModel.h").read_text()
CC_H = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/ChopperController.h").read_text()
AUDIT = (ROOT / "scripts/audit.sh").read_text()


# ---------------------------------------------------------------------------
# 1. API publica intacta
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
    ]:
        assert m in SCM_H, f"API publica perdida: {m}"


# ---------------------------------------------------------------------------
# 2. Mensajes de estado golden intactos (F3-3c: viven en ChopperController,
#    la capa pura de los flujos de edicion)
# ---------------------------------------------------------------------------
def check_status_strings():
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
        '"No sample to clear"',
        '"Sample too small"',
    ]:
        assert s in CC_H, f"Mensaje golden perdido de la capa: {s}"


# ---------------------------------------------------------------------------
# 3. Estado extraido (miembros raw fuera, chopModel_ dentro)
# ---------------------------------------------------------------------------
def check_state_extraction():
    assert "ChopModel chopModel_;" in SCM_H
    # Los miembros raw no deben existir como campos (posible cola de
    # nombre: selectedChop_ aparece solo en metodos y estado de historia).
    for raw in ["int selectedChop_;\n", "int boundaryCount_;\n",
                "int boundaries_[MAX_CHOP_BOUNDARIES];"]:
        assert raw not in SCM_H, f"Miembro raw aun en header: {raw!r}"
    # F3-3c: los flujos de edicion delegan a ChopperController; los
    # algoritmos golden (Sort/InitRange/Split/Remove) viven en la capa.
    assert "chopperController_.InitializeChopsIfNeeded();" in SCM_CPP
    assert "chopperController_.AddChopAtCursor();" in SCM_CPP
    assert "chopperController_.DeleteSelectedChop();" in SCM_CPP
    assert "chopperController_.SplitSampleIntoEqualParts(parts);" in SCM_CPP
    assert "chopModel_.Sort();" in SCM_CPP  # sortBoundaries sigue en la vista
    assert "model_.InitRange(sourceSize_);" in CC_H
    assert "model_.SplitIntoEqualParts(parts, sourceSize_);" in CC_H
    assert "model_.RemoveChop(removeIdx, sourceSize_);" in CC_H


# ---------------------------------------------------------------------------
# 4. ChopModel con constante y metodos golden
# ---------------------------------------------------------------------------
def check_chop_model():
    assert "class ChopModel {" in CHOP_H
    assert "kMaxBoundaries = 101" in CHOP_H
    assert "MAX_CHOP_BOUNDARIES = 101," in SCM_H
    for m in [
        "void InitRange(int sourceSize)",
        "void Append(int frame)",
        "void Sort()",
        "int Find(int frame) const",
        "void RemoveChop(int index, int sourceSize)",
        "void ClampSelectedToChops()",
        "void ClearAll(int sourceSize)",
        "int StartFrameForSelected() const",
        "int EndFrameForSelected(int sourceSize) const",
        "void SplitIntoEqualParts(int parts, int sourceSize)",
        "static int ClampInt(int value, int minValue, int maxValue)",
    ]:
        assert m in CHOP_H, f"Metodo ChopModel perdido: {m}"
    for member in ["int boundaryCount;", "int selected;",
                   "int boundaries[kMaxBoundaries];"]:
        assert member in CHOP_H, f"Campo ChopModel perdido: {member}"


# ---------------------------------------------------------------------------
# 5. Test host en audit
# ---------------------------------------------------------------------------
def check_audit_wiring():
    assert "run_host_chop_model.sh" in AUDIT
    host = ROOT / "tests" / "host" / "chop_model_host_test.cpp"
    assert host.exists(), "chop_model_host_test.cpp ausente"
    assert "equivalencia golden" in host.read_text()


def main():
    check_public_api()
    check_status_strings()
    check_state_extraction()
    check_chop_model()
    check_audit_wiring()
    print("F3-1 baseline chopper: OK")


if __name__ == "__main__":
    main()