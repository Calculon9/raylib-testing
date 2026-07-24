/**********************************************************************************************
*
VIEWPORT SYSTEM MODULE
*
**********************************************************************************************/
#ifndef VIEWPORT_SYSTEM_H
#define VIEWPORT_SYSTEM_H

#include "math/cvectors.h"
#include "camera/camera.h"

typedef enum ViewportSpaceId
{
	VIEWPORT_SPACE_LPANEL = 0,
	VIEWPORT_SPACE_GAME,
	VIEWPORT_SPACE_RPANEL,
	VIEWPORT_SPACE_COUNT
} ViewportSpaceId;

// Viewport frames and tunnels for left panel, game region, and right panel.
extern Frame2d viewport_frame;
extern Frame2d screen_frame;
extern FrameTunnel viewport_tunnel;
extern Frame2d lpanel_viewport_frame;
extern Frame2d rpanel_viewport_frame;
extern Frame2d game_viewport_frame;
extern FrameTunnel lpanel_viewport_tunnel;
extern FrameTunnel rpanel_viewport_tunnel;
extern FrameTunnel game_viewport_tunnel;

// Game viewport region resolved during viewport layout.
extern Vector2d game_viewport_local_origin;
extern Vector2d game_viewport_local_end;
extern Vector2d game_viewport_local_resolution;
extern Vector2d game_viewport_resolution;
extern Vector2d game_viewport_pixel_origin;
extern Vector2d game_viewport_pixel_end;
extern Vector2d game_viewport_pixel_u;
extern Vector2d game_viewport_pixel_v;

// Left panel region resolved during viewport layout.
extern Vector2d lpanel_u;
extern Vector2d lpanel_v;
extern Vector2d lpanel_viewport_local_origin;
extern Vector2d lpanel_viewport_local_end;
extern Vector2d lpanel_viewport_resolution;
extern Vector2d lpanel_origin;
extern Vector2d lpanel_end;
extern Vector2d lpanel_resolution;
extern Vector2d lpanel_pixel_origin;
extern Vector2d lpanel_pixel_u;
extern Vector2d lpanel_pixel_v;
extern Vector2d local_to_lpanel_scale;
extern Vector2d lpanel_to_local_scale;

// Right panel region resolved during viewport layout.
extern Vector2d rpanel_viewport_local_origin;
extern Vector2d rpanel_viewport_local_end;
extern Vector2d rpanel_viewport_resolution;
extern Vector2d rpanel_origin;
extern Vector2d rpanel_end;
extern Vector2d rpanel_resolution;
extern Vector2d rpanel_u;
extern Vector2d rpanel_v;
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

// Runtime debug utilities to inspect and modify viewport-space basis vectors.
bool SetViewportSpaceBasis(ViewportSpaceId space_id, Vector2d basis_u, Vector2d basis_v);
void ResetViewportSpaceBasis(ViewportSpaceId space_id);

// Viewport debug overlay helpers.
void DrawViewportDebugGrid(void);
void ToggleViewportDebugGrid(void);
int IsViewportDebugGridEnabled(void);

// Viewport center helpers.
Vector2d ResolveGameViewportPixelCenter(void);
Vector2d ResolveGameViewportLocalCenter(void);

#endif
