#!/usr/bin/env bash
# U2.20 Persistent Chops stable candidate.
# Applies on top of validated U2.19. Adds persistent chop sidecars, stable crop overlay OK/continue,
# and simplified physical pitch controls in CROP SAMPLE.

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
  echo "ERROR: expected U2.19 SELECT-enabled tree. Apply and validate U2.19 first."
  exit 4
fi
if ! grep -q "destructiveCropToSelectedRange" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: expected U2.15+ destructive chopper code not found."
  exit 5
fi

BACKUP="_backup_before_u2_20_persistent_chops_stable_candidate_$STAMP.tar.gz"
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

python3 - <<'PY_U220'
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
        if c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                end = i + 1
                while end < len(text) and text[end] in ' \t\r\n':
                    end += 1
                return text[:start] + replacement.rstrip() + '\n\n' + text[end:]
        i += 1
    raise SystemExit('Patch failed: unterminated function: ' + signature)

cpp = cpp_path.read_text()

# Add sstream for sidecar parsing.
if '#include <sstream>' not in cpp:
    cpp = cpp.replace('#include <math.h>\n', '#include <math.h>\n#include <sstream>\n')

# Insert persistent sidecar helpers after saved-state lookup/free-slot functions.
if 'lgptPersistentChopSidecarPathForSampleName' not in cpp:
    marker = '''static int lgptFindChopperFreeSavedStateSlot() {
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        if (!g_lgptChopperSavedStates[i].used) return i;
    }
    return 0; /* deterministic eviction; enough for the current R36SX test path */
}
'''
    if marker not in cpp:
        raise SystemExit('Patch failed: saved state free slot marker not found')
    helpers = r'''
static std::string lgptPersistentChopSidecarPathForSampleName(const std::string &sampleName) {
    if (sampleName.empty()) return "";
    std::string logical = "samples:";
    logical += sampleName;
    Path p(logical.c_str());
    std::string resolved = p.GetPath();
    if (resolved.empty()) return "";
    return resolved + ".u2chop";
}

static bool lgptReadWholeFile(const std::string &path, std::string &out) {
    out.clear();
    if (path.empty()) return false;
    FileSystem *fs = FileSystem::GetInstance();
    if (!fs) return false;
    I_File *file = fs->Open(path.c_str(), "r");
    if (!file) return false;
    char buffer[256];
    int n = 0;
    while ((n = file->Read(buffer, 1, sizeof(buffer))) > 0) {
        out.append(buffer, n);
        if (n < (int)sizeof(buffer)) break;
    }
    file->Close();
    delete file;
    return !out.empty();
}

static bool lgptGetSampleIdentityBySampleIndex(int sampleIndex,
                                               std::string &sampleName,
                                               int &sourceSize) {
    sampleName.clear();
    sourceSize = 0;
    if (sampleIndex < 0) return false;
    SamplePool *pool = SamplePool::GetInstance();
    if (!pool) return false;
    char **names = pool->GetNameList();
    int count = pool->GetNameListSize();
    if (!names || sampleIndex >= count || !names[sampleIndex]) return false;
    sampleName = names[sampleIndex];
    SoundSource *source = pool->GetSource(sampleIndex);
    sourceSize = source ? source->GetSize(-1) : 0;
    return !sampleName.empty() && sourceSize > 1;
}

static bool lgptValidatePersistentBoundaries(int *boundaries, int boundaryCount, int sourceSize) {
    if (!boundaries || sourceSize <= 1) return false;
    if (boundaryCount < 2 || boundaryCount > LGPT_CHOPPER_SAVED_BOUNDARIES) return false;
    boundaries[0] = 0;
    boundaries[boundaryCount - 1] = sourceSize - 1;
    int previous = -1;
    for (int i = 0; i < boundaryCount; i++) {
        if (boundaries[i] < 0 || boundaries[i] >= sourceSize || boundaries[i] <= previous) return false;
        previous = boundaries[i];
    }
    return true;
}

static bool lgptLoadPersistentChopState(const void *projectKey,
                                        int sampleIndex,
                                        const std::string &sampleName,
                                        int sourceSize,
                                        int *slotOut) {
    if (slotOut) *slotOut = -1;
    if (sampleIndex < 0 || sampleName.empty() || sourceSize <= 1) return false;
    std::string sidecar = lgptPersistentChopSidecarPathForSampleName(sampleName);
    std::string content;
    if (!lgptReadWholeFile(sidecar, content)) return false;

    std::istringstream iss(content);
    std::string tok;
    int fileSampleIndex = sampleIndex;
    int fileSourceSize = sourceSize;
    int selected = 0;
    int count = 0;
    int temp[LGPT_CHOPPER_SAVED_BOUNDARIES];
    for (int i = 0; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) temp[i] = 0;

    if (!(iss >> tok) || tok != "LGPT_U2_CHOPS_V1") return false;
    while (iss >> tok) {
        if (tok == "sampleIndex") iss >> fileSampleIndex;
        else if (tok == "sourceSize") iss >> fileSourceSize;
        else if (tok == "selected") iss >> selected;
        else if (tok == "boundaryCount") iss >> count;
        else if (tok == "boundaries") {
            for (int i = 0; i < count && i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) iss >> temp[i];
        } else if (tok == "END") break;
    }

    /* Size must match the currently loaded WAV. A destructive crop/pitch writes a new sidecar
       immediately, so a mismatch means stale chop data and must not be trusted. */
    if (fileSourceSize != sourceSize) return false;
    if (!lgptValidatePersistentBoundaries(temp, count, sourceSize)) return false;

    int slot = lgptFindChopperSavedStateLoose(projectKey, sampleIndex, sampleName, sourceSize);
    if (slot < 0) slot = lgptFindChopperFreeSavedStateSlot();
    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    saved.used = true;
    saved.projectKey = projectKey;
    saved.sampleIndex = sampleIndex;
    saved.sourceSize = sourceSize;
    saved.sampleName = sampleName;
    saved.boundaryCount = count;
    saved.selectedChop = selected;
    if (saved.selectedChop < 0) saved.selectedChop = 0;
    if (saved.selectedChop > saved.boundaryCount - 2) saved.selectedChop = saved.boundaryCount - 2;
    for (int i = 0; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) saved.boundaries[i] = 0;
    for (int i = 0; i < count; i++) saved.boundaries[i] = temp[i];
    for (int i = 0; i < 100; i++) saved.chopInstrument[i] = -1;
    if (slotOut) *slotOut = slot;
    return true;
}

static bool lgptEnsurePersistentChopStateLoaded(const void *projectKey,
                                                int sampleIndex,
                                                const std::string &sampleName,
                                                int sourceSize,
                                                int *slotOut) {
    int slot = lgptFindChopperSavedStateLoose(projectKey, sampleIndex, sampleName, sourceSize);
    if (slot >= 0) {
        if (slotOut) *slotOut = slot;
        return true;
    }
    return lgptLoadPersistentChopState(projectKey, sampleIndex, sampleName, sourceSize, slotOut);
}

static void lgptWritePersistentChopState(const LGPTChopperSavedState &saved) {
    if (!saved.used || saved.sampleName.empty() || saved.sourceSize <= 1 || saved.boundaryCount < 2) return;
    std::string sidecar = lgptPersistentChopSidecarPathForSampleName(saved.sampleName);
    if (sidecar.empty()) return;
    FileSystem *fs = FileSystem::GetInstance();
    if (!fs) return;
    I_File *file = fs->Open(sidecar.c_str(), "w");
    if (!file) return;
    file->Printf("LGPT_U2_CHOPS_V1\n");
    file->Printf("sampleIndex %d\n", saved.sampleIndex);
    file->Printf("sourceSize %d\n", saved.sourceSize);
    file->Printf("selected %d\n", saved.selectedChop);
    file->Printf("boundaryCount %d\n", saved.boundaryCount);
    file->Printf("boundaries");
    for (int i = 0; i < saved.boundaryCount; i++) file->Printf(" %d", saved.boundaries[i]);
    file->Printf("\nEND\n");
    file->Close();
    delete file;
}
'''
    cpp = cpp.replace(marker, marker + helpers + '\n')

