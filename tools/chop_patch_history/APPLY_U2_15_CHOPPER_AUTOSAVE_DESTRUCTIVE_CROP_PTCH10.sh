#!/usr/bin/env bash
# U2.15 Chopper autosave + destructive crop/undo + trim previews + PTCH +/-10.
# Apply on top of U2.14.
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

if ! grep -q "MAX_CHOPS = 100" sources/Application/Views/ModalDialogs/SampleChopperModal.h; then
  echo "ERROR: U2.14 100-chop model not found. Apply U2.14 first."
  exit 4
fi
if ! grep -q "isPtchParamCell" sources/Application/Views/PhraseView.cpp; then
  echo "ERROR: U2.14 PTCH focused UI not found. Apply U2.14 first."
  exit 5
fi

BACKUP="_backup_before_u2_15_chopper_autosave_destructive_crop_ptch10_$STAMP.tar.gz"
tar -czf "$BACKUP" \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/PhraseView.cpp \
  sources/Application/Instruments/WavFile.h \
  sources/Application/Instruments/WavFile.cpp \
  projects/Makefile

echo "Backup written: $SRC/$BACKUP"

python3 - <<'U215PY'
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

# WavFile: in-memory replace + save cropped WAV to disk.
p = require('sources/Application/Instruments/WavFile.h')
s = p.read_text()
if 'bool ReplaceBuffer(short *samples, int frameCount, int channelCount, int sampleRate);' not in s:
    lines = s.splitlines(True)
    out = []
    inserted = False
    for line in lines:
        out.append(line)
        if (not inserted) and 'bool GetBuffer(long start,long sampleCount)' in line:
            out.append('\tbool ReplaceBuffer(short *samples, int frameCount, int channelCount, int sampleRate);\n')
            out.append('\tbool SaveBufferToPath(const char *path);\n')
            inserted = True
    if not inserted:
        raise SystemExit('Patch failed: WavFile.h method declarations')
    s = ''.join(out)
p.write_text(s)

p = require('sources/Application/Instruments/WavFile.cpp')
s = p.read_text()
if 'bool WavFile::ReplaceBuffer(short *samples, int frameCount, int channelCount, int sampleRate)' not in s:
    insert_after = '''int WavFile::GetRootNote(int note) {\n\treturn 60 ;\n} \n'''
    methods = r'''

static void lgptWavWriteU16(I_File *file, unsigned short value) {
    unsigned short v = Swap16(value);
    file->Write(&v, 1, 2);
}

static void lgptWavWriteU32(I_File *file, unsigned int value) {
    unsigned int v = Swap32((int)value);
    file->Write(&v, 1, 4);
}

bool WavFile::ReplaceBuffer(short *samples, int frameCount, int channelCount, int sampleRate) {
    if (!samples || frameCount <= 0 || channelCount <= 0 || sampleRate <= 0) return false;
    int bytes = frameCount * channelCount * 2;
    short *next = (short *)SYS_MALLOC(bytes);
    if (!next) return false;
    memcpy(next, samples, bytes);
    SAFE_FREE(samples_);
    samples_ = next;
    sampleBufferSize_ = bytes;
    size_ = frameCount;
    channelCount_ = channelCount;
    sampleRate_ = sampleRate;
    bytePerSample_ = 2;
    dataPosition_ = 44;
    return true;
}

bool WavFile::SaveBufferToPath(const char *path) {
    if (!path || !samples_ || size_ <= 0 || channelCount_ <= 0 || sampleRate_ <= 0) return false;
    FileSystem *fs = FileSystem::GetInstance();
    if (!fs) return false;
    I_File *file = fs->Open(path, "wb");
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
}
'''
    s = replace_once(s, insert_after, insert_after + methods, 'WavFile.cpp method insertion')
p.write_text(s)

