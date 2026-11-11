/**********************************************************************************************
*
WORLD MANAGER SYSTEM MODULE (right panel)
*
**********************************************************************************************/
#ifndef WORLD_MANAGER_SYSTEM_H
#define WORLD_MANAGER_SYSTEM_H

#include "camera/camera.h"
#include "system/view_host_system.h"
#include "ui/ui.h"

// Load world_manager.xml and build the right-hand panel (mirrors the Entity Manager for worlds / cells).
void InitWorldManagerSystem(void);

// Destroy the World Manager and clear its cached UI references.
void DestroyWorldManagerSystem(void);

// Refresh the selection-driven layout and draw the panel.
void DrawWorldManagerSystem(void);

// Debug-basis hooks for the panel's coordinate space (used by the diagnostics basis editor).
Frame2d *GetWorldManagerSpaceFrame(void);
bool SetWorldManagerSpaceBasis(Vector2d basis_u, Vector2d basis_v);
void ResetWorldManagerSpaceBasis(void);

// Get the root UI element (for external systems like input handling).
UIElement *GetWorldManagerRoot(void);

// Get the panel system instance.
struct ViewHostSystem *GetWorldManagerViewHost(void);

#endif
