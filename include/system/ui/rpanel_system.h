/**********************************************************************************************
*
RIGHT PANEL SYSTEM MODULE
*
**********************************************************************************************/
#ifndef RPANEL_SYSTEM_H
#define RPANEL_SYSTEM_H

#include "camera/camera.h"
#include "system/panel_system.h"
#include "ui/ui.h"

extern UIBox rpanel_seed_box;

void InitRPanel(void);
// Destroy the right panel and clear its cached UI references.
void DestroyRPanel(void);
void DrawRPanel(void);
Frame2d *GetRPanelSpaceFrame(void);
bool SetRPanelSpaceBasis(Vector2d basis_u, Vector2d basis_v);
void ResetRPanelSpaceBasis(void);

// Get the root UI element (for external systems like input handling)
UIElement* GetRPanelRoot(void);

// Get the panel system instance.
struct PanelSystem *GetRPanelSystem(void);

#endif
