#!/usr/bin/env bash
# U2.17 Chopper destructive-edit fixes: 1s end preview, visible completion overlay,
# in-memory undo/redo, stronger crop start/end, and L2/R2 pitch shortcuts.
# Apply on top of U2.16.
set -eu
SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"

cd "$SRC" || { echo "ERROR: cannot enter SRC=$SRC"; exit 2; }

for f in \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Instruments/WavFile.h \
  sources/Application/Instruments/WavFile.cpp; do
  test -f "$f" || { echo "ERROR: required file missing: $f"; exit 3; }
done

if ! grep -q "destructivePitchSample(int semitones)" sources/Application/Views/ModalDialogs/SampleChopperModal.h; then
  echo "ERROR: U2.16 adjust-sample pitch model not found. Apply U2.16 first."
  exit 4
fi
if ! grep -q "showOperationProgress" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: U2.16 operation progress model not found. Apply U2.16 first."
  exit 5
fi

BACKUP="_backup_before_u2_17_chopper_destructive_edit_fix_$STAMP.tar.gz"
tar -czf "$BACKUP" \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Instruments/WavFile.h \
  sources/Application/Instruments/WavFile.cpp \
  projects/Makefile

echo "Backup written: $SRC/$BACKUP"

python3 - <<'U217PY'
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

p = require('sources/Application/Views/ModalDialogs/SampleChopperModal.cpp')
s = p.read_text()

# Add in-memory physical undo/redo buffers after the U2.16 destructive edit globals.
if 'g_lgptPhysicalUndoSamples' not in s:
    anchor = '''static int g_lgptLastDestructiveEditUndoBoundaries[LGPT_CHOPPER_SAVED_BOUNDARIES];\nstatic int g_lgptLastDestructiveEditRedoBoundaries[LGPT_CHOPPER_SAVED_BOUNDARIES];\n'''
    addition = r'''
static short *g_lgptPhysicalUndoSamples = 0;
static short *g_lgptPhysicalRedoSamples = 0;
static int g_lgptPhysicalUndoFrames = 0;
static int g_lgptPhysicalRedoFrames = 0;
static int g_lgptPhysicalUndoChannels = 0;
static int g_lgptPhysicalRedoChannels = 0;
static int g_lgptPhysicalUndoRate = 0;
static int g_lgptPhysicalRedoRate = 0;
'''
    s = replace_once(s, anchor, anchor + addition, 'physical undo globals')

# Add buffer snapshot helpers before lgptStoreDestructiveEditSnapshot.
if 'static void lgptFreePhysicalSnapshot' not in s:
    anchor = 'static void lgptStoreDestructiveEditSnapshot(int *dstBoundaries,\n'
    helpers = r'''static void lgptFreePhysicalSnapshot(short *&buffer, int &frames, int &channels, int &rate) {
    if (buffer) free(buffer);
    buffer = 0;
    frames = 0;
    channels = 0;
    rate = 0;
}

static bool lgptCapturePhysicalSnapshot(SoundSource *source,
                                        short *&buffer,
                                        int &frames,
                                        int &channels,
                                        int &rate) {
    lgptFreePhysicalSnapshot(buffer, frames, channels, rate);
    if (!source) return false;
    frames = source->GetSize(-1);
    channels = source->GetChannelCount(-1);
    rate = source->GetSampleRate(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || frames <= 1 || channels <= 0 || rate <= 0) {
        frames = 0; channels = 0; rate = 0;
        return false;
    }
    int words = frames * channels;
    buffer = (short *)malloc(words * sizeof(short));
    if (!buffer) {
        frames = 0; channels = 0; rate = 0;
        return false;
    }
    memcpy(buffer, samples, words * sizeof(short));
    return true;
}

static bool lgptRestorePhysicalSnapshotToWav(WavFile *wav,
                                             const char *path,
                                             short *buffer,
                                             int frames,
                                             int channels,
                                             int rate) {
    if (!wav || !path || !buffer || frames <= 1 || channels <= 0 || rate <= 0) return false;
    if (!wav->ReplaceBuffer(buffer, frames, channels, rate)) return false;
    return wav->SaveBufferToPath(path);
}

'''
    s = replace_once(s, anchor, helpers + anchor, 'physical undo helpers')

# Make begin edit clear old snapshots only when a new destructive edit starts.
if 'lgptFreePhysicalSnapshot(g_lgptPhysicalUndoSamples' not in s:
    needle = '''    g_lgptLastDestructiveEditSamplePath = samplePath;\n'''
    s = replace_once(s, needle, '''    lgptFreePhysicalSnapshot(g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames, g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate);\n    lgptFreePhysicalSnapshot(g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames, g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate);\n''' + needle, 'clear physical snapshots on edit begin')

