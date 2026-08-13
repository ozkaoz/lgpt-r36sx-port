#ifndef APPLICATION_VIEWS_MODALDIALOGS_CHOPPERCONTROLLER_H_
#define APPLICATION_VIEWS_MODALDIALOGS_CHOPPERCONTROLLER_H_

#include <cstdio>
#include <cstdlib>

#include "Application/Views/ModalDialogs/ChopModel.h"
#include "Application/Views/ModalDialogs/PreviewService.h"

/*
 * ChopperController.h -- F3-3c (docs/F3_ARCHITECTURE_ES.md): logica de
 * edicion del chopper, extraida de SampleChopperModal SIN cambiar su
 * comportamiento.  Cada metodo replica exactamente el flujo golden
 * (Bacon 1.2.1, SampleChopperModal.cpp): mismo orden de escrituras,
 * mismos clamps, mismas constantes y mensajes.
 *
 * El controller es puro (header-only): no conoce Player, SamplePool ni la
 * vista.  Los efectos de vista (mensajes, undo logico, persistencia del
 * estado por sample, preview, overlay, dirty) y el acceso a audio (posicion
 * de streaming, buffer WAV para zero-cross) se declaran en Host; la vista
 * implementa el adapter.  Los escalares de estado (sourceSize/cursor/trim/
 * pitch/split/initialized) se vinculan por referencia (la vista los sigue
 * poseyendo para captura/restauro de historia y lectura durante el dibujo).
 *
 * Invariantes: chopModel tiene kMaxBoundaries == MAX_CHOP_BOUNDARIES (101)
 * de la vista y preview_ conserva la semantica de PreviewService
 * (Active/StartFrame/EndFrame).
 */
class ChopperController {
public:
    /* Efectos de vista y acceso a audio del dueno.  Los Query* devuelven
     * informacion; los demas son escrituras golden. */
    struct Host {
        virtual ~Host() {}

        /* Golden setStatus (la vista copia a statusMessage_ y marca dirty). */
        virtual void SetStatus(const char *message) = 0;
        /* Golden setOperationCombo. */
        virtual void SetOperationCombo(const char *combo) = 0;
        /* Golden pushLogicalUndo (captura + push; la vista gestiona el
         * gancho de destructivos). */
        virtual void PushLogicalUndo(const char *action) = 0;
        /* Golden saveChopStateForCurrentSample. */
        virtual void SaveChopState() = 0;
        /* Golden ensureCursorVisible. */
        virtual void EnsureCursorVisible() = 0;
        /* Golden prepareWaveformPreview. */
        virtual void PrepareWaveformPreview() = 0;
        /* Golden publishOverlayState. */
        virtual void PublishOverlayState() = 0;
        /* Golden isDirty_ = true (escritura explicita del flujo). */
        virtual void MarkDirty() = 0;
        /* Golden hasAssignedSample. */
        virtual bool SampleLoaded() = 0;
        /* Golden: devuelve 0=ok, 1="No WAV source", 2="Bad sample buffer". */
        virtual int QuerySnapBuffer(short **samples, int *channels) = 0;
        /* Golden Player::IsStreaming(); si true, escribe la posicion
         * (GetStreamingPosition). */
        virtual bool LiveStreamingPosition(int *frame) = 0;
    };

    /* Ref a los escalares que la vista posee (captura/restauro de historia,
     * dibujo, input los siguen leyendo directamente). */
    ChopperController(Host &host,
                      ChopModel &model,
                      PreviewService &preview,
                      int &sourceSize,
                      int &cursorFrame,
                      bool &trimMode,
                      bool &pitchMode,
                      int &splitParts,
                      bool &chopsInitialized)
        : host_(host),
          model_(model),
          preview_(preview),
          sourceSize_(sourceSize),
          cursorFrame_(cursorFrame),
          trimMode_(trimMode),
          pitchMode_(pitchMode),
          splitParts_(splitParts),
          chopsInitialized_(chopsInitialized) {}

    /* Golden initializeChopsIfNeeded. */
    void InitializeChopsIfNeeded() {
        if (chopsInitialized_ || sourceSize_ <= 1) return;
        model_.InitRange(sourceSize_);
        chopsInitialized_ = true;
    }

