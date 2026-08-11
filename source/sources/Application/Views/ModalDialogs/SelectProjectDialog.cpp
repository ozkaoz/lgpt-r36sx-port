
// TREEFROG_V42_NO_WHITE_BOX_UI
#include "SelectProjectDialog.h"
#include "NewProjectDialog.h"
#include "TreeFrogTextEditor.h"
#include "TreeFrogMenuModal.h"
#include "System/Console/Trace.h"
#include "Application/Views/ModalDialogs/MessageBox.h"
#include "Application/AppWindow.h"

#include <algorithm>
#include <ctype.h>
#include <stdio.h>

#define LIST_SIZE 20
#define LIST_WIDTH 32

static char *buttonText[3] = {"Load", "New", "Exit"};

// TREEFROG_UI_PROJECT_START:
// Layout validado: abrir directamente en root:projects.
// No depender de roms/gme; runtime dentro de /mnt/sdcard/lgpt.
static const char *kTreeFrogInitialProjectFolder = "root:projects" ;

Path SelectProjectDialog::lastFolder_(kTreeFrogInitialProjectFolder) ;
int SelectProjectDialog::lastProject_ = 0 ;

// TREEFROG_V40_ESTABLE_SELECT
// Un directorio lgpt_* solo se muestra/carga como proyecto si tiene lgptsav.dat.
// LittleGPTracker crea primero la carpeta y samples/, pero hasta que existe
// lgptsav.dat el proyecto está incompleto y en TreeFrog/R36SX provoca SIG=11
// en el primer retro_run() de la siguiente ejecución.
static bool TreeFrogV40IsLgptProjectName(const std::string &name) {
    if (name.size() < 4) return false;
    std::string firstFourChars = name.substr(0,4);
    std::transform(firstFourChars.begin(), firstFourChars.end(), firstFourChars.begin(), ::tolower);
    return firstFourChars == "lgpt";
}

static bool TreeFrogV40ProjectHasSaveFile(Path projectPath) {
    if (!projectPath.IsDirectory()) return false;
    Path savePath = projectPath.Descend("lgptsav.dat");
    return savePath.Exists();
}

static void TreeFrogV40SelectLog(const char *where, const Path &p, bool shown) {
    FILE *fp = fopen("/tmp/r36sx_lgpt_logs/reentry_debug.log", "a");
    if (fp) {
        fprintf(fp, "TREEFROG_V40_ESTABLE_SELECT: %s shown=%d path=%s\n", where ? where : "unknown", shown ? 1 : 0, p.GetPath().c_str());
        fclose(fp);
    }
}


static void NewProjectCallback(View &v,ModalView &dialog) {

	NewProjectDialog &npd=(NewProjectDialog &)dialog ;
	if (dialog.GetReturnCode()>0) {
		std::string selected=npd.GetName() ;
		SelectProjectDialog &spd=(SelectProjectDialog&)v ;
		Result result = spd.OnNewProject(selected) ;
        if (result.Failed()) {
            Trace::Error(result.GetDescription().c_str());
        }
    }
}

static void DeleteProjectCallback(View &v, ModalView &dialog) {
    SelectProjectDialog &spd = (SelectProjectDialog&) v;
	if (dialog.GetReturnCode() == MBL_YES) {
        Path projectPath = spd.GetCurrentProjectPath();
        Result result = spd.OnDeleteProject(projectPath);
		if (result.Failed()) {
			Trace::Error(result.GetDescription().c_str());
		}
	}
}

// TREEFROG_PROJECT_RENAME_V2 (H38.6): the generic text editor confirms with
// A and cancels with R1+LEFT; the project directory is renamed on disk.
static void RenameProjectCallback(View &v, ModalView &dialog) {
    if (dialog.GetReturnCode() == 1) {
        TreeFrogTextEditor &editor = (TreeFrogTextEditor &)dialog;
        SelectProjectDialog &spd = (SelectProjectDialog &)v;
        Result result = spd.OnRenameProject(editor.GetName());
        if (result.Failed()) {
            Trace::Error(result.GetDescription().c_str());
        }
    }
}

// TREEFROG_MIXER_STARTUP_MENU_V1 (H38.7): second picker that chooses the
// export flavour once "Export" is selected in the actions menu.
// Return 1 = full project (master mixdown), 2 = multitrack (stems).
static void ProjectExportMenuCallback(View &v, ModalView &dialog) {
    int code = dialog.GetReturnCode();
    if (code == 0) return;
    SelectProjectDialog &spd = (SelectProjectDialog &)v;
    spd.StartProjectExport((code == 1) ? 1 : 2);
}

