#ifndef _COMMAND_SELECTOR_MODAL_H_
#define _COMMAND_SELECTOR_MODAL_H_

#include "Application/UI/Views/BaseClasses/ModalView.h"
#include "Application/UI/Views/BaseClasses/View.h"
#include "Application/Instruments/CommandList.h"
#include "Application/UI/Views/CommandSelectorCommon.h"

// TREEFROG_SELECTOR_FAMILIES_RC2 (RC2):
// The FX command selector stays named "FX" and is organized by functional
// families on two pages:
//   FX 1/2  (INST | FILTER | DELAY | REVERB | MASTER)
//   FX 2/2  (LEGACY COMB: CFM / CFT)
// Each page-1 column is one family; empty cells are never selectable
// (navigateGrid skips I_CMD_NONE).  The CommandList::_specs_ order is not
// changed: only this selector groups by family.
class CommandSelectorModal : public ModalView {
  public:
    CommandSelectorModal(View &parentView, FourCC *liveTarget,
                        ModalViewCallback previewCb = 0);
    virtual ~CommandSelectorModal();

    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
    virtual void DrawView();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int tick = 0);
    virtual void OnFocus();

  private:
    enum { PAGE_FAMILIES = 0, PAGE_LEGACY = 1, PAGE_COUNT = 2 };

    void navigateGrid(int deltaRow, int deltaCol);
    void moveToCommand(FourCC command);
    int contentRows(int page) const;
    int popupRows() const;
    FourCC cellAtGridPos(int page, int row, int col) const;
    void drawContentRows(int firstY, GUITextProperties &props);

    int page_;
    int selectedRow_;
    int selectedCol_;
    FourCC selectedCommand_;
    View &parentView_;
    FourCC *liveTarget_;
    FourCC savedCmd_;
    ModalViewCallback previewCb_;

    static const int GRID_COLUMNS = CommandSelectorCommon::kColumns;
};

#endif
