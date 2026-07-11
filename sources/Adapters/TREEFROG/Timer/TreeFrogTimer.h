#ifndef TREEFROG_TIMER_H
#define TREEFROG_TIMER_H

#include "System/Timer/Timer.h"

class TreeFrogTimer: public I_Timer {
public:
    TreeFrogTimer();
    virtual ~TreeFrogTimer();
    virtual void SetPeriod(float msec);
    virtual bool Start();
    virtual void Stop();
    virtual float GetPeriod();
    void Tick(unsigned long now);
private:
    float period_;
    unsigned long nextTick_;
    bool running_;
};

class TreeFrogTimerService: public TimerService {
public:
    TreeFrogTimerService();
    virtual ~TreeFrogTimerService();
    virtual I_Timer *CreateTimer();
    virtual void TriggerCallback(int msec, timerCallback cb);
    void Tick();
};

TreeFrogTimerService *TreeFrogGetTimerService();

#endif
