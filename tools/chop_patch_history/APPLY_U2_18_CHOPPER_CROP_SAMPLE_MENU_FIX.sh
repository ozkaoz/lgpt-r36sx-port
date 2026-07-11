#!/usr/bin/env bash
# U2.18 Chopper Crop Sample menu fix: SELECT crop menu, centered progress bar,
# robust WAV write, physical crop start+end, undo/redo, and alternate pitch shortcuts.
# Apply on top of U2.17.
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
  echo "ERROR: U2.16/U2.17 adjust-sample pitch model not found. Apply U2.17 first."
  exit 4
fi
if ! grep -q "g_lgptPhysicalUndoSamples" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: U2.17 in-memory destructive edit model not found. Apply U2.17 first."
  exit 5
fi

BACKUP="_backup_before_u2_18_chopper_crop_sample_menu_fix_$STAMP.tar.gz"
tar -czf "$BACKUP" \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Instruments/WavFile.h \
  sources/Application/Instruments/WavFile.cpp \
  projects/Makefile

echo "Backup written: $SRC/$BACKUP"

python3 - <<'U218PY'
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

# WavFile: close read handle before writing same WAV path. This fixes Cannot write crop/pitch.
p = require('sources/Application/Instruments/WavFile.cpp')
s = p.read_text()
old = '''bool WavFile::SaveBufferToPath(const char *path) {
    if (!path || !samples_ || size_ <= 0 || channelCount_ <= 0 || sampleRate_ <= 0) return false;
    FileSystem *fs = FileSystem::GetInstance();
    if (!fs) return false;
    I_File *file = fs->Open(path, "wb");
    if (!file) return false;
'''
new = '''bool WavFile::SaveBufferToPath(const char *path) {
    if (!path || !samples_ || size_ <= 0 || channelCount_ <= 0 || sampleRate_ <= 0) return false;
    /* U2.18: when editing a loaded WAV destructively, this WavFile often still owns
       the original read handle. Close it before opening the same path for writing. */
    if (file_) {
        file_->Close();
        delete file_;
        file_ = 0;
    }
    FileSystem *fs = FileSystem::GetInstance();
    if (!fs) return false;
    I_File *file = fs->Open(path, "wb");
    if (!file) file = fs->Open(path, "w");
    if (!file) return false;
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'U2.18: when editing a loaded WAV destructively' not in s:
    raise SystemExit('Patch failed: WavFile SaveBufferToPath block')
p.write_text(s)

p = require('sources/Application/Views/ModalDialogs/SampleChopperModal.cpp')
s = p.read_text()

# TreeFrog progress globals.
if 'g_chopperOperationActive' not in s:
    anchor = '''static int g_chopperPreviewActive = 0;
static int g_chopperPreviewStartFrame = 0;
static int g_chopperPreviewEndFrame = 0;
'''
    add = '''static int g_chopperOperationActive = 0;
static int g_chopperOperationPercent = 0;
'''
    s = replace_once(s, anchor, anchor + add, 'operation globals')

# TreeFrog centered progress bar drawn after waveform/cursor.
if 'U2.18 centered destructive edit progress bar' not in s:
    marker = '''    } else {
        tf_vline(cx, TF_WAVE_Y - 4, TF_WAVE_Y + TF_WAVE_H + 4, cursor);
        tf_rect(cx - 2, TF_WAVE_Y - 4, 5, 1, marker);
        tf_rect(cx - 2, TF_WAVE_Y + TF_WAVE_H + 3, 5, 1, marker);
    }
}
#else
'''
    repl = '''    } else {
        tf_vline(cx, TF_WAVE_Y - 4, TF_WAVE_Y + TF_WAVE_H + 4, cursor);
        tf_rect(cx - 2, TF_WAVE_Y - 4, 5, 1, marker);
        tf_rect(cx - 2, TF_WAVE_Y + TF_WAVE_H + 3, 5, 1, marker);
    }

    /* U2.18 centered destructive edit progress bar. Kept in the framebuffer overlay
       because text DrawView elements at the waveform rows are overwritten by this overlay. */
    if (g_chopperOperationActive) {
        const unsigned short opBg = tf_rgb565(8, 8, 12);
        const unsigned short opBorder = tf_rgb565(255, 226, 0);
        const unsigned short opFill = tf_rgb565(0, 255, 190);
        const unsigned short opEmpty = tf_rgb565(42, 42, 52);
        int boxX = 52;
        int boxY = 104;
        int boxW = 216;
        int boxH = 32;
        int barX = boxX + 12;
        int barY = boxY + 14;
        int barW = boxW - 24;
        int barH = 8;
        int pct = g_chopperOperationPercent;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        int fillW = (barW * pct) / 100;
        tf_rect(boxX, boxY, boxW, boxH, opBorder);
        tf_rect(boxX + 2, boxY + 2, boxW - 4, boxH - 4, opBg);
        tf_rect(barX, barY, barW, barH, opEmpty);
        if (fillW > 0) tf_rect(barX, barY, fillW, barH, opFill);
        tf_rect(barX, barY - 2, barW, 1, opBorder);
        tf_rect(barX, barY + barH + 1, barW, 1, opBorder);
    }
}
#else
'''
    s = replace_once(s, marker, repl, 'treefrog progress bar')

# show/clear operation globals.
old = '''void SampleChopperModal::showOperationProgress(const char *message, int percent) {
    operationActive_ = true;
    operationPercent_ = clampInt(percent, 0, 100);
    snprintf(operationMessage_, sizeof(operationMessage_), "%s", message ? message : "Working");
    operationMessage_[sizeof(operationMessage_) - 1] = 0;
    char status[64]; snprintf(status, sizeof(status), "%s %d%%", operationMessage_, operationPercent_);
    setStatus(status);
    DrawView();
}
'''
new = '''void SampleChopperModal::showOperationProgress(const char *message, int percent) {
    operationActive_ = true;
    operationPercent_ = clampInt(percent, 0, 100);
    snprintf(operationMessage_, sizeof(operationMessage_), "%s", message ? message : "Working");
    operationMessage_[sizeof(operationMessage_) - 1] = 0;
#if defined(PLATFORM_TREEFROG)
    g_chopperOperationActive = 1;
    g_chopperOperationPercent = operationPercent_;
#endif
    char status[64]; snprintf(status, sizeof(status), "%s %d%%", operationMessage_, operationPercent_);
    setStatus(status);
    DrawView();
    publishOverlayState();
}
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'g_chopperOperationPercent = operationPercent_' not in s:
    raise SystemExit('Patch failed: showOperationProgress')

