#!/usr/bin/env bash
# U2.13 Chopper boundaries only + Phrase S-note slice editor + live chopping + PTCH +/-24.
# Apply on top of U2.12.
# Design:
#   - Chopper defines/saves cuts; it does not write Phrase rows with R1+A.
#   - Phrase rows keep the original source instrument number, e.g. I05.
#   - If that instrument has saved chops, the note field behaves as S01..Sxx instead of C-3/C-4.
#   - If the instrument has no saved chops, Phrase note editing stays normal.
#   - PTCH param editing is clamped to -24..+24 semitones while preserving high-byte ramp speed.
set -u
SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"

cd "$SRC" || { echo "ERROR: cannot enter SRC=$SRC"; exit 2; }

for f in \
  sources/Application/Views/ModalDialogs/SampleChopperModal.h \
  sources/Application/Views/ModalDialogs/SampleChopperModal.cpp \
  sources/Application/Views/PhraseView.h \
  sources/Application/Views/PhraseView.cpp \
  sources/Application/Instruments/SampleInstrument.cpp \
  sources/Application/Instruments/CommandList.cpp \
  sources/Application/Instruments/CommandList.h \
  sources/Application/Player/Player.h \
  sources/Application/Player/Player.cpp \
  sources/Application/Player/PlayerMixer.h \
  sources/Application/Player/PlayerMixer.cpp; do
  test -f "$f" || { echo "ERROR: required file missing: $f"; exit 3; }
done

if ! grep -q "LGPTChopperIsChopNoteForInstrument" sources/Application/Views/ModalDialogs/SampleChopperModal.cpp; then
  echo "ERROR: U2.12 slice-note helper not found. Apply U2.12 first."
  exit 4
fi

if ! grep -q "GetStreamingPosition" sources/Application/Player/Player.h; then
  echo "ERROR: U2.11/U2.12 streaming position API not found."
  exit 5
fi

BACKUP="_backup_before_u2_13_chopper_phrase_slice_editor_live_cut_pitch_$STAMP.tar.gz"
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

# -----------------------------------------------------------------------------
# SampleChopperModal.cpp
# -----------------------------------------------------------------------------
p = require('sources/Application/Views/ModalDialogs/SampleChopperModal.cpp')
s = p.read_text()

s = s.replace('R1+LEFT/RIGHT browses loaded project samples; R1+A assigns selected chop to the current phrase row.',
              'R1+LEFT/RIGHT browses loaded project samples; R1+A is intentionally not a phrase write action.')
s = s.replace('R1+LEFT/RIGHT browses loaded project samples; R1+A exports chops to phrase rows as cloned instruments.',
              'R1+LEFT/RIGHT browses loaded project samples; R1+A is intentionally not a phrase write action.')

old = '''    if (r1 && a) {
        assignSelectedChopToPhrase();
        return;
    }
'''
new = '''    if (r1 && a) {
        saveChopStateForCurrentSample();
        setStatus("Cuts saved: assign Sxx in Phrase");
        return;
    }
'''
if old in s:
    s = s.replace(old, new, 1)
else:
    new_s = re.sub(r'    if \(r1 && a\) \{\s*(?:assignSelectedChopToPhrase\(\);|exportChopsToPhrase\(\);)\s*return;\s*\}\n', new, s, count=1)
    if new_s == s and 'Cuts saved: assign Sxx in Phrase' not in s:
        raise SystemExit('Patch failed: Chopper R1+A block')
    s = new_s

