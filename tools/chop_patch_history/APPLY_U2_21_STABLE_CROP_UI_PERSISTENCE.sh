#!/usr/bin/env bash
# U2.21 Stable Crop UI + Persistence cleanup.
# Applies on top of validated U2.20. Cleans crop keymap, adds destructive delete-selection,
# improves OK/continue overlays, keeps crop mode active for undo/redo, and prepares stable docs.

SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"

cd "$SRC" || { echo "ERROR: cannot enter SRC=$SRC"; exit 2; }

for f in \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Instruments/WavFile.cpp \
  sources/Application/Instruments/SampleInstrument.cpp \
  sources/Application/Views/PhraseView.cpp \
  BUILD_TREEFROG_R36SX_BADGE_OFF.sh \
  BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh \
  projects/Makefile.TREEFROG; do
  test -f "$f" || { echo "ERROR: missing $f"; exit 3; }
done

if ! grep -q "TREEFROG_ENABLE_SELECT=1" BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh; then
  echo "ERROR: SELECT is not enabled. Apply/validate U2.19+ first."
  exit 4
fi
if ! grep -q "lgptWritePersistentChopState" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: expected U2.20 persistent chop sidecar code not found. Apply U2.20 first."
  exit 5
fi
if ! grep -q "OK - press A to continue" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: expected U2.20 operation overlay code not found. Apply U2.20 first."
  exit 6
fi

BACKUP="_backup_before_u2_21_stable_crop_ui_persistence_$STAMP.tar.gz"
tar -czf "$BACKUP" \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Instruments/WavFile.cpp \
  sources/Application/Instruments/SampleInstrument.cpp \
  sources/Application/Views/PhraseView.cpp \
  BUILD_TREEFROG_R36SX_BADGE_OFF.sh \
  BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh \
  projects/Makefile.TREEFROG \
  projects/Makefile

echo "Backup written: $SRC/$BACKUP"

python3 - <<'PY_U221'
from pathlib import Path

root = Path('.')
cpp_path = root/'sources/Application/Views/ModalDialogs/SampleChopperModal.cpp'
h_path = root/'sources/Application/Views/ModalDialogs/SampleChopperModal.h'


def replace_function(text, signature, replacement):
    start = text.find(signature)
    if start < 0:
        raise SystemExit('Patch failed: missing function signature: ' + signature)
    brace = text.find('{', start)
    if brace < 0:
        raise SystemExit('Patch failed: missing opening brace for ' + signature)
    depth = 0
    i = brace
    in_str = in_chr = esc = in_line = in_block = False
    while i < len(text):
        c = text[i]
        n = text[i+1] if i + 1 < len(text) else ''
        if in_line:
            if c == '\n': in_line = False
            i += 1; continue
        if in_block:
            if c == '*' and n == '/':
                in_block = False; i += 2
            else: i += 1
            continue
        if in_str:
            if esc: esc = False
            elif c == '\\': esc = True
            elif c == '"': in_str = False
            i += 1; continue
        if in_chr:
            if esc: esc = False
            elif c == '\\': esc = True
            elif c == "'": in_chr = False
            i += 1; continue
        if c == '/' and n == '/': in_line = True; i += 2; continue
        if c == '/' and n == '*': in_block = True; i += 2; continue
        if c == '"': in_str = True; i += 1; continue
        if c == "'": in_chr = True; i += 1; continue
        if c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                end = i + 1
                while end < len(text) and text[end] in ' \t\r\n': end += 1
                return text[:start] + replacement.rstrip() + '\n\n' + text[end:]
        i += 1
    raise SystemExit('Patch failed: unterminated function: ' + signature)

cpp = cpp_path.read_text()
h = h_path.read_text()

# Header declaration for delete-selection destructive operation.
if 'bool destructiveDeleteSelectedRange();' not in h:
    h = h.replace('    bool destructiveCropToSelectedRange();\n',
                  '    bool destructiveCropToSelectedRange();\n    bool destructiveDeleteSelectedRange();\n')

