#!/usr/bin/env bash
# U2.16 Chopper adjust-sample workflow: progress overlay, undo/redo, 2s end preview, destructive pitch.
# Apply on top of U2.15.
set -eu
SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"

cd "$SRC" || { echo "ERROR: cannot enter SRC=$SRC"; exit 2; }

for f in \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/PhraseView.cpp \
  sources/Application/Instruments/WavFile.h \
  sources/Application/Instruments/WavFile.cpp; do
  test -f "$f" || { echo "ERROR: required file missing: $f"; exit 3; }
done

if ! grep -q "Auto-save on: assign Sxx in Phrase" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: U2.15 autosave chopper model not found. Apply U2.15 first."
  exit 4
fi
if ! grep -q "undoLastDestructiveCrop" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: U2.15 destructive crop/undo model not found. Apply U2.15 first."
  exit 5
fi

BACKUP="_backup_before_u2_16_chopper_adjust_sample_progress_pitch_undoredo_$STAMP.tar.gz"
tar -czf "$BACKUP" \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/PhraseView.cpp \
  sources/Application/Instruments/WavFile.h \
  sources/Application/Instruments/WavFile.cpp \
  projects/Makefile

echo "Backup written: $SRC/$BACKUP"

python3 - <<'U216PY'
from pathlib import Path


def require(path):
    p = Path(path)
    if not p.exists():
        raise SystemExit(f"Patch failed: missing {path}")
    return p


def replace_once(s, old, new, label):
    if old not in s:
        raise SystemExit(f"Patch failed: {label}")
    return s.replace(old, new, 1)


def replace_between(s, start, end, new, label):
    a = s.find(start)
    if a < 0:
        raise SystemExit(f"Patch failed: {label} start")
    b = s.find(end, a + len(start))
    if b < 0:
        raise SystemExit(f"Patch failed: {label} end")
    return s[:a] + new + s[b:]

# Header additions.
p = require('sources/Application/Views/ModalDialogs/SampleChopperModal.h')
s = p.read_text()
if 'bool operationActive_;' not in s:
    s = replace_once(s,
        '    bool previewActive_;\n    int previewStartFrame_;\n    int previewEndFrame_;\n',
        '    bool previewActive_;\n    int previewStartFrame_;\n    int previewEndFrame_;\n    bool operationActive_;\n    int operationPercent_;\n    char operationMessage_[64];\n',
        'operation state fields')
if 'bool destructivePitchSample(int semitones);' not in s:
    s = replace_once(s,
        '    bool undoLastDestructiveCrop();\n    void previewTrimStart();\n    void previewTrimEnd();\n',
        '    bool undoLastDestructiveCrop();\n    bool destructivePitchSample(int semitones);\n    void previewTrimStart();\n    void previewTrimEnd();\n    void showOperationProgress(const char *message, int percent);\n    void clearOperationProgress();\n    void drawOperationOverlay(GUITextProperties &props);\n',
        'destructive pitch/progress declarations')
p.write_text(s)

p = require('sources/Application/Views/ModalDialogs/SampleChopperModal.cpp')
s = p.read_text()
if '#include <math.h>' not in s:
    s = replace_once(s, '#include <stdint.h>\n', '#include <stdint.h>\n#include <math.h>\n', 'math include')

old_globals = '''static std::string g_lgptLastDestructiveCropSamplePath;\nstatic std::string g_lgptLastDestructiveCropBackupPath;\nstatic int g_lgptLastDestructiveCropSampleIndex = -1;\n'''
new_globals = r'''static std::string g_lgptLastDestructiveEditSamplePath;
static std::string g_lgptLastDestructiveEditUndoPath;
static std::string g_lgptLastDestructiveEditRedoPath;
static std::string g_lgptLastDestructiveEditAction;
static int g_lgptLastDestructiveEditSampleIndex = -1;
static bool g_lgptLastDestructiveEditUndone = false;
static int g_lgptLastDestructiveEditUndoSize = 0;
static int g_lgptLastDestructiveEditRedoSize = 0;
static int g_lgptLastDestructiveEditUndoSelected = 0;
static int g_lgptLastDestructiveEditRedoSelected = 0;
static int g_lgptLastDestructiveEditUndoBoundaryCount = 0;
static int g_lgptLastDestructiveEditRedoBoundaryCount = 0;
static int g_lgptLastDestructiveEditUndoBoundaries[LGPT_CHOPPER_SAVED_BOUNDARIES];
static int g_lgptLastDestructiveEditRedoBoundaries[LGPT_CHOPPER_SAVED_BOUNDARIES];
'''
if old_globals in s:
    s = s.replace(old_globals, new_globals, 1)
