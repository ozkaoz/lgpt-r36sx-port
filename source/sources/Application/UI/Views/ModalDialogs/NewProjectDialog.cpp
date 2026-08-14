// TREEFROG_TEXT_EDITOR_V1 (H38.6): the new-project name editor now reuses
// the generic TreeFrogTextEditor input logic (same as USB-C Record and the
// project rename dialog) instead of the old on-screen QWERTY keyboard.
#include "NewProjectDialog.h"
#include "Application/Utils/RandomNames.h"
#include <stdio.h>

NewProjectDialog::NewProjectDialog(View &view, Path currentPath)
    : TreeFrogTextEditor(view, "NEW PROJECT", 0, 24),
      currentPath_(currentPath),
      randomCount_(0) {}

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

// RANDOM_NAME_V1: R1+A generates a random "adjective+verb" project name
// (djdiskmachine LGPT behaviour). If the generated project directory already
// exists, it keeps regenerating until a free name is found (same loop as the
// upstream Random button, bounded so a full folder cannot hang the dialog).
void NewProjectDialog::OnRandomize() {
    char buf[40];
    int attempts = 0;
    do {
        setInitialText(getRandomName().c_str());
        if (++attempts >= 12) break;
    } while (currentPath_.Descend(GetName()).Exists());
    ++randomCount_;
    snprintf(buf, sizeof(buf), "Random name (R1+A again for another)");
    setStatus(buf);
}
