#include "TreeFrogTimer.h"

#include <vector>
#include <stdio.h>
#include "System/System/System.h"

#ifndef TREEFROG_TIMER_MODE
#define TREEFROG_TIMER_MODE 1
#endif

#ifndef TREEFROG_INPUT_DEBUG
#define TREEFROG_INPUT_DEBUG 0
#endif

static TreeFrogTimerService *g_timer_service = 0;
static std::vector<TreeFrogTimer *> g_timers;

static void treefrog_timer_log(const char *msg) {
#if TREEFROG_INPUT_DEBUG
    FILE *f = fopen("/tmp/r36sx_lgpt_logs/timer_debug.log", "a");
    if (!f) return;
    fprintf(f, "%lu %s timers=%u mode=%d\n",
            (unsigned long)System::GetInstance()->GetClock(),
            msg ? msg : "timer",
            (unsigned)g_timers.size(),
            TREEFROG_TIMER_MODE);
    fclose(f);
#else
    (void)msg;
#endif
}

TreeFrogTimerService *TreeFrogGetTimerService() { return g_timer_service; }

static bool timer_is_registered(TreeFrogTimer *timer) {
    for (std::vector<TreeFrogTimer *>::iterator it = g_timers.begin(); it != g_timers.end(); ++it) {
        if (*it == timer) return true;
    }
    return false;
}

TreeFrogTimer::TreeFrogTimer() : period_(0), nextTick_(0), running_(false) {
    g_timers.push_back(this);
}

TreeFrogTimer::~TreeFrogTimer() {
    for (std::vector<TreeFrogTimer *>::iterator it = g_timers.begin(); it != g_timers.end(); ++it) {
        if (*it == this) { g_timers.erase(it); break; }
    }
}

void TreeFrogTimer::SetPeriod(float msec) { period_ = msec; }

bool TreeFrogTimer::Start() {
    running_ = true;
    nextTick_ = System::GetInstance()->GetClock() + (unsigned long)period_;
    return true;
}

void TreeFrogTimer::Stop() { running_ = false; }
float TreeFrogTimer::GetPeriod() { return period_; }

void TreeFrogTimer::Tick(unsigned long now) {
    if (!running_ || period_ <= 0) return;
    if (now >= nextTick_) {
        SetChanged();
        NotifyObservers();
        nextTick_ += (unsigned long)period_;
        if (now > nextTick_ + (unsigned long)period_) nextTick_ = now + (unsigned long)period_;
    }
}

TreeFrogTimerService::TreeFrogTimerService() {
    g_timer_service = this;
}

TreeFrogTimerService::~TreeFrogTimerService() {
    if (g_timer_service == this) g_timer_service = 0;
}

I_Timer *TreeFrogTimerService::CreateTimer() {
    g_timer_service = this;
    return new TreeFrogTimer();
}

void TreeFrogTimerService::TriggerCallback(int msec, timerCallback cb) {
    (void)msec;
    if (cb) cb();
}

void TreeFrogTimerService::Tick() {
    g_timer_service = this;

#if TREEFROG_TIMER_MODE == 0
    /* Diagnostic mode: disable LGPT timer callbacks. This intentionally removes
     * auto-repeat and some periodic UI callbacks, but it can isolate crashes that
     * happen shortly after entering PhraseView. */
    return;
#endif

    unsigned long now = System::GetInstance()->GetClock();

    /* Iterate over a snapshot: LGPT callbacks can create/destroy timers.  Before
     * using a pointer from the snapshot, verify that it is still registered. */
    std::vector<TreeFrogTimer *> snapshot = g_timers;
    for (std::vector<TreeFrogTimer *>::iterator it = snapshot.begin(); it != snapshot.end(); ++it) {
        TreeFrogTimer *timer = *it;
        if (timer && timer_is_registered(timer)) {
            treefrog_timer_log("tick.enter");
            timer->Tick(now);
            treefrog_timer_log("tick.leave");
        }
    }
}
