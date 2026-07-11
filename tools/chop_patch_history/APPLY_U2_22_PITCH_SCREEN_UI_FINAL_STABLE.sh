#!/usr/bin/env bash
# U2.22 Pitch Screen + Operation UI cleanup + Stable package candidate.
# Applies on top of validated U2.21. Keeps the already validated crop/persist/Sxx flow.
# Main changes: L1+R1 opens a non-destructive pitch selection screen, A applies, B previews;
# operation feedback is drawn as centered text without the old green framebuffer bar overlay.

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
  echo "ERROR: SELECT is not enabled. Apply U2.19+ first."
  exit 4
fi
if ! grep -q "lgptWritePersistentChopState" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: persistent chop sidecar code not found. Apply U2.20+ first."
  exit 5
fi
if ! grep -q "destructiveDeleteSelectedRange" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: U2.21 delete-selection code not found. Apply U2.21 first."
  exit 6
fi

BACKUP="_backup_before_u2_22_pitch_screen_ui_final_stable_$STAMP.tar.gz"
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

python3 - <<'PY_U222'
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

# Header: add pitch-screen state and declarations.
if 'bool pitchMode_;' not in h:
    h = h.replace('    bool trimMode_;\n', '    bool trimMode_;\n    bool pitchMode_;\n    int pitchSemitones_;\n', 1)
for decl in [
    '    void togglePitchMode();\n',
    '    void nudgePitchSemitones(int delta);\n',
    '    void drawPitchScreen(GUITextProperties &props);\n',
    '    bool buildPitchedBuffer(int semitones, short **outSamples, int *outFrames, int *outChannels, int *outRate);\n',
    '    bool writePreviewPitchWav(short *samples, int frames, int channels, int rate, std::string &logicalPath);\n',
    '    void previewPitchSetting();\n',
]:
    if decl.strip() not in h:
        h = h.replace('    bool destructivePitchSample(int semitones);\n', '    bool destructivePitchSample(int semitones);\n' + decl, 1)

# Constructor: initialize pitch state.
if 'pitchMode_(false)' not in cpp:
    cpp = cpp.replace('      trimMode_(false),\n', '      trimMode_(false),\n      pitchMode_(false),\n      pitchSemitones_(0),\n', 1)

# Ensure reset paths clear pitch mode.
cpp = cpp.replace('    trimMode_ = false;\n    selectedChop_ = 0;\n', '    trimMode_ = false;\n    pitchMode_ = false;\n    pitchSemitones_ = 0;\n    selectedChop_ = 0;\n', 1)
cpp = cpp.replace('        trimMode_ = false;\n        selectedChop_ = 0;\n        boundaryCount_ = 0;\n', '        trimMode_ = false;\n        pitchMode_ = false;\n        pitchSemitones_ = 0;\n        selectedChop_ = 0;\n        boundaryCount_ = 0;\n', 1)
cpp = cpp.replace('    trimMode_ = false;\n    selectedChop_ = 0;\n    boundaryCount_ = 0;\n', '    trimMode_ = false;\n    pitchMode_ = false;\n    pitchSemitones_ = 0;\n    selectedChop_ = 0;\n    boundaryCount_ = 0;\n', 1)

