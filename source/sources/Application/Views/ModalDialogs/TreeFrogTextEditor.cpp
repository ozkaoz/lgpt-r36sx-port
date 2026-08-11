// TREEFROG_TEXT_EDITOR_V1 (H38.6)
// Implementation shared by the project rename dialog (startup menu),
// the new-project dialog and the sample rename dialog. The input FSM is
// identical to the USB-C Record file name editor.
#include "TreeFrogTextEditor.h"
#include "Adapters/TREEFROG/GUI/TreeFrogEventManager.h"
#include "Adapters/TREEFROG/Main/TreeFrogSamplerInput.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char kTextEditorCharactersUpper[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
static const char kTextEditorCharactersLower[] =
    "abcdefghijklmnopqrstuvwxyz0123456789_-";
static const int kTextEditorCharacterCount =
    sizeof(kTextEditorCharactersUpper) - 1;

TreeFrogTextEditor::TreeFrogTextEditor(View &view,
                                       const char *title,
                                       const char *initialName,
                                       int maxStem,
                                       const char *suffix)
    : ModalView(view), maxStem_(maxStem), suffix_(suffix), cursor_(0),
      lowercase_(false), armed_(false), physicalMask_(0), neutralFrames_(0) {
    snprintf(title_, sizeof(title_), "%s", title ? title : "");
    title_[sizeof(title_) - 1] = 0;
    stem_[0] = 0;
    status_[0] = 0;
    setInitialText(initialName);
}

TreeFrogTextEditor::~TreeFrogTextEditor() {
    TreeFrogEventManager::GetInstance()->ClearQueue();
}

void TreeFrogTextEditor::setInitialText(const char *text) {
    stem_[0] = 0;
    cursor_ = 0;
    lowercase_ = false;
    const char *source = text ? text : "";
    size_t length = strlen(source);
    if (suffix_ && length > strlen(suffix_) &&
        strcasecmp(source + length - strlen(suffix_), suffix_) == 0)
        length -= strlen(suffix_);
    if (length > (size_t)maxStem_) length = (size_t)maxStem_;
    memcpy(stem_, source, length);
    stem_[length] = 0;
    if (!stem_[0]) snprintf(stem_, sizeof(stem_), "%c", 'A');
    cursor_ = (int)strlen(stem_) - 1;
    for (int i = 0; stem_[i]; ++i) {
        if (stem_[i] >= 'a' && stem_[i] <= 'z') {
            lowercase_ = true;
            break;
        }
    }
    snprintf(status_, sizeof(status_), "Release controls");
}

const char *TreeFrogTextEditor::GetFinalName() {
    snprintf(finalName_, sizeof(finalName_), "%s%s", stem_,
             suffix_ ? suffix_ : "");
    finalName_[sizeof(finalName_) - 1] = 0;
    return finalName_;
}

void TreeFrogTextEditor::DrawView() {
    SetWindow(38, 9);
    GUITextProperties props;
    props.invert_ = false;
    SetColor(CD_HILITE2);
    props.invert_ = true;
    char titleLine[39];
    memset(titleLine, ' ', 38);
    titleLine[38] = 0;
    int titleLen = (int)strlen(title_);
    int titleStart = (38 - titleLen) / 2;
    if (titleStart < 0) titleStart = 0;
    memcpy(titleLine + titleStart, title_, titleLen);
    DrawString(0, 0, titleLine, props);
    props.invert_ = false;

    char line[40];
    snprintf(line, sizeof(line), "Name: %-24.24s", stem_);
    SetColor(CD_HILITE2);
    DrawString(1, 2, line, props);

    char cursorLine[40];
    memset(cursorLine, ' ', sizeof(cursorLine));
    cursorLine[sizeof(cursorLine) - 1] = 0;
    int caret = 6 + cursor_;
    if (caret < 1) caret = 1;
    if (caret > 35) caret = 35;
    cursorLine[caret] = '^';
    DrawString(1, 3, cursorLine, props);

    SetColor(CD_NORMAL);
    DrawString(1, 5, "UP/DOWN char   X+UP/DOWN +/-5", props);
    DrawString(1, 6, "LEFT/RIGHT cursor   L1+X case", props);
    DrawString(1, 7, "A confirm  B erase  R1+LEFT cancel", props);
    DrawString(1, 8, "SELECT random name", props);
    SetColor(CD_HILITE1);
    DrawString(1, 9, status_, props);
}

void TreeFrogTextEditor::OnFocus() {
    armed_ = false;
    physicalMask_ = 0;
    neutralFrames_ = 0;
    TreeFrogEventManager::GetInstance()->ClearQueue();
}

void TreeFrogTextEditor::OnFrameUpdate(unsigned long) {
    processPhysicalInput();
}

void TreeFrogTextEditor::setStatus(const char *text) {
    snprintf(status_, sizeof(status_), "%s", text ? text : "");
    status_[sizeof(status_) - 1] = 0;
    isDirty_ = true;
}

