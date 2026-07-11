#!/usr/bin/env bash
# U2.14 Chopper 100 slices + persistent Sxx restore + PTCH focused display + logical crop keep.
# Apply on top of U2.13.
# Design:
#   - Chopper supports up to 100 slices: S01..S100.
#   - Chopper restore falls back by sample identity if the exact project pointer match fails.
#   - Full-range state is not treated as chopped; cropped/trimmed single-slice state is treated as S01.
#   - PTCH param stays displayed as P+00 while focused; generic hex params get an active-nibble visual cue.
#   - L2+A in Chopper keeps the selected/trimmed range and logically discards outside fragments without rewriting WAV data.
set -u
SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"

cd "$SRC" || { echo "ERROR: cannot enter SRC=$SRC"; exit 2; }

for f in \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/PhraseView.h \
  sources/Application/Views/PhraseView.cpp \
  sources/Application/Views/BaseClasses/UIBigHexVarField.cpp \
  sources/Application/Instruments/SampleInstrument.cpp \
  sources/Application/Instruments/CommandList.cpp \
  sources/Application/Instruments/CommandList.h; do
  test -f "$f" || { echo "ERROR: required file missing: $f"; exit 3; }
done

if ! grep -q "Cuts saved: assign Sxx in Phrase" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: U2.13 Chopper/Phrase Sxx model not found. Apply U2.13 first."
  exit 4
fi
if ! grep -q "formatPtchParam" sources/Application/Views/PhraseView.cpp; then
  echo "ERROR: U2.13 PTCH display helper not found. Apply U2.13 first."
  exit 5
fi

BACKUP="_backup_before_u2_14_chopper_100_cropfix_ptch_ui_$STAMP.tar.gz"
tar -czf "$BACKUP" \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/PhraseView.h \
  sources/Application/Views/PhraseView.cpp \
  sources/Application/Views/BaseClasses/UIBigHexVarField.cpp \
  sources/Application/Instruments/SampleInstrument.cpp \
  projects/Makefile

echo "Backup written: $SRC/$BACKUP"

python3 - <<'PY'
from pathlib import Path
import re


def require(path):
    p = Path(path)
    if not p.exists():
        raise SystemExit(f"Patch failed: missing {path}")
    return p


def replace_once(s, old, new, label):
    if old not in s:
        raise SystemExit(f"Patch failed: {label}")
    return s.replace(old, new, 1)

# -----------------------------------------------------------------------------
# SampleChopperModal.h: 100 chops + crop/active range helpers.
# -----------------------------------------------------------------------------
p = require('sources/Application/Views/ModalDialogs/SampleChopperModal.h')
s = p.read_text()
s = s.replace('MAX_CHOP_BOUNDARIES = 33,', 'MAX_CHOP_BOUNDARIES = 101,')
s = s.replace('MAX_CHOPS = 32', 'MAX_CHOPS = 100')
if 'bool hasActiveSliceRange() const;' not in s:
    s = replace_once(s, '    bool hasUserChops() const;\n', '    bool hasUserChops() const;\n    bool hasActiveSliceRange() const;\n', 'SampleChopperModal.h hasActiveSliceRange declaration')
if 'void cropToSelectedRange();' not in s:
    s = replace_once(s, '    void nudgeSelectedEnd(int deltaFrames);\n', '    void nudgeSelectedEnd(int deltaFrames);\n    void cropToSelectedRange();\n', 'SampleChopperModal.h crop declaration')
p.write_text(s)

# -----------------------------------------------------------------------------
# SampleChopperModal.cpp
# -----------------------------------------------------------------------------
p = require('sources/Application/Views/ModalDialogs/SampleChopperModal.cpp')
s = p.read_text()

# Constants and fixed-size arrays.
s = s.replace('static const int LGPT_CHOPPER_SAVED_BOUNDARIES = 33;', 'static const int LGPT_CHOPPER_SAVED_BOUNDARIES = 101;')
s = s.replace('int chopInstrument[32];', 'int chopInstrument[100];')
s = s.replace('for (int i = 0; i < 32; i++) chopInstrument[i] = -1;', 'for (int i = 0; i < 100; i++) chopInstrument[i] = -1;')
s = s.replace('for (int i = 0; i < 32; i++) saved.chopInstrument[i] = -1;', 'for (int i = 0; i < 100; i++) saved.chopInstrument[i] = -1;')
s = s.replace('if (chopCount > 32) chopCount = 32;', 'if (chopCount > 100) chopCount = 100;')
s = s.replace('chopIndex >= 32', 'chopIndex >= 100')
s = s.replace('for (int i = chopCount; i < 32; i++) saved.chopInstrument[i] = -1;', 'for (int i = chopCount; i < 100; i++) saved.chopInstrument[i] = -1;')
s = s.replace('note byte stores the chop index 0..31', 'note byte stores the chop index 0..99')
s = s.replace('S01..S32', 'S01..S100')
s = s.replace('if (noteValue < 0 || noteValue >= 32) return false;', 'if (noteValue < 0 || noteValue >= 100) return false;')
s = s.replace('static const int TF_MAX_CHOP_MARKERS = 31;', 'static const int TF_MAX_CHOP_MARKERS = 100;')