elif 'g_lgptLastDestructiveEditSamplePath' not in s:
    raise SystemExit('Patch failed: destructive edit globals')

if 'static void lgptStoreDestructiveEditSnapshot' not in s:
    anchor = 'static bool lgptEndsWithWav(const std::string &name) {\n'
    helpers = r'''static void lgptStoreDestructiveEditSnapshot(int *dstBoundaries,
                                            int &dstCount,
                                            int &dstSelected,
                                            int &dstSize,
                                            const int *srcBoundaries,
                                            int srcCount,
                                            int selected,
                                            int size) {
    dstCount = srcCount;
    if (dstCount < 0) dstCount = 0;
    if (dstCount > LGPT_CHOPPER_SAVED_BOUNDARIES) dstCount = LGPT_CHOPPER_SAVED_BOUNDARIES;
    dstSelected = selected;
    dstSize = size;
    for (int i = 0; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) dstBoundaries[i] = 0;
    for (int i = 0; i < dstCount; i++) dstBoundaries[i] = srcBoundaries[i];
}

static void lgptBeginDestructiveEdit(const std::string &samplePath,
                                     int sampleIndex,
                                     const char *action,
                                     const int *boundaries,
                                     int boundaryCount,
                                     int selectedChop,
                                     int sourceSize) {
    g_lgptLastDestructiveEditSamplePath = samplePath;
    g_lgptLastDestructiveEditUndoPath = samplePath + ".u2undo";
    g_lgptLastDestructiveEditRedoPath = samplePath + ".u2redo";
    g_lgptLastDestructiveEditAction = action ? action : "Edit";
    g_lgptLastDestructiveEditSampleIndex = sampleIndex;
    g_lgptLastDestructiveEditUndone = false;
    lgptStoreDestructiveEditSnapshot(g_lgptLastDestructiveEditUndoBoundaries,
                                     g_lgptLastDestructiveEditUndoBoundaryCount,
                                     g_lgptLastDestructiveEditUndoSelected,
                                     g_lgptLastDestructiveEditUndoSize,
                                     boundaries, boundaryCount, selectedChop, sourceSize);
}

static void lgptFinishDestructiveEdit(const int *boundaries,
                                      int boundaryCount,
                                      int selectedChop,
                                      int sourceSize) {
    lgptStoreDestructiveEditSnapshot(g_lgptLastDestructiveEditRedoBoundaries,
                                     g_lgptLastDestructiveEditRedoBoundaryCount,
                                     g_lgptLastDestructiveEditRedoSelected,
                                     g_lgptLastDestructiveEditRedoSize,
                                     boundaries, boundaryCount, selectedChop, sourceSize);
    g_lgptLastDestructiveEditUndone = false;
}

'''
    s = replace_once(s, anchor, helpers + anchor, 'destructive edit snapshot helpers')

# Constructor init.
if 'operationActive_(false)' not in s:
    s = replace_once(s,
        '      previewActive_(false),\n      previewStartFrame_(0),\n      previewEndFrame_(0),\n',
        '      previewActive_(false),\n      previewStartFrame_(0),\n      previewEndFrame_(0),\n      operationActive_(false),\n      operationPercent_(0),\n',
        'constructor operation init')
    s = replace_once(s, '    statusMessage_[0] = 0;\n', '    statusMessage_[0] = 0;\n    operationMessage_[0] = 0;\n', 'constructor operation message init')

