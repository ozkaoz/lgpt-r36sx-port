// TREEFROG_TEXT_EDITOR_V1 (H38.6): the new-project name editor now reuses
// the generic TreeFrogTextEditor input logic (same as USB-C Record and the
// project rename dialog) instead of the old on-screen QWERTY keyboard.
#include "NewProjectDialog.h"

NewProjectDialog::NewProjectDialog(View &view, Path currentPath)
    : TreeFrogTextEditor(view, "NEW PROJECT", 0, 24),
      currentPath_(currentPath) {}

NewProjectDialog::~NewProjectDialog() {}

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