# Chopper header.
p = require('sources/Application/Views/ModalDialogs/SampleChopperModal.h')
s = p.read_text()
if 'bool destructiveCropToSelectedRange();' not in s:
    s = replace_once(s,
        '    void cropToSelectedRange();\n',
        '    void cropToSelectedRange();\n    bool destructiveCropToSelectedRange();\n    bool undoLastDestructiveCrop();\n    void previewTrimStart();\n    void previewTrimEnd();\n',
        'SampleChopperModal.h helper declarations')
p.write_text(s)

# Chopper implementation.
p = require('sources/Application/Views/ModalDialogs/SampleChopperModal.cpp')
s = p.read_text()
if '#include "Application/Instruments/WavFile.h"' not in s:
    s = replace_once(s, '#include "Application/Instruments/SoundSource.h"\n', '#include "Application/Instruments/SoundSource.h"\n#include "Application/Instruments/WavFile.h"\n', 'WavFile include')

anchor = 'static const int LGPT_CHOPPER_SAVED_BOUNDARIES = 101;\n'
if 'g_lgptLastDestructiveCropSamplePath' not in s:
    s = replace_once(s, anchor, anchor + r'''
static std::string g_lgptLastDestructiveCropSamplePath;
static std::string g_lgptLastDestructiveCropBackupPath;
static int g_lgptLastDestructiveCropSampleIndex = -1;

static bool lgptEndsWithWav(const std::string &name) {
    if (name.size() < 4) return false;
    char a = name[name.size() - 4];
    char b = name[name.size() - 3];
    char c = name[name.size() - 2];
    char d = name[name.size() - 1];
    if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
    if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    if (d >= 'A' && d <= 'Z') d = (char)(d + 32);
    return a == '.' && b == 'w' && c == 'a' && d == 'v';
}
''', 'destructive crop globals')

anchor_fn = '''static void lgptReconfigureMappedChopInstruments(ViewData *viewData,
                                                 int sourceInstrumentIndex,
                                                 LGPTChopperSavedState &saved) {
    int chopCount = saved.boundaryCount - 1;
    if (chopCount > 100) chopCount = 100;
    for (int i = 0; i < chopCount; i++) {
        int mapped = saved.chopInstrument[i];
        if (mapped >= 0 && mapped < MAX_SAMPLEINSTRUMENT_COUNT) {
            lgptConfigureSavedChopInstrument(viewData, mapped, saved.sampleIndex,
                                             saved.sourceSize,
                                             saved.boundaries[i],
                                             saved.boundaries[i + 1]);
        }
    }
    for (int i = chopCount; i < 100; i++) saved.chopInstrument[i] = -1;
}
'''
if 'lgptNormalizePhraseRowsForSavedChops' not in s:
    s = replace_once(s, anchor_fn, anchor_fn + r'''
static void lgptNormalizePhraseRowsForSavedChops(ViewData *viewData,
                                                 int sourceInstrumentIndex,
                                                 const LGPTChopperSavedState &saved) {
    if (!viewData || !viewData->song_ || !viewData->song_->phrase_) return;
    if (sourceInstrumentIndex < 0 || sourceInstrumentIndex >= MAX_SAMPLEINSTRUMENT_COUNT) return;
    if (lgptSavedStateIsFullRange(saved)) return;
    int chopCount = saved.boundaryCount - 1;
    if (chopCount <= 0) return;
    if (chopCount > 100) chopCount = 100;
    Phrase *phrase = viewData->song_->phrase_;
    for (int p = 0; p < PHRASE_COUNT; p++) {
        for (int r = 0; r < 16; r++) {
            int offset = 16 * p + r;
            if (phrase->instr_[offset] == sourceInstrumentIndex &&
                phrase->note_[offset] != 0xFF &&
                phrase->note_[offset] < 100 &&
                phrase->note_[offset] >= chopCount) {
                phrase->note_[offset] = 0xFF;
            }
        }
    }
}
''', 'phrase rows normalization helper')

