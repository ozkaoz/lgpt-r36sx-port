#!/usr/bin/env bash
# U2.19 Select Crop Sample exact fix.
# Applies on top of U2.18/FIX1. Enables SELECT and rewrites the destructive edit path deterministically.

SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"

cd "$SRC" || { echo "ERROR: cannot enter SRC=$SRC"; exit 2; }

for f in \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Instruments/WavFile.cpp \
  BUILD_TREEFROG_R36SX_BADGE_OFF.sh \
  BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh \
  projects/Makefile.TREEFROG; do
  test -f "$f" || { echo "ERROR: missing $f"; exit 3; }
done

if ! grep -q "destructiveCropToSelectedRange" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: expected U2.15+ chopper destructive edit code not found. Apply U2.18/FIX1 first."
  exit 4
fi

BACKUP="_backup_before_u2_19_select_crop_sample_exact_fix_$STAMP.tar.gz"
tar -czf "$BACKUP" \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Instruments/WavFile.cpp \
  BUILD_TREEFROG_R36SX_BADGE_OFF.sh \
  BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh \
  projects/Makefile.TREEFROG \
  projects/Makefile

echo "Backup written: $SRC/$BACKUP"

python3 - <<'PY_U219'
from pathlib import Path

root = Path('.')
cpp_path = root/'sources/Application/Views/ModalDialogs/SampleChopperModal.cpp'
wav_path = root/'sources/Application/Instruments/WavFile.cpp'

def replace_function(text, signature, replacement):
    start = text.find(signature)
    if start < 0:
        raise SystemExit('Patch failed: missing function signature: ' + signature)
    brace = text.find('{', start)
    if brace < 0:
        raise SystemExit('Patch failed: missing opening brace for ' + signature)
    depth = 0
    i = brace
    in_str = False
    in_chr = False
    esc = False
    in_line = False
    in_block = False
    while i < len(text):
        c = text[i]
        n = text[i+1] if i + 1 < len(text) else ''
        if in_line:
            if c == '\n': in_line = False
            i += 1; continue
        if in_block:
            if c == '*' and n == '/':
                in_block = False; i += 2
            else:
                i += 1
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
        if c == '/' and n == '/':
            in_line = True; i += 2; continue
        if c == '/' and n == '*':
            in_block = True; i += 2; continue
        if c == '"':
            in_str = True; i += 1; continue
        if c == "'":
            in_chr = True; i += 1; continue
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                end = i + 1
                while end < len(text) and text[end] in ' \t\r\n':
                    end += 1
                return text[:start] + replacement.rstrip() + '\n\n' + text[end:]
        i += 1
    raise SystemExit('Patch failed: unterminated function: ' + signature)

# 1) Enable SELECT in the TreeFrog build. The user compile log showed TREEFROG_ENABLE_SELECT=0.
for rel in ['BUILD_TREEFROG_R36SX_BADGE_OFF.sh', 'BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh', 'projects/Makefile.TREEFROG']:
    p = root/rel
    s = p.read_text()
    s = s.replace('TREEFROG_ENABLE_SELECT=0', 'TREEFROG_ENABLE_SELECT=1')
    s = s.replace('TREEFROG_ENABLE_SELECT ?= 0', 'TREEFROG_ENABLE_SELECT ?= 1')
    s = s.replace('TREEFROG_ENABLE_SELECT= 0', 'TREEFROG_ENABLE_SELECT=1')
    p.write_text(s)