// TREEFROG_MIXER_STARTUP_MENU_V1 (H38.7): R1+A opens a project actions menu.
// Return 1 = Rename (existing editor), 2 = Export (mode picker), 3 = Delete
// (confirmation). The nested modals are NOT opened here: View::ProcessButton
// calls this callback while the finished menu is still installed as the modal
// and immediately SAFE_DELETEs it afterwards, which would free a modal opened
// from inside the callback. We defer the action and launch it from the next
// frame tick (OnFrameUpdate) once the menu has been removed.
static void ProjectActionsMenuCallback(View &v, ModalView &dialog) {
    int code = dialog.GetReturnCode();
    if (code == 0) return;
    SelectProjectDialog &spd = (SelectProjectDialog &)v;
    spd.DeferProjectAction(code);
}

void SelectProjectDialog::DeferProjectAction(int code) {
    pendingAction_ = code;
}

// TREEFROG_MIXER_STARTUP_MENU_V2 (H38.7): actually launches the deferred
// action. Runs from OnFrameUpdate, i.e. after the actions menu modal has been
// deleted by the base class, so a nested modal opened here survives.
void SelectProjectDialog::launchProjectAction(int code) {
    switch (code) {
    case 1:
        {
            TreeFrogTextEditor *editor = new TreeFrogTextEditor(
                *this, "RENAME PROJECT",
                GetCurrentProjectBaseName().c_str(), 24);
            DoModal(editor, RenameProjectCallback);
        }
        break;
    case 2:
        {
            static const char *exportItems[] = {"Full project (master)",
                                                "Multitrack"};
            TreeFrogMenuModal *menu =
                new TreeFrogMenuModal(*this, "EXPORT MODE", exportItems, 2);
            DoModal(menu, ProjectExportMenuCallback);
        }
        break;
    case 3:
        {
            Path projectPath = GetCurrentProjectPath();
            std::string message =
                "Delete project '" + projectPath.GetName() + "' ?";
            MessageBox *mb =
                new MessageBox(*this, message.c_str(), MBBF_YES | MBBF_NO);
            DoModal(mb, DeleteProjectCallback);
            DrawView();
        }
        break;
    }
}

// Recursive helper to delete directory and all contents
static void RecursiveDeleteDirectory(const Path &dirPath) {
	FileSystem *fs = FileSystem::GetInstance();
	FileType type = fs->GetFileType(dirPath.GetPath().c_str());
	
	if (type == FT_DIR) {
		I_Dir *dir = fs->Open(dirPath.GetPath().c_str());
		if (dir) {
			dir->GetContent("*");
			
			// Collect all items first to avoid iterator invalidation
			T_SimpleList<Path> itemsToDelete(false);
			IteratorPtr<Path> it(dir->GetIterator());
			
			for (it->Begin(); !it->IsDone(); it->Next()) {
				Path itemCopy = it->CurrentItem();
				std::string name = itemCopy.GetName();
				
				// Skip . and .. entries
				if (name != "." && name != "..") {
					Path *ptrCopy = new Path(itemCopy);
					itemsToDelete.Insert(ptrCopy);
				}
			}
			
			delete dir;
			
			// Now delete all collected items
			IteratorPtr<Path> deleteIt(itemsToDelete.GetIterator());
			for (deleteIt->Begin(); !deleteIt->IsDone(); deleteIt->Next()) {
				const Path &item = deleteIt->CurrentItem();
				RecursiveDeleteDirectory(item);
			}
		}
	}

    fs->Delete(dirPath.GetPath().c_str());
}

