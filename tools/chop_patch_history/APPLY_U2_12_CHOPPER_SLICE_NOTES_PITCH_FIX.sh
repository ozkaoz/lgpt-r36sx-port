#!/usr/bin/env bash
# U2.12 Chopper slice-note assignment model + full-play route fix.
# Apply on top of U2.11 FIX1 (or a tree partially patched to U2.11).
# Changes: Phrase rows use the same source instrument and S01/S02 slice notes; no cloned/imported chop instruments.
set -u
SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"

cd "$SRC" || { echo "ERROR: cannot enter SRC=$SRC"; exit 2; }

for f in \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/PhraseView.cpp \
  sources/Application/Instruments/SampleInstrument.cpp \
  sources/Application/Instruments/CommandList.cpp \
  sources/Application/Instruments/CommandList.h \
  sources/Application/Player/Player.h \
  sources/Application/Player/Player.cpp \
  sources/Application/Player/PlayerMixer.h \
  sources/Application/Player/PlayerMixer.cpp \
  sources/Application/Audio/AudioFileStreamer.h \
  sources/Application/Audio/AudioFileStreamer.cpp; do
  test -f "$f" || { echo "ERROR: required file missing: $f"; exit 3; }
done

if ! grep -q "LGPTChopperAssignSavedChopToPhraseRow" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: U2.11 chopper assignment API not found. Apply U2.11 FIX1 first."
  exit 4
fi

BACKUP="_backup_before_u2_12_chopper_slice_notes_pitch_fix_$STAMP.tar.gz"
tar -czf "$BACKUP" \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/PhraseView.h \
  sources/Application/Views/PhraseView.cpp \
  sources/Application/Instruments/SampleInstrument.cpp \
  sources/Application/Instruments/CommandList.cpp \
  sources/Application/Instruments/CommandList.h \
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

# --- Header: public runtime-slice helpers used by PhraseView and SampleInstrument. ---
p = require('sources/Application/Views/ModalDialogs/SampleChopperModal.h')
s = p.read_text()
if 'LGPTChopperIsChopNoteForInstrument' not in s:
    anchor = """int LGPTChopperGetSavedChopCountForInstrument(ViewData *viewData,
                                               int sourceInstrumentIndex);
"""
    insert = anchor + """bool LGPTChopperIsChopNoteForInstrument(ViewData *viewData,
                                          int sourceInstrumentIndex,
                                          int noteValue,
                                          int *displayChopNumber);
bool LGPTChopperGetChopRangeForSampleIndex(int sampleIndex,
                                           int chopIndex,
                                           int *startFrame,
                                           int *endFrameExclusive);
"""
    s = replace_once(s, anchor, insert, 'SampleChopperModal.h helper declarations')
p.write_text(s)

# --- SampleChopperModal.cpp: S-note assignment, runtime range lookup, R2+A full-play and R1+B only exit. ---
p = require('sources/Application/Views/ModalDialogs/SampleChopperModal.cpp')
s = p.read_text()

start = s.find('bool LGPTChopperAssignSavedChopToPhraseRow(ViewData *viewData,')
if start < 0:
    raise SystemExit('Patch failed: assignment helper start')
end = s.find('\nint LGPTChopperGetSavedChopCountForInstrument', start)
if end < 0:
    raise SystemExit('Patch failed: assignment helper end')