old = '''    int frame = getCursorFrame();
    if (frame <= 0 || frame >= sourceSize_ - 1) { setStatus("Cannot chop at edge"); return; }
'''
new = '''    int frame = getCursorFrame();
    bool liveCut = false;
    if (previewActive_ && Player::GetInstance()->IsStreaming()) {
        int liveFrame = Player::GetInstance()->GetStreamingPosition();
        if (liveFrame >= previewStartFrame_ && liveFrame <= previewEndFrame_) {
            frame = clampInt(liveFrame, 0, sourceSize_ - 1);
            cursorFrame_ = frame;
            liveCut = true;
        }
    }
    if (frame <= 0 || frame >= sourceSize_ - 1) { setStatus("Cannot chop at edge"); return; }
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'bool liveCut = false;' not in s:
    raise SystemExit('Patch failed: addChopAtCursor frame block')

old = '''    char msg[64]; snprintf(msg, sizeof(msg), "Chop %02d at %d", selectedChop_, frame); setStatus(msg);
'''
new = '''    char msg[64]; snprintf(msg, sizeof(msg), liveCut ? "Live chop %02d at %d" : "Chop %02d at %d", selectedChop_, frame); setStatus(msg);
'''
if old in s:
    s = s.replace(old, new, 1)

s = s.replace('drawStringAbs(0, 25, "A cut Y del B play R1+A assign", props);',
              'drawStringAbs(0, 25, "A cut/live Y del B play", props);')
s = s.replace('drawStringAbs(0, 25, "A cut Y del B play R1+A export", props);',
              'drawStringAbs(0, 25, "A cut/live Y del B play", props);')
s = s.replace('"L2+Y trim  R1+B back  Phr:R2+LR"', '"L2+Y trim  R1+B back  Phrase:Sxx"')
s = s.replace('"L2+Y trim  L1+B back/export unsaved"', '"L2+Y trim  R1+B back  Phrase:Sxx"')
p.write_text(s)

# -----------------------------------------------------------------------------
# PhraseView.h
# -----------------------------------------------------------------------------
p = require('sources/Application/Views/PhraseView.h')
s = p.read_text()
if 'getChopSourceInstrumentForRow' not in s:
    old = '    int getChopSourceInstrumentForCurrentRow();\n    bool assignChopFromPhrase(int delta, bool advanceRow);'
    new = '    int getChopSourceInstrumentForCurrentRow();\n    int getChopSourceInstrumentForRow(int row);\n    int getSavedChopCountForRow(int row, int *sourceInstrument);\n    bool updateChopNoteValueForRow(int row, ViewUpdateDirection direction);\n    bool pasteDefaultChopForRow(int row);\n    bool adjustPtchParamForRow(int row, int paramCol, ViewUpdateDirection direction);\n    void formatPtchParam(ushort value, char *buffer, int bufferLen) const;\n    bool assignChopFromPhrase(int delta, bool advanceRow);'
    if old not in s:
        raise SystemExit('Patch failed: PhraseView.h helper declarations')
    s = s.replace(old, new, 1)
p.write_text(s)

# -----------------------------------------------------------------------------
# PhraseView.cpp
# -----------------------------------------------------------------------------
p = require('sources/Application/Views/PhraseView.cpp')
s = p.read_text()

# PTCH param edit: inject before generic param editor if not already present.
old = '''    case 3:
        switch (direction) {
        case VUD_RIGHT:
            cmdEditField_->ProcessArrow(EPBM_RIGHT);
            break;
        case VUD_UP:
            cmdEditField_->ProcessArrow(EPBM_UP);
            break;
        case VUD_LEFT:
            cmdEditField_->ProcessArrow(EPBM_LEFT);
            break;
        case VUD_DOWN:
            cmdEditField_->ProcessArrow(EPBM_DOWN);
            break;
        }
        *(phrase_->param1_ + (16 * viewData_->currentPhrase_ + row_ +
                              yOffset)) = cmdEdit_.GetInt();
        lastParam_ = cmdEdit_.GetInt();
        break;
'''
new = '''    case 3:
        if (adjustPtchParamForRow(row_ + yOffset, 3, direction)) break;
        switch (direction) {
        case VUD_RIGHT:
            cmdEditField_->ProcessArrow(EPBM_RIGHT);
            break;
        case VUD_UP:
            cmdEditField_->ProcessArrow(EPBM_UP);
            break;
        case VUD_LEFT:
            cmdEditField_->ProcessArrow(EPBM_LEFT);
            break;
        case VUD_DOWN:
            cmdEditField_->ProcessArrow(EPBM_DOWN);
            break;
        }
        *(phrase_->param1_ + (16 * viewData_->currentPhrase_ + row_ +
                              yOffset)) = cmdEdit_.GetInt();
        lastParam_ = cmdEdit_.GetInt();
        break;
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'adjustPtchParamForRow(row_ + yOffset, 3, direction)' not in s:
    raise SystemExit('Patch failed: PTCH param1 edit block')