# Operation should not use framebuffer overlay. It will be text-only, centered, with waveform overlay disabled.
new_publish = r'''void SampleChopperModal::publishOverlayState() {
#if defined(PLATFORM_TREEFROG)
    g_chopperOverlayActive = (hasWaveform_ && !operationActive_ && !pitchMode_) ? 1 : 0;
    g_chopperCursorPx = frameToPixel(cursorFrame_);
    if (g_chopperCursorPx < 0) g_chopperCursorPx = 0;
    for (int i = 0; i < MAX_COLUMNS; i++) { g_chopperMinColumn[i] = minColumn_[i]; g_chopperMaxColumn[i] = maxColumn_[i]; }
    g_chopperMarkerCount = 0;
    for (int b = 1; b < boundaryCount_ - 1 && g_chopperMarkerCount < TF_MAX_CHOP_MARKERS; b++) {
        int px = frameToPixel(boundaries_[b]);
        if (px >= 0) g_chopperMarkerPx[g_chopperMarkerCount++] = px;
    }
    g_chopperSelectedStartPx = -1;
    g_chopperSelectedEndPx = -1;
    g_chopperSelectedRangeStartPx = -1;
    g_chopperSelectedRangeEndPx = -1;
    g_chopperTrimMode = trimMode_ ? 1 : 0;
    g_chopperViewStartFrame = viewStartFrame_;
    g_chopperViewFrameCount = getViewFrameCount();
    g_chopperPreviewActive = previewActive_ ? 1 : 0;
    g_chopperPreviewStartFrame = previewStartFrame_;
    g_chopperPreviewEndFrame = previewEndFrame_;
    if (hasActiveSliceRange() && selectedChop_ >= 0 && selectedChop_ <= boundaryCount_ - 2) {
        int startFrame = boundaries_[selectedChop_];
        int endFrame = boundaries_[selectedChop_ + 1];
        int viewFrames = getViewFrameCount();
        int viewEnd = viewStartFrame_ + viewFrames - 1;
        g_chopperSelectedStartPx = frameToPixel(startFrame);
        g_chopperSelectedEndPx = frameToPixel(endFrame);
        int clipStart = startFrame < viewStartFrame_ ? viewStartFrame_ : startFrame;
        int clipEnd = endFrame > viewEnd ? viewEnd : endFrame;
        if (clipStart <= clipEnd) {
            g_chopperSelectedRangeStartPx = frameToPixel(clipStart);
            g_chopperSelectedRangeEndPx = frameToPixel(clipEnd);
        }
    }
#endif
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::publishOverlayState()', new_publish)

# Remove old framebuffer operation progress bar drawing. Keep code harmless if present by disabling the branch body.
old_marker = '    /* U2.18 centered destructive edit progress bar. Kept in the framebuffer overlay\n       because text DrawView elements at the waveform rows are overwritten by this overlay. */\n    if (g_chopperOperationActive) {'
pos = cpp.find(old_marker)
if pos >= 0:
    brace = cpp.find('{', pos)
    depth = 0; i = brace
    while i < len(cpp):
        if cpp[i] == '{': depth += 1
        elif cpp[i] == '}':
            depth -= 1
            if depth == 0:
                end = i+1
                repl = '    /* U2.22: operation feedback is text-only; framebuffer waveform overlay is disabled while operationActive_. */\n    if (0 && g_chopperOperationActive) { }'
                cpp = cpp[:pos] + repl + cpp[end:]
                break
        i += 1

new_draw_controls = r'''void SampleChopperModal::drawControls(GUITextProperties &props) {
    SetColor(CD_NORMAL);
    drawStringAbs(0, 24, "R1+LR sample  L1+LR fast cursor", props);
    drawStringAbs(0, 25, "A cut/live Y del B play SELECT crop", props);
    if (pitchMode_) drawStringAbs(0, 26, "PITCH: LR/UD set A apply B preview", props);
    else drawStringAbs(0, 26, trimMode_ ? "R1+A keep  L2+Y delete R1+X undo" : "R2+LR chop  R2+A full", props);
    SetColor(CD_HILITE1);
    if (pitchMode_) drawStringAbs(0, 28, "L1+R1 exit pitch", props);
    else drawStringAbs(0, 28, trimMode_ ? "CROP: A/B range Y start X end1s" : "SELECT crop  L1+R1 pitch R1+B back", props);
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::drawControls(GUITextProperties &props)', new_draw_controls)

new_draw_overlay = r'''void SampleChopperModal::drawOperationOverlay(GUITextProperties &props) {
    if (!operationActive_) return;
    char msg[64];
    SetColor(CD_HILITE1);
    props.invert_ = true;
    drawStringAbs(2, 9,  "+------------------------------------+", props);
    snprintf(msg, sizeof(msg), "| %-34s |", operationMessage_);
    msg[38] = 0;
    drawStringAbs(2, 10, msg, props);
    snprintf(msg, sizeof(msg), "|              %3d%%                  |", operationPercent_);
    msg[38] = 0;
    drawStringAbs(2, 11, msg, props);
    if (operationPercent_ >= 100) {
        drawStringAbs(2, 12, "|                 OK                 |", props);
        drawStringAbs(2, 13, "|        Press A to continue         |", props);
    } else {
        drawStringAbs(2, 12, "|            Please wait             |", props);
        drawStringAbs(2, 13, "|         Processing sample          |", props);
    }
    drawStringAbs(2, 14, "+------------------------------------+", props);
    props.invert_ = false;
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::drawOperationOverlay(GUITextProperties &props)', new_draw_overlay)

new_show_progress = r'''void SampleChopperModal::showOperationProgress(const char *message, int percent) {
    operationActive_ = true;
    operationPercent_ = clampInt(percent, 0, 100);
    snprintf(operationMessage_, sizeof(operationMessage_), "%s", message ? message : "Working");
    operationMessage_[sizeof(operationMessage_) - 1] = 0;
#if defined(PLATFORM_TREEFROG)
    g_chopperOperationActive = 0;
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

# Pitch screen helpers and preview implementation.
pitch_helpers = r'''
void SampleChopperModal::togglePitchMode() {
    stopSamplePreview();
    if (!hasAssignedSample()) { setStatus("No sample for pitch"); return; }
    pitchMode_ = !pitchMode_;
    if (pitchMode_) {
        trimMode_ = false;
        pitchSemitones_ = 0;
        setStatus("Pitch sample");
    } else {
        setStatus("Pitch off");
    }
    publishOverlayState();
    isDirty_ = true;
}

void SampleChopperModal::nudgePitchSemitones(int delta) {
    pitchSemitones_ = clampInt(pitchSemitones_ + delta, -12, 12);
    char msg[64]; snprintf(msg, sizeof(msg), "Pitch %+d st", pitchSemitones_);
    setStatus(msg);
    isDirty_ = true;
}

void SampleChopperModal::drawPitchScreen(GUITextProperties &props) {
    if (!pitchMode_) return;
    char msg[64];
    SetColor(CD_HILITE1);
    props.invert_ = true;
    drawStringAbs(2, 8,  "+------------------------------------+", props);
    drawStringAbs(2, 9,  "|            PITCH SAMPLE            |", props);
    snprintf(msg, sizeof(msg), "| Pitch: %+3d semitones              |", pitchSemitones_);
    msg[38] = 0;
    drawStringAbs(2, 10, msg, props);
    drawStringAbs(2, 11, "| LEFT/RIGHT or UP/DOWN: +/-1        |", props);
    drawStringAbs(2, 12, "| B preview   A apply                |", props);
    drawStringAbs(2, 13, "| L1+R1 exit                         |", props);
    drawStringAbs(2, 14, "+------------------------------------+", props);
    props.invert_ = false;
}

bool SampleChopperModal::buildPitchedBuffer(int semitones, short **outSamples, int *outFrames, int *outChannels, int *outRate) {
    if (outSamples) *outSamples = 0;
    if (outFrames) *outFrames = 0;
    if (outChannels) *outChannels = 0;
    if (outRate) *outRate = 0;
    if (!hasAssignedSample() || sourceSize_ <= 1) return false;
    if (semitones < -12) semitones = -12;
    if (semitones > 12) semitones = 12;

    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    if (!source) return false;
    int channels = source->GetChannelCount(-1);
    int rate = source->GetSampleRate(-1);
    int size = source->GetSize(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || channels <= 0 || rate <= 0 || size <= 1) return false;

    double ratio = pow(2.0, ((double)semitones) / 12.0);
    if (ratio <= 0.0) return false;
    int nextSize = (int)(((double)size / ratio) + 0.5);
    if (nextSize < 2) nextSize = 2;
    if (nextSize > 40000000) return false;
    short *pitched = (short *)malloc(nextSize * channels * sizeof(short));
    if (!pitched) return false;

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
    }

    if (outSamples) *outSamples = pitched;
    if (outFrames) *outFrames = nextSize;
    if (outChannels) *outChannels = channels;
    if (outRate) *outRate = rate;
    return true;
}

static void lgptWriteU2PreviewLE16(I_File *file, unsigned short value) {
    unsigned char b[2];
    b[0] = (unsigned char)(value & 0xFF);
    b[1] = (unsigned char)((value >> 8) & 0xFF);
    file->Write(b, 1, 2);
}

static void lgptWriteU2PreviewLE32(I_File *file, unsigned int value) {
    unsigned char b[4];
    b[0] = (unsigned char)(value & 0xFF);
    b[1] = (unsigned char)((value >> 8) & 0xFF);
    b[2] = (unsigned char)((value >> 16) & 0xFF);
    b[3] = (unsigned char)((value >> 24) & 0xFF);
    file->Write(b, 1, 4);
}

bool SampleChopperModal::writePreviewPitchWav(short *samples, int frames, int channels, int rate, std::string &logicalPath) {
    logicalPath = "samples:__u2_pitch_preview.wav";
    if (!samples || frames <= 0 || channels <= 0 || rate <= 0) return false;
    Path p(logicalPath.c_str());
    std::string resolved = p.GetPath();
    if (resolved.empty()) return false;
    FileSystem *fs = FileSystem::GetInstance();
    if (!fs) return false;
    I_File *file = fs->Open(resolved.c_str(), "wb");
    if (!file) file = fs->Open(resolved.c_str(), "w");
    if (!file) return false;

    unsigned int dataBytes = (unsigned int)(frames * channels * 2);
    unsigned int riffBytes = 36 + dataBytes;
    unsigned int byteRate = (unsigned int)(rate * channels * 2);
    unsigned short blockAlign = (unsigned short)(channels * 2);

    file->Write((void *)"RIFF", 1, 4);
    lgptWriteU2PreviewLE32(file, riffBytes);
    file->Write((void *)"WAVE", 1, 4);
    file->Write((void *)"fmt ", 1, 4);
    lgptWriteU2PreviewLE32(file, 16);
    lgptWriteU2PreviewLE16(file, 1);
    lgptWriteU2PreviewLE16(file, (unsigned short)channels);
    lgptWriteU2PreviewLE32(file, (unsigned int)rate);
    lgptWriteU2PreviewLE32(file, byteRate);
    lgptWriteU2PreviewLE16(file, blockAlign);
    lgptWriteU2PreviewLE16(file, 16);
    file->Write((void *)"data", 1, 4);
    lgptWriteU2PreviewLE32(file, dataBytes);
    file->Write(samples, 2, frames * channels);
    file->Close();
    delete file;
    return true;
}

void SampleChopperModal::previewPitchSetting() {
    if (!pitchMode_) return;
    if (pitchSemitones_ == 0) { playFullSample(); setStatus("Preview pitch +0"); return; }
    short *pitched = 0;
    int frames = 0, channels = 0, rate = 0;
    if (!buildPitchedBuffer(pitchSemitones_, &pitched, &frames, &channels, &rate)) { setStatus("Pitch preview fail"); return; }
    std::string logical;
    bool ok = writePreviewPitchWav(pitched, frames, channels, rate, logical);
    free(pitched);
    if (!ok) { setStatus("Pitch preview write fail"); return; }
    Path path(logical.c_str());
    Player::GetInstance()->StopStreaming();
    Player::GetInstance()->StartStreamingAt(path, 0);
    playbackTriggered_ = true;
    clearPreviewPlaybackRange();
    char msg[64]; snprintf(msg, sizeof(msg), "Preview pitch %+d", pitchSemitones_);
    setStatus(msg);
}
'''
insert_before = 'bool SampleChopperModal::destructivePitchSample(int semitones)'
if 'void SampleChopperModal::togglePitchMode()' not in cpp:
    pos = cpp.find(insert_before)
    if pos < 0: raise SystemExit('Patch failed: missing destructivePitchSample for pitch helper insert')
    cpp = cpp[:pos] + pitch_helpers + '\n' + cpp[pos:]

# Allow destructive pitch from the dedicated PITCH SAMPLE screen, not only from old crop mode.
cpp = cpp.replace('    if (!trimMode_) { setStatus("Use ADJUST SAMPLE first"); return false; }\n', '    if (!trimMode_ && !pitchMode_) { setStatus("Use PITCH SAMPLE first"); return false; }\n', 1)

# Make destructive pitch complete message include actual semitone and keep pitch mode active.
old_tail = '    showOperationProgress("Pitch complete", 100);\n    return true;\n}'
if old_tail in cpp:
    cpp = cpp.replace(old_tail, '    char done[48]; snprintf(done, sizeof(done), "Pitch %+d complete", semitones);\n    showOperationProgress(done, 100);\n    return true;\n}', 1)

# Toggle crop mode should close pitch mode.
new_toggle_trim = r'''void SampleChopperModal::toggleTrimMode() {
    if (pitchMode_) pitchMode_ = false;
    initializeChopsIfNeeded();
    trimMode_ = !trimMode_;
    if (trimMode_) {
        cursorFrame_ = selectedChopStartFrame();
        setStatus("CROP SAMPLE");
    } else {
        setStatus("Crop mode off");
    }
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    isDirty_ = true;
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::toggleTrimMode()', new_toggle_trim)

# DrawView includes pitch screen before operation overlay.
new_draw_view = r'''void SampleChopperModal::DrawView() {
    GUITextProperties props; props.invert_ = false;
    clearTextScreen();
    drawTopBar(props);
    drawFrame(props);
    drawSampleInfo(props);
    if (!hasWaveform_) drawEmptyWaveformText(props);
    drawControls(props);
    drawPitchScreen(props);
    drawOperationOverlay(props);
    publishOverlayState();
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::DrawView()', new_draw_view)

# ProcessButtonMask: add pitch mode and L1+R1 toggle; remove automatic R1+arrow pitch inside crop.
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

    if (l1 && r1 && !(left || right || up || down || a || b || x || y || l2 || r2 || select)) {
        togglePitchMode(); return;
    }

    if (pitchMode_) {
        if ((left || right || up || down) && !(a || b || x || y || l1 || r1 || l2 || r2 || select)) {
            int delta = (right || up) ? 1 : -1;
            nudgePitchSemitones(delta);
            return;
        }
        if (b && !(left || right || up || down || a || x || y || l1 || r1 || l2 || r2 || select)) {
            previewPitchSetting(); return;
        }
        if (a && !(left || right || up || down || b || x || y || l1 || r1 || l2 || r2 || select)) {
            destructivePitchSample(pitchSemitones_); return;
        }
        if (select && !(left || right || up || down || a || b || x || y || l1 || r1 || l2 || r2)) {
            togglePitchMode(); return;
        }
        setStatus("Pitch: arrows, B preview, A apply");
        return;
    }

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

# Update top label if present.
cpp = cpp.replace('Graphical Chopper U2.11', 'Graphical Chopper U2.22')

cpp_path.write_text(cpp)
h_path.write_text(h)

# Verification checks.
checks = [
    ('SELECT enabled', 'TREEFROG_ENABLE_SELECT=1', (root/'BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh').read_text()),
    ('pitch mode state', 'bool pitchMode_;', h),
    ('pitch screen function', 'drawPitchScreen', cpp),
    ('L1+R1 pitch entry', 'togglePitchMode(); return;', cpp),
    ('pitch manual +/-1', 'nudgePitchSemitones(delta);', cpp),
    ('pitch A apply', 'destructivePitchSample(pitchSemitones_); return;', cpp),
    ('pitch B preview', 'previewPitchSetting(); return;', cpp),
    ('no auto R1 arrow pitch branch', 'int semis = (up || right) ? 1 : -1;' not in cpp, cpp),
    ('operation overlay text centered', 'Press A to continue', cpp),
    ('framebuffer overlay disabled during operation', 'g_chopperOverlayActive = (hasWaveform_ && !operationActive_ && !pitchMode_) ? 1 : 0;', cpp),
    ('persistent chops still present', 'lgptWritePersistentChopState', cpp),
]
for name, expected, hay in checks:
    ok = expected if isinstance(expected, bool) else (expected in hay)
    print(('OK   ' if ok else 'FAIL ') + name)
    if not ok:
        raise SystemExit('Verification failed: ' + name)

print('U2.22 patches applied: pitch screen, manual pitch apply/preview, cleaned operation overlay, stable candidate docs.')
PY_U222

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_22_PITCH_SCREEN_UI_FINAL_STABLE_$STAMP.log"
echo "Starting U2.22 build..."
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