# Replace crop and undo functions with U2.16 versions.
new_crop = r'''bool SampleChopperModal::destructiveCropToSelectedRange() {
    if (!hasAssignedSample() || samplePath_.empty() || sourceSize_ <= 1) { setStatus("No WAV to crop"); return false; }
    if (!lgptEndsWithWav(sampleName_)) { setStatus("Crop WAV only"); return false; }
    initializeChopsIfNeeded();
    if (boundaryCount_ < 2) { setStatus("No range to crop"); return false; }
    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();
    if (end <= start) { setStatus("Bad crop range"); return false; }

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    if (!source) { setStatus("No source"); return false; }
    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    int size = source->GetSize(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || channels <= 0 || rate <= 0 || size <= 1) { setStatus("Bad sample buffer"); return false; }
    if (start < 0) start = 0;
    if (end >= size) end = size - 1;
    int frameCount = end - start + 1;
    if (frameCount <= 0) { setStatus("Empty crop"); return false; }
    stopSamplePreview();

    showOperationProgress("Crop WAV", 0);
    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Crop", boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    FileSystemService fs;
    fs.Copy(Path(samplePath_.c_str()), Path(g_lgptLastDestructiveEditUndoPath.c_str()));
    showOperationProgress("Crop WAV", 20);

    int sampleWords = frameCount * channels;
    short *cropped = (short *)malloc(sampleWords * sizeof(short));
    if (!cropped) { clearOperationProgress(); setStatus("No crop memory"); return false; }
    memcpy(cropped, samples + (start * channels), sampleWords * sizeof(short));
    showOperationProgress("Crop WAV", 45);

    WavFile *wav = (WavFile *)source;
    bool ok = wav->ReplaceBuffer(cropped, frameCount, channels, rate);
    free(cropped);
    if (!ok) { clearOperationProgress(); setStatus("Cannot crop buffer"); return false; }
    showOperationProgress("Crop WAV", 70);
    if (!wav->SaveBufferToPath(samplePath_.c_str())) { clearOperationProgress(); setStatus("Cannot write crop"); return false; }
    fs.Copy(Path(samplePath_.c_str()), Path(g_lgptLastDestructiveEditRedoPath.c_str()));
    showOperationProgress("Crop WAV", 90);

    sourceSize_ = frameCount;
    sampleSize_ = frameCount;
    viewStartFrame_ = 0;
    cursorFrame_ = 0;
    boundaryCount_ = 2;
    boundaries_[0] = 0;
    boundaries_[1] = frameCount - 1;
    for (int i = 2; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;
    selectedChop_ = 0;
    trimMode_ = false;
    chopsInitialized_ = true;
    lgptFinishDestructiveEdit(boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    saveChopStateForCurrentSample();
    prepareWaveformPreview();
    publishOverlayState();
    showOperationProgress("Crop WAV", 100);
    clearOperationProgress();
    char msg[64]; snprintf(msg, sizeof(msg), "Cropped WAV %d fr", frameCount); setStatus(msg);
    return true;
}

'''
s = replace_between(s, 'bool SampleChopperModal::destructiveCropToSelectedRange() {', 'bool SampleChopperModal::undoLastDestructiveCrop() {', new_crop, 'destructive crop replacement')

new_undo = r'''bool SampleChopperModal::undoLastDestructiveCrop() {
    if (g_lgptLastDestructiveEditUndoPath.empty() || g_lgptLastDestructiveEditRedoPath.empty()) { setStatus("No edit undo"); return false; }
    if (g_lgptLastDestructiveEditSampleIndex != sampleIndex_ || g_lgptLastDestructiveEditSamplePath != samplePath_) { setStatus("Undo: wrong sample"); return false; }

    bool redo = g_lgptLastDestructiveEditUndone;
    const std::string &restorePath = redo ? g_lgptLastDestructiveEditRedoPath : g_lgptLastDestructiveEditUndoPath;
    int restoreCount = redo ? g_lgptLastDestructiveEditRedoBoundaryCount : g_lgptLastDestructiveEditUndoBoundaryCount;
    int restoreSelected = redo ? g_lgptLastDestructiveEditRedoSelected : g_lgptLastDestructiveEditUndoSelected;
    int restoreSize = redo ? g_lgptLastDestructiveEditRedoSize : g_lgptLastDestructiveEditUndoSize;
    int *restoreBoundaries = redo ? g_lgptLastDestructiveEditRedoBoundaries : g_lgptLastDestructiveEditUndoBoundaries;

    stopSamplePreview();
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 0);
    FileSystemService fs;
    fs.Copy(Path(restorePath.c_str()), Path(samplePath_.c_str()));
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 35);

    WavFile *reload = WavFile::Open(samplePath_.c_str());
    if (!reload) { clearOperationProgress(); setStatus("Undo reload fail"); return false; }
    reload->GetBuffer(0, reload->GetSize(-1));
    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    WavFile *wav = (WavFile *)source;
    int restoredSize = reload->GetSize(-1);
    bool ok = wav && wav->ReplaceBuffer((short *)reload->GetSampleBuffer(-1), restoredSize, reload->GetChannelCount(-1), reload->GetSampleRate(-1));
    delete reload;
    if (!ok || restoredSize <= 1) { clearOperationProgress(); setStatus("Undo buffer fail"); return false; }
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 70);

    sourceSize_ = restoredSize;
    sampleSize_ = restoredSize;
    if (restoreSize > 0 && restoreSize != restoredSize && restoreCount >= 2) restoreSize = restoredSize;
    boundaryCount_ = clampInt(restoreCount, 2, MAX_CHOP_BOUNDARIES);
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;
    for (int i = 0; i < boundaryCount_; i++) boundaries_[i] = clampInt(restoreBoundaries[i], 0, restoredSize - 1);
    boundaries_[0] = 0;
    boundaries_[boundaryCount_ - 1] = restoredSize - 1;
    for (int i = 1; i < boundaryCount_; i++) {
        if (boundaries_[i] <= boundaries_[i - 1]) boundaries_[i] = boundaries_[i - 1] + 1;
        if (boundaries_[i] >= restoredSize) boundaries_[i] = restoredSize - 1;
    }
    selectedChop_ = clampInt(restoreSelected, 0, boundaryCount_ - 2);
    viewStartFrame_ = 0;
    cursorFrame_ = boundaries_[selectedChop_];
    trimMode_ = false;
    chopsInitialized_ = true;
    g_lgptLastDestructiveEditUndone = !redo;
    saveChopStateForCurrentSample();
    prepareWaveformPreview();
    publishOverlayState();
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 100);
    clearOperationProgress();
    char msg[64]; snprintf(msg, sizeof(msg), "%s %s", redo ? "Redo" : "Undo", g_lgptLastDestructiveEditAction.c_str()); setStatus(msg);
    return true;
}

'''
s = replace_between(s, 'bool SampleChopperModal::undoLastDestructiveCrop() {', 'void SampleChopperModal::previewTrimStart() {', new_undo, 'undo/redo replacement')

