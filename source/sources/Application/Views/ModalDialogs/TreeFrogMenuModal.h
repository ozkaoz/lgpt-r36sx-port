#ifndef _TREEFROG_MENU_MODAL_H_
#define _TREEFROG_MENU_MODAL_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include <string>
#include <vector>

// TREEFROG_MIXER_STARTUP_MENU_V1 (H38.7):
// Generic vertical-list menu modal used by the startup project actions menu
// (Rename/Export/Delete) and the export mode picker (master/multitrack).
// UP/DOWN wrap through the items, A confirms (return code = index+1),
// B cancels (return code = 0).
class TreeFrogMenuModal : public ModalView {
public:
	TreeFrogMenuModal(View &view, const char *title,
	                  const char *const *items, int itemCount);
	virtual ~TreeFrogMenuModal();

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