old = '''    case 5:
        switch (direction) {
        case VUD_RIGHT:
            cmdEditField_->ProcessArrow(EPBM_RIGHT);
            break;
        case VUD_UP:
            cmdEditField_->ProcessArrow(EPBM_UP);
            break;
        case VUD_LEFT:
            cmdEditField_->ProcessArrow(EPBM_LEFT);
            break;
        case VUD_DOWN:
            cmdEditField_->ProcessArrow(EPBM_DOWN);
            break;
        }
        *(phrase_->param2_ + (16 * viewData_->currentPhrase_ + row_ +
                              yOffset)) = cmdEdit_.GetInt();
        lastParam_ = cmdEdit_.GetInt();
        break;
'''
new = '''    case 5:
        if (adjustPtchParamForRow(row_ + yOffset, 5, direction)) break;
        switch (direction) {
        case VUD_RIGHT:
            cmdEditField_->ProcessArrow(EPBM_RIGHT);
            break;
        case VUD_UP:
            cmdEditField_->ProcessArrow(EPBM_UP);
            break;
        case VUD_LEFT:
            cmdEditField_->ProcessArrow(EPBM_LEFT);
            break;
        case VUD_DOWN:
            cmdEditField_->ProcessArrow(EPBM_DOWN);
            break;
        }
        *(phrase_->param2_ + (16 * viewData_->currentPhrase_ + row_ +
                              yOffset)) = cmdEdit_.GetInt();
        lastParam_ = cmdEdit_.GetInt();
        break;
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'adjustPtchParamForRow(row_ + yOffset, 5, direction)' not in s:
    raise SystemExit('Patch failed: PTCH param2 edit block')

# Replace note/instrument data edit block in updateCursorValue.
start = s.find('\n    if ((c) && (*c != 0xFF)) {')
end = s.find('\n    Player *player = Player::GetInstance();', start)
if start < 0 or end < 0:
    raise SystemExit('Patch failed: PhraseView updateCursorValue data block')
new_block = r'''
    if ((c) && (*c != 0xFF)) {
        int editCol = col_ + xOffset;
        if (editCol == 0 && updateChopNoteValueForRow(row_ + yOffset, direction)) {
            lastNote_ = *c;
        } else {
            int offset = offsets_[editCol][direction];
            // If note column apply the selected musical scale only for normal, non-chopped instruments.
            if (editCol == 0) {
                int scale = viewData_->project_->GetScale();
                while (!scaleSteps[scale][(*c + offset) % 12]) {
                    offset > 0 ? offset++ : offset--;
                }
            }
            updateData(c, offset, limit, wrap);
            switch (editCol) {
            case 0:
                lastNote_ = *c;
                break;
            case 1:
                lastInstr_ = *c;
                break;
            }
        }
    }
'''
s = s[:start] + new_block + s[end:]

# pasteLast: if row instrument has saved chops, A on note inserts S01/S-last instead of C-3.
old = '''    case 0:
        c = phrase_->note_ + (16 * viewData_->currentPhrase_ + row_);
        if ((*c == 0xFF)) {
            *c = lastNote_;
            c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
            *c = lastInstr_;
            isDirty_ = true;
        } else {
            lastNote_ = *c;
            c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
            lastInstr_ = *c;
        }
        break;
'''
new = '''    case 0:
        c = phrase_->note_ + (16 * viewData_->currentPhrase_ + row_);
        if ((*c == 0xFF)) {
            if (pasteDefaultChopForRow(row_)) break;
            *c = lastNote_;
            c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
            *c = lastInstr_;
            isDirty_ = true;
        } else {
            lastNote_ = *c;
            c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
            lastInstr_ = *c;
        }
        break;
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'pasteDefaultChopForRow(row_)' not in s:
    raise SystemExit('Patch failed: pasteLast note block')

