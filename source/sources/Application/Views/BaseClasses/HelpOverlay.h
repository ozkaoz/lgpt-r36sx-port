#ifndef _HELP_OVERLAY_H_
#define _HELP_OVERLAY_H_

#include "Application/Views/BaseClasses/ModalView.h"

class View;

/*
 * TREEFROG_HELP_OVERLAY_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, point 13)
 * + RC4 P1 (PLAN_RC4 sections 11.1/11.2).
 *
 * Navigable context-help overlay.  It opens on SELECT+R1 and shows the
 * context help for the active view, then lets the user browse the whole
 * help registry:
 *
 *   UP/DN    scroll the current section's lines (window fits 8 rows)
 *   L/R      previous / next section
 *   L1/R1    previous / next section (same as L/R)
 *   L2/R2    jump to first / last section
 *   A        toggle the section index
 *   B        close (also SELECT+R1)
 *
 * It is a real modal: every event reaching it is consumed so nothing
 * propagates to the view underneath, and it always closes deterministically
 * via EndModal().  When opened over another modal (RC4 11.3), the underlying
 * modal is suspended by View::PushModal and restored on close.
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
    static const int kMaxWindowLines_ = 8;

    // The section browser starts on the section of the view that opened it.
    void StartAt(ViewType vt);
    void ClampCursor();

    ViewType helpViewType_;
    int sectionIndex_;  // section in the registry currently shown
    int lineScroll_;    // first line of the section visible in the window
    bool showIndex_;    // A toggles the section index list
};

void HelpOverlayApplyCallback(View &view, ModalView &dialog);

#endif // _HELP_OVERLAY_H_