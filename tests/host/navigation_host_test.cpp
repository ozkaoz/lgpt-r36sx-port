/*
 * navigation_host_test.cpp -- F2: verificacion host del NavigationController
 * (stack de modales: Open/Replace/Push/CloseActive/RestoreSuspended y el
 * ciclo de vida NavFocus/NavSuspend/NavRestore/NavIsFinished).
 *
 * Compila sin dependencias de la app:
 *   g++ -std=gnu++03 -Wall -Wextra -Werror \
 *     Application/UI/Navigation/NavigationController.cpp \
 *     navigation_host_test.cpp
 *
 * Cada assertion transcribe el comportamiento del codigo dorado
 * (View.cpp DoModal/ReplaceModal/PushModal/RestoreSuspendedModal y la rama
 * IsFinished de View::ProcessButton en Bacon 1.2.1). Si falla, ALGUIEN
 * CAMBIO el protocolo o el test no lo transcribe.
 */
#include <stdio.h>
#include <string.h>
#include "Application/UI/Navigation/NavigationController.h"

using namespace UI::Navigation;

static int g_failures = 0;
static int g_nextId = 1;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        ++g_failures; \
    } \
} while (0)

static void CHECK_ORDER(const char *expected, const char *got) {
    if (strcmp(expected, got) != 0) {
        printf("FAIL %s:%d: orden esperada '%s' got '%s'\n", __FILE__,
               __LINE__, expected, got);
        ++g_failures;
    }
}

/* Fake modal: registra el orden de las llamadas de ciclo de vida. */
class FakeModal : public NavModal {
  public:
    FakeModal(bool finished = false) : id_(g_nextId++), finished_(finished) {
        trace_[0] = 0;
    }
    void NavOnFocus() { trace("F"); }
    void NavOnSuspend() { trace("S"); }
    void NavOnRestore() { trace("R"); }
    bool NavIsFinished() { return finished_; }

    int id_;
    bool finished_;
    char trace_[32];
    void trace(const char *c) {
        strncat(trace_, c, sizeof(trace_) - 1 - strlen(trace_));
    }
};

/* Golden View::ProcessButton sin modal en curso: exclusividad de prensas. */
static void test_process_button_exclusivity() {
    NavigationController c;
    FakeModal *a = new FakeModal();
    c.Open(a);
    CHECK(c.HasModal());
    CHECK(c.Active() == a);
    CHECK(c.Suspended() == 0);
    /* El input solo llega al activo: nada que comprobar aqui sino la
     * ausencia de suspendido; el test real de prensas es el F1 de input. */
    CHECK_ORDER("F", a->trace_);
}

static void test_replace() {
    NavigationController c;
    FakeModal *a = new FakeModal();
    c.Open(a);
    /* Solo un activo: Replace lo borra (golden SAFE_DELETE) sin tocar el
     * suspendido (que no existe). */
    FakeModal *b = new FakeModal();
    c.Replace(b);
    CHECK(c.Active() == b);
    CHECK(c.Suspended() == 0);
    CHECK_ORDER("F", b->trace_);
}

static void test_push_suspend_restore() {
    NavigationController c;
    FakeModal *a = new FakeModal();
    FakeModal *b = new FakeModal();
    FakeModal *d = new FakeModal();
    /* DoModal golden. */
    c.Open(a);
    CHECK_ORDER("F", a->trace_);
    /* PushModal golden: suspende a y apila b sobre el. */
    CHECK(c.Push(b));
    CHECK(c.HasModal());
    CHECK(c.Active() == b);
    CHECK(c.Suspended() == a);
    CHECK_ORDER("FS", a->trace_);
    CHECK_ORDER("F", b->trace_);
    /* Un segundo push machaca el suspendido b sin borrarlo (golden
     * suspendedModal_ = modalView_ sin SAFE_DELETE del anterior). a queda
     * perdido para siempre: el stack tiene UN solo nivel de suspension. */
    CHECK(c.Push(d));
    CHECK(c.Active() == d);
    CHECK(c.Suspended() == b);
    CHECK_ORDER("FS", b->trace_);
    /* Rama IsFinished de ProcessButton golden: SAFE_DELETE del activo
     * (CloseActive) y luego RestoreSuspendedModal (RestoreSuspended). */
    c.CloseActive();
    CHECK(!c.HasModal());
    CHECK(c.Suspended() == b);
    c.RestoreSuspended();
    CHECK(c.HasModal());
    CHECK(c.Active() == b);
    CHECK(c.Suspended() == 0);
    CHECK_ORDER("FSRF", b->trace_);
}

static void test_restore_suspended() {
    NavigationController c;
    FakeModal *a = new FakeModal();
    FakeModal *b = new FakeModal();
    c.Open(a);
    c.Push(b);
    /* Pop del push: SAFE_DELETE de b + RestoreSuspended golden
     * (OnRestore + OnFocus de a). */
    c.CloseActive();
    c.RestoreSuspended();
    CHECK(c.HasModal());
    CHECK(c.Active() == a);
    CHECK(c.Suspended() == 0);
    CHECK_ORDER("FSRF", a->trace_);
}

static void test_close_active_last_alone() {
    NavigationController c;
    FakeModal *a = new FakeModal();
    c.Open(a);
    c.CloseActive();
    CHECK(!c.HasModal());
    CHECK(c.Suspended() == 0);
}

static void test_close_active_finishes() {
    NavigationController c;
    FakeModal *a = new FakeModal(true /* finished */);
    FakeModal *b = new FakeModal();
    c.Open(a);
    CHECK(c.ActiveIsFinished());
    c.CloseActive();
    CHECK(!c.HasModal());
    c.Open(b);
    CHECK(!c.ActiveIsFinished());
}

int main() {
    test_process_button_exclusivity();
    test_replace();
    test_push_suspend_restore();
    test_restore_suspended();
    test_close_active_last_alone();
    test_close_active_finishes();

    if (g_failures != 0) {
        printf("NavigationController host test: %d FAILURES\n", g_failures);
        return 1;
    }
    printf("NavigationController host test: OK\n");
    return 0;
}