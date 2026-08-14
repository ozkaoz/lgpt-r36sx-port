#!/usr/bin/env python3
"""F3-3a baseline (docs/F3_ARCHITECTURE_ES.md): PreviewService (rango de
playback) + ChopperView (geometria zoom/cursor/waveform) extraidos de
SampleChopperModal como capas puras header-only.

La vista conserva audio (Player::*Streaming), mensajes de estado, overlay
(g_chopper*) y la API publica; los algoritmos golden viven en las capas.
Verifica:

1. La API publica de SampleChopperModal sigue intacta (lista F3-1/F3-2
   + preview/zoom/waveform).
2. Los mensajes de estado golden (play/zoom/preview) siguen literalmente
   en el .cpp de la vista.
3. El header de la vista YA NO declara previewActive_ /
   previewStartFrame_ / previewEndFrame_ (raw), y declara
   PreviewService preview_;
4. Los metodos delegados de la vista llaman a PreviewService /
   ChopperView (SetRange/ClearRange/TrimStart/TrimEnd/ClampPlayFrame/
   ClampPlayRange, GetZoomFactor/GetViewFrameCount/ClampViewStart/
   CenterOnCursor/EnsureCursorVisible/FrameToPixel/PixelToFrame/
   NudgeCursorPixels/NudgeZoom/BuildWaveformColumns).
5. PreviewService.h: Range, Active/StartFrame/EndFrame, ClearRange/
   Deactivate, SetRange (clamps en orden golden), TrimStart (5s/220500),
   TrimEnd (1s/44100), ClampPlayFrame, ClampPlayRange.
6. ChopperView.h: constantes golden (288/5/100) y estaticas puras de
   geometria + BuildWaveformColumns.
7. Los runners host y los tests estaticos del tramo existen y estan en
   audit.sh.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SCM_H = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.h").read_text()
SCM_CPP = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.cpp").read_text()
PS_H = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/PreviewService.h").read_text()
CV_H = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/ChopperView.h").read_text()
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
        "void previewPitchSetting();",
        "bool buildPitchedBuffer(int semitones, short **outSamples, int *outFrames, int *outChannels, int *outRate);",
        "bool preparePitchEnvelopePreviewBuffer(short **outSamples, int *outFrames, int *outChannels, int *outRate);",
        "void playFullSample();",
        "void playFromFrame(int frame, const char *label);",
        "void playFrameRange(int startFrame, int endFrame, const char *label);",
        "void playSelectedChop();",
        "void setPreviewPlaybackRange(int startFrame, int endFrame);",
        "void clearPreviewPlaybackRange();",
        "void stopSamplePreview();",
        "void prepareWaveformPreview();",
        "void previewTrimStart();",
        "void previewTrimEnd();",
    ]:
        assert m in SCM_H, f"API publica perdida o privada cambiada: {m}"


# ---------------------------------------------------------------------------
# 2. Mensajes de estado golden (play/zoom/preview + historial)
#    F3-3c: los de edicion (chop/split/delete) viven en ChopperController.
# ---------------------------------------------------------------------------
def check_status_strings():
    for s in [
        '"No sample to play"',
        '"Preview start"',
        '"Preview end"',
        '"Zoom %d%%"',
        '"%s %d"',
        '"%s %d-%d"',
        '"Undo: %.46s"',
        '"Redo: %.46s"',
        '"Pitch/Env sample"',
        '"Scope %s"',
    ]:
        assert s in SCM_CPP, f"Mensaje golden perdido: {s}"
    for s in [
        '"Cannot chop at edge"',
        '"Chop already exists"',
        '"Max 100 chops reached"',
        '"No sample to chop"',
        '"Deleted cut"',
        '"No chop to delete"',
        '"Cannot delete edge"',
        '"Selected chop %02d"',
        '"Split sample in %d parts"',
    ]:
        assert s in CC_H, f"Mensaje golden de edicion perdido de la capa: {s}"


# ---------------------------------------------------------------------------
# 3. Members raw extraidos del header
# ---------------------------------------------------------------------------
def check_raw_removed():
    for raw in [
        "previewActive_",
        "previewStartFrame_",
        "previewEndFrame_",
    ]:
        assert raw not in SCM_H, f"member raw todavia en header: {raw}"
        # En el cpp solo pueden aparecer citados en un comentario golden;
        # ninguna escritura/lectura raw debe quedar.
        assert f"{raw} " not in SCM_CPP.replace("(golden 2472", ""), (
            f"uso raw de {raw} todavia en cpp")


def check_pure_member_declared():
    assert "PreviewService preview_;" in SCM_H, (
        "header no declara PreviewService por valor")
    assert 'ModalDialogs/PreviewService.h"' in SCM_H
    assert 'ModalDialogs/ChopperView.h"' in SCM_H


# ---------------------------------------------------------------------------
# 4. Delegacion en el cpp
# ---------------------------------------------------------------------------
def check_delegation():
    for d in [
        "preview_.SetRange(",
        "preview_.ClearRange()",
        "preview_.Deactivate()",
        "preview_.TrimStart(",
        "preview_.TrimEnd(",
        "preview_.ClampPlayFrame(",
        "preview_.ClampPlayRange(",
        "preview_.Active()",
        "preview_.StartFrame()",
        "preview_.EndFrame()",
        "ChopperView::GetZoomFactor(",
        "ChopperView::GetViewFrameCount(",
        "ChopperView::ClampViewStart(",
        "ChopperView::CenterOnCursor(",
        "ChopperView::EnsureCursorVisible(",
        "ChopperView::FrameToPixel(",
        "ChopperView::PixelToFrame(",
        "ChopperView::NudgeCursorPixels(",
        "ChopperView::NudgeZoom(",
        "ChopperView::BuildWaveformColumns(",
    ]:
        assert d in SCM_CPP, f"delegacion perdida: {d}"


# ---------------------------------------------------------------------------
# 5. Capa PreviewService
# ---------------------------------------------------------------------------
def check_preview_layer():
    assert "class PreviewService" in PS_H
    for m in [
        "struct Range",
        "bool Active() const",
        "int StartFrame() const",
        "int EndFrame() const",
        "void ClearRange()",
        "void Deactivate()",
        "void SetRange(int startFrame, int endFrame, int sourceSize)",
        "static Range TrimStart(",
        "static Range TrimEnd(",
        "static int ClampPlayFrame(",
        "static Range ClampPlayRange(",
    ]:
        assert m in PS_H, f"PreviewService sin {m}"
    for g in [
        "sourceRate * 5 : 220500",
        "sourceRate * 1 : 44100",
    ]:
        assert g in PS_H, f"golden PreviewService falta: {g}"


# ---------------------------------------------------------------------------
# 6. Capa ChopperView
# ---------------------------------------------------------------------------
def check_chopper_layer():
    assert "class ChopperView" in CV_H
    for c in [
        "LGPT_CHOPPER_WAVE_W (288)",
        "LGPT_CHOPPER_MAX_COLUMNS (288)",
        "LGPT_CHOPPER_MIN_ZOOM_PERCENT (5)",
        "LGPT_CHOPPER_MAX_ZOOM_PERCENT (100)",
    ]:
        assert c in CV_H, f"constante golden falta: {c}"
    for m in [
        "static int GetZoomFactor(",
        "static int GetViewFrameCount(",
        "static int ClampViewStart(",
        "static int CenterOnCursor(",
        "static int EnsureCursorVisible(",
        "static int FrameToPixel(",
        "static int PixelToFrame(",
        "static int NudgeCursorPixels(",
        "static int NudgeZoom(",
        "static bool BuildWaveformColumns(",
    ]:
        assert m in CV_H, f"ChopperView sin {m}"


# ---------------------------------------------------------------------------
# 7. Runners y audit
# ---------------------------------------------------------------------------
def check_runners():
    for p in ["tests/run_host_preview_service.sh",
              "tests/run_host_chopper_view.sh"]:
        assert (ROOT / p).exists(), f"runner falta: {p}"
    for run in ["run_host_preview_service.sh", "run_host_chopper_view.sh"]:
        assert run in AUDIT, f"{run} no esta en audit.sh"


def main():
    check_public_api()
    check_status_strings()
    check_raw_removed()
    check_pure_member_declared()
    check_delegation()
    check_preview_layer()
    check_chopper_layer()
    check_runners()
    print("test_f3_3a_baseline: ALL OK")


if __name__ == "__main__":
    main()