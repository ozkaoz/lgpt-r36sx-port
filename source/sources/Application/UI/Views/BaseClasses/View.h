
#ifndef _VIEW_H_
#define _VIEW_H_

#include "Application/Model/Config.h"
#include "Application/Model/Project.h"
#include "Application/Player/Player.h"
#include "Application/UI/Navigation/NavigationController.h"
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
    VT_MIXER,
    // TREEFROG_CHOPPER_HELP_V1 (Bacon 1.1.1 V13): view type of the graphical
    // sample chopper, so HelpOverlay can open on the CHOPPER help section.
    // Kept at the end of the enum so existing VT_* ordinal values (used by
    // HelpRegistry and focus switching) are unaffected.
    VT_CHOPPER,
    // F2 (0722cb2 validacion consola): submodo Pitch/Env del chopper; su
    // HelpOverlay debe abrir en la seccion CHOP PITCH.
    VT_CHOPPITCH
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

    // TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): global button combos offered
    // by View::ProcessButton to the view before the mask is dispatched (only
    // when no modal is open, so the chopper/help keep their own L1+X/R1+X
    // handling).  L1+X = undo, R1+X = redo, A+B = reset the focused option
    // to default.  A view claims a combo by returning true; when it returns
    // false View::ProcessButton lets the mask fall through to the view's own
    // legacy handling (Song A+B clear, Phrase/Table A+B cut, ...).
    virtual bool GlobalUndo() { return false; }
    virtual bool GlobalRedo() { return false; }
    virtual bool GlobalResetOption() { return false; }

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
    // TREEFROG_CHOPPER_HELP_V1 (Bacon 1.1.1 V13): returns the active modal,
    // so AppWindow can show Help for the modal that actually has focus (the
    // chopper) instead of the base view underneath it.
    ModalView *GetModal() const {
        UI::Navigation::NavModal *n = nav_.Active();
        return n ? static_cast<ModalView *>(n->ModalSelf()) : 0;
    }
    // Modal suspendido bajo el tope (RC4 P1: help pushado sobre un dialogo).
    ModalView *SuspendedModal() const {
        UI::Navigation::NavModal *n = nav_.Suspended();
        return n ? static_cast<ModalView *>(n->ModalSelf()) : 0;
    }
    // Alias del tope del stack (mismo ajuste que GetModal).
    ModalView *ActiveModal() const { return GetModal(); }
    // TREEFROG_CHOPPER_HELP_V1 (Bacon 1.1.1 V13): made virtual so the
    // chopper modal can report VT_CHOPPER and Help opens on its section.
    virtual ViewType GetViewType() const { return viewType_; }

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
    // F2 (REFACTOR_ROADMAP_ES.md): el stack de modales (activo + suspendido:
    // golden modalView_ / suspendedModal_) vive en el NavigationController.
    // Los callbacks tipados (ModalViewCallback) son glue de la vista y se
    // mantienen aqui, en sincronia con los push/pop del controller.
    UI::Navigation::NavigationController nav_;
    ModalViewCallback modalViewCallback_;
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