// TREEFROG_PROJECT_RENAME_V2 (H38.6): recursive copy used to rename a
// project directory on disk (no native Rename exists in FileSystem).
static bool RecursiveCopyDirectory(const Path &srcPath, Path &dstPath) {
	FileSystem *fs = FileSystem::GetInstance();
	FileType type = fs->GetFileType(srcPath.GetPath().c_str());

	if (type != FT_DIR) return false;

	if (fs->MakeDir(dstPath.GetPath().c_str()).Failed()) {
		Trace::Log("SelectProjectDialog:Rename", "copy mkdir failed %s",
		           dstPath.GetPath().c_str());
		return false;
	}

	FileSystemService FSS;
	I_Dir *dir = fs->Open(srcPath.GetPath().c_str());
	if (!dir) return false;

	T_SimpleList<Path> items(false);
	dir->GetContent("*");
	IteratorPtr<Path> it(dir->GetIterator());
	for (it->Begin(); !it->IsDone(); it->Next()) {
		Path itemCopy = it->CurrentItem();
		std::string name = itemCopy.GetName();
		if (name != "." && name != "..") {
			Path *ptrCopy = new Path(itemCopy);
			items.Insert(ptrCopy);
		}
	}
	delete dir;

	bool ok = true;
	IteratorPtr<Path> copyIt(items.GetIterator());
	for (copyIt->Begin(); !copyIt->IsDone(); copyIt->Next()) {
		Path &item = copyIt->CurrentItem();
		Path dstItem = dstPath.Descend(item.GetName());
		if (item.IsDirectory()) {
			if (!RecursiveCopyDirectory(item, dstItem)) ok = false;
		} else {
			if (FSS.Copy(item.GetPath(), dstItem.GetPath()) < 0) {
				Trace::Log("SelectProjectDialog:Rename", "copy failed %s -> %s",
				           item.GetPath().c_str(), dstItem.GetPath().c_str());
				ok = false;
			}
		}
	}
	return ok;
}

SelectProjectDialog::SelectProjectDialog(View &view)
    : ModalView(view), content_(true), pendingAction_(0) {}

SelectProjectDialog::~SelectProjectDialog() {
}

void SelectProjectDialog::DrawView() {

	SetWindow(LIST_WIDTH,LIST_SIZE+3) ;

	GUITextProperties props ;

	SetColor(CD_NORMAL) ;
    View::EnableNotification();

    // Draw projects

    int x = 1;
    int y = 1;

    if (currentProject_ < topIndex_) {
        topIndex_ = currentProject_;
    };
    if (currentProject_>=topIndex_+LIST_SIZE) {
		topIndex_=currentProject_-LIST_SIZE+1 ;
	} ;

	IteratorPtr<Path> it(content_.GetIterator()) ;
	int count=0 ;
	char buffer[256] ;
	for(it->Begin();!it->IsDone();it->Next()) {
		if ((count>=topIndex_)&&(count<topIndex_+LIST_SIZE)) {
			Path &current=it->CurrentItem() ;
			std::string p=current.GetName() ;

			std::string firstFourChars = p.substr(0,4);
			std::transform(firstFourChars.begin(), firstFourChars.end(), firstFourChars.begin(), ::tolower);
			if(firstFourChars == "lgpt" && p.size()>4)
      {
        int namestart = 4;
        // skip _ if needed
        if ((!isalnum(p[4])) && (p.size()>4))
        {
          namestart++;
        }
				std::string t=p ;
				p=" " ;
        p+=t.substr(namestart) ;
      }
      else
      {
				std::string t=p ;
				p="[" ;
				p+=t ;
				p+="]" ;
      };

			if (count==currentProject_) {
				SetColor(CD_HILITE2) ;
				props.invert_ = false;
			} else {
				SetColor(CD_NORMAL) ;
				props.invert_=false ;
			}
			strcpy(buffer,p.c_str()) ;
			buffer[LIST_WIDTH-1]=0 ;
			DrawString(x,y,buffer,props) ;
			y+=1 ;
		}
		count++ ;
	} ;

	y=LIST_SIZE+2 ;
    int offset=LIST_WIDTH/4 ;

    // TREEFROG_V51_START_MENU_TEXT_FOCUS
    // No usar props.invert_ en los botones inferiores del menú inicial.
    // Se evita el recuadro/barra de selección y se conserva una marca visual
    // clara: texto seleccionado en CD_HILITE2, texto normal en CD_NORMAL.
    for (int i=0;i<3;i++) {
        const char *text=buttonText[i] ;
        x=offset*(i+1)-strlen(text)/2 ;
        if (i==selected_) {
            SetColor(CD_HILITE2) ;
        } else {
            SetColor(CD_NORMAL) ;
        }
        props.invert_=false ;
        DrawString(x,y,text,props) ;
    }
};

void SelectProjectDialog::OnPlayerUpdate(PlayerEventType,
                                         unsigned int currentTick) {};

