#ifndef STATE_MANAGER_SYSTEM_H
#define STATE_MANAGER_SYSTEM_H

#include "system/view_host_system.h"
#include "ui/ui.h"

void InitStateManagerSystem(void);
// Destroy the state-manager panel and clear its cached UI references.
void DestroyStateManagerSystem(void);
void DrawStateManagerSystem(void);
UIElement *GetStateManagerRoot(void);
void MarkStateManagerRefreshDirty(void);
void UpdateStateManagerSelectedObject(void);

// Get the panel system instance.
struct ViewHostSystem *GetStateManagerViewHost(void);

#endif