# Add pitch function before previewTrimStart if absent.
if 'bool SampleChopperModal::destructivePitchSample(int semitones)' not in s:
    pitch_fn = r'''bool SampleChopperModal::destructivePitchSample(int semitones) {
    if (!hasAssignedSample() || samplePath_.empty() || sourceSize_ <= 1) { setStatus("No WAV to pitch"); return false; }
    if (!lgptEndsWithWav(sampleName_)) { setStatus("Pitch WAV only"); return false; }
    if (semitones == 0) { setStatus("Pitch unchanged"); return false; }
    if (semitones < -24) semitones = -24;
    if (semitones > 24) semitones = 24;

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    if (!source) { setStatus("No source"); return false; }
    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    int size = source->GetSize(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || channels <= 0 || rate <= 0 || size <= 1) { setStatus("Bad sample buffer"); return false; }

    double ratio = pow(2.0, ((double)semitones) / 12.0);
    if (ratio <= 0.0) { setStatus("Bad pitch ratio"); return false; }
    int nextSize = (int)(((double)size / ratio) + 0.5);
    if (nextSize < 2) nextSize = 2;
    if (nextSize > 40000000) { setStatus("Pitch too large"); return false; }
    int sampleWords = nextSize * channels;
    short *pitched = (short *)malloc(sampleWords * sizeof(short));
    if (!pitched) { setStatus("No pitch memory"); return false; }

    stopSamplePreview();
    char label[32]; snprintf(label, sizeof(label), "Pitch %+d", semitones);
    showOperationProgress(label, 0);
    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Pitch", boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    FileSystemService fs;
    fs.Copy(Path(samplePath_.c_str()), Path(g_lgptLastDestructiveEditUndoPath.c_str()));
    showOperationProgress(label, 20);

    for (int i = 0; i < nextSize; i++) {
        double srcPos = (double)i * ratio;
        int idx = (int)srcPos;
        double frac = srcPos - (double)idx;
        if (idx < 0) idx = 0;
        if (idx >= size - 1) { idx = size - 1; frac = 0.0; }
        int idx2 = idx + 1;
        if (idx2 >= size) idx2 = size - 1;
        for (int ch = 0; ch < channels; ch++) {
            int a = samples[idx * channels + ch];
            int b = samples[idx2 * channels + ch];
            int v = (int)((double)a + ((double)(b - a) * frac));
            if (v < -32768) v = -32768;
            if (v > 32767) v = 32767;
            pitched[i * channels + ch] = (short)v;
        }
        if (i == nextSize / 4) showOperationProgress(label, 35);
        if (i == nextSize / 2) showOperationProgress(label, 50);
        if (i == (nextSize * 3) / 4) showOperationProgress(label, 65);
    }

    WavFile *wav = (WavFile *)source;
    bool ok = wav->ReplaceBuffer(pitched, nextSize, channels, rate);
    free(pitched);
    if (!ok) { clearOperationProgress(); setStatus("Cannot pitch buffer"); return false; }
    showOperationProgress(label, 80);
    if (!wav->SaveBufferToPath(samplePath_.c_str())) { clearOperationProgress(); setStatus("Cannot write pitch"); return false; }
    fs.Copy(Path(samplePath_.c_str()), Path(g_lgptLastDestructiveEditRedoPath.c_str()));
    showOperationProgress(label, 90);

    int oldBoundaries[MAX_CHOP_BOUNDARIES];
    int oldCount = boundaryCount_;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) oldBoundaries[i] = boundaries_[i];
    int nextBoundaries[MAX_CHOP_BOUNDARIES];
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) nextBoundaries[i] = 0;
    int out = 0;
    double scale = (size > 1) ? ((double)(nextSize - 1) / (double)(size - 1)) : 1.0;
    for (int i = 0; i < oldCount && i < MAX_CHOP_BOUNDARIES; i++) {
        int v = (int)(((double)oldBoundaries[i] * scale) + 0.5);
        if (v < 0) v = 0;
        if (v >= nextSize) v = nextSize - 1;
        if (out == 0 || v > nextBoundaries[out - 1]) nextBoundaries[out++] = v;
    }
    if (out <= 0) nextBoundaries[out++] = 0;
    nextBoundaries[0] = 0;
    if (nextBoundaries[out - 1] != nextSize - 1) {
        if (out < MAX_CHOP_BOUNDARIES) nextBoundaries[out++] = nextSize - 1;
        else nextBoundaries[out - 1] = nextSize - 1;
    }
    if (out < 2) { out = 2; nextBoundaries[0] = 0; nextBoundaries[1] = nextSize - 1; }
    boundaryCount_ = out;
    for (int i = 0; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = (i < boundaryCount_) ? nextBoundaries[i] : 0;

    sourceSize_ = nextSize;
    sampleSize_ = nextSize;
    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
    cursorFrame_ = boundaries_[selectedChop_];
    viewStartFrame_ = 0;
    chopsInitialized_ = true;
    lgptFinishDestructiveEdit(boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    saveChopStateForCurrentSample();
    prepareWaveformPreview();
    publishOverlayState();
    showOperationProgress(label, 100);
    clearOperationProgress();
    char msg[64]; snprintf(msg, sizeof(msg), "Pitch %+d applied", semitones); setStatus(msg);
    return true;
}

'''
    s = replace_once(s, 'void SampleChopperModal::previewTrimStart() {', pitch_fn + 'void SampleChopperModal::previewTrimStart() {', 'pitch function insertion')

