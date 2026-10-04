/**********************************************************************************************
*
LEFT PANEL SYSTEM MODULE
*
**********************************************************************************************/
#ifndef LPANEL_SYSTEM_H
#define LPANEL_SYSTEM_H

#include "math/cvectors.h"
#include "camera/camera.h"
#include "system/view_host_system.h"
#include "ui/ui.h"

// Left-panel UI tree state.
extern UIBox seed_box;

void InitLPanel(void);
// Destroy the left panel and clear its cached UI references.
void DestroyLPanel(void);
void DrawLPanel(void);
Frame2d *GetLPanelSpaceFrame(void);
bool SetLPanelSpaceBasis(Vector2d basis_u, Vector2d basis_v);
void ResetLPanelSpaceBasis(void);

// Get the root UI element (for external systems like input handling)
UIElement* GetLPanelRoot(void);

// Get the panel system instance.
struct ViewHostSystem *GetLPanelViewHost(void);

// Handle debug toggle button clicks for the left panel.
// Called by the UI loader when a toggle-debug-* action is invoked.
void HandleLPanelDebugToggleClick(UIElement *button);

#endif
