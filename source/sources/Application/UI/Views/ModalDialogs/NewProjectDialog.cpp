// TREEFROG_TEXT_EDITOR_V1 (H38.6): the new-project name editor now reuses
// the generic TreeFrogTextEditor input logic (same as USB-C Record and the
// project rename dialog) instead of the old on-screen QWERTY keyboard.
// TREEFROG_STARTUP_PROJECT_ACTIONS_V1: startup-only Random mode uses
// getRandomName() for A and START for confirm.
#include "NewProjectDialog.h"
#include "Foundation/Variables/Variable.h"
#ifndef MAX_NAME_LENGTH
#define MAX_NAME_LENGTH Variable::MAX_NAME_LENGTH
#endif
#include "Application/Utils/RandomNames.h"
#undef MAX_NAME_LENGTH
#include "Adapters/TREEFROG/Main/TreeFrogSamplerInput.h"
#include <string.h>

NewProjectDialog::NewProjectDialog(View &view, Path currentPath,
                                   bool startupRandomMode)
    : TreeFrogTextEditor(view, "NEW PROJECT", 0, 24),
      currentPath_(currentPath), startupRandomMode_(startupRandomMode) {}

NewProjectDialog::~NewProjectDialog() {}

unsigned int NewProjectDialog::GetAdditionalActionMask() const {
    if (startupRandomMode_) return TFSP_START;
    return 0;
}

const char *NewProjectDialog::GetActionHintLine() const {
    if (startupRandomMode_) return "A random START confirm B erase";
    return TreeFrogTextEditor::GetActionHintLine();
}

bool NewProjectDialog::HandlePhysicalAction(unsigned int action) {
    if (!startupRandomMode_) return false;
    if (action == TFSP_A) {
        // Startup NEW: A generates a random name, does NOT confirm.
        std::string randomName;
        int tries = 0;
        const int kMaxTries = 100;
        do {
            randomName = getRandomName();
            setInitialText(randomName.c_str());
            tries++;
            if (tries >= kMaxTries) {
                setStatus("Random failed");
                return true;
            }
        } while (currentPath_.Descend(GetName()).Exists());
        setStatus("Random name");
        isDirty_ = true;
        return true;
    }
    if (action == TFSP_START) {
        const char *stem = TreeFrogTextEditor::GetName();
        if (!stem || !stem[0]) {
            setStatus("Name cannot be empty");
            return true;
        }
        // Trim spaces? TreeFrogTextEditor ensures at least one char, but
        // validate non-empty after potential spaces.
        bool hasNonSpace = false;
        for (const char *p = stem; *p; ++p) {
            if (*p != ' ' && *p != '\t') { hasNonSpace = true; break; }
        }
        if (!hasNonSpace) {
            setStatus("Name cannot be empty");
            return true;
        }
        std::string finalName = GetName();
        if (finalName.empty() || finalName == "lgpt_") {
            setStatus("Name cannot be empty");
            return true;
        }
        Path dest = currentPath_.Descend(finalName);
        if (dest.Exists()) {
            setStatus("Name busy");
            View::SetNotification("Name busy", 0);
            return true;
        }
        EndModal(1);
        return true;
    }
    return false;
}

// TREEFROG_TEXT_EDITOR_V1 (H38.6): keep the "lgpt_" prefix convention used
// by the startup menu and the save-as flow.
std::string NewProjectDialog::GetName() {
    std::string name = "lgpt_";
    name += TreeFrogTextEditor::GetName();
    return name;
}

// TREEFROG_PROJECT_RENAME_V1 (H38.5)
void NewProjectDialog::SetInitialName(const std::string &name) {
    setInitialText(name.c_str());
}
