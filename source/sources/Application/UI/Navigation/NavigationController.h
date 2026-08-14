/*
 * NavigationController.h -- F2 (REFACTOR_ROADMAP_ES.md): stack formal de
 * modales con las transiciones identicas al golden (DoModal, ReplaceModal,
 * PushModal con suspension, pop con restauracion) y exclusividad de
 * prensas (el input solo llega al modal activo).
 *
 * Capa pura: NO depende de la app (no incluye View.h ni SDL). Trabaja
 * sobre la interfaz NavModal (la implementa ModalView en la app; el test
 * host usa fakes). El stack tiene UN solo nivel de suspension, como el
 * golden (modalView_ + suspendedModal_ de View.cpp Bacon 1.2.1); un push
 * sobre un modal ya suspendido machaca el suspendido mas antiguo sin
 * borrarlo (igual que el golden; los guards de AppWindow evitan el caso).
 * Los callbacks de cierre son glue de la vista (ModalViewCallback tipado)
 * y NO entran aqui: View los resume tras CloseActive.
 */
#ifndef UI_NAVIGATION_NAVIGATION_CONTROLLER_H_
#define UI_NAVIGATION_NAVIGATION_CONTROLLER_H_

namespace UI {
namespace Navigation {

/* Interfaz minima de modal para el stack. */
class NavModal {
  public:
    virtual ~NavModal() {}
    /* Ciclo de vida: al abrir/push (OnFocus), al suspender bajo un push
     * (OnSuspend) y al restaurarse tras el pop (OnRestore). */
    virtual void NavOnFocus() = 0;
    virtual void NavOnSuspend() = 0;
    virtual void NavOnRestore() = 0;
    /* El modal ha terminado (EndModal en la app) y debe hacerse el pop. */
    virtual bool NavIsFinished() = 0;
    /* Devuelve la direccion del ModalView completo (el contenedor), no la
     * del subobjeto NavModal: el stack trabaja con NavModal* y un modal de
     * la app (ModalView : public View, public NavModal) puede vivir en un
     * offset de subobjeto distinto del NavModal.  El llamador la usa para
     * recuperar el puntero ajustado (equivalente a un downcast correcto)
     * sin depender de RTTI. */
    virtual void *ModalSelf() = 0;
};

class NavigationController {
  public:
    NavigationController();

    /* DoModal del golden (View::DoModal): sustituye el modal activo sin
     * borrarlo y le da foco. */
    void Open(NavModal *modal);

    /* ReplaceModal del golden (View::ReplaceModal): borra el activo
     * (SAFE_DELETE) y pone el nuevo con foco. No toca el suspendido. */
    void Replace(NavModal *modal);

    /* PushModal del golden (View::PushModal): suspende el activo si existe
     * (OnSuspend) y apila el nuevo encima (OnFocus). Devuelve true si hubo
     * suspension (el golden: hadModal). */
    bool Push(NavModal *modal);

    /* El modal activo termino (el golden: rama IsFinished de
     * View::ProcessButton): SAFE_DELETE del activo. La restauracion del
     * suspendido (si existe) la hace el llamador via RestoreSuspended()
     * despues del callback tipado, igual que el golden
     * (modalViewCallback_... SAFE_DELETE(modalView_), RestoreSuspendedModal). */
    void CloseActive();

    /* Restaura el modal suspendido bajo el activo
     * (View::RestoreSuspendedModal del golden). */
    void RestoreSuspended();

    /* El golden: exclusividad de prensas mientras hay modal. */
    bool HasModal() const { return active_ != 0; }

    /* Modal activo (tope del stack). */
    NavModal *Active() const { return active_; }

    /* Modal suspendido bajo el tope (null si no hay). */
    NavModal *Suspended() const { return suspended_; }

    /* El activo ha terminado (el golden: modalView_->IsFinished()). */
    bool ActiveIsFinished() {
        return active_ && active_->NavIsFinished();
    }

  private:
    NavModal *active_;
    NavModal *suspended_;
};

}  /* namespace Navigation */
}  /* namespace UI */

#endif  /* UI_NAVIGATION_NAVIGATION_CONTROLLER_H_ */