# Ensure SELECT is documented as the only entry in controls; L2+Y becomes delete-selection only inside crop mode.
new_draw_controls = r'''void SampleChopperModal::drawControls(GUITextProperties &props) {
    SetColor(CD_NORMAL);
    drawStringAbs(0, 24, "R1+LR sample  L1+LR fast cursor", props);
    drawStringAbs(0, 25, "A cut/live Y del B play SELECT crop", props);
    drawStringAbs(0, 26, trimMode_ ? "R1+A keep  L2+Y delete R1+X undo" : "R2+LR chop  R2+A full", props);
    SetColor(CD_HILITE1);
    drawStringAbs(0, 28, trimMode_ ? "CROP: A/B range Y start X end1s" : "SELECT crop  R1+B back", props);
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::drawControls(GUITextProperties &props)', new_draw_controls)

# More explicit centered text overlay. This is separate from the framebuffer bar.
new_draw_overlay = r'''void SampleChopperModal::drawOperationOverlay(GUITextProperties &props) {
    if (!operationActive_) return;
    char msg[64];
    SetColor(CD_HILITE1);
    props.invert_ = true;
    drawStringAbs(3, 7,  "+--------------------------------+", props);
    snprintf(msg, sizeof(msg), "| %-20s %3d%%     |", operationMessage_, operationPercent_);
    msg[34] = 0;
    drawStringAbs(3, 8, msg, props);
    if (operationPercent_ >= 100) {
        drawStringAbs(3, 9,  "| OK                             |", props);
        drawStringAbs(3, 10, "| Press A to continue            |", props);
    } else {
        drawStringAbs(3, 9,  "| Please wait                    |", props);
        drawStringAbs(3, 10, "| Processing sample              |", props);
    }
    drawStringAbs(3, 11, "+--------------------------------+", props);
    props.invert_ = false;
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::drawOperationOverlay(GUITextProperties &props)', new_draw_overlay)

new_show_progress = r'''void SampleChopperModal::showOperationProgress(const char *message, int percent) {
    operationActive_ = true;
    operationPercent_ = clampInt(percent, 0, 100);
    snprintf(operationMessage_, sizeof(operationMessage_), "%s", message ? message : "Working");
    operationMessage_[sizeof(operationMessage_) - 1] = 0;
#if defined(PLATFORM_TREEFROG)
    g_chopperOperationActive = 1;
    g_chopperOperationPercent = operationPercent_;
#endif
    char status[64];
    if (operationPercent_ >= 100) snprintf(status, sizeof(status), "%s OK - press A", operationMessage_);
    else snprintf(status, sizeof(status), "%s %d%%", operationMessage_, operationPercent_);
    setStatus(status);
    DrawView();
    publishOverlayState();
    isDirty_ = true;
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::showOperationProgress(const char *message, int percent)', new_show_progress)

# Keep-range crop: keep crop mode active, present complete overlay, no trailing setStatus that hides OK prompt.
new_crop = r'''bool SampleChopperModal::destructiveCropToSelectedRange() {
    if (!hasAssignedSample() || samplePath_.empty() || sourceSize_ <= 1) { setStatus("No WAV to crop"); return false; }
    if (!lgptEndsWithWav(sampleName_)) { setStatus("Crop WAV only"); return false; }

    initializeChopsIfNeeded();
    if (boundaryCount_ < 2) { setStatus("No range to crop"); return false; }

    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    WavFile *wav = (WavFile *)source;
    if (!source || !wav) { setStatus("No WAV source"); return false; }

    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    int size = source->GetSize(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || channels <= 0 || rate <= 0 || size <= 1) { setStatus("Bad sample buffer"); return false; }

    start = clampInt(start, 0, size - 1);
    end = clampInt(end, start, size - 1);
    int frameCount = end - start + 1;
    if (frameCount <= 1) { setStatus("Empty crop"); return false; }
    if (start == 0 && end == size - 1) { setStatus("Crop unchanged"); return false; }

    stopSamplePreview();
    showOperationProgress("Keep range", 5);

    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Crop", boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        clearOperationProgress(); setStatus("Undo capture fail"); return false;
    }
    showOperationProgress("Keep range", 25);

    int sampleWords = frameCount * channels;
    short *cropped = (short *)malloc(sampleWords * sizeof(short));
    if (!cropped) { clearOperationProgress(); setStatus("No crop memory"); return false; }
    memcpy(cropped, samples + (start * channels), sampleWords * sizeof(short));
    showOperationProgress("Keep range", 50);

    if (!wav->ReplaceBuffer(cropped, frameCount, channels, rate)) {
        free(cropped); clearOperationProgress(); setStatus("Cannot crop buffer"); return false;
    }
    free(cropped);
    showOperationProgress("Keep range", 70);

    if (!wav->SaveBufferToPath(samplePath_.c_str())) {
        wav->ReplaceBuffer(g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                           g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate);
        sourceSize_ = g_lgptPhysicalUndoFrames;
        sampleSize_ = g_lgptPhysicalUndoFrames;
        prepareWaveformPreview(); publishOverlayState();
        clearOperationProgress(); setStatus("Cannot write crop"); return false;
    }
    showOperationProgress("Keep range", 85);

    sourceSize_ = frameCount;
    sampleSize_ = frameCount;
    viewStartFrame_ = 0;
    cursorFrame_ = 0;
    boundaryCount_ = 2;
    boundaries_[0] = 0;
    boundaries_[1] = frameCount - 1;
    for (int i = 2; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;
    selectedChop_ = 0;
    trimMode_ = true;
    chopsInitialized_ = true;

    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }

    lgptFinishDestructiveEdit(boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    saveChopStateForCurrentSample();
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    isDirty_ = true;
    showOperationProgress("Crop complete", 100);
    return true;
}'''
cpp = replace_function(cpp, 'bool SampleChopperModal::destructiveCropToSelectedRange()', new_crop)

# New destructive delete-selection: opposite of keep-range crop; concatenates audio before and after selection.
new_delete = r'''bool SampleChopperModal::destructiveDeleteSelectedRange() {
    if (!hasAssignedSample() || samplePath_.empty() || sourceSize_ <= 1) { setStatus("No WAV to edit"); return false; }
    if (!lgptEndsWithWav(sampleName_)) { setStatus("Delete WAV only"); return false; }

    initializeChopsIfNeeded();
    if (boundaryCount_ < 2) { setStatus("No range to delete"); return false; }

    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    WavFile *wav = (WavFile *)source;
    if (!source || !wav) { setStatus("No WAV source"); return false; }

    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    int size = source->GetSize(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || channels <= 0 || rate <= 0 || size <= 1) { setStatus("Bad sample buffer"); return false; }

    start = clampInt(start, 0, size - 1);
    end = clampInt(end, start, size - 1);
    int removed = end - start + 1;
    int nextSize = size - removed;
    if (nextSize <= 1) { setStatus("Cannot delete all"); return false; }

    stopSamplePreview();
    showOperationProgress("Delete range", 5);

    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Delete", boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        clearOperationProgress(); setStatus("Undo capture fail"); return false;
    }
    showOperationProgress("Delete range", 25);

    int sampleWords = nextSize * channels;
    short *edited = (short *)malloc(sampleWords * sizeof(short));
    if (!edited) { clearOperationProgress(); setStatus("No edit memory"); return false; }

    int outFrame = 0;
    if (start > 0) {
        memcpy(edited, samples, start * channels * sizeof(short));
        outFrame = start;
    }
    int tailFrames = size - end - 1;
    if (tailFrames > 0) {
        memcpy(edited + (outFrame * channels), samples + ((end + 1) * channels), tailFrames * channels * sizeof(short));
    }
    showOperationProgress("Delete range", 50);

    if (!wav->ReplaceBuffer(edited, nextSize, channels, rate)) {
        free(edited); clearOperationProgress(); setStatus("Cannot edit buffer"); return false;
    }
    free(edited);
    showOperationProgress("Delete range", 70);

    if (!wav->SaveBufferToPath(samplePath_.c_str())) {
        wav->ReplaceBuffer(g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                           g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate);
        sourceSize_ = g_lgptPhysicalUndoFrames;
        sampleSize_ = g_lgptPhysicalUndoFrames;
        prepareWaveformPreview(); publishOverlayState();
        clearOperationProgress(); setStatus("Cannot write edit"); return false;
    }
    showOperationProgress("Delete range", 85);

    int oldBoundaries[MAX_CHOP_BOUNDARIES];
    int oldCount = boundaryCount_;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) oldBoundaries[i] = boundaries_[i];
    int nextBoundaries[MAX_CHOP_BOUNDARIES];
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) nextBoundaries[i] = 0;
    int out = 0;
    nextBoundaries[out++] = 0;
    for (int i = 1; i < oldCount - 1 && out < MAX_CHOP_BOUNDARIES - 1; i++) {
        int v = oldBoundaries[i];
        if (v <= start) {
            if (v > nextBoundaries[out - 1]) nextBoundaries[out++] = v;
        } else if (v > end) {
            int shifted = v - removed;
            if (shifted > nextBoundaries[out - 1] && shifted < nextSize - 1) nextBoundaries[out++] = shifted;
        }
    }
    if (nextBoundaries[out - 1] != nextSize - 1 && out < MAX_CHOP_BOUNDARIES) nextBoundaries[out++] = nextSize - 1;
    if (out < 2) { out = 2; nextBoundaries[0] = 0; nextBoundaries[1] = nextSize - 1; }

    sourceSize_ = nextSize;
    sampleSize_ = nextSize;
    boundaryCount_ = out;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = (i < boundaryCount_) ? nextBoundaries[i] : 0;
    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
    viewStartFrame_ = 0;
    cursorFrame_ = boundaries_[selectedChop_];
    trimMode_ = true;
    chopsInitialized_ = true;

    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }

    lgptFinishDestructiveEdit(boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    saveChopStateForCurrentSample();
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    isDirty_ = true;
    showOperationProgress("Delete complete", 100);
    return true;
}'''
# Insert after crop function if not already present; replace if present.
if 'bool SampleChopperModal::destructiveDeleteSelectedRange()' in cpp:
    cpp = replace_function(cpp, 'bool SampleChopperModal::destructiveDeleteSelectedRange()', new_delete)
else:
    sig = 'bool SampleChopperModal::undoLastDestructiveCrop()'
    pos = cpp.find(sig)
    if pos < 0: raise SystemExit('Patch failed: missing undo function for delete insert')
    cpp = cpp[:pos] + new_delete.rstrip() + '\n\n' + cpp[pos:]

# Keep undo/redo in crop mode and keep OK overlay visible.
new_undo = r'''bool SampleChopperModal::undoLastDestructiveCrop() {
    if (!g_lgptPhysicalUndoSamples || !g_lgptPhysicalRedoSamples) { setStatus("No edit undo"); return false; }
    if (g_lgptLastDestructiveEditSampleIndex != sampleIndex_ || g_lgptLastDestructiveEditSamplePath != samplePath_) { setStatus("Undo: wrong sample"); return false; }

    bool redo = g_lgptLastDestructiveEditUndone;
    short *restoreSamples = redo ? g_lgptPhysicalRedoSamples : g_lgptPhysicalUndoSamples;
    int restoreFrames = redo ? g_lgptPhysicalRedoFrames : g_lgptPhysicalUndoFrames;
    int restoreChannels = redo ? g_lgptPhysicalRedoChannels : g_lgptPhysicalUndoChannels;
    int restoreRate = redo ? g_lgptPhysicalRedoRate : g_lgptPhysicalUndoRate;
    int restoreCount = redo ? g_lgptLastDestructiveEditRedoBoundaryCount : g_lgptLastDestructiveEditUndoBoundaryCount;
    int restoreSelected = redo ? g_lgptLastDestructiveEditRedoSelected : g_lgptLastDestructiveEditUndoSelected;
    int *restoreBoundaries = redo ? g_lgptLastDestructiveEditRedoBoundaries : g_lgptLastDestructiveEditUndoBoundaries;

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    WavFile *wav = (WavFile *)source;
    if (!source || !wav) { setStatus("Undo source fail"); return false; }

    stopSamplePreview();
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 10);
    if (!lgptRestorePhysicalSnapshotToWav(wav, samplePath_.c_str(), restoreSamples,
                                          restoreFrames, restoreChannels, restoreRate)) {
        clearOperationProgress(); setStatus("Undo restore fail"); return false;
    }
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 70);

    sourceSize_ = restoreFrames;
    sampleSize_ = restoreFrames;
    boundaryCount_ = clampInt(restoreCount, 2, MAX_CHOP_BOUNDARIES);
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;
    for (int i = 0; i < boundaryCount_; i++) boundaries_[i] = clampInt(restoreBoundaries[i], 0, restoreFrames - 1);
    boundaries_[0] = 0;
    boundaries_[boundaryCount_ - 1] = restoreFrames - 1;
    for (int i = 1; i < boundaryCount_; i++) {
        if (boundaries_[i] <= boundaries_[i - 1]) boundaries_[i] = boundaries_[i - 1] + 1;
        if (boundaries_[i] >= restoreFrames) boundaries_[i] = restoreFrames - 1;
    }
    selectedChop_ = clampInt(restoreSelected, 0, boundaryCount_ - 2);
    viewStartFrame_ = 0;
    cursorFrame_ = boundaries_[selectedChop_];
    trimMode_ = true;
    chopsInitialized_ = true;
    g_lgptLastDestructiveEditUndone = !redo;

    saveChopStateForCurrentSample();
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    isDirty_ = true;
    showOperationProgress(redo ? "Redo complete" : "Undo complete", 100);
    return true;
}'''
cpp = replace_function(cpp, 'bool SampleChopperModal::undoLastDestructiveCrop()', new_undo)

# Physical pitch: final OK prompt; values are applied one semitone per R1+arrow.
old_pitch_tail = 'showOperationProgress(label, 100);\n    char msg[64]; snprintf(msg, sizeof(msg), "Pitch %+d applied", semitones); setStatus(msg);\n    return true;'
if old_pitch_tail in cpp:
    cpp = cpp.replace(old_pitch_tail, 'showOperationProgress("Pitch complete", 100);\n    return true;', 1)
else:
    cpp = cpp.replace('showOperationProgress(label, 100);\n    return true;', 'showOperationProgress("Pitch complete", 100);\n    return true;', 1)

# ProcessButtonMask: strict cleaned keymap.
new_process = r'''void SampleChopperModal::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed) return;
    if (operationActive_ && operationPercent_ >= 100) {
        bool plainA = (mask & EPBM_A) && !(mask & EPBM_LEFT) && !(mask & EPBM_RIGHT) && !(mask & EPBM_UP) && !(mask & EPBM_DOWN) && !(mask & EPBM_L) && !(mask & EPBM_R) && !(mask & EPBM_L2) && !(mask & EPBM_R2) && !(mask & EPBM_B) && !(mask & EPBM_X) && !(mask & EPBM_Y);
        if (plainA) { clearOperationProgress(); DrawView(); publishOverlayState(); }
        return;
    }

    bool left = (mask & EPBM_LEFT) != 0;
    bool right = (mask & EPBM_RIGHT) != 0;
    bool up = (mask & EPBM_UP) != 0;
    bool down = (mask & EPBM_DOWN) != 0;
    bool l1 = (mask & EPBM_L) != 0;
    bool r1 = (mask & EPBM_R) != 0;
    bool l2 = (mask & EPBM_L2) != 0;
    bool r2 = (mask & EPBM_R2) != 0;
    bool y = (mask & EPBM_Y) != 0;
    bool x = (mask & EPBM_X) != 0;
    bool select = (mask & EPBM_SELECT) != 0;
    bool a = (mask & EPBM_A) != 0;
    bool b = (mask & EPBM_B) != 0;

    if (r1 && b && !(left || right || up || down || a || x || y)) {
        stopSamplePreview(); EndModal(0); isDirty_ = true; return;
    }

    if (l2 && b && !(left || right || up || down || a || x || y)) {
        stopSamplePreview(); setStatus("Stop playback"); return;
    }

    if (select && !(left || right || up || down || a || b || x || y || l1 || r1 || l2 || r2)) {
        toggleTrimMode(); return;
    }

    if (trimMode_) {
        if (r1 && a && !(left || right || up || down || b || x || y || l2 || r2)) {
            destructiveCropToSelectedRange(); return;
        }
        if (l2 && y && !(left || right || up || down || a || b || x || r1 || r2)) {
            destructiveDeleteSelectedRange(); return;
        }
        if (r1 && x && !(left || right || up || down || a || b || y || l2 || r2)) {
            undoLastDestructiveCrop(); return;
        }
        if (r1 && (up || down || left || right) && !(a || b || x || y || l2 || r2)) {
            int semis = (up || right) ? 1 : -1;
            destructivePitchSample(semis); return;
        }
        if ((left || right) && (a || b) && !(r1 || r2 || l2 || x || y)) {
            int delta = getFrameStepForEdit();
            if (!right) delta = -delta;
            if (l1) delta *= 10;
            if (a) nudgeSelectedStart(delta); else if (b) nudgeSelectedEnd(delta);
            return;
        }
        if (y && !x && !l1 && !r1 && !l2 && !r2 && !(left || right || up || down || a || b)) {
            previewTrimStart(); return;
        }
        if (x && !y && !l1 && !r1 && !l2 && !r2 && !(left || right || up || down || a || b)) {
            previewTrimEnd(); return;
        }
        if ((a || b) && !(left || right)) {
            setStatus(a ? "Crop: A+LEFT/RIGHT" : "Crop: B+LEFT/RIGHT"); return;
        }
        if (l2 && y) { setStatus("Delete: L2+Y"); return; }
    }

    if (r1 && a) {
        saveChopStateForCurrentSample();
        setStatus("Auto-save on: assign Sxx in Phrase");
        return;
    }
    if (!trimMode_ && r1 && (left || right)) {
        selectSample(right ? 1 : -1); return;
    }
    if (r2 && a) { playFullSample(); return; }
    if (r2 && (left || right)) { selectChop(right ? 1 : -1); return; }

    if (!trimMode_) {
        if (y && !l1 && !r1 && !l2 && !r2) { deleteSelectedChop(); return; }
        if (b && !l1 && !r1 && !l2 && !r2) { playSelectedChop(); return; }
        if (a && !l1 && !r1 && !l2 && !r2) { addChopAtCursor(); return; }
    }
    if (left || right) {
        int deltaPx = right ? 2 : -2;
        if (l1) deltaPx = right ? 24 : -24;
        nudgeCursorPixels(deltaPx); return;
    }
    if (up || down) {
        int deltaPercent = up ? 5 : -5;
        if (l1) deltaPercent = up ? 10 : -10;
        nudgeZoomPercent(deltaPercent); return;
    }
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::ProcessButtonMask(unsigned short mask, bool pressed)', new_process)