# 2) Resolve logical sample aliases before destructive WAV writes.
wav = wav_path.read_text()
new_save = r'''bool WavFile::SaveBufferToPath(const char *path) {
    if (!path || !samples_ || size_ <= 0 || channelCount_ <= 0 || sampleRate_ <= 0) return false;

    Path outPath(path);
    std::string resolvedPath = outPath.GetPath();
    if (resolvedPath.empty()) return false;

    /* U2.19: destructive sample edits pass logical paths such as samples:break.wav.
       The loaded WAV was opened through Path::GetPath(), so writes must do the same.
       Also close the read handle before opening the same file for writing. */
    if (file_) {
        file_->Close();
        delete file_;
        file_ = 0;
    }

    FileSystem *fs = FileSystem::GetInstance();
    if (!fs) return false;
    I_File *file = fs->Open(resolvedPath.c_str(), "wb");
    if (!file) file = fs->Open(resolvedPath.c_str(), "w");
    if (!file) return false;

    unsigned int dataBytes = (unsigned int)(size_ * channelCount_ * 2);
    unsigned int riffBytes = 36 + dataBytes;
    unsigned int byteRate = (unsigned int)(sampleRate_ * channelCount_ * 2);
    unsigned short blockAlign = (unsigned short)(channelCount_ * 2);

    unsigned int chunk = Swap32(0x46464952); /* RIFF */
    file->Write(&chunk, 1, 4);
    lgptWavWriteU32(file, riffBytes);
    chunk = Swap32(0x45564157); /* WAVE */
    file->Write(&chunk, 1, 4);
    chunk = Swap32(0x20746D66); /* fmt  */
    file->Write(&chunk, 1, 4);
    lgptWavWriteU32(file, 16);
    lgptWavWriteU16(file, 1);
    lgptWavWriteU16(file, (unsigned short)channelCount_);
    lgptWavWriteU32(file, (unsigned int)sampleRate_);
    lgptWavWriteU32(file, byteRate);
    lgptWavWriteU16(file, blockAlign);
    lgptWavWriteU16(file, 16);
    chunk = Swap32(0x61746164); /* data */
    file->Write(&chunk, 1, 4);
    lgptWavWriteU32(file, dataBytes);
    file->Write(samples_, 2, size_ * channelCount_);
    file->Close();
    delete file;
    return true;
}'''
wav = replace_function(wav, 'bool WavFile::SaveBufferToPath(const char *path)', new_save)
wav_path.write_text(wav)

cpp = cpp_path.read_text()
cpp = cpp.replace('    cleanupInvalidPhraseChopNotesForCurrentSample();\n', '')
cpp = cpp.replace('    /* U2.18 FIX1: no phrase cleanup call here; crop resets this WAV to a normal shortened sample. */\n', '')

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
    showOperationProgress("Crop sample", 5);

    lgptBeginDestructiveEdit(samplePath_, sampleIndex_, "Crop", boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                                     g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate)) {
        clearOperationProgress();
        setStatus("Undo capture fail");
        return false;
    }
    showOperationProgress("Crop sample", 25);

    int sampleWords = frameCount * channels;
    short *cropped = (short *)malloc(sampleWords * sizeof(short));
    if (!cropped) {
        clearOperationProgress();
        setStatus("No crop memory");
        return false;
    }
    memcpy(cropped, samples + (start * channels), sampleWords * sizeof(short));
    showOperationProgress("Crop sample", 50);

    if (!wav->ReplaceBuffer(cropped, frameCount, channels, rate)) {
        free(cropped);
        clearOperationProgress();
        setStatus("Cannot crop buffer");
        return false;
    }
    free(cropped);
    showOperationProgress("Crop sample", 70);

    if (!wav->SaveBufferToPath(samplePath_.c_str())) {
        wav->ReplaceBuffer(g_lgptPhysicalUndoSamples, g_lgptPhysicalUndoFrames,
                           g_lgptPhysicalUndoChannels, g_lgptPhysicalUndoRate);
        sourceSize_ = g_lgptPhysicalUndoFrames;
        sampleSize_ = g_lgptPhysicalUndoFrames;
        prepareWaveformPreview();
        publishOverlayState();
        clearOperationProgress();
        setStatus("Cannot write crop");
        return false;
    }
    showOperationProgress("Crop sample", 85);

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

    if (!lgptCapturePhysicalSnapshot(source, g_lgptPhysicalRedoSamples, g_lgptPhysicalRedoFrames,
                                     g_lgptPhysicalRedoChannels, g_lgptPhysicalRedoRate)) {
        clearOperationProgress();
        setStatus("Redo capture fail");
        return false;
    }

    lgptFinishDestructiveEdit(boundaries_, boundaryCount_, selectedChop_, sourceSize_);
    saveChopStateForCurrentSample();
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    isDirty_ = true;
    showOperationProgress("Crop sample", 100);
    char msg[64];
    snprintf(msg, sizeof(msg), "Cropped %d-%d", start, end);
    setStatus(msg);
    return true;
}'''
cpp = replace_function(cpp, 'bool SampleChopperModal::destructiveCropToSelectedRange()', new_crop)

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
        clearOperationProgress();
        setStatus("Undo restore fail");
        return false;
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
    trimMode_ = false;
    chopsInitialized_ = true;
    g_lgptLastDestructiveEditUndone = !redo;

    saveChopStateForCurrentSample();
    prepareWaveformPreview();
    centerViewOnCursor();
    publishOverlayState();
    isDirty_ = true;
    showOperationProgress(redo ? "Redo edit" : "Undo edit", 100);
    char msg[64];
    snprintf(msg, sizeof(msg), "%s %s", redo ? "Redo" : "Undo", g_lgptLastDestructiveEditAction.c_str());
    setStatus(msg);
    return true;
}'''
cpp = replace_function(cpp, 'bool SampleChopperModal::undoLastDestructiveCrop()', new_undo)

