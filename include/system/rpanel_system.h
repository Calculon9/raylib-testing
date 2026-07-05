/**********************************************************************************************
*
RIGHT PANEL SYSTEM MODULE
*
**********************************************************************************************/
#ifndef RPANEL_SYSTEM_H
#define RPANEL_SYSTEM_H

#include "camera/camera.h"
#include "ui/ui.h"

extern UIElement *rpanel_root;
extern Camera2d camera_rpanel;
extern UIBox rpanel_seed_box;

void InitRPanel(void);
void UpdateRPanel(int mouse_x, int mouse_y);
void DrawRPanel(void);

#endif