old = '''void SampleChopperModal::clearOperationProgress() {
    operationActive_ = false;
    operationPercent_ = 0;
    operationMessage_[0] = 0;
    isDirty_ = true;
}
'''
new = '''void SampleChopperModal::clearOperationProgress() {
    operationActive_ = false;
    operationPercent_ = 0;
    operationMessage_[0] = 0;
#if defined(PLATFORM_TREEFROG)
    g_chopperOperationActive = 0;
    g_chopperOperationPercent = 0;
#endif
    isDirty_ = true;
}
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'g_chopperOperationActive = 0;' not in s:
    raise SystemExit('Patch failed: clearOperationProgress')

# Clear overlay state operation globals too.
old = '''    g_chopperPreviewActive = 0;
    g_chopperTrimMode = 0;
#endif
}
'''
new = '''    g_chopperPreviewActive = 0;
    g_chopperOperationActive = 0;
    g_chopperOperationPercent = 0;
    g_chopperTrimMode = 0;
#endif
}
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'g_chopperOperationPercent = 0;' not in s:
    raise SystemExit('Patch failed: clearOverlayState operation globals')

# Move text operation overlay above waveform so it is not overwritten; graphical bar is still centered.
s = s.replace('drawStringAbs(5, 10, "+----------------------------+", props);', 'drawStringAbs(5, 3,  "+----------------------------+", props);')
s = s.replace('drawStringAbs(5, 11, "|     PROCESSING ACTION      |", props);', 'drawStringAbs(5, 4,  "|     PROCESSING ACTION      |", props);')
s = s.replace('drawStringAbs(5, 12, msg, props);', 'drawStringAbs(5, 5,  msg, props);')
s = s.replace('drawStringAbs(5, 13, "+----------------------------+", props);', 'drawStringAbs(5, 6,  "+----------------------------+", props);')

# Replace crop to become true crop-sample action, returning to main Chopper and clearing false S01/full-range state.
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

    showOperationProgress("Crop sample", 0);
    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Crop", boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        clearOperationProgress(); setStatus("Undo capture fail"); return false;
    }
    showOperationProgress("Crop sample", 15);

    int sampleWords = frameCount * channels;
    short *cropped = (short *)malloc(sampleWords * sizeof(short));
    if (!cropped) { clearOperationProgress(); setStatus("No crop memory"); return false; }
    memcpy(cropped, samples + (start * channels), sampleWords * sizeof(short));
    showOperationProgress("Crop sample", 45);

    WavFile *wav = (WavFile *)source;
    bool ok = wav->ReplaceBuffer(cropped, frameCount, channels, rate);
    free(cropped);
    if (!ok) { clearOperationProgress(); setStatus("Cannot crop buffer"); return false; }
    showOperationProgress("Crop sample", 70);
    if (!wav->SaveBufferToPath(samplePath_.c_str())) { clearOperationProgress(); setStatus("Cannot write crop"); return false; }
    showOperationProgress("Crop sample", 85);

    sourceSize_ = frameCount;
    sampleSize_ = frameCount;
    viewStartFrame_ = 0;
    cursorFrame_ = 0;

    /* U2.18: a destructive crop edits the WAV itself. After the crop, the sample is a
       normal shorter sample again, not an artificial one-slice chop. This removes the
       confusing mark at the start and avoids forcing S01 unless the user creates cuts. */
    boundaryCount_ = 2;
    boundaries_[0] = 0;
    boundaries_[1] = frameCount - 1;
    for (int i = 2; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;
    selectedChop_ = 0;
    trimMode_ = false;
    chopsInitialized_ = true;

    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress(); setStatus("Redo capture fail"); return false;
    }

    lgptFinishDestructiveEdit(boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    saveChopStateForCurrentSample();
    cleanupInvalidPhraseChopNotesForCurrentSample();
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    showOperationProgress("Crop sample", 100);
    char msg[64]; snprintf(msg, sizeof(msg), "Cropped to %d frames", frameCount); setStatus(msg);
    return true;
}