cpp_path.write_text(cpp)
h_path.write_text(h)

# Verification markers.
checks = [
    ('select enabled in build script', 'TREEFROG_ENABLE_SELECT=1', (root/'BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh').read_text()),
    ('delete selection function', 'destructiveDeleteSelectedRange', cpp),
    ('L2+Y delete in crop mode', 'destructiveDeleteSelectedRange(); return;', cpp),
    ('no L2+Y entry alias', 'if (!trimMode_ && l2 && y' not in cpp, cpp),
    ('R1+A keep range', 'destructiveCropToSelectedRange(); return;', cpp),
    ('R1+X undo redo', 'undoLastDestructiveCrop(); return;', cpp),
    ('OK continue English', 'Press A to continue', cpp),
    ('pitch +/-1 controls', 'int semis = (up || right) ? 1 : -1;', cpp),
]
for name, expected, hay in checks:
    ok = expected if isinstance(expected, bool) else (expected in hay)
    print(('OK   ' if ok else 'FAIL ') + name)
    if not ok:
        raise SystemExit('Verification failed: ' + name)

print('U2.21 patches applied: SELECT-only crop entry, L2+Y delete selection, stable OK overlay, persistent stable package candidate.')
PY_U221

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_21_STABLE_CROP_UI_PERSISTENCE_$STAMP.log"
echo "Starting U2.21 build..."
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh 2>&1 | tee "$LOG"
RC=${PIPESTATUS[0]}

echo
echo "BUILD_RC=$RC"
echo "LOG=$SRC/$LOG"
if [ "$RC" -eq 0 ]; then
  ls -lh "$SRC/dist/lgpt_libretro.so"
  sha256sum "$SRC/dist/lgpt_libretro.so"
else
  echo "Build failed. Relevant errors:"
  grep -nE "error:|undefined reference|No such file|fatal error|make.*Error|Error [0-9]+" "$LOG" | sed -n '1,220p'
  tail -n 120 "$LOG"
fi
exit "$RC"