# Add fallback finder and full-range detector after exact finder.
anchor = '''static int lgptFindChopperSavedState(const void *projectKey,
                                     int sampleIndex,
                                     const std::string &sampleName,
                                     int sourceSize) {
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        const LGPTChopperSavedState &s = g_lgptChopperSavedStates[i];
        if (s.used &&
            s.projectKey == projectKey &&
            s.sampleIndex == sampleIndex &&
            s.sourceSize == sourceSize &&
            s.sampleName == sampleName) return i;
    }
    return -1;
}
'''
insert = anchor + r'''
static int lgptFindChopperSavedStateLoose(const void *projectKey,
                                          int sampleIndex,
                                          const std::string &sampleName,
                                          int sourceSize) {
    int slot = lgptFindChopperSavedState(projectKey, sampleIndex, sampleName, sourceSize);
    if (slot >= 0) return slot;

    /* U2.14: reopening from Phrase/Instrument can arrive with equivalent sample identity
       but not the exact state lookup tuple. Fallback in stages so session cuts do not vanish. */
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        const LGPTChopperSavedState &s = g_lgptChopperSavedStates[i];
        if (s.used && s.sampleIndex == sampleIndex && s.sourceSize == sourceSize && s.sampleName == sampleName) return i;
    }
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        const LGPTChopperSavedState &s = g_lgptChopperSavedStates[i];
        if (s.used && s.sampleIndex == sampleIndex && s.sourceSize == sourceSize) return i;
    }
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        const LGPTChopperSavedState &s = g_lgptChopperSavedStates[i];
        if (s.used && s.sampleIndex == sampleIndex) return i;
    }
    return -1;
}

static bool lgptSavedStateIsFullRange(const LGPTChopperSavedState &saved) {
    if (saved.boundaryCount != 2) return false;
    if (saved.sourceSize <= 1) return false;
    return saved.boundaries[0] <= 0 && saved.boundaries[1] >= saved.sourceSize - 1;
}
'''
if 'lgptFindChopperSavedStateLoose' not in s:
    s = replace_once(s, anchor, insert, 'loose saved-state finder insertion')

# Replace exact state lookups used by assign/count/restore/save with loose matching.
s = s.replace('int slot = lgptFindChopperSavedState(projectKey, sampleIndex, sampleName, sourceSize);',
              'int slot = lgptFindChopperSavedStateLoose(projectKey, sampleIndex, sampleName, sourceSize);')
s = s.replace('int slot = lgptFindChopperSavedState(projectKey, sampleIndex_, sampleName_, sourceSize_);',
              'int slot = lgptFindChopperSavedStateLoose(projectKey, sampleIndex_, sampleName_, sourceSize_);')
# Keep the loose helper itself anchored to the exact matcher; the replacements above are broad by design.
s = s.replace('int slot = lgptFindChopperSavedStateLoose(projectKey, sampleIndex, sampleName, sourceSize);\n    if (slot >= 0) return slot;\n\n    /* U2.14: reopening from Phrase/Instrument',
              'int slot = lgptFindChopperSavedState(projectKey, sampleIndex, sampleName, sourceSize);\n    if (slot >= 0) return slot;\n\n    /* U2.14: reopening from Phrase/Instrument')

# Saved chop count: full-range state is not a chopped instrument; cropped single range is S01.
old = r'''int count = g_lgptChopperSavedStates[slot].boundaryCount - 1;
    return count > 0 ? count : 0;'''
new = r'''const LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    if (lgptSavedStateIsFullRange(saved)) return 0;
    int count = saved.boundaryCount - 1;
    if (count > 100) count = 100;
    return count > 0 ? count : 0;'''
if old in s:
    s = s.replace(old, new, 1)
elif 'lgptSavedStateIsFullRange(saved)' not in s:
    raise SystemExit('Patch failed: saved chop count full-range filter')

