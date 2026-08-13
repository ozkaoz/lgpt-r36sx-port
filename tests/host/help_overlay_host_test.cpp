/*
 * help_overlay_host_test.cpp -- F2: repro host del crash "R1 en el
 * HelpOverlay" reportado en consola (SELECT+R1 abre help; R1 navega de
 * seccion -> el port se cae en todas las vistas).
 *
 * Compila los .cpp REALES (View/ModalView/HelpOverlay/HelpRegistry/UiDraw/
 * NavigationController) + stubs, y reproduce la secuencia exacta de
 * AppWindow::onEvent (ayuda: bloque ET_PADMASKDOWN de Help NAV V15) +
 * View::ProcessButton + Redraw, bajo ASAN/UBSAN. Si hay un OOB/UB ahi,
 * lo dispara.
 */
#include <stdio.h>
#include <string.h>
#include "UIFramework/Interfaces/I_GUIWindowImp.h"
#include "Application/AppWindow.h"
#include "Application/Views/BaseClasses/HelpOverlay.h"
#include "Application/Views/BaseClasses/HelpRegistry.h"
#include "Application/Views/BaseClasses/ModalView.h"
#include "Application/Views/BaseClasses/UiDraw.h"

static int g_failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        ++g_failures; \
    } \
} while (0)

/* Implementacion grafica no-op. */
class FakeImp : public I_GUIWindowImp {
  public:
    void PushEvent(GUIEvent &) {}
    void Clear(GUIColor &, bool) {}
    void SetColor(GUIColor &) {}
    void ClearRect(GUIRect &) {}
    void DrawString(const char *, GUIPoint &, GUITextProperties &, bool) {}
    void DrawChar(const char, GUIPoint &, GUITextProperties &) {}
    GUIRect GetRect() { return GUIRect(0, 0, 40, 30); }
    void Invalidate() {}
    void Lock() {}
    void Unlock() {}
    void Flush() {}
};

class FakeWindow : public AppWindow {
  public:
    FakeWindow(I_GUIWindowImp &imp) : AppWindow(imp) {}
};

/* Vista base fake (pinta nada). */
class FakeView : public View {
  public:
    FakeView(GUIWindow &w) : View(w, 0) {}
    void DrawView() {}
    void ProcessButtonMask(unsigned short, bool) {}
    void OnFocus() {}
    void OnPlayerUpdate(PlayerEventType, unsigned int) {}
};

/* Modal generico (para el caso chopper suspendido). */
class FakeModalView : public ModalView {
  public:
    FakeModalView(View &v) : ModalView(v) {}
    void DrawView() {}
    void ProcessButtonMask(unsigned short, bool) {}
    void OnFocus() {}
    void OnPlayerUpdate(PlayerEventType, unsigned int) {}
};

/*
 * Transcripcion del bloque de AppWindow::onEvent (ET_PADMASKDOWN) para el
 * help: si hay un modal HelpOverlay y la prensa trae bits L/R/L2/R2, se
 * reenvian SOLO esos bits a ProcessButton y el evento muere ahi.
 */
static void dispatchPadMaskDown(View *cv, unsigned short mask,
                                unsigned short chordOldMask,
                                bool &helpLatched, bool &audioLatched) {
    unsigned short theMask = mask;
    (void)theMask;
    /* F1b SynchronizeInputMask es identidad en host. */
    ModalView *navModal = cv ? cv->GetModal() : 0;
    if (navModal && navModal->IsHelpOverlay()) {
        const unsigned short navPress =
            mask & (EPBM_L | EPBM_R | EPBM_L2 | EPBM_R2);
        if (navPress != 0) {
            cv->ProcessButton(navPress, true, 0);
            return;
        }
    }
    /* help/audio chords (SELECT+R1 / SELECT+R2). */
    const bool helpChord = (mask & (EPBM_SELECT | EPBM_R)) ==
                           (EPBM_SELECT | EPBM_R);
    const bool audioChord = (mask & (EPBM_SELECT | EPBM_R2)) ==
                            (EPBM_SELECT | EPBM_R2);
    if (helpChord && cv) {
        if (!helpLatched) {
            helpLatched = true;
            ModalView *active = cv->GetModal();
            if (active && active->IsHelpOverlay()) {
                helpLatched = false;
                return;
            }
            View *helpTarget = cv;
            ModalView *topModal = cv->GetModal();
            if (topModal) helpTarget = topModal;
            cv->PushModal(new HelpOverlay(*helpTarget),
                          HelpOverlayApplyCallback);
        }
        return;
    }
    if (audioChord && cv && !cv->HasModal()) {
        if (!audioLatched) {
            audioLatched = true;
            cv->ReplaceModal(new FakeModalView(*cv), 0);
        }
        return;
    }
    if (helpLatched || audioLatched) {
        if (!helpChord && !audioChord) {
            helpLatched = false;
            audioLatched = false;
        } else {
            return;
        }
    }
    if (cv) cv->ProcessButton(mask, true, 0);
}

