#ifndef _NEW_PROJECT_DIALOG_H_
#define _NEW_PROJECT_DIALOG_H_

#include "TreeFrogTextEditor.h"
#include <string>

// TREEFROG_TEXT_EDITOR_V1 (H38.6): the new-project name editor is now the
// same generic text editor used by USB-C Record and the project rename
// dialog (X+UP/DOWN fast, L1+X case, A confirms, B erases, R1+LEFT cancels).
class NewProjectDialog : public TreeFrogTextEditor {
public:
  NewProjectDialog(View &view, Path currentPath = "root:");
  virtual ~NewProjectDialog();

  // Returns "lgpt_<name>" for compatibility with existing callbacks.
  std::string GetName();
  // TREEFROG_PROJECT_RENAME_V1 (H38.5): pre-fills the name editor so the
  // dialog can act as a project renamer instead of a blank name picker.
  void SetInitialName(const std::string &name);
  // RANDOM_NAME_V1: SELECT generates a random project name (adjective+verb,
  // djdiskmachine LGPT behaviour) replacing the edited stem.
  virtual void OnRandomize();

private:
  Path currentPath_;
  int randomCount_;
};
#endif
