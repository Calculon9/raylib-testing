/**********************************************************************************************
*
VIEWPORT SYSTEM MODULE
*
**********************************************************************************************/
#ifndef VIEWPORT_SYSTEM_H
#define VIEWPORT_SYSTEM_H

#include "math/cvectors.h"

// Left panel region resolved during viewport layout.
extern Vector2d lpanel_u;
extern Vector2d lpanel_v;
extern Vector2d lpanel_origin;
extern Vector2d lpanel_end;
extern Vector2d lpanel_resolution;
extern Vector2d lpanel_pixel_origin;
extern Vector2d lpanel_pixel_u;
extern Vector2d lpanel_pixel_v;
extern Vector2d local_to_lpanel_scale;
extern Vector2d lpanel_to_local_scale;

// Right panel region resolved during viewport layout.
extern Vector2d rpanel_origin;
extern Vector2d rpanel_end;
extern Vector2d rpanel_resolution;
extern Vector2d rpanel_pixel_origin;
extern Vector2d rpanel_pixel_u;
extern Vector2d rpanel_pixel_v;

// Resolve logical and pixel-space regions for left panel, game region, and right panel.
// game_pixels_per_unit_override <= 0 uses dynamic scaling from target game logical height.
void InitViewportLayout(int screen_width, int screen_height, int game_pixels_per_unit_override);

// Configure desired game logical height used when game_pixels_per_unit_override <= 0.
void SetViewportTargetLogicalHeight(float logical_height);

// Configure optional UI pixels-per-unit override; <= 0 means follow game scale.
void SetViewportUIScaleScalar(int ui_pixels_per_unit_override);

// Configure left/right panel width ratios before layout initialisation.
void SetViewportPanelRatios(float left_panel_ratio, float right_panel_ratio);

#endif