// TREEFROG_PROJECT_RENAME_V2 (H38.6): SelectProjectDialog is itself a modal
// hosted by the app window. The rename editor is a second-level modal, so
// forward the retro-frame tick to the nested modal (same pattern as
// ImportSampleDialog); without this the editor's input FSM never runs.
// TREEFROG_MIXER_STARTUP_MENU_V2 (H38.7): also launches any deferred project
// action first, before forwarding the tick, so the nested modal opened here
// gets its input from the following frames.
void SelectProjectDialog::OnFrameUpdate(unsigned long frameClock) {
    if (pendingAction_) {
        int code = pendingAction_;
        pendingAction_ = 0;
        launchProjectAction(code);
    }
    if (HasModal()) UpdateActiveModalFrame(frameClock);
}

void SelectProjectDialog::OnFocus() {

	setCurrentFolder(lastFolder_) ;
    currentProject_ = lastProject_;
};

void SelectProjectDialog::ProcessButtonMask(unsigned short mask,bool pressed) {
	if (!pressed) return ;

    // TREEFROG_MIXER_STARTUP_MENU_V1 (H38.7): SELECT on the startup menu
    // opens a project actions menu (Rename / Export / Delete). Must be
    // checked before the plain A branch below (Load/New/Exit).
    if (mask & EPBM_SELECT) {
        if (currentProject_ >= 0 && currentProject_ < content_.Size()) {
            static const char *actionItems[] = {"Rename", "Export", "Delete"};
            TreeFrogMenuModal *menu =
                new TreeFrogMenuModal(*this, "PROJECT ACTIONS",
                                      actionItems, 3);
            DoModal(menu, ProjectActionsMenuCallback);
        }
        return;
    }

    if (mask&EPBM_B) {
        // Handle A + B combination for delete
        if (mask & EPBM_A) {
            int count = 0;
            Path *current = 0;

            IteratorPtr<Path> it(content_.GetIterator());
            for (it->Begin(); !it->IsDone(); it->Next()) {
                if (count == currentProject_) {
                    current = &it->CurrentItem();
                    break;
                }
                count++;
            }

            if (current != 0) {
                std::string message =
                    "Delete project '" + current->GetName() + "' ?";
                MessageBox *mb =
                    new MessageBox(*this, message.c_str(), MBBF_YES | MBBF_NO);
                DoModal(mb, DeleteProjectCallback);
                DrawView();
            }
            return;
        }
        if (mask & EPBM_UP)
            warpToNextProject(-LIST_SIZE);
        if (mask&EPBM_DOWN) warpToNextProject(LIST_SIZE) ;
    } else {

        // A modifier
        if (mask & EPBM_A) {
            switch (selected_) {
			case 0: // load
				{
                // locate folder user had selected when they hit a
                int count = 0;
                Path *current = 0;

                IteratorPtr<Path> it(content_.GetIterator());
                for (it->Begin(); !it->IsDone(); it->Next()) {
                    if (count == currentProject_) {
                        current = &it->CurrentItem();
                        break;
                    }
                    count++;
                }

					//check if folder is a project, indicated by 'lgpt' being the first 4 characters of the folder name
					std::string name = current->GetName() ;
					std::string firstFourChars = name.substr(0,4);
					std::transform(firstFourChars.begin(), firstFourChars.end(), firstFourChars.begin(), ::tolower);
					if(firstFourChars == "lgpt" && TreeFrogV40ProjectHasSaveFile(*current)){
						//ugly hack to make the "name" include subdirectories
						//we pass along everything past the root dir
						selection_ = *current ;
						lastFolder_=currentPath_ ;
						lastProject_=currentProject_ ;
						//load the project
						EndModal(1) ;
					} else {
						if (current->GetName() == "..") {
							Path parent=currentPath_.GetParent() ;
							setCurrentFolder(parent) ;
						} else {
							Path newdir=*current ;
							setCurrentFolder(newdir) ;
						}
					}
				break ;
			}
			case 1: // new
			{
                NewProjectDialog *npd =
                    new NewProjectDialog(*this, currentPath_);
                DoModal(npd,NewProjectCallback) ;
				break ;
            }
            case 2: // Exit ;
                EndModal(0) ;
				break ;
		}
        } else {

            // R Modifier

            if (mask & EPBM_R) {
            } else {
                // No modifier
				if (mask==EPBM_UP) warpToNextProject(-1) ;
				if (mask==EPBM_DOWN) warpToNextProject(1) ;
				if (mask==EPBM_LEFT) {
					selected_-- ;
					if (selected_<0) selected_+=3 ;
					isDirty_=true ;
				}
				if (mask==EPBM_RIGHT) {
					selected_=(selected_+1)%3 ;
					isDirty_=true ;
				}
            }
        }
    }
};

