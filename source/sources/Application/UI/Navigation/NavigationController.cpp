/*
 * NavigationController.cpp -- F2 (REFACTOR_ROADMAP_ES.md). Transiciones
 * transcritas 1:1 del golden Bacon 1.2.1 (View.cpp DoModal/ReplaceModal/
 * PushModal/RestoreSuspendedModal y la rama IsFinished de ProcessButton).
 * Compila en host (sin deps de la app) y en el build MIPS del core.
 */
#include "NavigationController.h"

namespace UI {
namespace Navigation {

NavigationController::NavigationController() : active_(0), suspended_(0) {}

void NavigationController::Open(NavModal *modal) {
    active_ = modal;
    if (active_) active_->NavOnFocus();
}

void NavigationController::Replace(NavModal *modal) {
    if (active_) {
        delete active_;
        active_ = 0;
    }
    /* Golden View::ReplaceModal: no toca el suspendido. */
    Open(modal);
}

bool NavigationController::Push(NavModal *modal) {
    const bool hadModal = (active_ != 0);
    if (hadModal) {
        /* Golden View::PushModal: suspendedModal_ = modalView_ (machaca el
         * suspendido anterior sin borrarlo). */
        suspended_ = active_;
        if (suspended_) suspended_->NavOnSuspend();
    }
    active_ = modal;
    if (active_) active_->NavOnFocus();
    return hadModal;
}

void NavigationController::CloseActive() {
    if (!active_) return;
    /* Golden View::ProcessButton: SAFE_DELETE(modalView_) primero; la
     * restauracion del suspendido la decide el llamador (View: callback
     * tipado + RestoreSuspendedModal, como el golden que invoca el callback
     * antes de borrar). */
    delete active_;
    active_ = 0;
}

void NavigationController::RestoreSuspended() {
    if (!suspended_) return;
    active_ = suspended_;
    suspended_ = 0;
    /* Golden View::RestoreSuspendedModal: OnRestore + OnFocus. */
    if (active_) {
        active_->NavOnRestore();
        active_->NavOnFocus();
    }
}

}  /* namespace Navigation */
}  /* namespace UI */