# Add helper definitions before current-row helper.
anchor = 'int PhraseView::getChopSourceInstrumentForCurrentRow() {'
if 'int PhraseView::getChopSourceInstrumentForRow(int row)' not in s:
    idx = s.find(anchor)
    if idx < 0:
        raise SystemExit('Patch failed: current-row chop helper anchor')
    helpers = r'''
int PhraseView::getChopSourceInstrumentForRow(int row) {
    if (row < 0 || row > 15) row = row_;
    int offset = 16 * viewData_->currentPhrase_ + row;
    unsigned char rowInstr = phrase_->instr_[offset];
    if (rowInstr != 0xFF) return rowInstr;
    if (viewData_->currentInstrument_ != 0xFF) return viewData_->currentInstrument_;
    if (lastInstr_ != 0xFF) return lastInstr_;
    return -1;
}

int PhraseView::getSavedChopCountForRow(int row, int *sourceInstrument) {
    int instr = getChopSourceInstrumentForRow(row);
    if (sourceInstrument) *sourceInstrument = instr;
    if (instr < 0) return 0;
    int count = LGPTChopperGetSavedChopCountForInstrument(viewData_, instr);
    if (count < 0) count = 0;
    if (count > 32) count = 32;
    return count;
}

bool PhraseView::updateChopNoteValueForRow(int row, ViewUpdateDirection direction) {
    int sourceInstrument = -1;
    int count = getSavedChopCountForRow(row, &sourceInstrument);
    if (count <= 0 || sourceInstrument < 0) return false;

    int offset = 16 * viewData_->currentPhrase_ + row;
    unsigned char *note = phrase_->note_ + offset;
    unsigned char *instr = phrase_->instr_ + offset;

    int current = (*note == 0xFF || *note >= count) ? 0 : *note;
    int delta = 0;
    switch (direction) {
    case VUD_LEFT:  delta = -1; break;
    case VUD_RIGHT: delta = 1; break;
    case VUD_UP:    delta = 4; break;
    case VUD_DOWN:  delta = -4; break;
    }
    current += delta;
    while (current < 0) current += count;
    current %= count;

    *note = (unsigned char)current;
    *instr = (unsigned char)sourceInstrument;
    lastNote_ = *note;
    lastInstr_ = *instr;
    char status[48];
    snprintf(status, sizeof(status), "S%02d I%02X", current + 1, sourceInstrument);
    View::SetNotification(status);
    isDirty_ = true;
    return true;
}

bool PhraseView::pasteDefaultChopForRow(int row) {
    int sourceInstrument = -1;
    int count = getSavedChopCountForRow(row, &sourceInstrument);
    if (count <= 0 || sourceInstrument < 0) return false;

    int offset = 16 * viewData_->currentPhrase_ + row;
    unsigned char *note = phrase_->note_ + offset;
    unsigned char *instr = phrase_->instr_ + offset;
    int chop = (lastNote_ >= 0 && lastNote_ < count) ? lastNote_ : 0;
    *note = (unsigned char)chop;
    *instr = (unsigned char)sourceInstrument;
    lastNote_ = *note;
    lastInstr_ = *instr;
    char status[48];
    snprintf(status, sizeof(status), "S%02d I%02X", chop + 1, sourceInstrument);
    View::SetNotification(status);
    isDirty_ = true;
    return true;
}

bool PhraseView::adjustPtchParamForRow(int row, int paramCol, ViewUpdateDirection direction) {
    if (row < 0 || row > 15) return false;
    int offset = 16 * viewData_->currentPhrase_ + row;
    FourCC cmd = I_CMD_NONE;
    ushort *param = 0;
    if (paramCol == 3) {
        cmd = phrase_->cmd1_[offset];
        param = phrase_->param1_ + offset;
    } else if (paramCol == 5) {
        cmd = phrase_->cmd2_[offset];
        param = phrase_->param2_ + offset;
    }
    if (cmd != I_CMD_PTCH || !param) return false;

    int pitch = (int)((signed char)(*param & 0xFF));
    int delta = 0;
    switch (direction) {
    case VUD_LEFT:
    case VUD_DOWN:
        delta = -1;
        break;
    case VUD_RIGHT:
    case VUD_UP:
        delta = 1;
        break;
    }
    pitch += delta;
    if (pitch < -24) pitch = -24;
    if (pitch > 24) pitch = 24;

    *param = (ushort)((*param & 0xFF00) | ((unsigned char)(pitch & 0xFF)));
    cmdEdit_.SetInt(*param);
    lastParam_ = *param;
    char status[48];
    snprintf(status, sizeof(status), "PTCH %+d", pitch);
    View::SetNotification(status);
    isDirty_ = true;
    return true;
}

void PhraseView::formatPtchParam(ushort value, char *buffer, int bufferLen) const {
    if (!buffer || bufferLen <= 0) return;
    int pitch = (int)((signed char)(value & 0xFF));
    if (pitch < -24) pitch = -24;
    if (pitch > 24) pitch = 24;
    snprintf(buffer, bufferLen, "P%+03d", pitch);
}

'''
    s = s[:idx] + helpers + s[idx:]

