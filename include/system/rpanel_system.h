/**********************************************************************************************
*
RIGHT PANEL SYSTEM MODULE
*
**********************************************************************************************/
#ifndef RPANEL_SYSTEM_H
#define RPANEL_SYSTEM_H

#include "camera/camera.h"
#include "ui/ui.h"

extern UIBox rpanel_seed_box;

void InitRPanel(void);
void UpdateRPanel(int mouse_x, int mouse_y);
void DrawRPanel(void);
Frame2d *GetRPanelSpaceFrame(void);
bool SetRPanelSpaceBasis(Vector2d basis_u, Vector2d basis_v);
void ResetRPanelSpaceBasis(void);

// Get the root UI element (for external systems like input handling)
UIElement* GetRPanelRoot(void);

#endif
