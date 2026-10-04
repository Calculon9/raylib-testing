#ifndef UTILITY_PANEL_SYSTEM_H
#define UTILITY_PANEL_SYSTEM_H

#include "system/view_host_system.h"
#include "ui/ui.h"

void InitUtilityPanel(void);
// Destroy the utility panel and clear its cached UI references.
void DestroyUtilityPanel(void);
void DrawUtilityPanel(void);
UIElement *GetUtilityPanelRoot(void);

// Get the panel system instance.
struct ViewHostSystem *GetUtilityPanelViewHost(void);

#endif