# Runtime range supports 100 and ignores pure full-range state.
old = '''    int chopCount = saved.boundaryCount - 1;
    if (chopCount <= 0 || chopCount > 32) return false;
    if (chopIndex < 0 || chopIndex >= chopCount) return false;
'''
new = '''    if (lgptSavedStateIsFullRange(saved)) return false;
    int chopCount = saved.boundaryCount - 1;
    if (chopCount <= 0) return false;
    if (chopCount > 100) chopCount = 100;
    if (chopIndex < 0 || chopIndex >= chopCount) return false;
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'if (lgptSavedStateIsFullRange(saved)) return false;' not in s:
    raise SystemExit('Patch failed: runtime range 100/full-range block')

# Restore: preserve cropped endpoints instead of forcing 0/end.
s = s.replace('''        if (i == 0) value = 0;
        if (i == count - 1) value = sourceSize_ - 1;
        if (value < 0 || value >= sourceSize_ || value <= previous) {''',
              '''        if (value < 0 || value >= sourceSize_ || value <= previous) {''')

# Add hasActiveSliceRange definition after hasUserChops.
old = '''bool SampleChopperModal::hasUserChops() const {
    return (boundaryCount_ > 2);
}
'''
new = old + r'''
bool SampleChopperModal::hasActiveSliceRange() const {
    if (boundaryCount_ < 2 || sourceSize_ <= 1) return false;
    if (boundaryCount_ > 2) return true;
    return (boundaries_[0] > 0 || boundaries_[1] < sourceSize_ - 1);
}
'''
if 'bool SampleChopperModal::hasActiveSliceRange() const' not in s:
    s = replace_once(s, old, new, 'hasActiveSliceRange definition')

# Draw info and overlay should show selected cropped range too.
s = s.replace('hasUserChops() ? selectedChop_ : 0, hasUserChops() ? (boundaryCount_ - 1) : 0',
              'hasActiveSliceRange() ? selectedChop_ : 0, hasActiveSliceRange() ? (boundaryCount_ - 1) : 0')
s = s.replace('if (hasUserChops() && selectedChop_ >= 0 && selectedChop_ <= boundaryCount_ - 2) {',
              'if (hasActiveSliceRange() && selectedChop_ >= 0 && selectedChop_ <= boundaryCount_ - 2) {')

# Trim can operate on full sample to create a cropped single slice.
old = '''void SampleChopperModal::toggleTrimMode() {
    if (!hasUserChops()) { setStatus("No chop to trim"); return; }
    trimMode_ = !trimMode_;
    cursorFrame_ = selectedChopStartFrame();
'''
new = '''void SampleChopperModal::toggleTrimMode() {
    if (sourceSize_ <= 1) { setStatus("No sample to trim"); return; }
    initializeChopsIfNeeded();
    if (boundaryCount_ < 2) { setStatus("No range to trim"); return; }
    trimMode_ = !trimMode_;
    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
    cursorFrame_ = selectedChopStartFrame();
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'No sample to trim' not in s:
    raise SystemExit('Patch failed: toggleTrimMode full-sample trim')

s = s.replace('if (!hasUserChops()) { setStatus("No chop to trim"); return; }\n    int idx = selectedChop_;',
              'if (boundaryCount_ < 2) { setStatus("No range to trim"); return; }\n    int idx = selectedChop_;')
s = s.replace('if (!hasUserChops()) { setStatus("No chop to trim"); return; }\n    int idx = selectedChop_ + 1;',
              'if (boundaryCount_ < 2) { setStatus("No range to trim"); return; }\n    int idx = selectedChop_ + 1;')

# Add crop logical keep function after nudgeSelectedEnd.
anchor = 'void SampleChopperModal::prepareWaveformPreview() {'
if 'void SampleChopperModal::cropToSelectedRange()' not in s:
    idx = s.find(anchor)
    if idx < 0:
        raise SystemExit('Patch failed: crop insertion anchor')
    crop_fn = r'''
void SampleChopperModal::cropToSelectedRange() {
    if (sourceSize_ <= 1) { setStatus("No sample to crop"); return; }
    initializeChopsIfNeeded();
    if (boundaryCount_ < 2) { setStatus("No range to crop"); return; }

    selectedChop_ = clampInt(selectedChop_, 0, boundaryCount_ - 2);
    int start = selectedChopStartFrame();
    int end = selectedChopEndFrame();
    if (end <= start) { setStatus("Bad crop range"); return; }

    /* U2.14: safe logical crop. We keep the chosen/trimmed range as a single S01 slice
       and ignore material outside it at playback time. We do not rewrite the WAV file here. */
    boundaryCount_ = 2;
    boundaries_[0] = start;
    boundaries_[1] = end;
    for (int i = 2; i < MAX_CHOP_BOUNDARIES; i++) boundaries_[i] = 0;
    selectedChop_ = 0;
    trimMode_ = false;
    cursorFrame_ = start;
    saveChopStateForCurrentSample();
    ensureCursorVisible();
    prepareWaveformPreview();
    publishOverlayState();
    char msg[64];
    snprintf(msg, sizeof(msg), "Keep range %d-%d", start, end);
    setStatus(msg);
}

'''
    s = s[:idx] + crop_fn + s[idx:]

# Play selected crop range even when it is a single cropped slice.
old = '''void SampleChopperModal::playSelectedChop() {
    initializeChopsIfNeeded();
    if (!hasUserChops()) { playFullSample(); return; }
    playFrameRange(selectedChopStartFrame(), selectedChopEndFrame(), "Play chop");
}
'''
new = '''void SampleChopperModal::playSelectedChop() {
    initializeChopsIfNeeded();
    if (!hasActiveSliceRange()) { playFullSample(); return; }
    playFrameRange(selectedChopStartFrame(), selectedChopEndFrame(), "Play chop");
}
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'if (!hasActiveSliceRange()) { playFullSample(); return; }' not in s:
    raise SystemExit('Patch failed: playSelectedChop crop-aware')

# Add cuts only inside current active crop/full range.
old = '''    if (frame <= 0 || frame >= sourceSize_ - 1) { setStatus("Cannot chop at edge"); return; }
'''
new = '''    int minEdge = (boundaryCount_ >= 2) ? boundaries_[0] : 0;
    int maxEdge = (boundaryCount_ >= 2) ? boundaries_[boundaryCount_ - 1] : (sourceSize_ - 1);
    if (frame <= minEdge || frame >= maxEdge) { setStatus("Cannot chop at edge"); return; }
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'int minEdge = (boundaryCount_ >= 2)' not in s:
    raise SystemExit('Patch failed: addChopAtCursor crop-edge guard')

# L2+A crop action before normal R1+A save.
old = '''    if (l2 && y) {
        toggleTrimMode();
        return;
    }

    if (r1 && a) {
'''
new = '''    if (l2 && y) {
        toggleTrimMode();
        return;
    }

    if (l2 && a && !(left || right || up || down)) {
        cropToSelectedRange();
        return;
    }

    if (r1 && a) {
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'cropToSelectedRange();' not in s:
    raise SystemExit('Patch failed: L2+A crop binding')

# Update help text.
s = s.replace('drawStringAbs(0, 25, "A cut/live Y del B play", props);',
              'drawStringAbs(0, 25, "A cut/live Y del B play L2+A crop", props);')
s = s.replace('"L2+Y trim  R1+B back  Phrase:Sxx"', '"L2+Y trim  L2+A crop  R1+B back"')

# Status/comment max.
s = s.replace('Max chops reached', 'Max 100 chops reached')

p.write_text(s)

# -----------------------------------------------------------------------------
# PhraseView.h/cpp: S100 and PTCH focused display.
# -----------------------------------------------------------------------------
p = require('sources/Application/Views/PhraseView.h')
s = p.read_text()
if 'bool isPtchParamCell(int row, int col) const;' not in s:
    s = replace_once(s, '    void formatPtchParam(ushort value, char *buffer, int bufferLen) const;\n',
                     '    void formatPtchParam(ushort value, char *buffer, int bufferLen) const;\n    bool isPtchParamCell(int row, int col) const;\n',
                     'PhraseView.h isPtchParamCell declaration')
p.write_text(s)

p = require('sources/Application/Views/PhraseView.cpp')
s = p.read_text()
s = s.replace('S01..S32', 'S01..S100')
s = s.replace('if (count > 32) count = 32;', 'if (count > 100) count = 100;')
# If any hard note/chop cap remains in helper logic.
s = s.replace('noteValue >= 32', 'noteValue >= 100')

# Add isPtchParamCell after formatPtchParam.
anchor = r'''void PhraseView::formatPtchParam(ushort value, char *buffer, int bufferLen) const {
    if (!buffer || bufferLen <= 0) return;
    int pitch = (int)((signed char)(value & 0xFF));
    if (pitch < -24) pitch = -24;
    if (pitch > 24) pitch = 24;
    snprintf(buffer, bufferLen, "P%+03d", pitch);
}
'''
insert = anchor + r'''
bool PhraseView::isPtchParamCell(int row, int col) const {
    if (row < 0 || row > 15) return false;
    int offset = 16 * viewData_->currentPhrase_ + row;
    if (col == 3) return phrase_->cmd1_[offset] == I_CMD_PTCH;
    if (col == 5) return phrase_->cmd2_[offset] == I_CMD_PTCH;
    return false;
}
'''
if 'bool PhraseView::isPtchParamCell(int row, int col) const' not in s:
    s = replace_once(s, anchor, insert, 'PhraseView.cpp isPtchParamCell definition')

# Do not overlay the generic hex editor over P+00/P-24 while focused.
old = '''    if ((viewMode_ != VM_SELECTION) && ((col_ == 3) || (col_ == 5))) {
        cmdEditField_->SetFocus();
        cmdEditField_->Draw(w_);
    };
'''
new = '''    if ((viewMode_ != VM_SELECTION) && ((col_ == 3) || (col_ == 5))) {
        if (!isPtchParamCell(row_, col_)) {
            cmdEditField_->SetFocus();
            cmdEditField_->Draw(w_);
        }
    };
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'if (!isPtchParamCell(row_, col_))' not in s:
    raise SystemExit('Patch failed: skip generic editor on PTCH cell')

# Help text via notification while adjusting PTCH.
s = s.replace('snprintf(status, sizeof(status), "PTCH %+d", pitch);', 'snprintf(status, sizeof(status), "PTCH P%+03d", pitch);')

p.write_text(s)

# -----------------------------------------------------------------------------
# SampleInstrument.cpp: S01..S100 runtime range mapping.
# -----------------------------------------------------------------------------
p = require('sources/Application/Instruments/SampleInstrument.cpp')
s = p.read_text()
s = s.replace('if (midinote < 32 && LGPTChopperGetChopRangeForSampleIndex',
              'if (midinote < 100 && LGPTChopperGetChopRangeForSampleIndex')
p.write_text(s)

# -----------------------------------------------------------------------------
# UIBigHexVarField.cpp: active nibble visual cue for all generic hex params.
# -----------------------------------------------------------------------------
p = require('sources/Application/Views/BaseClasses/UIBigHexVarField.cpp')
s = p.read_text()
old = '''\t\t#if defined(PLATFORM_TREEFROG)\n\t\t\t/* TREEFROG_V1_3_1_BIGHEX_FULL_FOCUS: the classic editor redraws the active nibble in\n\t\t\t   CD_NORMAL after drawing the whole hex field. With TreeFrog's\n\t\t\t   color-only focus this made 0000 look like only 3 chars were\n\t\t\t   selected in Phrase/Table param fields. Keep the active nibble\n\t\t\t   in highlight color while the field has focus. */\n\t\t\t((AppWindow&)w).SetColor(focus_ ? CD_HILITE2 : CD_NORMAL) ;\n\t\t#else\n\t\t\t((AppWindow&)w).SetColor(CD_NORMAL) ;\n\t\t#endif\n\t\tw.DrawString(buffer+offset,position,props) ;\n'''
new = '''\t\t#if defined(PLATFORM_TREEFROG)\n\t\t\t/* U2.14: draw the active nibble with a distinct visual treatment so\n\t\t\t   Phrase/Table parameter editing shows which digit LEFT/RIGHT selected. */\n\t\t\tGUITextProperties digitProps = props ;\n\t\t\tdigitProps.invert_ = focus_ ? true : props.invert_ ;\n\t\t\t((AppWindow&)w).SetColor(focus_ ? CD_HILITE1 : CD_NORMAL) ;\n\t\t\tw.DrawString(buffer+offset,position,digitProps) ;\n\t\t#else\n\t\t\t((AppWindow&)w).SetColor(CD_NORMAL) ;\n\t\t\tw.DrawString(buffer+offset,position,props) ;\n\t\t#endif\n'''
if old in s:
    s = s.replace(old, new, 1)
elif 'U2.14: draw the active nibble' not in s:
    raise SystemExit('Patch failed: UIBigHexVarField active nibble cue')
p.write_text(s)

print('U2.14 patches applied: 100 chops, robust restore, PTCH focused display, generic digit cue, L2+A logical crop.')
PY

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_14_CHOPPER_100_CROPFIX_PTCH_UI_$STAMP.log"
echo "Starting U2.14 build..."
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
  grep -nE "error:|undefined reference|No such file|fatal error|make.*Error|Error [0-9]+" "$LOG" | sed -n '1,280p'
  tail -n 180 "$LOG"
fi
exit "$RC"