# restore loads sidecar if memory slot is missing.
new_restore = r'''bool SampleChopperModal::restoreChopStateForCurrentSample() {
    if (!hasAssignedSample() || sourceSize_ <= 1) {
        resetChopState();
        return false;
    }

    const void *projectKey = (viewData_ && viewData_->project_) ? (const void *)viewData_->project_ : 0;
    int slot = -1;
    if (!lgptEnsurePersistentChopStateLoaded(projectKey, sampleIndex_, sampleName_, sourceSize_, &slot) || slot < 0) {
        resetChopState();
        return false;
    }

    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    int count = saved.boundaryCount;
    if (count < 2 || count > MAX_CHOP_BOUNDARIES) {
        resetChopState();
        return false;
    }

    int previous = -1;
    for (int i = 0; i < count; i++) {
        int value = saved.boundaries[i];
        if (value < 0 || value >= sourceSize_ || value <= previous) {
            resetChopState();
            return false;
        }
        boundaries_[i] = value;
        previous = value;
    }
    for (int i = count; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;

    boundaryCount_ = count;
    selectedChop_ = clampInt(saved.selectedChop, 0, boundaryCount_ - 2);
    cursorFrame_ = boundaries_[selectedChop_];
    viewStartFrame_ = 0;
    trimMode_ = false;
    chopsInitialized_ = true;
    return true;
}'''
cpp = replace_function(cpp, 'bool SampleChopperModal::restoreChopStateForCurrentSample()', new_restore)