void TreeFrogTextEditor::moveCursor(int delta) {
    int length = (int)strlen(stem_);
    if (length <= 0) {
        snprintf(stem_, sizeof(stem_), "%c", lowercase_ ? 'a' : 'A');
        length = 1;
    }
    cursor_ += delta;
    if (cursor_ < 0) cursor_ = 0;
    if (cursor_ >= length) {
        if (delta > 0 && length < maxStem_) {
            stem_[length] = lowercase_ ? 'a' : 'A';
            stem_[length + 1] = 0;
            cursor_ = length;
        } else cursor_ = length - 1;
    }
    isDirty_ = true;
}

void TreeFrogTextEditor::cycleCharacter(int delta) {
    int length = (int)strlen(stem_);
    if (length <= 0) {
        snprintf(stem_, sizeof(stem_), "%c", lowercase_ ? 'a' : 'A');
        length = 1;
        cursor_ = 0;
    }
    if (cursor_ < 0) cursor_ = 0;
    if (cursor_ >= length) cursor_ = length - 1;
    const char *characters = lowercase_ ? kTextEditorCharactersLower
                                        : kTextEditorCharactersUpper;
    int index = 0;
    for (int i = 0; i < kTextEditorCharacterCount; ++i) {
        if (characters[i] == stem_[cursor_]) {
            index = i;
            break;
        }
    }
    index += delta;
    while (index < 0) index += kTextEditorCharacterCount;
    while (index >= kTextEditorCharacterCount)
        index -= kTextEditorCharacterCount;
    stem_[cursor_] = characters[index];
    isDirty_ = true;
}

void TreeFrogTextEditor::toggleCase() {
    int length = (int)strlen(stem_);
    if (length <= 0) return;
    if (cursor_ < 0) cursor_ = 0;
    if (cursor_ >= length) cursor_ = length - 1;
    unsigned char value = (unsigned char)stem_[cursor_];
    if (value >= 'A' && value <= 'Z') {
        stem_[cursor_] = (char)tolower(value);
        lowercase_ = true;
    } else if (value >= 'a' && value <= 'z') {
        stem_[cursor_] = (char)toupper(value);
        lowercase_ = false;
    } else lowercase_ = !lowercase_;
    setStatus(lowercase_ ? "Lowercase mode" : "Uppercase mode");
}

void TreeFrogTextEditor::eraseCharacter() {
    int length = (int)strlen(stem_);
    if (length <= 1) {
        setStatus("Name cannot be empty");
        return;
    }
    if (cursor_ < 0) cursor_ = 0;
    if (cursor_ >= length) cursor_ = length - 1;
    memmove(stem_ + cursor_, stem_ + cursor_ + 1,
            (size_t)(length - cursor_));
    --length;
    if (cursor_ >= length) cursor_ = length - 1;
    isDirty_ = true;
}

// RANDOM_NAME_V1: base no-op. Dialogs that support random names override
// this (NewProjectDialog fills the stem with getRandomName()).
void TreeFrogTextEditor::OnRandomize() {
    setStatus("Random name not available here");
}

void TreeFrogTextEditor::processPhysicalInput() {
    TreeFrogSamplerInputSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    TreeFrogSamplerInput_Read(&snapshot);
    const unsigned int current = snapshot.selectedPhysical;
    if (!armed_) {
        physicalMask_ = current;
        if (current == 0) {
            if (neutralFrames_ < 8) ++neutralFrames_;
        } else neutralFrames_ = 0;
        if (neutralFrames_ >= 2) {
            armed_ = true;
            physicalMask_ = 0;
            setStatus("Editor ready");
        }
        return;
    }

    const unsigned int previous = physicalMask_;
    const unsigned int newBits = current & ~previous;
    physicalMask_ = current;
    if (!newBits) return;

    if ((current & TFSP_R1) && (newBits & TFSP_LEFT)) {
        EndModal(0);
        return;
    }
    if ((current & TFSP_L1) && (newBits & TFSP_X)) {
        toggleCase();
        return;
    }
    /* RANDOM_NAME_V1: SELECT generates a random name in dialogs that
     * support it (NewProjectDialog). Checked before the single-action
     * gate so it works even while other buttons are held. */
    if (newBits & TFSP_SELECT) {
        OnRandomize();
        return;
    }
    if (current & TFSP_X) {
        if (newBits & TFSP_UP) {
            cycleCharacter(5);
            return;
        }
        if (newBits & TFSP_DOWN) {
            cycleCharacter(-5);
            return;
        }
    }

    const unsigned int actions = newBits &
        (TFSP_UP | TFSP_DOWN | TFSP_LEFT | TFSP_RIGHT |
         TFSP_A | TFSP_B | TFSP_X | TFSP_Y);
    if (!actions) return;
    unsigned int bits = actions;
    int count = 0;
    while (bits) {
        bits &= bits - 1;
        ++count;
    }
    if (count != 1) {
        setStatus("Release and press one action");
        return;
    }

    if (actions == TFSP_LEFT) moveCursor(-1);
    else if (actions == TFSP_RIGHT) moveCursor(1);
    else if (actions == TFSP_UP) cycleCharacter(1);
    else if (actions == TFSP_DOWN) cycleCharacter(-1);
    else if (actions == TFSP_A) EndModal(1);
    else if (actions == TFSP_B) eraseCharacter();
    else if (actions == TFSP_X) setStatus("Hold X + UP/DOWN for +/-5");
    else if (actions == TFSP_Y) setStatus("Y unused; R1+LEFT cancels");
}