if 'lgptNormalizePhraseRowsForSavedChops(viewData_, instrumentIndex_, saved);' not in s:
    s = replace_once(s,
        '    lgptReconfigureMappedChopInstruments(viewData_, instrumentIndex_, saved);\n',
        '    lgptReconfigureMappedChopInstruments(viewData_, instrumentIndex_, saved);\n    lgptNormalizePhraseRowsForSavedChops(viewData_, instrumentIndex_, saved);\n',
        'normalization call')

insert_anchor = 'void SampleChopperModal::prepareWaveformPreview() {'
if 'bool SampleChopperModal::destructiveCropToSelectedRange()' not in s:
    s = replace_once(s, insert_anchor, r'''
bool SampleChopperModal::destructiveCropToSelectedRange() {
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

    std::string backupPath = samplePath_ + ".u2undo";
    FileSystemService fs;
    fs.Copy(Path(samplePath_.c_str()), Path(backupPath.c_str()));

    int sampleWords = frameCount * channels;
    short *cropped = (short *)malloc(sampleWords * sizeof(short));
    if (!cropped) { setStatus("No crop memory"); return false; }
    memcpy(cropped, samples + (start * channels), sampleWords * sizeof(short));

    WavFile *wav = (WavFile *)source;
    bool ok = wav->ReplaceBuffer(cropped, frameCount, channels, rate);
    free(cropped);
    if (!ok) { setStatus("Cannot crop buffer"); return false; }
    if (!wav->SaveBufferToPath(samplePath_.c_str())) { setStatus("Cannot write crop"); return false; }

    g_lgptLastDestructiveCropSamplePath = samplePath_;
    g_lgptLastDestructiveCropBackupPath = backupPath;
    g_lgptLastDestructiveCropSampleIndex = sampleIndex_;
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
    saveChopStateForCurrentSample();
    prepareWaveformPreview();
    publishOverlayState();
    char msg[64]; snprintf(msg, sizeof(msg), "Cropped WAV %d fr", frameCount); setStatus(msg);
    return true;
}

bool SampleChopperModal::undoLastDestructiveCrop() {
    if (g_lgptLastDestructiveCropBackupPath.empty()) { setStatus("No crop undo"); return false; }
    if (g_lgptLastDestructiveCropSampleIndex != sampleIndex_ || g_lgptLastDestructiveCropSamplePath != samplePath_) { setStatus("Undo: wrong sample"); return false; }
    stopSamplePreview();
    FileSystemService fs;
    fs.Copy(Path(g_lgptLastDestructiveCropBackupPath.c_str()), Path(samplePath_.c_str()));
    WavFile *reload = WavFile::Open(samplePath_.c_str());
    if (!reload) { setStatus("Undo reload fail"); return false; }
    reload->GetBuffer(0, reload->GetSize(-1));
    SoundSource *source = SamplePool::GetInstance()->GetSource(sampleIndex_);
    WavFile *wav = (WavFile *)source;
    int restoredSize = reload->GetSize(-1);
    bool ok = wav && wav->ReplaceBuffer((short *)reload->GetSampleBuffer(-1), restoredSize, reload->GetChannelCount(-1), reload->GetSampleRate(-1));
    delete reload;
    if (!ok || restoredSize <= 1) { setStatus("Undo buffer fail"); return false; }
    sourceSize_ = restoredSize;
    sampleSize_ = restoredSize;
    viewStartFrame_ = 0;
    cursorFrame_ = 0;
    boundaryCount_ = 2;
    boundaries_[0] = 0;
    boundaries_[1] = restoredSize - 1;
    for (int i = 2; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;
    selectedChop_ = 0;
    trimMode_ = false;
    chopsInitialized_ = true;
    saveChopStateForCurrentSample();
    prepareWaveformPreview();
    publishOverlayState();
    setStatus("Crop undo restored");
    return true;
}

void SampleChopperModal::previewTrimStart() {
    if (sourceSize_ <= 1) { setStatus("No sample"); return; }
    initializeChopsIfNeeded();
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();
    int previewEnd = start + (sourceRate_ > 0 ? sourceRate_ * 5 : 220500);
    if (previewEnd > end) previewEnd = end;
    playFrameRange(start, previewEnd, "Preview start");
}

void SampleChopperModal::previewTrimEnd() {
    if (sourceSize_ <= 1) { setStatus("No sample"); return; }
    initializeChopsIfNeeded();
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();
    int previewStart = end - (sourceRate_ > 0 ? sourceRate_ * 5 : 220500);
    if (previewStart < start) previewStart = start;
    playFrameRange(previewStart, end, "Preview end");
}

''' + insert_anchor, 'destructive crop and trim preview functions')

