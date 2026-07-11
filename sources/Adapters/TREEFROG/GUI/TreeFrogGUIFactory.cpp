#include "TreeFrogGUIFactory.h"
#include "TreeFrogGUIWindowImp.h"
#include "TreeFrogEventManager.h"

TreeFrogGUIFactory::TreeFrogGUIFactory() {}

I_GUIWindowImp &TreeFrogGUIFactory::CreateWindowImp(GUICreateWindowParams &p) {
    return *(new TreeFrogGUIWindowImp(p));
}

EventManager *TreeFrogGUIFactory::GetEventManager() {
    return TreeFrogEventManager::GetInstance();
}
