#ifndef _TREEFROG_PROJECT_ACTION_MODAL_H_
#define _TREEFROG_PROJECT_ACTION_MODAL_H_

#include "Application/UI/Views/BaseClasses/ModalView.h"
#include <string>
#include <vector>

// TREEFROG_STARTUP_PROJECT_ACTIONS_V1: startup-specific project menu
// triggered by plain SELECT on a valid project. Dedicated modal to avoid
// restyling the generic TreeFrogMenuModal (which is used by R1+A Export).
// Requires deferred launch via SelectProjectDialog::OnFrameUpdate.
class TreeFrogProjectActionModal : public ModalView {
public:
    TreeFrogProjectActionModal(View &view);
    virtual ~TreeFrogProjectActionModal();

    virtual void DrawView();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
    virtual void OnFocus();
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
private:
    std::string title_;
    std::vector<std::string> items_;
    int selected_;
};

#endif