# Preview end from 5s to 2s.
s = s.replace('''    int previewStart = end - (sourceRate_ > 0 ? sourceRate_ * 5 : 220500);\n    if (previewStart < start) previewStart = start;\n    playFrameRange(previewStart, end, "Preview end");\n''', '''    int previewStart = end - (sourceRate_ > 0 ? sourceRate_ * 2 : 88200);\n    if (previewStart < start) previewStart = start;\n    playFrameRange(previewStart, end, "Preview end");\n''')
# If literal escaped did not match due to actual newlines, do direct.
s = s.replace('''    int previewStart = end - (sourceRate_ > 0 ? sourceRate_ * 5 : 220500);
    if (previewStart < start) previewStart = start;
    playFrameRange(previewStart, end, "Preview end");
''', '''    int previewStart = end - (sourceRate_ > 0 ? sourceRate_ * 2 : 88200);
    if (previewStart < start) previewStart = start;
    playFrameRange(previewStart, end, "Preview end");
''')

# Add progress UI methods before DrawView.
if 'void SampleChopperModal::showOperationProgress' not in s:
    progress_methods = r'''void SampleChopperModal::showOperationProgress(const char *message, int percent) {
    operationActive_ = true;
    operationPercent_ = clampInt(percent, 0, 100);
    snprintf(operationMessage_, sizeof(operationMessage_), "%s", message ? message : "Working");
    operationMessage_[sizeof(operationMessage_) - 1] = 0;
    char status[64]; snprintf(status, sizeof(status), "%s %d%%", operationMessage_, operationPercent_);
    setStatus(status);
    DrawView();
}

void SampleChopperModal::clearOperationProgress() {
    operationActive_ = false;
    operationPercent_ = 0;
    operationMessage_[0] = 0;
    isDirty_ = true;
}

void SampleChopperModal::drawOperationOverlay(GUITextProperties &props) {
    if (!operationActive_) return;
    char msg[64];
    SetColor(CD_HILITE1);
    props.invert_ = true;
    drawStringAbs(5, 10, "+----------------------------+", props);
    drawStringAbs(5, 11, "|        PROCESSING          |", props);
    snprintf(msg, sizeof(msg), "| %-18s %3d%% |", operationMessage_, operationPercent_);
    msg[29] = 0;
    drawStringAbs(5, 12, msg, props);
    drawStringAbs(5, 13, "+----------------------------+", props);
    props.invert_ = false;
}

'''
    s = replace_once(s, 'void SampleChopperModal::DrawView() {', progress_methods + 'void SampleChopperModal::DrawView() {', 'progress methods insertion')

