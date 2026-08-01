// TREEFROG_V42_NO_WHITE_BOX_UI
#include "ImportSampleDialog.h"
#include "Adapters/TREEFROG/GUI/TreeFrogEventManager.h"
#include "Adapters/TREEFROG/Main/TreeFrogSamplerInput.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/WavFile.h"
#include "Services/Time/TimeService.h"
#include "Application/Views/ModalDialogs/SampleManagerDialog.h"
// TREEFROG_U2_34_SAMPLE_MANAGER_PURGE
// TREEFROG_U2_35_SAMPLE_MANAGER_IMPORT_FORCE_DELETE
// TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
#include "System/Console/Trace.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#define LIST_SIZE 15
#define LIST_WIDTH 34

static const char *kListenPreviewTemporaryPath =
    "/tmp/r36sx_lgpt_record/__u2_listen_preview.wav";

extern void LGPTChopperOnSamplePoolDelete(int deletedIndex);

class ImportBrowserDeleteConfirmModal : public ModalView {
public:
    ImportBrowserDeleteConfirmModal(View &view, const char *name)
        : ModalView(view) {
        snprintf(name_, sizeof(name_), "%s", name ? name : "sample");
        name_[sizeof(name_) - 1] = 0;
    }

    virtual ~ImportBrowserDeleteConfirmModal() {}

    virtual void DrawView() {
        SetWindow(36, 8);
        GUITextProperties props;
        props.invert_ = false;

        SetColor(CD_RECORD);
        props.invert_ = true;
        DrawString(0, 0, "     DELETE SELECTED WAV FROM SD     ", props);
        props.invert_ = false;

        SetColor(CD_NORMAL);
        char line[42];
        snprintf(line, sizeof(line), "Sample: %-27.27s", name_);
        DrawString(1, 2, line, props);
        DrawString(1, 3, "Also removes identical project copy", props);
        DrawString(1, 4, "and clears its instrument assignments.", props);

        SetColor(CD_RECORD);
        DrawString(1, 6, "A CONFIRM DELETE", props);
        SetColor(CD_HILITE2);
        DrawString(22, 6, "B CANCEL", props);
    }

    virtual void OnPlayerUpdate(PlayerEventType, unsigned int) {}
    virtual void OnFocus() {}

    virtual void ProcessButtonMask(unsigned short mask, bool pressed) {
        if (!pressed) return;
        if (mask == EPBM_A) { EndModal(1); return; }
        if (mask == EPBM_B) { EndModal(0); return; }
    }

private:
    char name_[96];
};

static void ImportBrowserDeleteCallback(View &view, ModalView &dialog) {
    if (dialog.GetReturnCode() == 1)
        ((ImportSampleDialog &)view).ConfirmPendingBrowserDelete();
}


