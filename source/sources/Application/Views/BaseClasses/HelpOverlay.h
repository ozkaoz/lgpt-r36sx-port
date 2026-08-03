#ifndef _HELP_OVERLAY_H_
#define _HELP_OVERLAY_H_

#include "Application/Views/BaseClasses/ModalView.h"

class View;

/*
 * TREEFROG_HELP_OVERLAY_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, point 13).
 *
 * Non-interactive help overlay.  It opens latched while SELECT+R1 is held
 * and shows the context help for the active view.  It does not change page,
 * does not propagate input, and never takes focus-changing actions: it only
 * draws and exits when SELECT+R1 is released.
 *
 * Audio-driver access (SELECT+R2) is handled separately in AppWindow and is
 * unchanged.
 */
class HelpOverlay : public ModalView {
  public:
    HelpOverlay(View &view);
    virtual ~HelpOverlay();

    virtual void DrawView();
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
    virtual void OnPlayerUpdate(PlayerEventType type, unsigned int currentTick);
    virtual void OnFocus();

  private:
    ViewType helpViewType_;
};

void HelpOverlayApplyCallback(View &view, ModalView &dialog);

#endif // _HELP_OVERLAY_H_