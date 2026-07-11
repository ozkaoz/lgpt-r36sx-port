#include "TreeFrogEventManager.h"
#include "Application/Application.h"
#include "System/System/System.h"

#include <string.h>
#include <stdio.h>
#include "Application/Player/Player.h"

#ifndef TREEFROG_INPUT_DEBUG
#define TREEFROG_INPUT_DEBUG 0
#endif

#ifndef TREEFROG_EVENT_DEBUG_ALL
#define TREEFROG_EVENT_DEBUG_ALL 0
#endif

static bool g_quit_requested = false;

void TreeFrogSetQuitRequested(bool q) { g_quit_requested = q; }
bool TreeFrogQuitRequested() { return g_quit_requested; }

struct QueuedPadEvent {
    GUIEventPadButtonType button;
    bool pressed;
    long when;
};

static QueuedPadEvent g_queue[64];
static unsigned g_qread = 0;
static unsigned g_qwrite = 0;
static unsigned g_qcount = 0;

static const char *buttonName(GUIEventPadButtonType button) {
    switch (button) {
        case EPBT_LEFT: return "LEFT";
        case EPBT_RIGHT: return "RIGHT";
        case EPBT_UP: return "UP";
        case EPBT_DOWN: return "DOWN";
        case EPBT_A: return "A";
        case EPBT_B: return "B";
        case EPBT_X: return "X";
        case EPBT_Y: return "Y";
        case EPBT_L2: return "L2";
        case EPBT_R2: return "R2";
        case EPBT_L: return "L";
        case EPBT_R: return "R";
        case EPBT_START: return "START";
        case EPBT_SELECT: return "SELECT";
    }
    return "?";
}

static void logEvent(const char *phase, GUIEventPadButtonType button, bool pressed, unsigned qcount) {
#if TREEFROG_INPUT_DEBUG
    FILE *f = fopen("/mnt/sdcard/lgpt/event_debug.log", "a");
    if (!f) return;
    fprintf(f, "%lu %s button=%s pressed=%d q=%u\n",
            (unsigned long)System::GetInstance()->GetClock(),
            phase ? phase : "event",
            buttonName(button), pressed ? 1 : 0, qcount);
    fclose(f);
#else
    (void)phase; (void)button; (void)pressed; (void)qcount;
#endif
}

TreeFrogEventManager::TreeFrogEventManager() {}
TreeFrogEventManager::~TreeFrogEventManager() { ClearQueue(); }

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
    memset(g_queue, 0, sizeof(g_queue));
}

void TreeFrogEventManager::PushPad(GUIEventPadButtonType button, bool pressed) {
    long when = System::GetInstance()->GetClock();

    if (g_qcount >= (sizeof(g_queue) / sizeof(g_queue[0]))) {
        /* Drop the oldest event rather than corrupting the queue. */
        g_qread = (g_qread + 1) % (sizeof(g_queue) / sizeof(g_queue[0]));
        --g_qcount;
        logEvent("drop-oldest", button, pressed, g_qcount);
    }

    g_queue[g_qwrite].button = button;
    g_queue[g_qwrite].pressed = pressed;
    g_queue[g_qwrite].when = when;
    g_qwrite = (g_qwrite + 1) % (sizeof(g_queue) / sizeof(g_queue[0]));
    ++g_qcount;

#if TREEFROG_EVENT_DEBUG_ALL
    logEvent("queued", button, pressed, g_qcount);
#else
    if (button == EPBT_START || button == EPBT_SELECT) {
        logEvent("queued", button, pressed, g_qcount);
    }
#endif
}

void TreeFrogEventManager::Flush() {
    GUIWindow *window = Application::GetInstance()->GetWindow();
    if (!window) {
        ClearQueue();
        return;
    }

    unsigned safety = 0;
    while (g_qcount > 0 && safety++ < 64) {
        QueuedPadEvent ev = g_queue[g_qread];
        g_qread = (g_qread + 1) % (sizeof(g_queue) / sizeof(g_queue[0]));
        --g_qcount;

#if TREEFROG_EVENT_DEBUG_ALL
        logEvent("dispatch.enter", ev.button, ev.pressed, g_qcount);
#else
        if (ev.button == EPBT_START || ev.button == EPBT_SELECT) {
            logEvent("dispatch.enter", ev.button, ev.pressed, g_qcount);
        }
#endif

        GUIEvent e((long)ev.button, ev.pressed ? ET_PADBUTTONDOWN : ET_PADBUTTONUP, ev.when);
        window->DispatchEvent(e);

#if TREEFROG_EVENT_DEBUG_ALL
        logEvent("dispatch.leave", ev.button, ev.pressed, g_qcount);
#else
        if (ev.button == EPBT_START || ev.button == EPBT_SELECT) {
            logEvent("dispatch.leave", ev.button, ev.pressed, g_qcount);
        }
#endif
    }
}