    /* Golden addChopAtCursor (incluye el live cut por streaming). */
    void AddChopAtCursor() {
        if (sourceSize_ <= 1) { host_.SetStatus("No sample to chop"); return; }
        InitializeChopsIfNeeded();
        if (model_.boundaryCount >= ChopModel::kMaxBoundaries) { host_.SetStatus("Max 100 chops reached"); return; }
        int frame = cursorFrame_;
        bool liveCut = false;
        int liveFrame = 0;
        if (preview_.Active() && host_.LiveStreamingPosition(&liveFrame)) {
            if (liveFrame >= preview_.StartFrame() && liveFrame <= preview_.EndFrame()) {
                frame = ClampInt(liveFrame, 0, sourceSize_ - 1);
                cursorFrame_ = frame;
                liveCut = true;
            }
        }
        int minEdge = (model_.boundaryCount >= 2) ? model_.boundaries[0] : 0;
        int maxEdge = (model_.boundaryCount >= 2) ? model_.boundaries[model_.boundaryCount - 1] : (sourceSize_ - 1);
        if (frame <= minEdge || frame >= maxEdge) { host_.SetStatus("Cannot chop at edge"); return; }
        for (int i = 0; i < model_.boundaryCount; i++) {
            if (abs(model_.boundaries[i] - frame) <= 1) { host_.SetStatus("Chop already exists"); return; }
        }
        host_.PushLogicalUndo("Add cut");
        // F3-1: append + sort golden en ChopModel.
        model_.Append(frame);
        model_.Sort();
        int idx = model_.Find(frame);
        if (idx > 0) model_.selected = idx - 1;
        model_.ClampSelectedToChops();
        host_.SaveChopState();
        host_.PublishOverlayState();
        char msg[64]; snprintf(msg, sizeof(msg), liveCut ? "Live chop %02d at %d" : "Chop %02d at %d", model_.selected, frame); host_.SetStatus(msg);
    }

    /* Golden deleteSelectedChop. */
    void DeleteSelectedChop() {
        InitializeChopsIfNeeded();
        if (!HasUserChops()) { host_.SetStatus("No chop to delete"); return; }

        /* Chops are stored as boundaries. Deleting a chop removes one internal boundary
           and merges the selected region with a neighbor. Edge boundaries 0/end are never removed. */
        int removeIdx = (model_.selected > 0) ? model_.selected : 1;
        if (removeIdx <= 0 || removeIdx >= model_.boundaryCount - 1) { host_.SetStatus("Cannot delete edge"); return; }

        host_.PushLogicalUndo("Merge cuts");
        // F3-1: shift-remove golden + reinit minimo en ChopModel.
        model_.RemoveChop(removeIdx, sourceSize_);
        model_.ClampSelectedToChops();
        trimMode_ = false;
        cursorFrame_ = SelectedChopStartFrame();
        host_.SaveChopState();
        host_.EnsureCursorVisible();
        host_.PrepareWaveformPreview();
        host_.PublishOverlayState();
        host_.SetStatus("Deleted cut");
    }

    /* Golden selectChop. */
    void SelectChop(int delta) {
        InitializeChopsIfNeeded();
        if (!HasUserChops()) { host_.SetStatus("No user chops"); return; }
        int maxChop = model_.boundaryCount - 2;
        model_.selected = ClampInt(model_.selected + delta, 0, maxChop);
        cursorFrame_ = model_.boundaries[model_.selected];
        host_.SaveChopState();
        host_.EnsureCursorVisible();
        host_.PrepareWaveformPreview();
        host_.PublishOverlayState();
        char msg[64]; snprintf(msg, sizeof(msg), "Selected chop %02d", model_.selected); host_.SetStatus(msg);
    }

    /* Golden hasUserChops. */
    bool HasUserChops() const {
        return (model_.boundaryCount > 2);
    }

    /* Golden hasActiveSliceRange. */
    bool HasActiveSliceRange() const {
        if (model_.boundaryCount < 2 || sourceSize_ <= 1) return false;
        if (model_.boundaryCount > 2) return true;
        return (model_.boundaries[0] > 0 || model_.boundaries[1] < sourceSize_ - 1);
    }

    /* Golden selectedChopStartFrame.
       F3-1: delegado a ChopModel (clamps golden identicos). */
    int SelectedChopStartFrame() const {
        return model_.StartFrameForSelected();
    }

    /* Golden selectedChopEndFrame.
       F3-1: delegado a ChopModel (clamps golden identicos). */
    int SelectedChopEndFrame() const {
        return model_.EndFrameForSelected(sourceSize_);
    }

    /* Golden toggleTrimMode. */
    void ToggleTrimMode() {
        if (pitchMode_) pitchMode_ = false;
        InitializeChopsIfNeeded();
        trimMode_ = !trimMode_;
        if (trimMode_) {
            cursorFrame_ = SelectedChopStartFrame();
            host_.SetStatus("CROP SAMPLE");
        } else {
            host_.SetStatus("Crop mode off");
        }
        host_.SaveChopState();
        host_.EnsureCursorVisible();
        host_.PrepareWaveformPreview();
        host_.PublishOverlayState();
        host_.MarkDirty();
    }

