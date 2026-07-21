#ifndef TREEFROG_GUI_FACTORY_H
#define TREEFROG_GUI_FACTORY_H

#include "UIFramework/Interfaces/I_GUIWindowFactory.h"

class TreeFrogGUIFactory: public I_GUIWindowFactory {
public:
    TreeFrogGUIFactory();
    virtual I_GUIWindowImp &CreateWindowImp(GUICreateWindowParams &p);
    virtual EventManager *GetEventManager();
};

#endif