s = s.replace('clearTextScreen(); drawTopBar(props); drawFrame(props); drawSampleInfo(props); if (!hasWaveform_) drawEmptyWaveformText(props); drawControls(props); publishOverlayState();',
              'clearTextScreen(); drawTopBar(props); drawFrame(props); drawSampleInfo(props); if (!hasWaveform_) drawEmptyWaveformText(props); drawControls(props); drawOperationOverlay(props); publishOverlayState();')

# Rename TRIM UI to ADJUST SAMPLE.
s = s.replace('trimMode_ ? " TRIM" : ""', 'trimMode_ ? " ADJ" : ""')
s = s.replace('"TRIM: A+LR start B+LR end Y start X end"', '"ADJUST: A/B edges Y start X end"')
s = s.replace('"L2+Y trim L2+A crop L2+X undo R1+B"', '"L2+Y adjust L2+A crop L2+X undo/redo"')
s = s.replace('"R2+LR chop R2+A full L2+X undo"', '"R2+LR chop R2+A full L2+X undo/redo"')

# ProcessButtonMask pitch in adjust mode. Put before generic R2 sample/chop handling.
old_r2_full = '''    if (r2 && a) {
        playFullSample();
        return;
    }

    if (r2 && (left || right)) {
        selectChop(right ? 1 : -1);
        return;
    }
'''
new_r2_full = '''    if (trimMode_ && r2 && (up || down || left || right)) {
        int semis = 0;
        if (up) semis = 1;
        else if (down) semis = -1;
        else if (right) semis = 12;
        else if (left) semis = -12;
        destructivePitchSample(semis);
        return;
    }

    if (r2 && a) {
        playFullSample();
        return;
    }

    if (r2 && (left || right)) {
        selectChop(right ? 1 : -1);
        return;
    }
'''
if old_r2_full in s:
    s = s.replace(old_r2_full, new_r2_full, 1)
elif 'destructivePitchSample(semis);' not in s:
    raise SystemExit('Patch failed: R2 pitch handling')

# Adjust mode help/status text.
s = s.replace('setStatus(trimMode_ ? "Trim: A+LR start B+LR end" : "Trim mode off");',
              'setStatus(trimMode_ ? "Adjust sample: A/B edges R2 pitch" : "Adjust sample off");')
s = s.replace('setStatus(a ? "Trim: A+LEFT/RIGHT" : "Trim: B+LEFT/RIGHT");',
              'setStatus(a ? "Adjust: A+LEFT/RIGHT" : "Adjust: B+LEFT/RIGHT");')
s = s.replace('   TRIM: A+LR adjusts start, B+LR adjusts end; A/B alone do not add or play.',
              '   ADJUST SAMPLE: A+LR adjusts start, B+LR adjusts end, R2+UD/LR destructive pitch +/-1/+/-12.')

p.write_text(s)

print('U2.16 patches applied: adjust-sample UI, 2s end preview, progress overlay, undo/redo, destructive pitch +/-1/+/-12.')
U216PY

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_16_CHOPPER_ADJUST_SAMPLE_PROGRESS_PITCH_UNDOREDO_$STAMP.log"
echo "Starting U2.16 build..."
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
  grep -nE "error:|undefined reference|No such file|fatal error|make.*Error|Error [0-9]+" "$LOG" | sed -n '1,320p'
  tail -n 200 "$LOG"
fi
exit "$RC"