new_helper = r'''bool LGPTChopperAssignSavedChopToPhraseRow(ViewData *viewData,
                                           int phraseIndex,
                                           int row,
                                           int sourceInstrumentIndex,
                                           int requestedChop,
                                           int delta,
                                           bool advanceSessionCursor,
                                           char *status,
                                           int statusLen) {
    lgptSetAssignStatus(status, statusLen, "");
    if (!viewData || !viewData->song_ || !viewData->song_->phrase_ || !viewData->project_) {
        lgptSetAssignStatus(status, statusLen, "No phrase/project");
        return false;
    }
    if (phraseIndex < 0 || phraseIndex >= PHRASE_COUNT || phraseIndex == 0xFE || phraseIndex == 0xFF) {
        phraseIndex = viewData->currentPhrase_;
    }
    if (phraseIndex < 0 || phraseIndex >= PHRASE_COUNT || phraseIndex == 0xFE || phraseIndex == 0xFF) {
        lgptSetAssignStatus(status, statusLen, "No phrase");
        return false;
    }
    if (row < 0 || row > 15) row = viewData->phraseCurPos_;
    if (row < 0 || row > 15) row = 0;

    int sampleIndex = NO_SAMPLE;
    int sourceSize = 0;
    std::string sampleName;
    if (!lgptGetSampleIdentityForInstrument(viewData, sourceInstrumentIndex,
                                            sampleIndex, sampleName, sourceSize)) {
        lgptSetAssignStatus(status, statusLen, "No source sample");
        return false;
    }

    const void *projectKey = viewData->project_ ? (const void *)viewData->project_ : 0;
    int slot = lgptFindChopperSavedState(projectKey, sampleIndex, sampleName, sourceSize);
    if (slot < 0) {
        lgptSetAssignStatus(status, statusLen, "No saved chops");
        return false;
    }

    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    int chopCount = saved.boundaryCount - 1;
    if (chopCount <= 0) {
        lgptSetAssignStatus(status, statusLen, "No chops");
        return false;
    }
    if (chopCount > 32) chopCount = 32;

    Phrase *phrase = viewData->song_->phrase_;
    int offset = 16 * phraseIndex + row;
    int currentChop = -1;
    int currentInstr = phrase->instr_[offset];
    int currentNote = phrase->note_[offset];
    if (currentInstr == sourceInstrumentIndex && currentNote >= 0 && currentNote < chopCount) {
        currentChop = currentNote;
    }

    int chopIndex = requestedChop;
    if (chopIndex < 0) {
        if (currentChop >= 0) chopIndex = currentChop + delta;
        else chopIndex = saved.selectedChop + delta;
    }
    while (chopIndex < 0) chopIndex += chopCount;
    chopIndex %= chopCount;

    /* U2.12 sampler-tracker model:
       - The phrase instrument remains the original sample instrument, e.g. I05 on every row.
       - The phrase note byte stores the chop index 0..31 and is rendered as S01..S32.
       - SampleInstrument maps S-note triggers to saved chopper boundaries at playback time.
       - Existing command columns are not cleared, so PTCH/ARPG/VOLM/FCUT/etc remain available. */
    phrase->note_[offset] = (unsigned char)chopIndex;
    phrase->instr_[offset] = (unsigned char)sourceInstrumentIndex;

    saved.selectedChop = chopIndex;
    viewData->currentInstrument_ = sourceInstrumentIndex;
    if (advanceSessionCursor) {
        int nextRow = row + 1;
        if (nextRow > 15) nextRow = 15;
        viewData->phraseCurPos_ = nextRow;
    } else {
        viewData->phraseCurPos_ = row;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "S%02d -> P%02X R%02X I%02X", chopIndex + 1, phraseIndex, row, sourceInstrumentIndex);
    lgptSetAssignStatus(status, statusLen, msg);
    return true;
}
'''
s = s[:start] + new_helper + s[end:]

if 'LGPTChopperGetChopRangeForSampleIndex' not in s:
    m = re.search(r'int\s+LGPTChopperGetSavedChopCountForInstrument\(ViewData \*viewData,\s*int sourceInstrumentIndex\)\s*\{.*?\n\}', s, flags=re.S)
    if not m:
        raise SystemExit('Patch failed: GetSavedChopCountForInstrument block')
    helpers = r'''

static int lgptFindSavedStateBySampleIndex(int sampleIndex) {
    if (sampleIndex < 0) return -1;
    for (int i = 0; i < LGPT_CHOPPER_SAVED_STATE_COUNT; i++) {
        const LGPTChopperSavedState &s = g_lgptChopperSavedStates[i];
        if (s.used && s.sampleIndex == sampleIndex && s.boundaryCount >= 2) return i;
    }
    return -1;
}

bool LGPTChopperGetChopRangeForSampleIndex(int sampleIndex,
                                           int chopIndex,
                                           int *startFrame,
                                           int *endFrameExclusive) {
    int slot = lgptFindSavedStateBySampleIndex(sampleIndex);
    if (slot < 0) return false;
    LGPTChopperSavedState &saved = g_lgptChopperSavedStates[slot];
    int chopCount = saved.boundaryCount - 1;
    if (chopCount <= 0 || chopCount > 32) return false;
    if (chopIndex < 0 || chopIndex >= chopCount) return false;
    int start = saved.boundaries[chopIndex];
    int endExclusive = saved.boundaries[chopIndex + 1] + 1;
    if (start < 0) start = 0;
    if (endExclusive <= start) endExclusive = start + 1;
    if (saved.sourceSize > 0 && endExclusive > saved.sourceSize) endExclusive = saved.sourceSize;
    if (startFrame) *startFrame = start;
    if (endFrameExclusive) *endFrameExclusive = endExclusive;
    return true;
}

bool LGPTChopperIsChopNoteForInstrument(ViewData *viewData,
                                          int sourceInstrumentIndex,
                                          int noteValue,
                                          int *displayChopNumber) {
    if (noteValue < 0 || noteValue >= 32) return false;
    int count = LGPTChopperGetSavedChopCountForInstrument(viewData, sourceInstrumentIndex);
    if (count <= 0 || noteValue >= count) return false;
    if (displayChopNumber) *displayChopNumber = noteValue + 1;
    return true;
}
'''
    s = s[:m.end()] + helpers + s[m.end():]