static const char kBrowserRenameCharactersUpper[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
static const char kBrowserRenameCharactersLower[] =
    "abcdefghijklmnopqrstuvwxyz0123456789_-";
static const int kBrowserRenameCharacterCount =
    sizeof(kBrowserRenameCharactersUpper) - 1;
static const int kBrowserRenameMaxStem = 24;

class ImportBrowserRenameModal : public ModalView {
public:
    ImportBrowserRenameModal(View &view, const char *name)
        : ModalView(view), cursor_(0), lowercase_(false), armed_(false),
          physicalMask_(0), neutralFrames_(0) {
        stem_[0] = 0;
        status_[0] = 0;
        const char *source = name ? name : "SAMPLE.wav";
        size_t length = strlen(source);
        if (length > 4 && strcasecmp(source + length - 4, ".wav") == 0)
            length -= 4;
        if (length > (size_t)kBrowserRenameMaxStem)
            length = (size_t)kBrowserRenameMaxStem;
        memcpy(stem_, source, length);
        stem_[length] = 0;
        if (!stem_[0]) snprintf(stem_, sizeof(stem_), "SAMPLE");
        cursor_ = (int)strlen(stem_) - 1;
        for (int i=0; stem_[i]; ++i) {
            if (stem_[i] >= 'a' && stem_[i] <= 'z') {
                lowercase_ = true;
                break;
            }
        }
        snprintf(status_, sizeof(status_), "Release controls");
    }

    virtual ~ImportBrowserRenameModal() {
        TreeFrogEventManager::GetInstance()->ClearQueue();
    }

    const char *GetFinalName() {
        snprintf(finalName_, sizeof(finalName_), "%s.wav", stem_);
        finalName_[sizeof(finalName_) - 1] = 0;
        return finalName_;
    }

    virtual void DrawView() {
        SetWindow(38, 9);
        GUITextProperties props;
        props.invert_ = false;
        SetColor(CD_HILITE2);
        props.invert_ = true;
        DrawString(0, 0, "          RENAME SAMPLE          ", props);
        props.invert_ = false;

        char line[40];
        snprintf(line, sizeof(line), "Name: %-24.24s", stem_);
        SetColor(CD_HILITE2);
        DrawString(1, 2, line, props);

        char cursorLine[40];
        memset(cursorLine, ' ', sizeof(cursorLine));
        cursorLine[sizeof(cursorLine)-1] = 0;
        // U2.52.3: DrawString starts at x=1 and "Name: " is 6 columns.
        // Therefore the first sample character and its caret both begin at x=7.
        int caret = 6 + cursor_;
        if (caret < 1) caret = 1;
        if (caret > 35) caret = 35;
        cursorLine[caret] = '^';
        DrawString(1, 3, cursorLine, props);

        SetColor(CD_NORMAL);
        DrawString(1, 5, "UP/DOWN char   X+UP/DOWN +/-5", props);
        DrawString(1, 6, "LEFT/RIGHT cursor   L1+X case", props);
        DrawString(1, 7, "A confirm  B erase  R1+LEFT cancel", props);
        SetColor(CD_HILITE1);
        DrawString(1, 8, status_, props);
    }

    virtual void OnFocus() {
        armed_ = false;
        physicalMask_ = 0;
        neutralFrames_ = 0;
        TreeFrogEventManager::GetInstance()->ClearQueue();
    }

    virtual void OnPlayerUpdate(PlayerEventType, unsigned int) {}

    virtual void OnFrameUpdate(unsigned long) {
        processPhysicalInput();
    }

    virtual void ProcessButtonMask(unsigned short, bool) {
        /* Physical-edge input is authoritative while this editor is open. */
    }

private:
    void setStatus(const char *text) {
        snprintf(status_, sizeof(status_), "%s", text ? text : "");
        status_[sizeof(status_) - 1] = 0;
        isDirty_ = true;
    }

    void moveCursor(int delta) {
        int length = (int)strlen(stem_);
        if (length <= 0) {
            snprintf(stem_, sizeof(stem_), "%c", lowercase_ ? 'a' : 'A');
            length = 1;
        }
        cursor_ += delta;
        if (cursor_ < 0) cursor_ = 0;
        if (cursor_ >= length) {
            if (delta > 0 && length < kBrowserRenameMaxStem) {
                stem_[length] = lowercase_ ? 'a' : 'A';
                stem_[length + 1] = 0;
                cursor_ = length;
            } else cursor_ = length - 1;
        }
        isDirty_ = true;
    }

    void cycleCharacter(int delta) {
        int length = (int)strlen(stem_);
        if (length <= 0) {
            snprintf(stem_, sizeof(stem_), "%c", lowercase_ ? 'a' : 'A');
            length = 1;
            cursor_ = 0;
        }
        if (cursor_ < 0) cursor_ = 0;
        if (cursor_ >= length) cursor_ = length - 1;
        const char *characters = lowercase_ ?
            kBrowserRenameCharactersLower : kBrowserRenameCharactersUpper;
        int index = 0;
        for (int i=0; i<kBrowserRenameCharacterCount; ++i) {
            if (characters[i] == stem_[cursor_]) { index = i; break; }
        }
        index += delta;
        while (index < 0) index += kBrowserRenameCharacterCount;
        while (index >= kBrowserRenameCharacterCount)
            index -= kBrowserRenameCharacterCount;
        stem_[cursor_] = characters[index];
        isDirty_ = true;
    }

    void toggleCase() {
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

    void eraseCharacter() {
        int length = (int)strlen(stem_);
        if (length <= 1) { setStatus("Name cannot be empty"); return; }
        if (cursor_ < 0) cursor_ = 0;
        if (cursor_ >= length) cursor_ = length - 1;
        memmove(stem_ + cursor_, stem_ + cursor_ + 1,
                (size_t)(length - cursor_));
        --length;
        if (cursor_ >= length) cursor_ = length - 1;
        isDirty_ = true;
    }

    void processPhysicalInput() {
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
                setStatus("Rename ready");
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
        if (current & TFSP_X) {
            if (newBits & TFSP_UP) { cycleCharacter(5); return; }
            if (newBits & TFSP_DOWN) { cycleCharacter(-5); return; }
        }

        const unsigned int actions = newBits &
            (TFSP_UP | TFSP_DOWN | TFSP_LEFT | TFSP_RIGHT |
             TFSP_A | TFSP_B | TFSP_X | TFSP_Y);
        if (!actions) return;
        unsigned int bits = actions;
        int count = 0;
        while (bits) { bits &= bits - 1; ++count; }
        if (count != 1) { setStatus("Release and press one action"); return; }

        if (actions == TFSP_LEFT) moveCursor(-1);
        else if (actions == TFSP_RIGHT) moveCursor(1);
        else if (actions == TFSP_UP) cycleCharacter(1);
        else if (actions == TFSP_DOWN) cycleCharacter(-1);
        else if (actions == TFSP_A) EndModal(1);
        else if (actions == TFSP_B) eraseCharacter();
        else if (actions == TFSP_X) setStatus("Hold X + UP/DOWN for +/-5");
        else if (actions == TFSP_Y) setStatus("Y unused; R1+LEFT cancels");
    }

    char stem_[32];
    char finalName_[40];
    char status_[40];
    int cursor_;
    bool lowercase_;
    bool armed_;
    unsigned int physicalMask_;
    int neutralFrames_;
};

static void ImportBrowserRenameCallback(View &view, ModalView &dialog) {
    if (dialog.GetReturnCode() == 1) {
        ImportBrowserRenameModal &renameDialog =
            (ImportBrowserRenameModal &)dialog;
        ((ImportSampleDialog &)view).ConfirmPendingBrowserRename(
            renameDialog.GetFinalName());
    }
}

bool ImportSampleDialog::initStatic_=false ;
Path ImportSampleDialog::sampleLib_("") ;
Path ImportSampleDialog::currentPath_("") ;

// U2521_BROWSER_RENAME_DEFERRED_DELETE_CRASH_SAFE inherited behavior
extern "C" const char *TreeFrogU2522BrowserManageBuildMarker(void) {
    return "U2522_NESTED_RENAME_FRAME_FORWARDING_CRASH_SAFE";
}

extern "C" const char *TreeFrogU2523BrowserManageBuildMarker(void) {
    return "U2523_RENAME_CARET_ALIGNMENT_GITHUB_FINAL";
}

static const char *buttonText[4]= {
	"Listen",
	"Import",
    "Manage",
	"Exit"	
} ;

static void copySampleDialogDisplayName(char *dst,int dstSize,Path &path) {
	if (!dst || dstSize<=0) return ;
	std::string name=path.GetName() ;
	if (path.IsDirectory()) {
		snprintf(dst,dstSize,"[%s]",name.c_str()) ;
	} else {
		snprintf(dst,dstSize,"%s",name.c_str()) ;
	}
	dst[dstSize-1]=0 ;
}

ImportSampleDialog::ImportSampleDialog(View &view):ModalView(view),sampleList_(true) {
	currentSample_=0 ;
	topIndex_=0 ;
	toInstr_=0 ;
	selected_=0 ;
	statusMessage_[0]=0 ;
	if (!initStatic_) {
		const char *slpath=SamplePool::GetInstance()->GetSampleLib() ;
		sampleLib_=Path(slpath?slpath:"") ;
		currentPath_=Path(slpath?slpath:"") ;
		initStatic_=true ;
	}
} ;

ImportSampleDialog::~ImportSampleDialog() {
	endPreview() ;
} ;

void ImportSampleDialog::setStatusMessage(const char *fmt, ...) {
	if (!fmt) {
		statusMessage_[0]=0 ;
		return ;
	}
	va_list args ;
	va_start(args,fmt) ;
	vsnprintf(statusMessage_,sizeof(statusMessage_),fmt,args) ;
	va_end(args) ;
	statusMessage_[sizeof(statusMessage_)-1]=0 ;
	isDirty_=true ;
}

void ImportSampleDialog::clampSelection() {
	int size=sampleList_.Size() ;
	if (size<=0) {
		currentSample_=0 ;
		topIndex_=0 ;
		return ;
	}
	if (currentSample_<0) currentSample_=0 ;
	if (currentSample_>=size) currentSample_=size-1 ;
	if (topIndex_<0) topIndex_=0 ;
	if (currentSample_<topIndex_) topIndex_=currentSample_ ;
	if (currentSample_>=topIndex_+LIST_SIZE) topIndex_=currentSample_-LIST_SIZE+1 ;
	if (topIndex_<0) topIndex_=0 ;
}

void ImportSampleDialog::DrawView() {

	SetWindow(LIST_WIDTH,LIST_SIZE+4) ;

	GUITextProperties props ;
	SetColor(CD_NORMAL) ;

	int x=1 ;
	int y=1 ;
	clampSelection() ;

	if (sampleList_.Size()<=0) {
		SetColor(CD_NORMAL) ;
		props.invert_=false ;
		DrawString(x,y,"No .wav files found",props) ;
		DrawString(x,y+1,"Check /lgpt/samples",props) ;
	} else {
		IteratorPtr<Path> it(sampleList_.GetIterator()) ;
		int count=0 ;
		char buffer[256] ;
		for(it->Begin();!it->IsDone();it->Next()) {
			if ((count>=topIndex_)&&(count<topIndex_+LIST_SIZE)) {
				Path &current=it->CurrentItem() ;
				if (count==currentSample_) {
					SetColor(CD_HILITE2) ;
					props.invert_ = false;
				} else {
					SetColor(CD_NORMAL) ;
					props.invert_=false ;
				}
				copySampleDialogDisplayName(buffer,sizeof(buffer),current) ;
				buffer[LIST_WIDTH-1]=0 ;
				DrawString(x,y,buffer,props) ;
				y+=1 ;
			}
			count++ ;
		} ;
	}

	if (statusMessage_[0]) {
		char status[LIST_WIDTH+1] ;
		snprintf(status,sizeof(status),"%s",statusMessage_) ;
		status[LIST_WIDTH]=0 ;
		SetColor(CD_HILITE1) ;
		props.invert_=false ;
		DrawString(1,LIST_SIZE+1,status,props) ;
	}

	// TREEFROG_U2_35_SAMPLE_MANAGER_IMPORT_FORCE_DELETE
	// TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
	// Single-line action layout with explicit spacing for R36S readability.
	y=LIST_SIZE+2 ;
	static const int actionX[4] = { 1, 10, 20, 29 };
	for (int i=0;i<4;i++) {
		const char *text=buttonText[i] ;
		x=actionX[i] ;
		SetColor((i==selected_)?CD_HILITE2:CD_NORMAL) ;
		props.invert_=false ;
		DrawString(x,y,text,props) ;
	}
    SetColor(CD_NORMAL);
    DrawString(1, LIST_SIZE+3, "L1+X rename   L1+Y delete", props);
} ;

void ImportSampleDialog::warpToNextSample(int direction) {
	int size=sampleList_.Size() ;
	if (size<=0) {
		currentSample_=0 ;
		topIndex_=0 ;
		endPreview();
		isDirty_=true ;
		return ;
	}
	currentSample_+=direction ;
	clampSelection() ;
	endPreview();
	isDirty_=true ;
}

void ImportSampleDialog::OnPlayerUpdate(PlayerEventType ,unsigned int currentTick) {
	(void)currentTick ;
} ;

void ImportSampleDialog::OnFrameUpdate(unsigned long frameClock) {
    /* U2.52.2: ImportSampleDialog is itself a modal hosted by InstrumentView.
     * The rename editor is a second-level modal. AppWindow advances only the
     * top-level active modal, so explicitly forward the retro-frame tick to
     * the nested modal. Without this bridge the rename editor draws but its
     * physical-edge input FSM never runs, making the port appear frozen. */
    if (HasModal()) UpdateActiveModalFrame(frameClock);
} ;

void ImportSampleDialog::OnFocus() {
	Path current(currentPath_) ;
	setCurrentFolder(&current) ;
	toInstr_=viewData_->currentInstrument_ ;
} ;

bool ImportSampleDialog::buildListenPreviewWav(Path &element, std::string &logicalPath, int &frames) {
    // TREEFROG_U2_51_8_LISTEN_PREVIEW_OUTSIDE_SAMPLE_POOL
    mkdir("/tmp/r36sx_lgpt_record", 0777);
    logicalPath = kListenPreviewTemporaryPath;
    unlink(kListenPreviewTemporaryPath);
    frames = 0;
    if (element.IsDirectory()) return false;

    WavFile *wav = WavFile::Open(element.GetPath().c_str());
    if (!wav) return false;

    int size = wav->GetSize(-1);
    int channels = wav->GetChannelCount(-1);
    int rate = wav->GetSampleRate(-1);
    if (size <= 0 || channels <= 0 || rate <= 0) {
        delete wav;
        return false;
    }

    if (!wav->GetBuffer(0, size)) {
        delete wav;
        return false;
    }

    short *buffer = (short *)wav->GetSampleBuffer(-1);
    if (!buffer) {
        delete wav;
        return false;
    }

    bool ok = wav->ReplaceBuffer(buffer, size, channels, rate);
    if (ok) ok = wav->SaveBufferToPath(logicalPath.c_str());
    if (ok) frames = size;
    delete wav;
    return ok;
}

void ImportSampleDialog::preview(Path &element) {
    // TREEFROG_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE
	if (!element.IsDirectory()) {
        std::string previewPath;
        int previewFrames = 0;
        Player::GetInstance()->StopStreaming();
        TimeService::GetInstance()->Sleep(80);

        /* U2.33: R36S confirmed that the Listen action reached this function
           and displayed a status message, but StartStreaming() on the generated
           preview WAV remained silent.  Pitch/Env and the temporary U2.31
           Instrument preview use StartStreamingRangeAt() successfully, so Listen
           now uses that same playback path.  No success message is drawn: A on
           Listen should simply sound. */
        if (buildListenPreviewWav(element, previewPath, previewFrames)) {
            Path path(previewPath.c_str());
            Player::GetInstance()->StartStreamingRangeAt(path, 0, previewFrames > 0 ? previewFrames - 1 : 0);
            statusMessage_[0] = 0;
            isDirty_ = true;
        } else {
            WavFile *wav = WavFile::Open(element.GetPath().c_str());
            int frames = wav ? wav->GetSize(-1) : 0;
            if (wav) delete wav;
            if (frames > 0) {
                Player::GetInstance()->StartStreamingRangeAt(element, 0, frames - 1);
            } else {
                Player::GetInstance()->StartStreaming(element);
            }
            statusMessage_[0] = 0;
            isDirty_ = true;
        }
	}
}

void ImportSampleDialog::endPreview() {
	Player::GetInstance()->StopStreaming() ;
    /* Preview is an internal transient artifact, never a project sample. */
    unlink(kListenPreviewTemporaryPath);
}

void ImportSampleDialog::import(Path &element) {
	if (element.IsDirectory()) return ;
	SamplePool *pool=SamplePool::GetInstance() ;
	int sampleID=pool->ImportSample(element) ;
	if (sampleID>=0) {
		InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
		int importedTo=toInstr_ ;
		I_Instrument *instr=bank->GetInstrument(importedTo) ;
		if (instr && instr->GetType()==IT_SAMPLE) {
			SampleInstrument *sinstr=(SampleInstrument *)instr ;
			sinstr->AssignSample(sampleID) ;
			char **names = pool->GetNameList();
			const char *importedName = (names && sampleID >= 0 && names[sampleID]) ? names[sampleID] : element.GetName().c_str();
			setStatusMessage("Imported %02X: %s",importedTo,importedName) ;
			toInstr_=bank->GetNext() ;
		} else {
			setStatusMessage("Import target invalid") ;
			Trace::Error("failed to assign imported sample") ;
		}
	} else {
		setStatusMessage("Import failed") ;
		Trace::Error("failed to import sample") ;
	};
	isDirty_=true ;
} ;

int ImportSampleDialog::unassignProjectSample(int sampleIndex) {
    if (!viewData_ || !viewData_->project_ || sampleIndex < 0) return 0;
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    if (!bank) return 0;

    int cleared = 0;
    for (int i = 0; i < MAX_SAMPLEINSTRUMENT_COUNT; ++i) {
        I_Instrument *instrument = bank->GetInstrument(i);
        if (!instrument || instrument->GetType() != IT_SAMPLE) continue;
        SampleInstrument *sampleInstrument = (SampleInstrument *)instrument;
        if (sampleInstrument->GetSampleIndex() == sampleIndex) {
            sampleInstrument->AssignSample(-1);
            ++cleared;
        }
    }
    return cleared;
}

void ImportSampleDialog::deleteProjectSidecar(const char *name) {
    if (!name || !name[0]) return;
    std::string logical = "samples:";
    logical += name;
    Path projectPath(logical.c_str());
    std::string sidecar = projectPath.GetPath();
    if (sidecar.empty()) return;
    sidecar += ".u2chop";
    FileSystem::GetInstance()->Delete(sidecar.c_str());
}

static bool BrowserRenamePathCaseSafe(const char *oldPath,const char *newPath) {
    if (!oldPath || !newPath || !oldPath[0] || !newPath[0]) return false;
    if (strcmp(oldPath,newPath)==0) return true;
    if (strcasecmp(oldPath,newPath)==0) {
        char temporary[1024];
        snprintf(temporary,sizeof(temporary),"%s.__u2521_case_%ld",oldPath,(long)getpid());
        unlink(temporary);
        if (rename(oldPath,temporary)!=0) return false;
        if (rename(temporary,newPath)!=0) {
            rename(temporary,oldPath);
            return false;
        }
        return true;
    }
    return rename(oldPath,newPath)==0;
}

static std::string BrowserSiblingPath(const std::string &oldPath,const char *newName) {
    size_t slash=oldPath.find_last_of('/');
    if (slash==std::string::npos) return std::string(newName ? newName : "");
    return oldPath.substr(0,slash+1)+std::string(newName ? newName : "");
}

static bool BrowserExactNameExists(const std::string &oldPath,const char *newName) {
    if (!newName || !newName[0]) return false;
    size_t slash=oldPath.find_last_of('/');
    std::string directory=(slash==std::string::npos)?std::string("."):oldPath.substr(0,slash);
    DIR *dir=opendir(directory.c_str());
    if (!dir) return false;
    bool exists=false;
    struct dirent *entry=0;
    while ((entry=readdir(dir))!=0) {
        if (strcmp(entry->d_name,newName)==0) { exists=true; break; }
    }
    closedir(dir);
    return exists;
}

int ImportSampleDialog::findSelectionByExactName(const char *name) {
    if (!name) return -1;
    IteratorPtr<Path> it(sampleList_.GetIterator());
    int index=0;
    for (it->Begin(); !it->IsDone(); it->Next(),++index) {
        if (strcmp(it->CurrentItem().GetName().c_str(),name)==0) return index;
    }
    return -1;
}

void ImportSampleDialog::requestBrowserRename(Path &element) {
    if (element.IsDirectory()) {
        setStatusMessage("L1+X renames WAV files only");
        return;
    }
    endPreview();
    pendingRenamePath_=element.GetPath();
    pendingRenameName_=element.GetName();
    if (pendingRenamePath_.empty() || pendingRenameName_.empty()) {
        setStatusMessage("Rename selection is invalid");
        pendingRenamePath_.clear();
        pendingRenameName_.clear();
        return;
    }
    DoModal(new ImportBrowserRenameModal(*this,pendingRenameName_.c_str()),
            ImportBrowserRenameCallback);
    isDirty_=true;
}

void ImportSampleDialog::ConfirmPendingBrowserRename(const char *newName) {
    if (pendingRenamePath_.empty() || pendingRenameName_.empty() ||
        !newName || !newName[0]) {
        setStatusMessage("Rename cancelled: stale selection");
        return;
    }
    if (strcmp(newName,pendingRenameName_.c_str())==0) {
        setStatusMessage("Name unchanged");
        pendingRenamePath_.clear();
        pendingRenameName_.clear();
        return;
    }
    size_t length=strlen(newName);
    if (length<=4 || strcasecmp(newName+length-4,".wav")!=0) {
        setStatusMessage("Rename requires a .wav name");
        return;
    }
    if (BrowserExactNameExists(pendingRenamePath_,newName)) {
        setStatusMessage("Exact duplicate name blocked");
        return;
    }

    Path source(pendingRenamePath_.c_str());
    Path sourceCheck(source.GetPath());
    if (!sourceCheck.Exists() || source.IsDirectory()) {
        setStatusMessage("Selected WAV no longer exists");
        pendingRenamePath_.clear();
        pendingRenameName_.clear();
        Path current(currentPath_);
        setCurrentFolder(&current);
        return;
    }

    Player *player=Player::GetInstance();
    if (player) { player->Stop(); player->StopStreaming(); }
    TimeService::GetInstance()->Sleep(60);

    SamplePool *pool=SamplePool::GetInstance();
    const int projectIndex=pool ? pool->FindIdenticalProjectSample(source) : -1;
    std::string projectPath;
    if (pool && projectIndex>=0 && projectIndex<pool->GetNameListSize()) {
        char **names=pool->GetNameList();
        if (names && names[projectIndex]) {
            std::string logical="samples:";
            logical+=names[projectIndex];
            Path p(logical.c_str());
            projectPath=p.GetPath();
        }
    }

    const std::string oldSourcePath=pendingRenamePath_;
    const std::string newSourcePath=BrowserSiblingPath(oldSourcePath,newName);
    const std::string oldSourceSidecar=oldSourcePath+".u2chop";
    const std::string newSourceSidecar=newSourcePath+".u2chop";
    struct stat sidecarInfo;
    const bool sourceSidecarExists=stat(oldSourceSidecar.c_str(),&sidecarInfo)==0;
    bool sourceRenamed=false;
    bool projectRenamed=false;

    if (!projectPath.empty() && projectPath==oldSourcePath) {
        projectRenamed=pool->RenameSample(projectIndex,newName);
        sourceRenamed=projectRenamed;
    } else {
        sourceRenamed=BrowserRenamePathCaseSafe(oldSourcePath.c_str(),newSourcePath.c_str());
        if (sourceRenamed && sourceSidecarExists) {
            if (!BrowserRenamePathCaseSafe(oldSourceSidecar.c_str(),newSourceSidecar.c_str())) {
                BrowserRenamePathCaseSafe(newSourcePath.c_str(),oldSourcePath.c_str());
                sourceRenamed=false;
            }
        }
        if (sourceRenamed && projectIndex>=0) {
            projectRenamed=pool->RenameSample(projectIndex,newName);
            if (!projectRenamed) {
                if (sourceSidecarExists)
                    BrowserRenamePathCaseSafe(newSourceSidecar.c_str(),oldSourceSidecar.c_str());
                BrowserRenamePathCaseSafe(newSourcePath.c_str(),oldSourcePath.c_str());
                sourceRenamed=false;
            }
        } else projectRenamed=(projectIndex<0);
    }

    const std::string previousName=pendingRenameName_;
    pendingRenamePath_.clear();
    pendingRenameName_.clear();
    Path current(currentPath_);
    setCurrentFolder(&current);
    int renamedIndex=findSelectionByExactName(newName);
    if (renamedIndex>=0) currentSample_=renamedIndex;
    clampSelection();
    TreeFrogEventManager::GetInstance()->ClearQueue();

    if (sourceRenamed && projectRenamed)
        setStatusMessage("Renamed %s -> %s",previousName.c_str(),newName);
    else
        setStatusMessage("Rename failed; original preserved");
    isDirty_=true;
}

void ImportSampleDialog::requestBrowserDelete(Path &element) {
    if (element.IsDirectory()) {
        setStatusMessage("L1+Y deletes WAV files only");
        return;
    }

    endPreview();
    pendingDeletePath_ = element.GetPath();
    pendingDeleteName_ = element.GetName();
    if (pendingDeletePath_.empty() || pendingDeleteName_.empty()) {
        setStatusMessage("Delete selection is invalid");
        pendingDeletePath_.clear();
        pendingDeleteName_.clear();
        return;
    }

    DoModal(new ImportBrowserDeleteConfirmModal(*this, pendingDeleteName_.c_str()),
            ImportBrowserDeleteCallback);
    isDirty_ = true;
}

void ImportSampleDialog::ConfirmPendingBrowserDelete() {
    if (pendingDeletePath_.empty() || pendingDeleteName_.empty()) {
        setStatusMessage("Delete cancelled: stale selection");
        return;
    }

    Path source(pendingDeletePath_.c_str());
    Path sourceCheck(source.GetPath());
    if (!sourceCheck.Exists() || source.IsDirectory()) {
        setStatusMessage("Selected WAV no longer exists");
        pendingDeletePath_.clear();
        pendingDeleteName_.clear();
        Path current(currentPath_);
        setCurrentFolder(&current);
        return;
    }

    Player *player = Player::GetInstance();
    if (player) {
        player->Stop();
        player->StopStreaming();
    }
    TimeService::GetInstance()->Sleep(60);
    unlink(kListenPreviewTemporaryPath);

    SamplePool *pool = SamplePool::GetInstance();
    int projectIndex = pool ? pool->FindIdenticalProjectSample(source) : -1;
    int cleared = 0;
    bool removedProjectCopy = false;

    if (pool && projectIndex >= 0 && projectIndex < pool->GetNameListSize()) {
        char **names = pool->GetNameList();
        char projectName[128];
        snprintf(projectName, sizeof(projectName), "%s",
                 names && names[projectIndex] ? names[projectIndex] : pendingDeleteName_.c_str());
        projectName[sizeof(projectName) - 1] = 0;

        cleared = unassignProjectSample(projectIndex);
        /* Allow the mixer callback to observe stopped/unassigned instruments.
         * SamplePool then retires the SoundSource instead of freeing it while
         * an unlocked TreeFrog audio callback could still reference it. */
        TimeService::GetInstance()->Sleep(60);
        deleteProjectSidecar(projectName);
        LGPTChopperOnSamplePoolDelete(projectIndex);
        pool->PurgeSample(projectIndex);
        removedProjectCopy = true;
    }

    std::string sourceSidecar = pendingDeletePath_ + ".u2chop";
    unlink(sourceSidecar.c_str());
    errno = 0;
    const int unlinkResult = unlink(pendingDeletePath_.c_str());
    const bool removedSource = (unlinkResult == 0 || errno == ENOENT);

    const std::string deletedName = pendingDeleteName_;
    pendingDeletePath_.clear();
    pendingDeleteName_.clear();

    Path current(currentPath_);
    setCurrentFolder(&current);
    clampSelection();

    if (!removedSource && !removedProjectCopy) {
        setStatusMessage("Delete failed: %s", deletedName.c_str());
    } else if (removedProjectCopy) {
        setStatusMessage("Deleted %s SD+project I%d", deletedName.c_str(), cleared);
    } else {
        setStatusMessage("Deleted %s from SD", deletedName.c_str());
    }
    TreeFrogEventManager::GetInstance()->ClearQueue();
    isDirty_ = true;
}

void ImportSampleDialog::ProcessButtonMask(unsigned short mask,bool pressed) {

	if (!pressed) return ;

    // U2.52.1: management actions live in the browser where the selected
    // WAV path is explicit. L1+X opens rename; L1+Y opens delete confirmation.
    const bool renameChord =
        (mask & EPBM_L) && (mask & EPBM_X) &&
        !(mask & (EPBM_LEFT | EPBM_RIGHT | EPBM_UP | EPBM_DOWN |
                  EPBM_A | EPBM_B | EPBM_Y | EPBM_R | EPBM_L2 |
                  EPBM_R2 | EPBM_SELECT | EPBM_START));
    if (renameChord) {
        Path *element = getImportElement();
        if (!element) setStatusMessage("No WAV selected");
        else requestBrowserRename(*element);
        return;
    }

    const bool deleteChord =
        (mask & EPBM_L) && (mask & EPBM_Y) &&
        !(mask & (EPBM_LEFT | EPBM_RIGHT | EPBM_UP | EPBM_DOWN |
                  EPBM_A | EPBM_B | EPBM_X | EPBM_R | EPBM_L2 |
                  EPBM_R2 | EPBM_SELECT | EPBM_START));
    if (deleteChord) {
        Path *element = getImportElement();
        if (!element) setStatusMessage("No WAV selected");
        else requestBrowserDelete(*element);
        return;
    }

    // TREEFROG_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE
    // In Listen/Import, A on Listen starts preview; plain B does not preview.
    // L2+B is the explicit stop combination, matching Chopper/PitchEnv.
    if ((mask & EPBM_L2) && (mask & EPBM_B)) {
        endPreview();
        setStatusMessage("Preview stopped");
        isDirty_ = true;
        return ;
    }

	if (mask&EPBM_B) {
		if (mask&EPBM_UP) warpToNextSample(-LIST_SIZE) ;
		if (mask&EPBM_DOWN) warpToNextSample(LIST_SIZE) ;
		return ;
	}

	if (mask&EPBM_A) {
		if (mask&EPBM_UP) { warpToNextSample(-1) ; return ; }
		if (mask&EPBM_DOWN) { warpToNextSample(1) ; return ; }

		Path *element = getImportElement();
		if (!element) {
			if (selected_==2) {
                SampleManagerDialog *smd = new SampleManagerDialog(*this);
                DoModal(smd);
			} else if (selected_==3) {
				endPreview();
				EndModal(0) ;
			}
			isDirty_=true ;
			return ;
		}
		if (enterFolderIfRequested(element, mask)) {
			isDirty_=true ;
			return ;
		}

		switch(selected_) {
			case 0:
				preview(*element);
				break ;
			case 1:
				endPreview();
				import(*element);
				break ;
            case 2: {
                endPreview();
                SampleManagerDialog *smd = new SampleManagerDialog(*this);
                DoModal(smd);
                break ;
            }
			case 3:
				endPreview();
				EndModal(0) ;
				break ;
		}
		return ;
	}

	if (mask&EPBM_START) {
		if (mask&EPBM_UP) { warpToNextSample(-1); return ; }
		if (mask&EPBM_DOWN) { warpToNextSample(1); return ; }
		Path *element = getImportElement();
		if (!element) {
			isDirty_=true ;
			return ;
		}
		if (enterFolderIfRequested(element, mask)) {
			isDirty_=true ;
			return ;
		}
		if (mask&EPBM_RIGHT) {
			endPreview();
			import(*element);
			return ;
		}
		if (mask&EPBM_LEFT) {
			if (!isSampleLibRoot()) {
				Path parent = currentPath_.GetParent();
				setCurrentFolder(&parent);
			}
			isDirty_=true;
			return ;
		}
		preview(*element);
		return ;
	}

	if (mask==EPBM_UP) warpToNextSample(-1);
	if (mask==EPBM_DOWN) warpToNextSample(1);
	if (mask==EPBM_LEFT) {
		selected_-=1;
		if (selected_<0) selected_+=4;
		isDirty_=true;
	}
	if (mask==EPBM_RIGHT) {
		selected_=(selected_+1)%4;
		isDirty_=true;
	}
} ;

bool ImportSampleDialog::isSampleLibRoot()
{
	return currentPath_.GetPath()==sampleLib_.GetPath();
};

Path* ImportSampleDialog::getImportElement() {
	clampSelection() ;
	if (sampleList_.Size()<=0) return 0 ;
	IteratorPtr<Path> it(sampleList_.GetIterator());
	int count = 0;
	for(it->Begin(); !it->IsDone(); it->Next()) {
		if (count++ == currentSample_) {
			return &it->CurrentItem();
		}
	}
	return 0 ;
}

bool ImportSampleDialog::enterFolderIfRequested(Path *element, unsigned short mask) {
	if (!element) return false ;
	if (mask&(EPBM_UP|EPBM_DOWN|EPBM_LEFT|EPBM_RIGHT)) return false ;
	if (selected_==2) return false ;
	if (!element->IsDirectory()) return false ;

	Path target(*element) ;
	if (target.GetName()=="..") {
		if (isSampleLibRoot()) return false ;
		target=currentPath_.GetParent() ;
	}
	setCurrentFolder(&target) ;
	isDirty_ = true;
	return true ;
}

void ImportSampleDialog::setCurrentFolder(Path *path) {

	Path formerPath(currentPath_) ;
	topIndex_=0 ;
	currentSample_=0 ;
	sampleList_.Empty() ;

	if (!path) {
		isDirty_=true ;
		return ;
	}

	currentPath_=Path(*path) ;
	statusMessage_[0]=0 ;
	I_Dir *dir=FileSystem::GetInstance()->Open(currentPath_.GetPath().c_str()) ;
	if (!dir) {
		isDirty_=true ;
		return ;
	}
	dir->GetContent("*") ;
	dir->Sort() ;

	IteratorPtr<Path>it(dir->GetIterator()) ;
	int count=0 ;
	for (it->Begin();!it->IsDone();it->Next()) {
		Path &current=it->CurrentItem() ;
		if (current.IsDirectory()) {
			std::string name=current.GetName() ;
			if (!name.empty() && (name[0]!='.' || name=="..")) {
				Path *sample=new Path(current) ;
				sampleList_.Insert(sample) ;
				if (!formerPath.Compare(current)) {
					currentSample_=count ;
				}
				count++ ;
			}
		}
	}
	for (it->Begin();!it->IsDone();it->Next()) {
		Path &current=it->CurrentItem() ;
		if (!current.IsDirectory()) {
			std::string name=current.GetName() ;
			if (!name.empty() && name[0]!='.' && current.Matches("*.wav")) {
				Path *sample=new Path(current) ;
				sampleList_.Insert(sample) ;
			}
		};
	}
	delete dir ;
	clampSelection() ;
	isDirty_=true ;
} ;
