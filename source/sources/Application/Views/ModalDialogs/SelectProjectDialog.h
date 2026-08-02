#ifndef _SELECT_PROJECT_DIALOG_H_
#define _SELECT_PROJECT_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Errors/Result.h"

class SelectProjectDialog:public ModalView {
public:
	SelectProjectDialog(View &view) ;
	~SelectProjectDialog() ;

	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType ,unsigned int currentTick) ;
	virtual void OnFocus() ;
	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	// TREEFROG_PROJECT_RENAME_V2 (H38.6): forwards the frame cadence to the
	// nested rename editor modal.
	virtual void OnFrameUpdate(unsigned long frameClock) ;

    Result OnNewProject(std::string &name) ;
    Result OnDeleteProject(const Path &projectPath);
    // TREEFROG_PROJECT_RENAME_V2 (H38.6): renames the selected project
    // directory in place (no project is loaded at the startup menu, so the
    // rename can never corrupt a running session).
    Result OnRenameProject(const char *newBaseName);
    // TREEFROG_MIXER_STARTUP_MENU_V1 (H38.7): loads the selected project and
    // immediately starts a render export. mode is 1 = mixdown (master),
    // 2 = stems (multitrack).
    void StartProjectExport(int mode);

    Path GetSelection();
    Path GetCurrentProjectPath();
    std::string GetCurrentProjectBaseName();

  protected:
    void warpToNextProject(int amount) ;
	void setCurrentFolder(Path &path) ;

private:
  T_SimpleList<Path> content_;
  int topIndex_;
  int currentProject_;
  int selected_;
  Path currentPath_;
  Path selection_;
  static Path lastFolder_;
  static int lastProject_;
} ;

#endif
