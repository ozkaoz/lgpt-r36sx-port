#ifndef TREEFROG_SYSTEM_H
#define TREEFROG_SYSTEM_H

#include "System/System/System.h"

class TreeFrogSystem: public System {
public:
    TreeFrogSystem();
    virtual ~TreeFrogSystem();

    static bool Boot(const char *contentPath);
    static void Shutdown();

    virtual unsigned long GetClock();
    virtual int GetBatteryLevel();
    virtual void *Malloc(unsigned size);
    virtual void Free(void *ptr);
    virtual void Memset(void *addr, char value, int size);
    virtual void *Memcpy(void *s1, const void *s2, int n);
    virtual void PostQuitMessage();
    virtual unsigned int GetMemoryUsage();

private:
    unsigned long clockBase_;
};

#endif
