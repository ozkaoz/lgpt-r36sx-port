
#ifndef _VIEW_H_
#define _VIEW_H_

#include "Application/Model/Config.h"
#include "Application/Model/Project.h"
#include "Application/Player/Player.h"
#include "Foundation/T_SimpleList.h"
#include "I_Action.h"
#include "UIFramework/Interfaces/I_GUIGraphics.h"
#include "UIFramework/SimpleBaseClasses/GUIWindow.h"
#include "ViewEvent.h"
#ifdef SDL2
#include <SDL2/SDL.h>
#else
#include <SDL/SDL.h>
#endif

enum GUIEventPadButtonMasks {
    EPBM_LEFT = 1,
    EPBM_DOWN = 2,
    EPBM_RIGHT = 4,
    EPBM_UP = 8,
    EPBM_L = 16,
    EPBM_B = 32,
    EPBM_A = 64,
    // TREEFROG_INPUT_XY_EVENT_MASKS
    // Reservados para separar X/Y de A/B en pasos posteriores.
    EPBM_X = 1024,
    EPBM_Y = 2048,
    // TREEFROG_INPUT_L2_R2_RESERVED_MASKS
    // Reservados para acciones futuras. No consumidos por vistas todavía.
    EPBM_L2 = 4096,
    EPBM_R2 = 8192,
    EPBM_R = 128,
    EPBM_START = 256,
    EPBM_SELECT = 512,
    EPBM_DOUBLE_A = 1024,
    EPBM_DOUBLE_B = 2048
};

enum ViewType {
    VT_SONG,
    VT_CHAIN,
    VT_PHRASE,
    VT_PROJECT,
    VT_INSTRUMENT,
    VT_TABLE,  // Table screen under phrase
    VT_TABLE2, // Table screen under instrument
    VT_GROOVE,
    VT_MIXER
};

enum ViewMode {
    VM_NORMAL,
    VM_NEW,
    VM_CLONE,
    VM_SELECTION,
    VM_MUTEON,
    VM_SOLOON
};

enum ColorDefinition {
    CD_BACKGROUND,
    CD_NORMAL,
    CD_BORDER,
    CD_HILITE1,
    CD_HILITE2,
    CD_CONSOLE,
    CD_CURSOR,
    CD_PLAY,
    CD_RECORD,
    CD_MUTE,
    CD_SONGVIEWFE,
    CD_SONGVIEW00,
    CD_ROW,
    CD_ROW2,
    CD_MAJORBEAT,
    CD_WARNING,
    CD_ERROR
};

enum ViewUpdateDirection { VUD_LEFT = 0, VUD_RIGHT, VUD_UP, VUD_DOWN };

class View;
class ModalView;
class UiDraw;

typedef void (*ModalViewCallback)(View &v, ModalView &d);

class View : public Observable {
  public:
    View(GUIWindow &w, ViewData *viewData);
    View(View &v);

    void SetFocus(ViewType vt) {
        viewType_ = vt;
        hasFocus_ = true;
        OnFocus();
    };

    void LooseFocus() { hasFocus_ = false; };

    void Clear();

    void ProcessButton(unsigned short mask, bool pressed, long eventWhen = 0);
    void UpdateActiveModal(PlayerEventType type, unsigned int currentTick);
    void UpdateActiveModalFrame(unsigned long frameClock);

    /*
     * Frame updates are independent of Player transport. Modal tools such as
     * the USB sampler can refresh meters and poll their private input source
     * while the project is stopped.
     */
    virtual void OnFrameUpdate(unsigned long frameClock) {
        (void)frameClock;
    }

    void Redraw();

    // TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1): global button combos dispatched
    // by View::ProcessButton before the view handles the mask (only when no
    // modal is open, so the chopper/help keep their own L1+X/R1+X handling).
    // L1+X = undo, R1+X = redo, A+B = reset the focused option to default.
    // Default no-op; views override with their own edit history.
    virtual void GlobalUndo() {}
    virtual void GlobalRedo() {}
    virtual void GlobalResetOption() {}

    // TREEFROG_MIXER_HALF_CELL_BARS_V1 (Bacon 1.1.1): called by
    // AppWindow::Flush after the char screen is rendered, so pixel-level
    // layers (the L/R half-cell mixer bars) can draw on top every frame.
    // Default no-op.
    virtual void PostFlushDraw() {}

    // Override in subclasses

    virtual void DrawView() = 0;
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick) = 0;
    virtual void OnFocus() = 0;

    void SetDirty(bool dirty);

    // Primitive locking mechanism

    bool Lock();
    void WaitForObject();
    void Unlock();

    // Char based draw routines

    virtual void SetColor(ColorDefinition cd);
    virtual void ClearRect(int x, int y, int w, int h);
    virtual void DrawString(int x, int y, const char *txt,
                            GUITextProperties &props);

    void DoModal(ModalView *view, ModalViewCallback cb = 0);
    void ReplaceModal(ModalView *view, ModalViewCallback cb = 0);
    // RC4 P1 (PLAN_RC4 section 11.3): opens a modal over an already-open
    // one.  The active modal (if any) is suspended and restored when the
    // pushed modal finishes, so Help can be shown over dialogs without
    // destroying them.
    bool PushModal(ModalView *view, ModalViewCallback cb = 0);
    bool HasModal() const;
    ViewType GetViewType() const { return viewType_; }

    void EnableNotification();
    void SetNotification(const char *notification, int offset = 2);

  protected:
    virtual void ProcessButtonMask(unsigned short mask, bool pressed) = 0;
    long GetInputEventWhen() const { return inputEventWhen_; }

    // to remove once everything got to viewdata

    inline void updateData(unsigned char *c, int offset, unsigned char limit,
                           bool wrap) {
        int v = *c;
        if (v == 0xFF) { // Uninitiaized data
            v = 0;
        }
        v += offset;
        if (v < 0)
            v = (wrap ? (limit + 1 + v) : 0);
        if (v > limit)
            v = (wrap ? v - (limit + 1) : limit);
        *c = v;
    }

    GUIPoint GetAnchor();
    GUIPoint GetTitlePosition();

    void drawMap();
    void drawNotes();

    friend class UiDraw;

  public: // temp hack for modl windo constructors
    GUIWindow &w_;
    ViewData *viewData_;

  protected:
    ViewMode viewMode_;
    bool isDirty_; // .Do we need to redraw screeen
    ViewType viewType_;
    bool hasFocus_;
    long inputEventWhen_;

  private:
    unsigned short mask_;
    bool locked_;
    uint32_t notificationTime_;
    uint16_t NOTIFICATION_TIMEOUT;
    std::string displayNotification_;
    int notiDistY_;
    static bool initPrivate_;
    ModalView *modalView_;
    ModalViewCallback modalViewCallback_;
    // RC4 P1 (PLAN_RC4 section 11.3): modal suspended while another modal is
    // pushed on top (used to show Help over an active dialog).
    ModalView *suspendedModal_;
    ModalViewCallback suspendedModalCallback_;
    // Restores the suspended modal after the pushed one finishes.
    void RestoreSuspendedModal();

  public:
    static int margin_;
    static int songRowCount_;
    static bool miniLayout_;
    static int altRowNumber_;
};

#endif
