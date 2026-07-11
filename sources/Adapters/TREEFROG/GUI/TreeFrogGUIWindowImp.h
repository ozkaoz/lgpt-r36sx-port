#ifndef TREEFROG_GUI_WINDOW_IMP_H
#define TREEFROG_GUI_WINDOW_IMP_H

#include <stdint.h>
#include "UIFramework/Interfaces/I_GUIWindowImp.h"

#define TREEFROG_LGPT_WIDTH  320
#define TREEFROG_LGPT_HEIGHT 240

struct TreeFrogCreateWindowParams: public GUICreateWindowParams {
    TreeFrogCreateWindowParams() : framebuffer_(0) {}
    uint16_t *framebuffer_;
};

class TreeFrogGUIWindowImp: public I_GUIWindowImp {
public:
    TreeFrogGUIWindowImp(GUICreateWindowParams &p);
    virtual ~TreeFrogGUIWindowImp();

    virtual void SetColor(GUIColor &c);
    virtual void DrawRect(GUIRect &r);
    virtual void DrawChar(const char c, GUIPoint &pos, GUITextProperties &props);
    virtual void DrawString(const char *string, GUIPoint &pos, GUITextProperties &props, bool overlay=false);
    virtual GUIRect GetRect();
    virtual void Invalidate();
    virtual void Flush();
    virtual void Lock();
    virtual void Unlock();
    virtual void Clear(GUIColor &c, bool overlay=false);
    virtual void ClearRect(GUIRect &r);
    virtual void PushEvent(GUIEvent &event);

    uint16_t *GetFramebuffer() { return framebuffer_; }

private:
    void DrawCharInternal(const char c, GUIPoint &pos, GUITextProperties &props, bool overlay);
    static uint16_t rgb565(const GUIColor &c);
    static bool clipRect(int &x0, int &y0, int &x1, int &y1);

    uint16_t *framebuffer_;
    uint16_t currentColor_;
    uint16_t backgroundColor_;
};

TreeFrogGUIWindowImp *TreeFrogGetWindowImp();
uint16_t *TreeFrogGetFramebuffer();

#endif
