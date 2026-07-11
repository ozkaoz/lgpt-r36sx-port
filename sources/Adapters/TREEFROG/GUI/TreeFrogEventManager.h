#ifndef TREEFROG_EVENT_MANAGER_H
#define TREEFROG_EVENT_MANAGER_H

#include "Foundation/T_Singleton.h"
#include "UIFramework/SimpleBaseClasses/EventManager.h"
#include "UIFramework/BasicDatas/GUIEvent.h"

class TreeFrogEventManager: public T_Singleton<TreeFrogEventManager>, public EventManager {
public:
    TreeFrogEventManager();
    virtual ~TreeFrogEventManager();
    virtual bool Init();
    virtual int MainLoop();
    virtual void PostQuitMessage();
    virtual int GetKeyCode(const char *name);

    void PushPad(GUIEventPadButtonType button, bool pressed);
    void Flush();
    void ClearQueue();
};

void TreeFrogSetQuitRequested(bool q);
bool TreeFrogQuitRequested();

#endif
