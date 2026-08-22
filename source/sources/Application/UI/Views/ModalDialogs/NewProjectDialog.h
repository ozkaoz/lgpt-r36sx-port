#ifndef _NEW_PROJECT_DIALOG_H_
#define _NEW_PROJECT_DIALOG_H_

#include "TreeFrogTextEditor.h"
#include <string>

// TREEFROG_TEXT_EDITOR_V1 (H38.6): the new-project name editor is now the
// same generic text editor used by USB-C Record and the project rename
// dialog (X+UP/DOWN fast, L1+X case, A confirms, B erases, R1+LEFT cancels).
// TREEFROG_STARTUP_PROJECT_ACTIONS_V1: startup-only Random mode.
class NewProjectDialog : public TreeFrogTextEditor {
public:
  NewProjectDialog(View &view, Path currentPath = "root:",
                   bool startupRandomMode = false);
  virtual ~NewProjectDialog();

  // Returns "lgpt_<name>" for compatibility with existing callbacks.
  std::string GetName();
  // TREEFROG_PROJECT_RENAME_V1 (H38.5): pre-fills the name editor so the
  // dialog can act as a project renamer instead of a blank name picker.
  void SetInitialName(const std::string &name);

protected:
  virtual unsigned int GetAdditionalActionMask() const;
  virtual bool HandlePhysicalAction(unsigned int action);
  virtual const char *GetActionHintLine() const;

private:
  Path currentPath_;
  bool startupRandomMode_;
};
#endif