if 'bool x = (mask & EPBM_X) != 0;' not in s:
    s = replace_once(s, '    bool y = (mask & EPBM_Y) != 0;\n    bool a = (mask & EPBM_A) != 0;\n', '    bool y = (mask & EPBM_Y) != 0;\n    bool x = (mask & EPBM_X) != 0;\n    bool a = (mask & EPBM_A) != 0;\n', 'X button bool')

s = s.replace('''    if (l2 && a && !(left || right || up || down)) {
        cropToSelectedRange();
        return;
    }

    if (r1 && a) {
        saveChopStateForCurrentSample();
        setStatus("Cuts saved: assign Sxx in Phrase");
        return;
    }
''', '''    if (l2 && a && !(left || right || up || down)) {
        destructiveCropToSelectedRange();
        return;
    }

    if (l2 && x && !(left || right || up || down)) {
        undoLastDestructiveCrop();
        return;
    }

    if (r1 && a) {
        saveChopStateForCurrentSample();
        setStatus("Auto-save on: assign Sxx in Phrase");
        return;
    }
''')

s = s.replace('''        if (y && !l1 && !r1 && !l2 && !r2) {
            deleteSelectedChop();
            return;
        }

        if ((a || b) && !(left || right)) {
            setStatus(a ? "Trim: A+LEFT/RIGHT" : "Trim: B+LEFT/RIGHT");
            return;
        }
''', '''        if (y && !x && !l1 && !r1 && !l2 && !r2 && !(left || right || up || down)) {
            previewTrimStart();
            return;
        }

        if (x && !y && !l1 && !r1 && !l2 && !r2 && !(left || right || up || down)) {
            previewTrimEnd();
            return;
        }

        if ((a || b) && !(left || right)) {
            setStatus(a ? "Trim: A+LEFT/RIGHT" : "Trim: B+LEFT/RIGHT");
            return;
        }
''', 1)

s = s.replace('drawStringAbs(0, 26, "R2+LR chop  R2+A full  L2+B stop", props);', 'drawStringAbs(0, 26, "R2+LR chop R2+A full L2+X undo", props);')
s = s.replace('trimMode_ ? "TRIM: A+LR start B+LR end Y del L2+Y exit" : "L2+Y trim  L2+A crop  R1+B back"', 'trimMode_ ? "TRIM: A+LR start B+LR end Y start X end" : "L2+Y trim L2+A crop L2+X undo R1+B"')
p.write_text(s)

# PhraseView PTCH +/-10.
p = require('sources/Application/Views/PhraseView.cpp')
s = p.read_text()
old = '''    switch (direction) {
    case VUD_LEFT:
    case VUD_DOWN:
        delta = -1;
        break;
    case VUD_RIGHT:
    case VUD_UP:
        delta = 1;
        break;
    }
'''
new = '''    switch (direction) {
    case VUD_LEFT:
        delta = -1;
        break;
    case VUD_RIGHT:
        delta = 1;
        break;
    case VUD_DOWN:
        delta = -10;
        break;
    case VUD_UP:
        delta = 10;
        break;
    }
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'delta = -10;' not in s:
    raise SystemExit('Patch failed: PTCH +/-10')
p.write_text(s)

print('U2.15 patches applied: autosave-only Sxx, trim previews, destructive crop/undo, PTCH +/-10.')
U215PY

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_15_CHOPPER_AUTOSAVE_DESTRUCTIVE_CROP_PTCH10_$STAMP.log"
echo "Starting U2.15 build..."
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