void SelectProjectDialog::warpToNextProject(int amount) {

    int offset = currentProject_ - topIndex_;
    int size = content_.Size();
    currentProject_+=amount ;
	if (currentProject_<0) currentProject_+=size ;
	if (currentProject_>=size) currentProject_-=size ;

	if ((amount>1)||(amount<-1)) {
		topIndex_=currentProject_-offset ;
		if (topIndex_<0) {
			topIndex_=0 ;
		} ;
	}
    isDirty_ = true;
}

Path SelectProjectDialog::GetSelection() {
	return selection_ ;
}

Result SelectProjectDialog::OnNewProject(std::string &name) {

    Path path = currentPath_.Descend(name);
    if (path.Exists()) {
        Trace::Log("SelectProjectDialog:OnNewProj","path already exists %s", path.GetPath().c_str());
		std::string res("Name " + name + " busy");
		View::SetNotification(res.c_str(), 0);
        return Result(res);
    }
    Trace::Log("TMP","creating project at %s",path.GetPath().c_str());
	selection_ = path ;
	Result result = FileSystem::GetInstance()->MakeDir(path.GetPath().c_str()) ;
	RETURN_IF_FAILED(result, ("Failed to create project dir for '%s", path.GetPath().c_str()));

	path = path.Descend("samples");
	Trace::Log("TMP","creating samples dir at %s",path.GetPath().c_str());
	result = FileSystem::GetInstance()->MakeDir(path.GetPath().c_str()) ;
	RETURN_IF_FAILED(result, ("Failed to create samples dir for '%s'", path.GetPath().c_str()));

	EndModal(1) ;
  return Result::NoError;
} ;

Result SelectProjectDialog::OnDeleteProject(const Path &projectPath) {

    Trace::Log("SelectProjectDialog:OnDelProj","deleting project at %s", projectPath.GetPath().c_str());
	
	// Make a non-const copy to check existence
	Path pathCopy = projectPath;
	
	// Check if project exists before deletion
    if (!pathCopy.Exists()) {
        std::string errMsg = "Project not found";
        View::SetNotification(errMsg.c_str(), 0);
        return Result("Project not found");
    }

    // Recursively delete the project directory and all contents
	RecursiveDeleteDirectory(projectPath);
	
	// Project deleted successfully, refresh the project list
	std::string successMsg = "Project deleted: " + pathCopy.GetName();
	View::SetNotification(successMsg.c_str(), 0);
	
	// Refresh current folder to update the list while preserving position
	int savedProject = currentProject_;
	int savedTopIndex = topIndex_;
	Path currentPathCopy = currentPath_;
	setCurrentFolder(currentPathCopy);
	
	// Restore position (adjust if necessary if we're at the end of list)
	int listSize = content_.Size();
	if (savedProject >= listSize && listSize > 0) {
		currentProject_ = listSize - 1;
	} else if (listSize > 0) {
		currentProject_ = savedProject;
	}
    topIndex_ = savedTopIndex;
    isDirty_ = true;
		
	return Result::NoError;
};

//copy-paste-mutilate'd from ImportSampleDialog
void SelectProjectDialog::setCurrentFolder(Path &path) {

	//get ready
	selected_=0 ;
	currentPath_=path ;
	content_.Empty() ;
	
	// Let's read all the directory in the root

	I_Dir *dir=FileSystem::GetInstance()->Open(currentPath_.GetPath().c_str()) ;

  if (dir) 
  {

		// Get all lgpt something

		dir->GetContent("*");
    dir->Sort();
      
		IteratorPtr<Path> it(dir->GetIterator()) ;
		for(it->Begin();!it->IsDone();it->Next())
    {
			Path &path=it->CurrentItem() ;

			if (path.IsDirectory()) {
                std::string name=path.GetName();
                bool treefrog_v40_show = true;
                if (TreeFrogV40IsLgptProjectName(name)) {
                    treefrog_v40_show = TreeFrogV40ProjectHasSaveFile(path);
                    TreeFrogV40SelectLog("setCurrentFolder.lgpt-entry", path, treefrog_v40_show);
                }
                if ((name[0] != '.' || name[1]== '.') && treefrog_v40_show) {
                    Path *p=new Path(path);
                    content_.Insert(p);
                }
            }
		}
		delete (dir) ;
  }

	//reset & redraw screen
	topIndex_=0 ;
	currentProject_=0 ;
    isDirty_ = true;
}