'''
s = replace_between(s, 'bool SampleChopperModal::destructiveCropToSelectedRange() {', 'bool SampleChopperModal::undoLastDestructiveCrop() {', new_crop, 'U2.18 crop')

# In undo/redo, keep crop-sample full range as non-chopped when restoring redo full-range; ensure progress is visible.
# Existing undo implementation is kept, but make it use Crop sample wording for crop action and center view.
s = s.replace('showOperationProgress(redo ? "Redo edit" : "Undo edit", 0);', 'showOperationProgress(redo ? "Redo edit" : "Undo edit", 0);')
s = s.replace('prepareWaveformPreview();\n    publishOverlayState();\n    showOperationProgress(redo ? "Redo edit" : "Undo edit", 100);', 'prepareWaveformPreview();\n    centerViewOnCursor();\n    publishOverlayState();\n    showOperationProgress(redo ? "Redo edit" : "Undo edit", 100);')

# X preview already 1s in U2.17, but force it.
s = s.replace('sourceRate_ * 2 : 88200', 'sourceRate_ * 1 : 44100')

# Controls/menu naming.
s = s.replace('A cut/live Y del B play L2+A crop', 'A cut/live Y del B play SELECT crop')
s = s.replace('R2+LR chop R2+A full L2+X undo/redo', 'R2+LR chop R2+A full L2+X undo/redo')
s = s.replace('trimMode_ ? "ADJUST: A/B trim Y start X end1s" : "L2+Y adjust L2+A crop L2+X undo/redo"', 'trimMode_ ? "CROP SAMPLE: A/B range Y start X end1s" : "SELECT crop  R1+B back"')
s = s.replace('Adjust sample: A/B edges R2/L2 pitch', 'Crop sample: A/B range X/R2/L2 pitch')
s = s.replace('Adjust sample off', 'Crop sample off')

# ProcessButtonMask: add SELECT bool and input path. Keep L2+Y as fallback alias.
old = '''    bool y = (mask & EPBM_Y) != 0;
    bool x = (mask & EPBM_X) != 0;
    bool a = (mask & EPBM_A) != 0;
    bool b = (mask & EPBM_B) != 0;
'''
new = '''    bool y = (mask & EPBM_Y) != 0;
    bool x = (mask & EPBM_X) != 0;
    bool select = (mask & EPBM_SELECT) != 0;
    bool a = (mask & EPBM_A) != 0;
    bool b = (mask & EPBM_B) != 0;
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'bool select = (mask & EPBM_SELECT)' not in s:
    raise SystemExit('Patch failed: select bool')

s = s.replace('ADJUST SAMPLE: A+LR adjusts start, B+LR adjusts end, R2/L2+UD/LR destructive pitch +/-1/+/-12.', 'CROP SAMPLE: A+LR adjusts start, B+LR adjusts end; X/R2/L2+UD/LR destructive pitch +/-1/+/-12.')

old = '''    if (l2 && y) {
        toggleTrimMode();
        return;
    }
'''
new = '''    if ((select && !(left || right || up || down || a || b || x || y || l1 || r1 || l2 || r2)) || (l2 && y)) {
        toggleTrimMode();
        return;
    }
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'select && !(left || right' not in s:
    raise SystemExit('Patch failed: select crop mode input')

# Add X+arrows as pitch shortcut in crop sample menu.
old = '''    if (trimMode_ && (r2 || l2) && (up || down || left || right)) {
        int semis = 0;
        if (up) semis = 1;
        else if (down) semis = -1;
        else if (right) semis = 12;
        else if (left) semis = -12;
        destructivePitchSample(semis);
        return;
    }
'''
new = '''    if (trimMode_ && (r2 || l2 || x) && (up || down || left || right)) {
        int semis = 0;
        if (up) semis = 1;
        else if (down) semis = -1;
        else if (right) semis = 12;
        else if (left) semis = -12;
        destructivePitchSample(semis);
        return;
    }
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'trimMode_ && (r2 || l2 || x)' not in s:
    raise SystemExit('Patch failed: X pitch input')

# After successful physical pitch, center redraw and preserve menu. SaveBuffer fix should make it actually write.
s = s.replace('prepareWaveformPreview();\n    publishOverlayState();\n    showOperationProgress(label, 100);', 'prepareWaveformPreview();\n    centerViewOnCursor();\n    publishOverlayState();\n    showOperationProgress(label, 100);')

p.write_text(s)
print('U2.18 patches applied: SELECT crop menu, robust WAV write, centered operation bar, physical crop reset, X/R2/L2 pitch shortcuts.')
U218PY

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_18_CHOPPER_CROP_SAMPLE_MENU_FIX_$STAMP.log"
echo "Starting U2.18 build..."
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