    /* Golden nudgeSelectedStart. */
    void NudgeSelectedStart(int deltaFrames) {
        InitializeChopsIfNeeded();
        if (model_.boundaryCount < 2) { host_.SetStatus("No range to trim"); return; }
        int idx = model_.selected;
        int minFrame = (idx == 0) ? 0 : model_.boundaries[idx - 1] + 1;
        int maxFrame = model_.boundaries[idx + 1] - 1;
        int nextFrame =
            ClampInt(
                model_.boundaries[idx] + deltaFrames,
                minFrame,
                maxFrame);
        if (nextFrame == model_.boundaries[idx]) return;
        host_.PushLogicalUndo("Move cut start");
        model_.boundaries[idx] = nextFrame;
        cursorFrame_ = model_.boundaries[idx];
        host_.SaveChopState();
        host_.EnsureCursorVisible();
        host_.PrepareWaveformPreview();
        host_.PublishOverlayState();
        host_.SetStatus("Adjusted chop start");
    }

    /* Golden nudgeSelectedEnd. */
    void NudgeSelectedEnd(int deltaFrames) {
        InitializeChopsIfNeeded();
        if (model_.boundaryCount < 2) { host_.SetStatus("No range to trim"); return; }
        int idx = model_.selected + 1;
        int minFrame = model_.boundaries[idx - 1] + 1;
        int maxFrame = (idx == model_.boundaryCount - 1) ? (sourceSize_ - 1) : (model_.boundaries[idx + 1] - 1);
        int nextFrame =
            ClampInt(
                model_.boundaries[idx] + deltaFrames,
                minFrame,
                maxFrame);
        if (nextFrame == model_.boundaries[idx]) return;
        host_.PushLogicalUndo("Move cut end");
        model_.boundaries[idx] = nextFrame;
        cursorFrame_ = model_.boundaries[idx];
        host_.SaveChopState();
        host_.EnsureCursorVisible();
        host_.PrepareWaveformPreview();
        host_.PublishOverlayState();
        host_.SetStatus("Adjusted chop end");
    }

    /* Golden cropToSelectedRange (crop logico U2.14, sin reescritura WAV). */
    void CropToSelectedRange() {
        if (sourceSize_ <= 1) { host_.SetStatus("No sample to crop"); return; }
        InitializeChopsIfNeeded();
        if (model_.boundaryCount < 2) { host_.SetStatus("No range to crop"); return; }

        model_.selected = ClampInt(model_.selected, 0, model_.boundaryCount - 2);
        int start = SelectedChopStartFrame();
        int end = SelectedChopEndFrame();
        if (end <= start) { host_.SetStatus("Bad crop range"); return; }

        /* U2.14: safe logical crop. We keep the chosen/trimmed range as a single S01 slice
           and ignore material outside it at playback time. We do not rewrite the WAV file here. */
        host_.PushLogicalUndo("Keep logical range");
        model_.boundaryCount = 2;
        model_.boundaries[0] = start;
        model_.boundaries[1] = end;
        for (int i = 2; i < ChopModel::kMaxBoundaries; i++) model_.boundaries[i] = 0;
        model_.selected = 0;
        trimMode_ = false;
        cursorFrame_ = start;
        host_.SaveChopState();
        host_.EnsureCursorVisible();
        host_.PrepareWaveformPreview();
        host_.PublishOverlayState();
        char msg[64];
        snprintf(msg, sizeof(msg), "Keep range %d-%d", start, end);
        host_.SetStatus(msg);
    }

    /* Golden splitSampleIntoEqualParts. */
    void SplitSampleIntoEqualParts(int parts) {
        InitializeChopsIfNeeded();
        if (sourceSize_ <= 1) { host_.SetStatus("No sample to split"); return; }
        host_.SetOperationCombo("L1 + B");
        if (parts < 2 || parts > 32) parts = 4;
        int step = sourceSize_ / parts;
        if (step < 1) { host_.SetStatus("Sample too small"); return; }
        host_.PushLogicalUndo("Split sample");
        // F3-1: rebuild de boundaryes golden en ChopModel (el guard de
        // step<1 y el status se evaluaron arriba, igual que el golden).
        model_.SplitIntoEqualParts(parts, sourceSize_);
        trimMode_ = false;
        cursorFrame_ = 0;
        host_.SaveChopState();
        host_.EnsureCursorVisible();
        host_.PrepareWaveformPreview();
        host_.PublishOverlayState();
        char m[64];
        snprintf(m, sizeof(m), "Split sample in %d parts", parts);
        host_.SetStatus(m);
    }