old = '''int PhraseView::getChopSourceInstrumentForCurrentRow() {
    int offset = 16 * viewData_->currentPhrase_ + row_;
    unsigned char rowInstr = phrase_->instr_[offset];
    if (rowInstr != 0xFF) return rowInstr;
    if (viewData_->currentInstrument_ != 0xFF) return viewData_->currentInstrument_;
    if (lastInstr_ != 0xFF) return lastInstr_;
    return -1;
}
'''
new = '''int PhraseView::getChopSourceInstrumentForCurrentRow() {
    return getChopSourceInstrumentForRow(row_);
}
'''
if old in s:
    s = s.replace(old, new, 1)

s = s.replace('// U2.12: R2+LEFT/RIGHT selects saved chops from the note/instrument column.\n    // Rows keep the same instrument; the note column displays S01..S32 instead of C-3.\n    // Pitch changes belong in PTCH/ARPG/FX command columns.',
              '// U2.13: Phrase-side S-note editing for chopped instruments.\n    // Rows keep the same source instrument; note values display as S01..S32 only when that instrument has chops.\n    // Normal unchopped instruments keep normal C-3/C-4 note behavior; pitch changes belong in PTCH/ARPG/FX.')

old = '''        if (d == 0xFF) {
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
'''
new = '''        if (d == 0xFF) {
            DrawString(pos._x, pos._y, "----", props);
        } else {
            int phraseOffset = 16 * viewData_->currentPhrase_ + j;
            int rowInstr = phrase_->instr_[phraseOffset];
            int chopCount = LGPTChopperGetSavedChopCountForInstrument(viewData_, rowInstr);
            int chopNumber = 0;
            if (chopCount > 0) {
                if (LGPTChopperIsChopNoteForInstrument(viewData_, rowInstr, d, &chopNumber)) {
                    snprintf(buffer, sizeof(buffer), "S%02d", chopNumber);
                } else {
                    snprintf(buffer, sizeof(buffer), "S--");
                }
                DrawString(pos._x, pos._y, buffer, props);
            } else {
                note2char(d, buffer);
                DrawString(pos._x, pos._y, buffer, props);
            }
        }
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'snprintf(buffer, sizeof(buffer), "S--")' not in s:
    raise SystemExit('Patch failed: note display Sxx block')

old = '''        setTextProps(props, 3, j, false);
        hexshort2char(p, buffer);
        DrawString(pos._x, pos._y, buffer, props);
'''
new = '''        setTextProps(props, 3, j, false);
        if (phrase_->cmd1_[16 * viewData_->currentPhrase_ + j] == I_CMD_PTCH) {
            formatPtchParam(p, buffer, sizeof(buffer));
        } else {
            hexshort2char(p, buffer);
        }
        DrawString(pos._x, pos._y, buffer, props);
'''
if old in s:
    s = s.replace(old, new, 1)
elif 'formatPtchParam(p, buffer, sizeof(buffer));' not in s:
    raise SystemExit('Patch failed: PTCH param1 display block')

old = '''        setTextProps(props, 5, j, false);
        hexshort2char(p, buffer);
        DrawString(pos._x, pos._y, buffer, props);
'''
new = '''        setTextProps(props, 5, j, false);
        if (phrase_->cmd2_[16 * viewData_->currentPhrase_ + j] == I_CMD_PTCH) {
            formatPtchParam(p, buffer, sizeof(buffer));
        } else {
            hexshort2char(p, buffer);
        }
        DrawString(pos._x, pos._y, buffer, props);
'''
if old in s:
    s = s.replace(old, new, 1)
elif s.count('formatPtchParam(p, buffer, sizeof(buffer));') < 2:
    raise SystemExit('Patch failed: PTCH param2 display block')

p.write_text(s)

cmd_h = require('sources/Application/Instruments/CommandList.h').read_text()
cmd_cpp = require('sources/Application/Instruments/CommandList.cpp').read_text()
if 'I_CMD_PTCH' not in cmd_h or 'I_CMD_PTCH' not in cmd_cpp:
    raise SystemExit('Patch failed: PTCH command missing from CommandList')

print('U2.13 patches applied: Chopper cuts only, live A cut at playhead, Phrase Sxx editor, PTCH +/-24.')
PY

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_13_CHOPPER_PHRASE_SLICE_EDITOR_LIVE_CUT_PITCH_$STAMP.log"
echo "Starting U2.13 build..."
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
  grep -nE "error:|undefined reference|No such file|fatal error|make.*Error|Error [0-9]+" "$LOG" | sed -n '1,260p'
  tail -n 160 "$LOG"
fi
exit "$RC"