static void runScenario(const char *name, View *cv, bool withSuspended) {
    bool helpLatched = false, audioLatched = false;
    int r1Presses = 0;
    for (int i = 0; i < 40; i++) {
        if (i == 0) {
            /* SELECT+R1 abre el help (chord en un solo poll). */
            dispatchPadMaskDown(cv, EPBM_SELECT | EPBM_R, 0, helpLatched,
                                audioLatched);
            /* GetModal() debe devolver el ModalView REAL ajustado (bug F2
             * de subobjeto NavModal: cast C-style -> offset 168). */
            ModalView *opened = cv->GetModal();
            CHECK(opened != 0);
            if (opened) CHECK(opened->IsHelpOverlay());
            /* release del chord: los latches se liberan. */
            dispatchPadMaskDown(cv, 0, 0, helpLatched, audioLatched);
            CHECK(helpLatched == false);
        } else if (i == 1 && withSuspended) {
            /* (con chopper abierto, el help se empuja encima: ya cubierto
             * por PushModal del dispatch). */
        } else if (i % 2 == 0) {
            /* R1: navegar seccion. */
            dispatchPadMaskDown(cv, EPBM_R, 0, helpLatched, audioLatched);
            r1Presses++;
        }
        cv->Redraw();
    }
    /* Cierre limpio del help (B) para no dejar modales abiertos. */
    dispatchPadMaskDown(cv, EPBM_B, 0, helpLatched, audioLatched);
    dispatchPadMaskDown(cv, 0, 0, helpLatched, audioLatched);
    cv->Redraw();
    CHECK(r1Presses >= 15);
    printf("scenario %s: %d R1 presses, OK\n", name, r1Presses);
    fflush(stdout);
}

int main() {
    /* F2: alineacion GetSection/ViewType (el help de MIXER abria en
     * CHOPPER por el desajuste de ordinales). */
    CHECK(HelpRegistry::GetSection(VT_MIXER) ==
          HelpRegistry::GetSectionAt(7));
    CHECK(HelpRegistry::GetSection(VT_GROOVE) ==
          HelpRegistry::GetSectionAt(6));
    CHECK(HelpRegistry::GetSection(VT_TABLE2) ==
          HelpRegistry::GetSectionAt(4));
    CHECK(HelpRegistry::GetSection(VT_CHOPPER) ==
          HelpRegistry::GetSectionAt(8));
    CHECK(strcmp(HelpRegistry::GetSection(VT_MIXER)->title, "MIXER") == 0);

    FakeImp imp;
    FakeWindow win(imp);
    FakeView view(win);
    view.SetFocus(VT_SONG);

    /* Escenario 1: help sobre vista base (SONG). */
    runScenario("song", &view, false);

    /* Escenario 2: chopper modal abierto -> help empujado encima. */
    FakeModalView chopper(view);
    view.DoModal(&chopper);
    CHECK(view.HasModal());
    runScenario("chopper-suspended", &view, true);

    /* Escenario 3: cierre con B y reapertura (regresion de latches). */
    view.ProcessButton(EPBM_B, true, 0); /* close opcional por B */
    view.Redraw();

    if (g_failures != 0) {
        printf("HelpOverlay host test: %d FAILURES\n", g_failures);
        return 1;
    }
    printf("HelpOverlay host test: OK (sin crash en la secuencia R1)\n");
    return 0;
}