    /* Golden clearAllChops. */
    void ClearAllChops() {
        InitializeChopsIfNeeded();
        if (sourceSize_ <= 1) { host_.SetStatus("No sample to clear"); return; }
        host_.PushLogicalUndo("Clear chops");
        // F3-1: estado en ChopModel (rango minimo + cero del resto).
        model_.ClearAll(sourceSize_);
        trimMode_ = false;
        cursorFrame_ = 0;
        host_.SaveChopState();
        host_.EnsureCursorVisible();
        host_.PrepareWaveformPreview();
        host_.PublishOverlayState();
        host_.SetStatus("No cuts (L1+B to split again)");
    }

    /* Golden cycleSplitParts
     * (TREEFROG_U2_39_CHOPPER_SPLIT_ZERO, Bacon 1.1.1): L1+B at 32 parts
     * clears every cut (whole sample shows as a single region, no visible
     * cut lines) and the next L1+B starts the cycle again at 4. */
    void CycleSplitParts() {
        static const int kSplitCycle[] = {4, 8, 16, 32, 0, 4};
        int next = 1;
        for (int i = 0; i < 4; i++) {
            if (splitParts_ == kSplitCycle[i]) { next = i + 1; break; }
        }
        splitParts_ = kSplitCycle[next];
        if (splitParts_ == 0) {
            ClearAllChops();
            return;
        }
        SplitSampleIntoEqualParts(splitParts_);
    }

    /* Golden snapSelectedBoundaryToZeroCross. */
    void SnapSelectedBoundaryToZeroCross(bool isStart) {
        if (!host_.SampleLoaded() || sourceSize_ <= 1) { host_.SetStatus("No sample loaded"); return; }
        InitializeChopsIfNeeded();
        if (model_.boundaryCount < 2) { host_.SetStatus("No chops to snap"); return; }
        short *samples = 0;
        int channels = 0;
        int sn = host_.QuerySnapBuffer(&samples, &channels);
        if (sn == 1) { host_.SetStatus("No WAV source"); return; }
        if (sn == 2) { host_.SetStatus("Bad sample buffer"); return; }

        int idx = model_.selected;
        if (!isStart) idx = ClampInt(idx + 1, 1, model_.boundaryCount - 1);
        if (idx < 0 || idx >= model_.boundaryCount) { host_.SetStatus("Invalid boundary"); return; }
        int frame = model_.boundaries[idx];
        int minFrame = (idx == 0) ? 0 : model_.boundaries[idx - 1] + 1;
        int maxFrame = (idx == model_.boundaryCount - 1) ? (sourceSize_ - 1) : (model_.boundaries[idx + 1] - 1);
        int lo = frame - 64; if (lo < minFrame) lo = minFrame;
        int hi = frame + 64; if (hi > maxFrame) hi = maxFrame;
        int best = frame;
        long bestScore = -1;
        for (int f = lo; f <= hi; f++) {
            long score = 0;
            for (int c = 0; c < channels; c++) {
                int s = samples[f * channels + c];
                score += (s < 0) ? -s : s;
            }
            if (bestScore < 0 || score < bestScore) { bestScore = score; best = f; }
        }
        if (best != frame) {
            host_.PushLogicalUndo(isStart ? "Snap start" : "Snap end");
            model_.boundaries[idx] = best;
            cursorFrame_ = best;
            model_.Sort();
            host_.SaveChopState();
            host_.EnsureCursorVisible();
            host_.PrepareWaveformPreview();
            host_.PublishOverlayState();
            char m[64];
            snprintf(m, sizeof(m), "Zero-cross %s %d", isStart ? "start" : "end", best);
            host_.SetStatus(m);
        } else {
            host_.SetStatus("Already at zero-cross");
        }
    }

private:
    static int ClampInt(int value, int minValue, int maxValue) {
        if (value < minValue) return minValue;
        if (value > maxValue) return maxValue;
        return value;
    }

    Host &host_;
    ChopModel &model_;
    PreviewService &preview_;
    int &sourceSize_;
    int &cursorFrame_;
    bool &trimMode_;
    bool &pitchMode_;
    int &splitParts_;
    bool &chopsInitialized_;
};

#endif  /* APPLICATION_VIEWS_MODALDIALOGS_CHOPPERCONTROLLER_H_ */