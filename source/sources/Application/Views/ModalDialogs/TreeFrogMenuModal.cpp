#include "TreeFrogMenuModal.h"

// TREEFROG_MIXER_STARTUP_MENU_V1 (H38.7): see header.
TreeFrogMenuModal::TreeFrogMenuModal(View &view, const char *title,
                                     const char *const *items, int itemCount)
    : ModalView(view), title_(title ? title : "") {
	for (int i = 0; i < itemCount; i++) {
		items_.push_back(items[i] ? items[i] : "");
	}
	selected_ = 0;
};

TreeFrogMenuModal::~TreeFrogMenuModal() {
};

void TreeFrogMenuModal::DrawView() {
	int width = 6;
	int titleLen = (int)title_.size();
	if (titleLen > width) width = titleLen;
	for (size_t i = 0; i < items_.size(); i++) {
		int len = (int)items_[i].size();
		if (len > width) width = len;
	}
	int height = (int)items_.size() + 3;
	SetWindow(width + 2, height);

	GUITextProperties props;

	int y = 0;
	int x = (width - titleLen) / 2;
	SetColor(CD_HILITE2);
	props.invert_ = false;
	DrawString(x, y, title_.c_str(), props);

	for (size_t i = 0; i < items_.size(); i++) {
		if ((int)i == selected_) {
			SetColor(CD_HILITE2);
		} else {
			SetColor(CD_NORMAL);
		}
		props.invert_ = false;
		DrawString(2, 2 + (int)i, items_[i].c_str(), props);
	}
};

void TreeFrogMenuModal::OnPlayerUpdate(PlayerEventType, unsigned int) {
};

void TreeFrogMenuModal::OnFocus() {
};

void TreeFrogMenuModal::ProcessButtonMask(unsigned short mask, bool pressed) {
	if (!pressed) return;

	if (mask & EPBM_UP) {
		selected_--;
		if (selected_ < 0) selected_ = (int)items_.size() - 1;
		isDirty_ = true;
	}
	if (mask & EPBM_DOWN) {
		selected_++;
		if (selected_ >= (int)items_.size()) selected_ = 0;
		isDirty_ = true;
	}
	if (mask & EPBM_A) {
		EndModal(selected_ + 1);
	}
	if (mask & EPBM_B) {
		EndModal(0);
	}
};
