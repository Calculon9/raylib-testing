#ifndef STATE_MANAGER_SYSTEM_H
#define STATE_MANAGER_SYSTEM_H

#include "ui/ui.h"

void InitStateManagerSystem(void);
void DrawStateManagerSystem(void);
UIElement *GetStateManagerRoot(void);
void MarkStateManagerRefreshDirty(void);
void UpdateStateManagerSelectedObject(void);

#endif
