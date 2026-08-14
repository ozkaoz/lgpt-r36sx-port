#ifndef _IMPORT_SAMPLE_DIALOG_H_
#define _IMPORT_SAMPLE_DIALOG_H_

#include "Application/UI/Views/BaseClasses/ModalView.h"
#include "Foundation/T_SimpleList.h"
#include "System/FileSystem/FileSystem.h"
#include <string>

class ImportSampleDialog:public ModalView {
public:
	ImportSampleDialog(View &view) ;
	virtual ~ImportSampleDialog() ;

	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType ,unsigned int currentTick) ;
    virtual void OnFrameUpdate(unsigned long frameClock) ;
	virtual void OnFocus() ;
	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
    void ConfirmPendingBrowserDelete() ;
    void ConfirmPendingBrowserRename(const char *newName) ;

protected:
	void setCurrentFolder(Path *path) ;
	void warpToNextSample(int dir) ;
	void import(Path &element) ;
	void preview(Path &element) ;
	void endPreview() ;
    bool buildListenPreviewWav(Path &element, std::string &logicalPath, int &frames) ;
private:
	Path *getImportElement();
	bool isSampleLibRoot();
	bool enterFolderIfRequested(Path *element, unsigned short mask);
	void clampSelection();
	void setStatusMessage(const char *fmt, ...);
    void requestBrowserDelete(Path &element);
    void requestBrowserRename(Path &element);
    int findSelectionByExactName(const char *name);
    int unassignProjectSample(int sampleIndex);
    void deleteProjectSidecar(const char *name);
	T_SimpleList<Path> sampleList_ ;
	int currentSample_ ;
	int topIndex_ ;
	int toInstr_ ;
	int selected_ ;
	char statusMessage_[64] ;
    std::string pendingDeletePath_ ;
    std::string pendingDeleteName_ ;
    std::string pendingRenamePath_ ;
    std::string pendingRenameName_ ;
	static bool initStatic_ ;
	static Path sampleLib_ ;
	static Path currentPath_ ;

} ;

#endif