Path SelectProjectDialog::GetCurrentProjectPath() {
	int count = 0;
	IteratorPtr<Path> it(content_.GetIterator());
	for (it->Begin(); !it->IsDone(); it->Next()) {
		if (count == currentProject_) {
			return it->CurrentItem();
		}
		count++;
	}
	return Path();
}

// TREEFROG_PROJECT_RENAME_V2 (H38.6): visible project base name, i.e. the
// directory name without the "lgpt_" prefix, used to pre-fill the editor.
std::string SelectProjectDialog::GetCurrentProjectBaseName() {
    Path current = GetCurrentProjectPath();
    std::string name = current.GetName();
    std::string firstFourChars = name.substr(0, 4);
    std::transform(firstFourChars.begin(), firstFourChars.end(),
                   firstFourChars.begin(), ::tolower);
    if (firstFourChars == "lgpt" && name.size() > 4) {
        int namestart = 4;
        if ((!isalnum(name[4])) && (name.size() > 4)) {
            namestart++;
        }
        return name.substr(namestart);
    }
    return name;
}

// TREEFROG_MIXER_STARTUP_MENU_V1 (H38.7): starts a render export of the
// selected project. No project is loaded yet at the startup menu, so we load
// it through the normal EndModal(1) path (ProjectSelectCallback) and pre-arm
// the render request. AppWindow::H35PollExternalExport picks the request up
// once the project is loaded and drives the render state machine to
// completion (mixdown.wav for master, channelN.wav for multitrack).
void SelectProjectDialog::StartProjectExport(int mode) {
    Path projectPath = GetCurrentProjectPath();
    if (!projectPath.IsDirectory()) {
        View::SetNotification("No project selected", 0);
        return;
    }
    selection_ = projectPath;
    lastFolder_ = currentPath_;
    lastProject_ = currentProject_;
    AppWindow::GetInstance()->RequestExportRender(mode);
    EndModal(1);
}

// TREEFROG_PROJECT_RENAME_V2 (H38.6): renames the selected project
// directory on disk. The startup menu has no loaded project, so this cannot
// corrupt a running session the way the old in-project save-as rename did.
Result SelectProjectDialog::OnRenameProject(const char *newBaseName) {

    if (!newBaseName || !newBaseName[0]) {
        View::SetNotification("Name cannot be empty", 0);
        return Result("Name cannot be empty");
    }

    Path oldPath = GetCurrentProjectPath();
    std::string oldName = oldPath.GetName();
    std::string newFullName = "lgpt_";
    newFullName += newBaseName;

    if (oldName == newFullName) {
        View::SetNotification("Name unchanged", 0);
        isDirty_ = true;
        return Result::NoError;
    }

    Path newPath = currentPath_.Descend(newFullName);
    if (newPath.Exists()) {
        View::SetNotification("Name busy", 0);
        isDirty_ = true;
        return Result("Name busy");
    }

    Trace::Log("SelectProjectDialog:Rename", "renaming %s to %s",
               oldPath.GetPath().c_str(), newPath.GetPath().c_str());

    if (!RecursiveCopyDirectory(oldPath, newPath)) {
        View::SetNotification("Rename failed", 0);
        RecursiveDeleteDirectory(newPath);
        return Result("Rename failed");
    }
    RecursiveDeleteDirectory(oldPath);

    // Refresh the list and keep the cursor on the renamed project.
    std::string successMsg = "Project renamed: " + std::string(newBaseName);
    View::SetNotification(successMsg.c_str(), 0);
    setCurrentFolder(currentPath_);
    int listSize = content_.Size();
    for (int i = 0; i < listSize; ++i) {
        IteratorPtr<Path> it(content_.GetIterator());
        it->Begin();
        for (int j = 0; j <= i; ++j) {
            Path &p = it->CurrentItem();
            if (p.GetName() == newFullName) {
                currentProject_ = i;
                break;
            }
            it->Next();
        }
        if (currentProject_ == i) break;
    }
    isDirty_ = true;
    return Result::NoError;
}

// TREEFROG_MIXER_STARTUP_MENU_V1 (H38.7): build marker verified on install.
extern "C" const char *TreeFrogH387StartupMenuBuildMarker(void) {
    return "H38_7_STARTUP_MENU_RENAME_EXPORT_DELETE_MIXER_VU_DYNAMICS_PHRASE_AUTOVOL_OPT_PERF_PITCH_COLUMN";
}
