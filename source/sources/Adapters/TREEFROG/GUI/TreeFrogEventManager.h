#ifndef TREEFROG_EVENT_MANAGER_H
#define TREEFROG_EVENT_MANAGER_H

#include "Foundation/T_Singleton.h"
#include "UIFramework/SimpleBaseClasses/EventManager.h"
#include "UIFramework/BasicDatas/GUIEvent.h"

class TreeFrogEventManager:
    public T_Singleton<TreeFrogEventManager>,
    public EventManager {
public:
    TreeFrogEventManager();
    virtual ~TreeFrogEventManager();

    virtual bool Init();
    virtual int MainLoop();
    virtual void PostQuitMessage();
    virtual int GetKeyCode(const char *name);

    /*
     * U2.50.1 ATOMIC_LOGICAL_MASK_QUEUE
     *
     * Push the complete logical LGPT mask as one event.  A transition that
     * releases and presses buttons in the same retro frame is represented by
     * a release snapshot followed by a press snapshot; no impossible
     * intermediate chord can be constructed by queue ordering.
     */
    void PushMask(unsigned short mask, bool pressed, long when = 0);

    /*
     * Legacy compatibility for code outside the rewritten input path.
     * TreeFrogLibretro no longer uses per-button queueing.
     */
    void PushPad(GUIEventPadButtonType button, bool pressed);

    void Flush();
    void ClearQueue();
};

void TreeFrogSetQuitRequested(bool q);
bool TreeFrogQuitRequested();

extern "C" const char *TreeFrogAtomicEventQueueBuildMarker(void);

#endif
