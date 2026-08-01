#ifndef _NEW_PROJECT_DIALOG_H_
#define _NEW_PROJECT_DIALOG_H_

#include "Application/Utils/KeyboardLayout.h"
#include "Application/Views/BaseClasses/ModalView.h"
#include <string>

#define MAX_NAME_LENGTH 12
#define BUTTONS_LENGTH 3

class NewProjectDialog:public ModalView {
public:
  NewProjectDialog(View &view, Path currentPath = "root:");
  virtual ~NewProjectDialog();

  virtual void DrawView();
  virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
  virtual void OnFocus();
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);

  std::string GetName();
  // TREEFROG_PROJECT_RENAME_V1 (H38.5): pre-fills the name editor so the
  // dialog can act as a project renamer instead of a blank name picker.
  void SetInitialName(const std::string &name);

private:
  Path currentPath_;
  int selected_;
  int lastChar_;
  char name_[MAX_NAME_LENGTH + 1];
  int currentChar_;
  bool keyboardMode_;
  int keyboardRow_;
  int keyboardCol_ ;
  std::string initialName_;
  void moveCursor(int direction);
};
#endif