# Replace destructive crop with in-memory undo capture and no fragile FileSystem copy dependency.
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
    if (frameCount <= 1) { setStatus("Empty crop"); return false; }
    if (start == 0 && end == size - 1) { setStatus("Crop unchanged"); return false; }
    stopSamplePreview();

    showOperationProgress("Crop WAV", 0);
    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Crop", boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        clearOperationProgress(); setStatus("Undo capture fail"); return false;
    }
    showOperationProgress("Crop WAV", 15);

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
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }
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
    char msg[64]; snprintf(msg, sizeof(msg), "Cropped %d-%d", start, end); setStatus(msg);
    return true;
}

'''
s = replace_between(s, 'bool SampleChopperModal::destructiveCropToSelectedRange() {', 'bool SampleChopperModal::undoLastDestructiveCrop() {', new_crop, 'destructive crop U2.17')

# Replace undo/redo with in-memory snapshot restore, no WavFile::Open reload.
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
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 0);
    if (!lgptRestorePhysicalSnapshotToWav(wav, samplePath_.c_str(), restoreSamples,
                                          restoreFrames, restoreChannels, restoreRate)) {
        clearOperationProgress(); setStatus("Undo restore fail"); return false;
    }
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 60);

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
    trimMode_ = false;
    chopsInitialized_ = true;
    g_lgptLastDestructiveEditUndone = !redo;
    saveChopStateForCurrentSample();
    prepareWaveformPreview();
    publishOverlayState();
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 100);
    char msg[64]; snprintf(msg, sizeof(msg), "%s %s", redo ? "Redo" : "Undo", g_lgptLastDestructiveEditAction.c_str()); setStatus(msg);
    return true;
}

'''
s = replace_between(s, 'bool SampleChopperModal::undoLastDestructiveCrop() {', 'bool SampleChopperModal::destructivePitchSample(int semitones) {', new_undo, 'undo/redo U2.17')

# Replace destructive pitch with memory undo capture and better progress latch.
new_pitch = r'''bool SampleChopperModal::destructivePitchSample(int semitones) {
    if (!trimMode_) { setStatus("Use ADJUST SAMPLE first"); return false; }
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
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        free(pitched); clearOperationProgress(); setStatus("Undo capture fail"); return false;
    }
    showOperationProgress(label, 15);

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
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }
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
    char msg[64]; snprintf(msg, sizeof(msg), "Pitch %+d applied", semitones); setStatus(msg);
    return true;
}

'''
s = replace_between(s, 'bool SampleChopperModal::destructivePitchSample(int semitones) {', 'void SampleChopperModal::previewTrimStart() {', new_pitch, 'destructive pitch U2.17')

# X preview = final 1 second.
s = s.replace('sourceRate_ * 2 : 88200', 'sourceRate_ * 1 : 44100')

# Keep 100% overlay visible until next key press; clear at start of next input.
old = '''void SampleChopperModal::ProcessButtonMask(unsigned short mask, bool pressed) {\n    if (!pressed) return;\n'''
new = '''void SampleChopperModal::ProcessButtonMask(unsigned short mask, bool pressed) {\n    if (!pressed) return;\n    if (operationActive_ && operationPercent_ >= 100) clearOperationProgress();\n'''
if old in s:
    s = s.replace(old, new, 1)
elif 'operationPercent_ >= 100' not in s:
    raise SystemExit('Patch failed: operation latch clear')

# More explicit overlay title.
s = s.replace('|        PROCESSING          |', '|     PROCESSING ACTION      |')

# L2 or R2 arrows pitch inside ADJUST SAMPLE.
old_pitch_input = '''    if (trimMode_ && r2 && (up || down || left || right)) {\n        int semis = 0;\n        if (up) semis = 1;\n        else if (down) semis = -1;\n        else if (right) semis = 12;\n        else if (left) semis = -12;\n        destructivePitchSample(semis);\n        return;\n    }\n'''
new_pitch_input = '''    if (trimMode_ && (r2 || l2) && (up || down || left || right)) {\n        int semis = 0;\n        if (up) semis = 1;\n        else if (down) semis = -1;\n        else if (right) semis = 12;\n        else if (left) semis = -12;\n        destructivePitchSample(semis);\n        return;\n    }\n'''
if old_pitch_input in s:
    s = s.replace(old_pitch_input, new_pitch_input, 1)
elif 'trimMode_ && (r2 || l2)' not in s:
    raise SystemExit('Patch failed: L2/R2 pitch input')

# Adjust control text.
s = s.replace('ADJUST: A/B edges Y start X end', 'ADJUST: A/B trim Y start X end1s')
s = s.replace('L2+Y adjust L2+A crop L2+X undo/redo', 'L2+Y adjust L2+A crop L2+X undo/redo')
s = s.replace('R2+UD/LR destructive pitch +/-1/+/-12', 'R2/L2+UD/LR destructive pitch +/-1/+/-12')
s = s.replace('Adjust sample: A/B edges R2 pitch', 'Adjust sample: A/B edges R2/L2 pitch')

p.write_text(s)
print('U2.17 patches applied: in-memory undo/redo, visible 100% operation overlay, 1s end preview, L2/R2 physical pitch shortcuts, stronger crop start/end.')
U217PY

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_17_CHOPPER_DESTRUCTIVE_EDIT_FIX_$STAMP.log"
echo "Starting U2.17 build..."
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
