/**********************************************************************************************
*
LEFT PANEL SYSTEM MODULE
*
**********************************************************************************************/
#ifndef LPANEL_SYSTEM_H
#define LPANEL_SYSTEM_H

#include "math/cvectors.h"
#include "camera/camera.h"
#include "ui/ui.h"

// Left-panel UI tree state.
extern UIBox seed_box;
extern UIElement *lpanel_root;

// Left-panel viewport state used by input and panel rendering.
extern Vector2d lpanel_origin;
extern Vector2d lpanel_end;
extern Vector2d lpanel_resolution;
extern Vector2d lpanel_pixel_origin;
extern Vector2d lpanel_pixel_end;
extern Vector2d lpanel_pixel_u;
extern Vector2d lpanel_pixel_v;
extern Vector2d lpanel_u;
extern Vector2d lpanel_v;
extern Vector2d local_to_lpanel_scale;
extern Vector2d lpanel_to_local_scale;
extern Camera2d camera_lpanel;

void InitLPanel(void);
void UpdateLPanel(int mouse_x, int mouse_y);
void DrawLPanel(void);

#endif