new_toggle = r'''void SampleChopperModal::toggleTrimMode() {
    if (sourceSize_ <= 1) { setStatus("No sample to crop"); return; }
    initializeChopsIfNeeded();
    if (boundaryCount_ < 2) { setStatus("No crop range"); return; }
    trimMode_ = !trimMode_;
    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
    cursorFrame_ = selectedChopStartFrame();
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    setStatus(trimMode_ ? "CROP SAMPLE: set range" : "Crop sample off");
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::toggleTrimMode()', new_toggle)

new_controls = r'''void SampleChopperModal::drawControls(GUITextProperties &props) {
    SetColor(CD_NORMAL);
    drawStringAbs(0, 24, "R1+LR sample  L1+LR fast cursor", props);
    drawStringAbs(0, 25, "A cut/live Y del B play SELECT crop", props);
    drawStringAbs(0, 26, trimMode_ ? "R1+A crop R1+X undo R1+UD pitch" : "R2+LR chop  R2+A full", props);
    SetColor(CD_HILITE1);
    drawStringAbs(0, 28, trimMode_ ? "CROP: A/B range Y start X end1s" : "SELECT crop  R1+B back", props);
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::drawControls(GUITextProperties &props)', new_controls)

new_process = r'''void SampleChopperModal::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed) return;
    if (operationActive_ && operationPercent_ >= 100) clearOperationProgress();

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
        stopSamplePreview();
        EndModal(0);
        isDirty_ = true;
        return;
    }

    if (l2 && b && !(left || right || up || down || a || x || y)) {
        stopSamplePreview();
        setStatus("Stop playback");
        return;
    }

    if (select && !(left || right || up || down || a || b || x || y || l1 || r1 || l2 || r2)) {
        toggleTrimMode();
        return;
    }

    if (trimMode_) {
        if (((r1 && a) || (l2 && a)) && !(left || right || up || down || b || x || y)) {
            destructiveCropToSelectedRange();
            return;
        }
        if (((r1 && x) || (l2 && x)) && !(left || right || up || down || a || b || y)) {
            undoLastDestructiveCrop();
            return;
        }
        if (r1 && (up || down || left || right) && !(a || b || x || y)) {
            int semis = 0;
            if (up) semis = 1; else if (down) semis = -1; else if (right) semis = 12; else if (left) semis = -12;
            destructivePitchSample(semis);
            return;
        }
        if ((r2 || l2) && (up || down || left || right) && !(a || b || x || y)) {
            int semis = 0;
            if (up) semis = 1; else if (down) semis = -1; else if (right) semis = 12; else if (left) semis = -12;
            destructivePitchSample(semis);
            return;
        }
        if (l2 && y && !(left || right || up || down || a || b || x)) {
            toggleTrimMode();
            return;
        }
        if ((left || right) && (a || b) && !(r1 || r2 || l2 || x || y)) {
            int delta = getFrameStepForEdit();
            if (!right) delta = -delta;
            if (l1) delta *= 10;
            if (a) nudgeSelectedStart(delta); else if (b) nudgeSelectedEnd(delta);
            return;
        }
        if (y && !x && !l1 && !r1 && !l2 && !r2 && !(left || right || up || down || a || b)) {
            previewTrimStart();
            return;
        }
        if (x && !y && !l1 && !r1 && !l2 && !r2 && !(left || right || up || down || a || b)) {
            previewTrimEnd();
            return;
        }
        if ((a || b) && !(left || right)) {
            setStatus(a ? "Crop: A+LEFT/RIGHT" : "Crop: B+LEFT/RIGHT");
            return;
        }
    }

    if (!trimMode_ && l2 && y && !(left || right || up || down || a || b || x)) {
        toggleTrimMode();
        return;
    }
    if (!trimMode_ && l2 && x && !(left || right || up || down || a || b || y)) {
        undoLastDestructiveCrop();
        return;
    }
    if (r1 && a) {
        saveChopStateForCurrentSample();
        setStatus("Auto-save on: assign Sxx in Phrase");
        return;
    }
    if (!trimMode_ && r1 && (left || right)) {
        selectSample(right ? 1 : -1);
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
    if (!trimMode_) {
        if (y && !l1 && !r1 && !l2 && !r2) { deleteSelectedChop(); return; }
        if (b && !l1 && !r1 && !l2 && !r2) { playSelectedChop(); return; }
        if (a && !l1 && !r1 && !l2 && !r2) { addChopAtCursor(); return; }
    }
    if (left || right) {
        int deltaPx = right ? 2 : -2;
        if (l1) deltaPx = right ? 24 : -24;
        nudgeCursorPixels(deltaPx);
        return;
    }
    if (up || down) {
        int deltaPercent = up ? 5 : -5;
        if (l1) deltaPercent = up ? 10 : -10;
        nudgeZoomPercent(deltaPercent);
        return;
    }
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::ProcessButtonMask(unsigned short mask, bool pressed)', new_process)

cpp_path.write_text(cpp)

checks = []
for rel in ['BUILD_TREEFROG_R36SX_BADGE_OFF.sh', 'BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh', 'projects/Makefile.TREEFROG']:
    txt = (root/rel).read_text()
    checks.append((rel + ' SELECT=1', 'TREEFROG_ENABLE_SELECT=1' in txt or 'TREEFROG_ENABLE_SELECT ?= 1' in txt))
wav_txt = wav_path.read_text()
cpp_txt = cpp_path.read_text()
checks.extend([
    ('WavFile SaveBufferToPath resolves Path', 'Path outPath(path);' in wav_txt and 'resolvedPath.c_str()' in wav_txt),
    ('Crop uses R1+A in trim mode', 'R1+A crop' in cpp_txt and 'destructiveCropToSelectedRange();' in cpp_txt),
    ('Undo uses R1+X in trim mode', 'R1+X undo' in cpp_txt and 'undoLastDestructiveCrop();' in cpp_txt),
    ('SELECT is read in ProcessButtonMask', 'EPBM_SELECT' in cpp_txt and 'select &&' in cpp_txt),
    ('Crop progress reaches 100', 'showOperationProgress("Crop sample", 100);' in cpp_txt),
])
print('U2.19 verification:')
failed = False
for name, ok in checks:
    print(('OK   ' if ok else 'FAIL ') + name)
    if not ok: failed = True
if failed:
    raise SystemExit('U2.19 verification failed; not building.')
print('U2.19 patches applied: SELECT enabled, alias-safe WAV write, exact crop/undo/pitch controls.')
PY_U219

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_19_SELECT_CROP_SAMPLE_EXACT_FIX_$STAMP.log"
echo "Starting U2.19 build..."
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh 2>&1 | tee "$LOG"
RC=${PIPESTATUS[0]}

echo
echo "BUILD_RC=$RC"
echo "LOG=$SRC/$LOG"
if [ "$RC" -eq 0 ]; then
  ls -lh "$SRC/dist/lgpt_libretro.so"
  sha256sum "$SRC/dist/lgpt_libretro.so"
  echo "VERIFY_COMPILE_FLAG_SELECT:"
  grep -n "TREEFROG_ENABLE_SELECT=1" "$LOG" | head -n 5 || true
else
  echo "Build failed. Relevant errors:"
  grep -nE "error:|undefined reference|No such file|fatal error|make.*Error|Error [0-9]+" "$LOG" | sed -n '1,320p'
  tail -n 200 "$LOG"
fi
exit "$RC"