# Full sample playback should use the proven range-stream path.
s = re.sub(r'void SampleChopperModal::playFullSample\(\)\s*\{\s*playFromFrame\(0,\s*"Play full"\);\s*\}',
           'void SampleChopperModal::playFullSample() {\n    if (sourceSize_ > 1) playFrameRange(0, sourceSize_ - 1, "Play full");\n    else playFromFrame(0, "Play full");\n}',
           s, count=1)

# Only R1+B should leave the Chopper; L1+B is no longer an exit alias.
s = s.replace('if ((l1 || r1) && b && !(left || right || up || down)) {',
              'if (r1 && !l1 && b && !(left || right || up || down)) {')
s = s.replace('"L2+Y trim  L1+B back  Phr:R2+LR"', '"L2+Y trim  R1+B back  Phr:R2+LR"')

p.write_text(s)

# --- PhraseView.cpp: display slice notes as S01/S02 and keep last note/instrument from row after assignment. ---
p = require('sources/Application/Views/PhraseView.cpp')
s = p.read_text()
old = """    if (status[0]) View::SetNotification(status);
    if (ok) {
        lastNote_ = 60;
        lastInstr_ = viewData_->currentInstrument_;
        if (advanceRow) updateCursor(0, 1);
        else updateCursor(0, 0);
        isDirty_ = true;
    }
"""
new = """    if (status[0]) View::SetNotification(status);
    if (ok) {
        int offset = 16 * viewData_->currentPhrase_ + row_;
        lastNote_ = phrase_->note_[offset];
        lastInstr_ = phrase_->instr_[offset];
        if (advanceRow) updateCursor(0, 1);
        else updateCursor(0, 0);
        isDirty_ = true;
    }
"""
if old in s:
    s = s.replace(old, new, 1)

s = s.replace("""    // U2.11: R2+LEFT/RIGHT assigns saved chops from the note/instrument column.
    // The note stays C-3 as a neutral trigger; pitch belongs in PTCH/ARPG/FX.
""", """    // U2.12: R2+LEFT/RIGHT selects saved chops from the note/instrument column.
    // Rows keep the same instrument; the note column displays S01..S32 instead of C-3.
    // Pitch changes belong in PTCH/ARPG/FX command columns.
""")

old = """        if (d == 0xFF) {
            DrawString(pos._x, pos._y, "----", props);
        } else {
            note2char(d, buffer);
            DrawString(pos._x, pos._y, buffer, props);
        }
"""
new = """        if (d == 0xFF) {
            DrawString(pos._x, pos._y, "----", props);
        } else {
            int phraseOffset = 16 * viewData_->currentPhrase_ + j;
            int rowInstr = phrase_->instr_[phraseOffset];
            int chopNumber = 0;
            if (LGPTChopperIsChopNoteForInstrument(viewData_, rowInstr, d, &chopNumber)) {
                snprintf(buffer, sizeof(buffer), "S%02d", chopNumber);
                DrawString(pos._x, pos._y, buffer, props);
            } else {
                note2char(d, buffer);
                DrawString(pos._x, pos._y, buffer, props);
            }
        }
"""
if old in s:
    s = s.replace(old, new, 1)
