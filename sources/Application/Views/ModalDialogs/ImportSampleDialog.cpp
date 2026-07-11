// TREEFROG_V42_NO_WHITE_BOX_UI
#include "ImportSampleDialog.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/WavFile.h"
#include "Services/Time/TimeService.h"
#include "Application/Player/Player.h"
#include "Application/Views/ModalDialogs/SampleManagerDialog.h"
// TREEFROG_U2_34_SAMPLE_MANAGER_PURGE
// TREEFROG_U2_35_SAMPLE_MANAGER_IMPORT_FORCE_DELETE
// TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
#include "System/Console/Trace.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>

#define LIST_SIZE 15
#define LIST_WIDTH 34

bool ImportSampleDialog::initStatic_=false ;
Path ImportSampleDialog::sampleLib_("") ;
Path ImportSampleDialog::currentPath_("") ;

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

void ImportSampleDialog::OnFocus() {
    // AU11U_IMPORT_MODAL_NO_LATE_TRANSPORT_STOP
    // Transport is stopped before DoModal() in InstrumentView.  Do not call
    // Player::Stop() from OnFocus; doing so is late in the modal stack and was
    // a likely contributor to first-open crashes on hardware.
    Player *player = Player::GetInstance();
    if (player && player->IsStreaming()) { player->StopStreaming(); Trace::Log("AU11M", "IMPORT_MODAL_STOP_STREAM_ONLY"); }
    TimeService::GetInstance()->Sleep(40);
	Path current(currentPath_) ;
	setCurrentFolder(&current) ;
	toInstr_=viewData_->currentInstrument_ ;
} ;

bool ImportSampleDialog::buildListenPreviewWav(Path &element, std::string &logicalPath, int &frames) {
    // TREEFROG_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE
    logicalPath = "samples:__u2_listen_preview.wav";
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

void ImportSampleDialog::ProcessButtonMask(unsigned short mask,bool pressed) {

	if (!pressed) return ;

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
