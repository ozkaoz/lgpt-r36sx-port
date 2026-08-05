
#ifndef _MODAL_VIEW_H_
#define _MODAL_VIEW_H_

#include "View.h"

class ModalView : public View {
  public:
    ModalView(View &);
    virtual ~ModalView();

    bool IsFinished();
    int GetReturnCode();

    // TREEFROG_CHOPPER_HELP_V1 (Bacon 1.1.1 V13): a modal pushed on top of
    // this one (e.g. Help over the chopper) suspends it.  Subclasses can
    // pause/restore their private overlay state (chopper waveform pixels)
    // so it does not draw over the pushed window.
    virtual void OnSuspend() {}
    virtual void OnRestore() {}

    // TREEFROG_HELP_NAV_V14 (Bacon 1.1.1 V14): lets AppWindow detect an
    // already-open Help so SELECT+R1 never stacks a second one.
    virtual bool IsHelpOverlay() const { return false; }

  protected:
    void SetWindow(int width, int height);
    // TREEFROG_MODAL_OFFSET_V1: shifts the centered window vertically.
    void SetWindowOffset(int dy);
    virtual void ClearRect(int x, int y, int w, int h);
    virtual void DrawString(int x, int y, const char *txt,
                            GUITextProperties &props);
    void EndModal(int returnCode);

    // RC5: geometry of the window set by SetWindow(), in local coordinates.
    // Derived modals use these to center their content on their own
    // viewport (e.g. the ProjectExit menu) instead of the full screen.
    int GetWindowWidth() { return width_; }
    int GetWindowHeight() { return height_; }

  private:
    bool finished_;
    int returnCode_;
    int left_;
    int top_;
    int topOffset_;
    int width_;
    int height_;
};
#endif