elif 'LGPTChopperIsChopNoteForInstrument' not in s:
    raise SystemExit('Patch failed: PhraseView note draw block')
p.write_text(s)

# --- SampleInstrument.cpp: interpret S-note values as chopper boundary ranges for this sample. ---
p = require('sources/Application/Instruments/SampleInstrument.cpp')
s = p.read_text()
if 'extern bool LGPTChopperGetChopRangeForSampleIndex' not in s:
    s = s.replace('#include "CommandList.h"', '#include "CommandList.h"\nextern bool LGPTChopperGetChopRangeForSampleIndex(int sampleIndex, int chopIndex, int *startFrame, int *endFrameExclusive);', 1)
old = """  rp->midiNote_=midinote ;
  
  if (lastMidiNote_[channel] == -1) // To prevent First LEGA to go bonkers
  {
    lastMidiNote_[channel]=midinote ;
  }
"""
new = """  int lgptRuntimeChop = -1;
  int lgptRuntimeChopStart = 0;
  int lgptRuntimeChopEnd = 0;
  unsigned char playbackNote = midinote;
  if (midinote < 32 && LGPTChopperGetChopRangeForSampleIndex(GetSampleIndex(), (int)midinote,
                                                              &lgptRuntimeChopStart,
                                                              &lgptRuntimeChopEnd)) {
    lgptRuntimeChop = (int)midinote;
    playbackNote = 60; // neutral C-3 trigger; pitch is controlled by PTCH/ARPG/FX.
  }

  rp->midiNote_=playbackNote ;
  
  if (lastMidiNote_[channel] == -1) // To prevent First LEGA to go bonkers
  {
    lastMidiNote_[channel]=playbackNote ;
  }
"""
if old in s:
    s = s.replace(old, new, 1)
s = s.replace('int isSliced = slices_->GetInt() > 1;', 'int isSliced = (lgptRuntimeChop < 0) && (slices_->GetInt() > 1);')
s = s.replace('SampleInstrumentLoopMode loopmode=(SampleInstrumentLoopMode)loopMode_->GetInt() ;',
              'SampleInstrumentLoopMode loopmode=(SampleInstrumentLoopMode)loopMode_->GetInt() ;\n     if (lgptRuntimeChop >= 0) loopmode = SILM_ONESHOT;')
if 'if (lgptRuntimeChop >= 0) {\n            rp->rendLoopStart_=lgptRuntimeChopStart;' not in s:
    s = s.replace("""        // Compute octave & note difference from root
        float fineTune = float(fineTune_->GetInt() - 0x7F);
""", """        if (lgptRuntimeChop >= 0) {
            rp->rendLoopStart_=lgptRuntimeChopStart;
            rp->rendLoopEnd_=lgptRuntimeChopEnd;
            rp->rendFirst_=lgptRuntimeChopStart;
            rp->position_=float(rp->rendFirst_);
            rp->reverse_=false;
        }

        // Compute octave & note difference from root
        float fineTune = float(fineTune_->GetInt() - 0x7F);
""", 1)
s = s.replace('int offset = midinote - rootNote;', 'int offset = playbackNote - rootNote;')
p.write_text(s)

# --- Command selector: PTCH should be present. It already is in this codebase; fail loudly if not. ---
cmd_h = require('sources/Application/Instruments/CommandList.h').read_text()
cmd_cpp = require('sources/Application/Instruments/CommandList.cpp').read_text()
if 'I_CMD_PTCH' not in cmd_h or 'I_CMD_PTCH' not in cmd_cpp:
    raise SystemExit('Patch failed: PTCH command is not present in CommandList; add it before U2.12')

print('U2.12 patches applied: S-note chops, no cloned chop instruments, R2+A full via range, R1+B only exit, PTCH verified.')
PY

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_12_CHOPPER_SLICE_NOTES_PITCH_FIX_$STAMP.log"
echo "Starting U2.12 build..."
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
  grep -nE "error:|undefined reference|No such file|fatal error|make.*Error|Error [0-9]+" "$LOG" | sed -n '1,240p'
  tail -n 140 "$LOG"
fi
exit "$RC"