new_save = r'''void SampleChopperModal::saveChopStateForCurrentSample() {
    if (!hasAssignedSample() || sourceSize_ <= 1 || !chopsInitialized_ || boundaryCount_ < 2) return;

    const void *projectKey = (viewData_ && viewData_->project_) ? (const void *)viewData_->project_ : 0;
    int slot = lgptFindChopperSavedStateLoose(projectKey, sampleIndex_, sampleName_, sourceSize_);
    if (slot < 0) slot = lgptFindChopperFreeSavedStateSlot();

    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    bool wasNewSlotForSample = !(saved.used &&
                                 saved.projectKey == projectKey &&
                                 saved.sampleIndex == sampleIndex_ &&
                                 saved.sourceSize == sourceSize_ &&
                                 saved.sampleName == sampleName_);
    saved.used = true;
    saved.projectKey = projectKey;
    saved.sampleIndex = sampleIndex_;
    saved.sourceSize = sourceSize_;
    saved.sampleName = sampleName_;
    saved.boundaryCount = clampInt(boundaryCount_, 2, MAX_CHOP_BOUNDARIES);
    saved.selectedChop = clampInt(selectedChop_, 0, saved.boundaryCount - 2);
    for (int i = 0; i < saved.boundaryCount; i++) saved.boundaries[i] = boundaries_[i];
    for (int i = saved.boundaryCount; i < LGPT_CHOPPER_SAVED_BOUNDARIES; i++) saved.boundaries[i] = 0;
    if (wasNewSlotForSample) { for (int i = 0; i < 100; i++) saved.chopInstrument[i] = -1; }
    lgptWritePersistentChopState(saved);
    lgptReconfigureMappedChopInstruments(viewData_, instrumentIndex_, saved);
    lgptNormalizePhraseRowsForSavedChops(viewData_, instrumentIndex_, saved);
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::saveChopStateForCurrentSample()', new_save)

# Lazy-load from sidecar for Phrase display/assignment.
new_count = r'''int LGPTChopperGetSavedChopCountForInstrument(ViewData *viewData,
                                               int sourceInstrumentIndex) {
    int sampleIndex = NO_SAMPLE;
    int sourceSize = 0;
    std::string sampleName;
    if (!lgptGetSampleIdentityForInstrument(viewData, sourceInstrumentIndex,
                                            sampleIndex, sampleName, sourceSize)) return 0;
    const void *projectKey = (viewData && viewData->project_) ? (const void *)viewData->project_ : 0;
    int slot = -1;
    if (!lgptEnsurePersistentChopStateLoaded(projectKey, sampleIndex, sampleName, sourceSize, &slot) || slot < 0) return 0;
    const LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    if (lgptSavedStateIsFullRange(saved)) return 0;
    int count = saved.boundaryCount - 1;
    if (count > 100) count = 100;
    return count > 0 ? count : 0;
}'''
cpp = replace_function(cpp, 'int LGPTChopperGetSavedChopCountForInstrument(ViewData *viewData,', new_count)

new_range = r'''bool LGPTChopperGetChopRangeForSampleIndex(int sampleIndex,
                                           int chopIndex,
                                           int *startFrame,
                                           int *endFrameExclusive) {
    int slot = lgptFindSavedStateBySampleIndex(sampleIndex);
    if (slot < 0) {
        std::string sampleName;
        int sourceSize = 0;
        if (lgptGetSampleIdentityBySampleIndex(sampleIndex, sampleName, sourceSize)) {
            lgptLoadPersistentChopState(0, sampleIndex, sampleName, sourceSize, &slot);
        }
    }
    if (slot < 0) return false;
    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    if (lgptSavedStateIsFullRange(saved)) return false;
    int chopCount = saved.boundaryCount - 1;
    if (chopCount <= 0) return false;
    if (chopCount > 100) chopCount = 100;
    if (chopIndex < 0 || chopIndex >= chopCount) return false;
    int start = saved.boundaries[chopIndex];
    int endExclusive = saved.boundaries[chopIndex + 1] + 1;
    if (start < 0) start = 0;
    if (endExclusive <= start) endExclusive = start + 1;
    if (saved.sourceSize > 0 && endExclusive > saved.sourceSize) endExclusive = saved.sourceSize;
    if (startFrame) *startFrame = start;
    if (endFrameExclusive) *endFrameExclusive = endExclusive;
    return true;
}'''
cpp = replace_function(cpp, 'bool LGPTChopperGetChopRangeForSampleIndex(int sampleIndex,', new_range)

# Keep crop mode after crop for undo/redo, write persistent full-range state, and keep OK overlay.
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
    trimMode_ = true;
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

# Change destructive pitch range to +/-12 and controls to one-semitone steps only.
cpp = cpp.replace('    if (semitones < -24) semitones = -24;\n    if (semitones > 24) semitones = 24;\n', '    if (semitones < -12) semitones = -12;\n    if (semitones > 12) semitones = 12;\n')
cpp = cpp.replace('if (up) semis = 1; else if (down) semis = -1; else if (right) semis = 12; else if (left) semis = -12;',
                  'if (up || right) semis = 1; else if (down || left) semis = -1;')

# Update controls text.
new_controls = r'''void SampleChopperModal::drawControls(GUITextProperties &props) {
    SetColor(CD_NORMAL);
    drawStringAbs(0, 24, "R1+LR sample  L1+LR fast cursor", props);
    drawStringAbs(0, 25, "A cut/live Y del B play SELECT crop", props);
    drawStringAbs(0, 26, trimMode_ ? "R1+A crop R1+X undo R1+UD pitch" : "R2+LR chop  R2+A full", props);
    SetColor(CD_HILITE1);
    drawStringAbs(0, 28, trimMode_ ? "CROP: A/B range Y start X end1s" : "SELECT crop  R1+B back", props);
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::drawControls(GUITextProperties &props)', new_controls)

new_overlay = r'''void SampleChopperModal::drawOperationOverlay(GUITextProperties &props) {
    if (!operationActive_) return;
    char msg[64];
    SetColor(CD_HILITE1);
    props.invert_ = true;
    drawStringAbs(4, 8,  "+------------------------------+", props);
    drawStringAbs(4, 9,  "|        SAMPLE OPERATION      |", props);
    snprintf(msg, sizeof(msg), "| %-18s %3d%%   |", operationMessage_, operationPercent_);
    msg[31] = 0;
    drawStringAbs(4, 10, msg, props);
    if (operationPercent_ >= 100) drawStringAbs(4, 11, "| OK - press A to continue     |", props);
    else drawStringAbs(4, 11, "| Please wait                  |", props);
    drawStringAbs(4, 12, "+------------------------------+", props);
    props.invert_ = false;
}'''
cpp = replace_function(cpp, 'void SampleChopperModal::drawOperationOverlay(GUITextProperties &props)', new_overlay)

# Operation overlay: keep stable until plain A; do not process other keys.
old = 'void SampleChopperModal::ProcessButtonMask(unsigned short mask, bool pressed) {\n    if (!pressed) return;\n    if (operationActive_ && operationPercent_ >= 100) clearOperationProgress();\n'
new = 'void SampleChopperModal::ProcessButtonMask(unsigned short mask, bool pressed) {\n    if (!pressed) return;\n    if (operationActive_ && operationPercent_ >= 100) {\n        bool plainA = (mask & EPBM_A) && !(mask & EPBM_LEFT) && !(mask & EPBM_RIGHT) && !(mask & EPBM_UP) && !(mask & EPBM_DOWN) && !(mask & EPBM_L) && !(mask & EPBM_R) && !(mask & EPBM_L2) && !(mask & EPBM_R2) && !(mask & EPBM_B) && !(mask & EPBM_X) && !(mask & EPBM_Y);\n        if (plainA) clearOperationProgress();\n        return;\n    }\n'
if old not in cpp:
    raise SystemExit('Patch failed: ProcessButtonMask operation guard not found')
cpp = cpp.replace(old, new, 1)

# Make showOperationProgress explicit at 100.
cpp = cpp.replace('    char status[64]; snprintf(status, sizeof(status), "%s %d%%", operationMessage_, operationPercent_);\n    setStatus(status);',
                  '    char status[64];\n    if (operationPercent_ >= 100) snprintf(status, sizeof(status), "%s OK - press A", operationMessage_);\n    else snprintf(status, sizeof(status), "%s %d%%", operationMessage_, operationPercent_);\n    setStatus(status);')


# Lazy-load from sidecar for direct Phrase assignment as well.
old_assign_lookup = '''    const void *projectKey = viewData->project_ ? (const void *)viewData->project_ : 0;
    int slot = lgptFindChopperSavedStateLoose(projectKey, sampleIndex, sampleName, sourceSize);
    if (slot < 0) {
        lgptSetAssignStatus(status, statusLen, "No saved chops");
        return false;
    }
'''
new_assign_lookup = '''    const void *projectKey = viewData->project_ ? (const void *)viewData->project_ : 0;
    int slot = -1;
    if (!lgptEnsurePersistentChopStateLoaded(projectKey, sampleIndex, sampleName, sourceSize, &slot) || slot < 0) {
        lgptSetAssignStatus(status, statusLen, "No saved chops");
        return false;
    }
'''
if old_assign_lookup in cpp:
    cpp = cpp.replace(old_assign_lookup, new_assign_lookup, 1)
else:
    raise SystemExit('Patch failed: direct phrase assign saved-state lookup not found')

# Make crop not clear full-range sidecar accidentally and verify persistent save already called by crop.
cpp_path.write_text(cpp)

# Verification markers.
cpp2 = cpp_path.read_text()
checks = [
    ('sidecar path helper', 'lgptPersistentChopSidecarPathForSampleName' in cpp2),
    ('sidecar writer', 'LGPT_U2_CHOPS_V1' in cpp2 and 'lgptWritePersistentChopState(saved);' in cpp2),
    ('restore loads sidecar', 'lgptEnsurePersistentChopStateLoaded(projectKey, sampleIndex_, sampleName_, sourceSize_' in cpp2),
    ('Phrase count lazy-loads sidecar', 'lgptEnsurePersistentChopStateLoaded(projectKey, sampleIndex, sampleName, sourceSize' in cpp2),
    ('playback lazy-loads sidecar', 'lgptLoadPersistentChopState(0, sampleIndex, sampleName, sourceSize' in cpp2),
    ('direct phrase assign lazy-loads sidecar', 'if (!lgptEnsurePersistentChopStateLoaded(projectKey, sampleIndex, sampleName, sourceSize, &slot)' in cpp2),
    ('overlay OK continue', 'OK - press A to continue' in cpp2),
    ('operation blocks until A', 'if (operationActive_ && operationPercent_ >= 100)' in cpp2 and 'plainA' in cpp2),
    ('physical pitch limited to 12', 'if (semitones < -12) semitones = -12;' in cpp2),
]
print('U2.20 verification:')
failed = False
for name, ok in checks:
    print(('OK   ' if ok else 'FAIL ') + name)
    if not ok: failed = True
if failed:
    raise SystemExit('U2.20 verification failed; not building.')
print('U2.20 patches applied: persistent sidecar chops, OK overlay, pitch +/-1 within +/-12.')
PY_U220

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_20_PERSISTENT_CHOPS_STABLE_CANDIDATE_$STAMP.log"
echo "Starting U2.20 build..."
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
  grep -nE "error:|undefined reference|No such file|fatal error|make.*Error|Error [0-9]+" "$LOG" | sed -n '1,360p'
  tail -n 220 "$LOG"
fi
exit "$RC"
