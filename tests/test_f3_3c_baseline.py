#!/usr/bin/env python3
"""F3-3c baseline (docs/F3_ARCHITECTURE_ES.md): ChopperController.

El tramo F3-3c extrajo la logica de edicion del chopper (12 flujos golden:
add/delete/select/trim/nudge x2/crop logico/split/clear/cycle/snap +
initialize, y los helpers hasUserChops/hasActiveSliceRange/start/end) de
SampleChopperModal a la capa pura ChopperController SIN cambiar los
mensajes de estado, el orden de escrituras ni la API publica de la vista.

La vista conserva audio, destructivos, undo/redo fisico+logico,
persistencia por sample, preview y overlay; el adapter ChopperHostAdapter
traduce los efectos de vista.  Verifica:

1. ChopperController.h existe como capa pura (header-only) con la clase y
   el Host interface (SetStatus/SetOperationCombo/PushLogicalUndo/
   SaveChopState/EnsureCursorVisible/PrepareWaveformPreview/
   PublishOverlayState/MarkDirty/SampleLoaded/QuerySnapBuffer/
   LiveStreamingPosition).
2. Los 12 flujos + helpers y sus strings golden viven literalmente en la
   capa (no en el .cpp de la vista).
3. Los lazos golden (action labels de undo: Add cut/Merge cuts/Move cut
   start/Move cut end/Keep logical range/Split sample/Clear chops/Snap
   start/Snap end) viven en la capa.
4. La vista declara el adapter por valor (chopperHost_) y el controller
   (chopperController_) e incluye la capa.
5. Los 16 metodos de la vista delegan one-line al controller.
6. El test host de equivalencia golden existe y esta en audit.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SCM_H = (ROOT / "source/sources/Application/Views/ModalDialogs/SampleChopperModal.h").read_text()
SCM_CPP = (ROOT / "source/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp").read_text()
CC_H = (ROOT / "source/sources/Application/Views/ModalDialogs/ChopperController.h").read_text()
AUDIT = (ROOT / "scripts/audit.sh").read_text()


# ---------------------------------------------------------------------------
# 1. Capa pura con Host interface
# ---------------------------------------------------------------------------
def check_controller_layer():
    assert "class ChopperController" in CC_H
    assert "struct Host {" in CC_H
    assert "virtual ~Host() {}" in CC_H
    for m in [
        "virtual void SetStatus(const char *message) = 0;",
        "virtual void SetOperationCombo(const char *combo) = 0;",
        "virtual void PushLogicalUndo(const char *action) = 0;",
        "virtual void SaveChopState() = 0;",
        "virtual void EnsureCursorVisible() = 0;",
        "virtual void PrepareWaveformPreview() = 0;",
        "virtual void PublishOverlayState() = 0;",
        "virtual void MarkDirty() = 0;",
        "virtual bool SampleLoaded() = 0;",
        "virtual int QuerySnapBuffer(short **samples, int *channels) = 0;",
        "virtual bool LiveStreamingPosition(int *frame) = 0;",
    ]:
        assert m in CC_H, f"Host sin {m}"
    # Pura: no depende de Player, SamplePool, ModalView ni SoundSource
    # (los comentarios documentales del Host pueden citar Player).
    for banned in ["Player::GetInstance()->", "SamplePool::GetInstance()->",
                   "ModalView", "SoundSource", "DrawString",
                   "GUITextProperties"]:
        assert banned not in CC_H, f"capa con dependencia impura: {banned}"


# ---------------------------------------------------------------------------
# 2. Flujos golden en la capa
# ---------------------------------------------------------------------------
def check_flows():
    for f in [
        "void InitializeChopsIfNeeded()",
        "void AddChopAtCursor()",
        "void DeleteSelectedChop()",
        "void SelectChop(int delta)",
        "bool HasUserChops() const",
        "bool HasActiveSliceRange() const",
        "int SelectedChopStartFrame() const",
        "int SelectedChopEndFrame() const",
        "void ToggleTrimMode()",
        "void NudgeSelectedStart(int deltaFrames)",
        "void NudgeSelectedEnd(int deltaFrames)",
        "void CropToSelectedRange()",
        "void SplitSampleIntoEqualParts(int parts)",
        "void ClearAllChops()",
        "void CycleSplitParts()",
        "void SnapSelectedBoundaryToZeroCross(bool isStart)",
    ]:
        assert f in CC_H, f"flujo golden perdido de la capa: {f}"


# ---------------------------------------------------------------------------
# 3. Strings golden en la capa (y fuera de la vista)
# ---------------------------------------------------------------------------
def check_strings():
    for s in [
        '"No sample to chop"',
        '"Max 100 chops reached"',
        '"Cannot chop at edge"',
        '"Chop already exists"',
        '"Live chop %02d at %d"',
        '"Chop %02d at %d"',
        '"No chop to delete"',
        '"Cannot delete edge"',
        '"Deleted cut"',
        '"No user chops"',
        '"Selected chop %02d"',
        '"CROP SAMPLE"',
        '"Crop mode off"',
        '"No range to trim"',
        '"Adjusted chop start"',
        '"Adjusted chop end"',
        '"No sample to crop"',
        '"No range to crop"',
        '"Bad crop range"',
        '"Keep range %d-%d"',
        '"No sample to split"',
        '"Sample too small"',
        '"Split sample in %d parts"',
        '"No sample to clear"',
        '"No cuts (L1+B to split again)"',
        '"No sample loaded"',
        '"No chops to snap"',
        '"No WAV source"',
        '"Bad sample buffer"',
        '"Invalid boundary"',
        '"Zero-cross %s %d"',
        '"Already at zero-cross"',
        '"Add cut"',
        '"Merge cuts"',
        '"Move cut start"',
        '"Move cut end"',
        '"Keep logical range"',
        '"Split sample"',
        '"Clear chops"',
        '"Snap start"',
        '"Snap end"',
        '"L1 + B"',
    ]:
        assert s in CC_H, f"string golden perdido de la capa: {s}"
    # Los strings de edicion se fueron de la vista (deben vivir solo en la
    # capa; el cpp conserva los de play/zoom/historial/pitch).
    for s in [
        '"Max 100 chops reached"',
        '"Deleted cut"',
        '"Zero-cross %s %d"',
        '"No cuts (L1+B to split again)"',
    ]:
        assert s not in SCM_CPP, f"string golden aun en la vista: {s}"
    # Escrituras golden fuera de la vista: la capa usa las constantes puras.
    assert "ChopModel::kMaxBoundaries" in CC_H


# ---------------------------------------------------------------------------
# 4. Vista: adapter + controller por valor
# ---------------------------------------------------------------------------
def check_view_host():
    assert "ModalDialogs/ChopperController.h" in SCM_H
    assert "struct ChopperHostAdapter : public ChopperController::Host" in SCM_H
    assert "ChopperHostAdapter chopperHost_;" in SCM_H
    assert "ChopperController chopperController_;" in SCM_H
    assert "chopperController_(chopperHost_, chopModel_, preview_, sourceSize_," in SCM_CPP


# ---------------------------------------------------------------------------
# 5. Delegados one-line en la vista
# ---------------------------------------------------------------------------
def check_delegation():
    for d in [
        "chopperController_.InitializeChopsIfNeeded();",
        "chopperController_.AddChopAtCursor();",
        "chopperController_.DeleteSelectedChop();",
        "chopperController_.SelectChop(delta);",
        "return chopperController_.HasUserChops();",
        "return chopperController_.HasActiveSliceRange();",
        "return chopperController_.SelectedChopStartFrame();",
        "return chopperController_.SelectedChopEndFrame();",
        "chopperController_.ToggleTrimMode();",
        "chopperController_.NudgeSelectedStart(deltaFrames);",
        "chopperController_.NudgeSelectedEnd(deltaFrames);",
        "chopperController_.CropToSelectedRange();",
        "chopperController_.SplitSampleIntoEqualParts(parts);",
        "chopperController_.ClearAllChops();",
        "chopperController_.CycleSplitParts();",
        "chopperController_.SnapSelectedBoundaryToZeroCross(isStart);",
    ]:
        assert d in SCM_CPP, f"delegado perdido: {d}"
    # Algoritmos golden fuera de la vista (read-only buscados por F3-1):
    assert "model_.InitRange(sourceSize_);" in CC_H
    assert "model_.SplitIntoEqualParts(parts, sourceSize_);" in CC_H
    assert "model_.RemoveChop(removeIdx, sourceSize_);" in CC_H
    assert "model_.Sort();" in CC_H
    # El adapter traduce la persistencia y el audio de la vista.
    for a in [
        "owner_.saveChopStateForCurrentSample();",
        "owner_.pushLogicalUndo(action);",
        "owner_.setStatus(message);",
        "Player::GetInstance()->IsStreaming()",
        "SamplePool::GetInstance()->GetSource(owner_.sampleIndex_)",
        "source->GetSampleBuffer(-1)",
        "source->GetChannelCount(-1)",
    ]:
        assert a in SCM_CPP, f"adapter sin {a}"


# ---------------------------------------------------------------------------
# 6. Test host en audit
# ---------------------------------------------------------------------------
def check_audit_wiring():
    assert "run_host_chopper_controller.sh" in AUDIT
    host = ROOT / "tests" / "host" / "chopper_controller_host_test.cpp"
    assert host.exists(), "chopper_controller_host_test.cpp ausente"
    text = host.read_text()
    assert "FakeHost" in text
    assert "ALL OK" in text


def main():
    check_controller_layer()
    check_flows()
    check_strings()
    check_view_host()
    check_delegation()
    check_audit_wiring()
    print("test_f3_3c_baseline: ALL OK")


if __name__ == "__main__":
    main()