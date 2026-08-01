#include "TreeFrogEventManager.h"

#include "Application/Application.h"
#include "System/System/System.h"

#include <stdio.h>
#include <string.h>

#ifndef TREEFROG_INPUT_DEBUG
#define TREEFROG_INPUT_DEBUG 0
#endif

#ifndef TREEFROG_EVENT_DEBUG_ALL
#define TREEFROG_EVENT_DEBUG_ALL 0
#endif

static bool g_quit_requested = false;

void TreeFrogSetQuitRequested(bool q) {
    g_quit_requested = q;
}

bool TreeFrogQuitRequested() {
    return g_quit_requested;
}

extern "C" const char *TreeFrogAtomicEventQueueBuildMarker(void) {
    return "U2510_CHORD_AWARE_GLOBAL_CHOP_STEREO_QUEUE";
}

struct QueuedMaskEvent {
    unsigned short mask;
    bool pressed;
    long when;
};

static QueuedMaskEvent g_queue[64];
static unsigned g_qread = 0;
static unsigned g_qwrite = 0;
static unsigned g_qcount = 0;
static unsigned short g_legacy_mask = 0;

static void logMaskEvent(
    const char *phase,
    unsigned short mask,
    bool pressed,
    unsigned qcount) {
#if TREEFROG_INPUT_DEBUG
    FILE *file =
        fopen("/tmp/r36sx_lgpt_logs/atomic_input_events.log", "a");
    if (!file) return;

    fprintf(
        file,
        "%lu %s mask=0x%04x pressed=%d q=%u\n",
        (unsigned long)System::GetInstance()->GetClock(),
        phase ? phase : "event",
        (unsigned int)mask,
        pressed ? 1 : 0,
        qcount);
    fclose(file);
#else
    (void)phase;
    (void)mask;
    (void)pressed;
    (void)qcount;
#endif
}

TreeFrogEventManager::TreeFrogEventManager() {
}

TreeFrogEventManager::~TreeFrogEventManager() {
    ClearQueue();
}

bool TreeFrogEventManager::Init() {
    ClearQueue();
    return EventManager::Init();
}

int TreeFrogEventManager::MainLoop() {
    return 0;
}

void TreeFrogEventManager::PostQuitMessage() {
    g_quit_requested = true;
}

int TreeFrogEventManager::GetKeyCode(const char *name) {
    if (!name) return -1;
    if (!strcmp(name, "left")) return EPBT_LEFT;
    if (!strcmp(name, "right")) return EPBT_RIGHT;
    if (!strcmp(name, "up")) return EPBT_UP;
    if (!strcmp(name, "down")) return EPBT_DOWN;
    if (!strcmp(name, "a")) return EPBT_A;
    if (!strcmp(name, "b")) return EPBT_B;
    if (!strcmp(name, "x")) return EPBT_X;
    if (!strcmp(name, "y")) return EPBT_Y;
    if (!strcmp(name, "l2")) return EPBT_L2;
    if (!strcmp(name, "r2")) return EPBT_R2;
    if (!strcmp(name, "l")) return EPBT_L;
    if (!strcmp(name, "r")) return EPBT_R;
    if (!strcmp(name, "start")) return EPBT_START;
    if (!strcmp(name, "select")) return EPBT_SELECT;
    return -1;
}

void TreeFrogEventManager::ClearQueue() {
    g_qread = 0;
    g_qwrite = 0;
    g_qcount = 0;
    g_legacy_mask = 0;
    memset(g_queue, 0, sizeof(g_queue));
}

void TreeFrogEventManager::PushMask(
    unsigned short mask,
    bool pressed,
    long when) {
    if (when == 0)
        when = System::GetInstance()->GetClock();

    /*
     * Adjacent exact duplicates carry no new state.  This is not a timing
     * filter; it only prevents redundant copies of the same atomic snapshot.
     */
    if (g_qcount > 0) {
        const unsigned lastIndex =
            (g_qwrite + (sizeof(g_queue) / sizeof(g_queue[0])) - 1) %
            (sizeof(g_queue) / sizeof(g_queue[0]));
        const QueuedMaskEvent &last = g_queue[lastIndex];
        if (last.mask == mask && last.pressed == pressed)
            return;
    }

    if (g_qcount >=
        (sizeof(g_queue) / sizeof(g_queue[0]))) {
        g_qread =
            (g_qread + 1) %
            (sizeof(g_queue) / sizeof(g_queue[0]));
        --g_qcount;
        logMaskEvent("drop-oldest", mask, pressed, g_qcount);
    }

    g_queue[g_qwrite].mask = mask;
    g_queue[g_qwrite].pressed = pressed;
    g_queue[g_qwrite].when = when;
    g_qwrite =
        (g_qwrite + 1) %
        (sizeof(g_queue) / sizeof(g_queue[0]));
    ++g_qcount;

#if TREEFROG_INPUT_DEBUG
    logMaskEvent("queued", mask, pressed, g_qcount);
#endif
}

void TreeFrogEventManager::PushPad(
    GUIEventPadButtonType button,
    bool pressed) {
    const unsigned value = (unsigned)button;
    if (value >= 16u) return;

    const unsigned short bit =
        (unsigned short)(1u << value);

    if (pressed)
        g_legacy_mask |= bit;
    else
        g_legacy_mask &= (unsigned short)~bit;

    PushMask(g_legacy_mask, pressed);
}

void TreeFrogEventManager::Flush() {
    GUIWindow *window =
        Application::GetInstance()->GetWindow();

    if (!window) {
        ClearQueue();
        return;
    }

    unsigned safety = 0;
    while (g_qcount > 0 && safety++ < 64) {
        const QueuedMaskEvent event = g_queue[g_qread];

        g_qread =
            (g_qread + 1) %
            (sizeof(g_queue) / sizeof(g_queue[0]));
        --g_qcount;

#if TREEFROG_INPUT_DEBUG
        logMaskEvent(
            "dispatch.enter",
            event.mask,
            event.pressed,
            g_qcount);
#endif

        GUIEvent guiEvent(
            (long)event.mask,
            event.pressed ? ET_PADMASKDOWN : ET_PADMASKUP,
            event.when);
        window->DispatchEvent(guiEvent);

#if TREEFROG_INPUT_DEBUG
        logMaskEvent(
            "dispatch.leave",
            event.mask,
            event.pressed,
            g_qcount);
#endif
    }
}
