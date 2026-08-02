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
	VIEWPORT_SPACE_ENTITY_PANEL,
	VIEWPORT_SPACE_COUNT
} ViewportSpaceId;

typedef struct ViewportRegion
{
	// Logical/local space (game units)
	Vector2d local_origin;
	Vector2d local_end;
	Vector2d local_resolution;
	
	// Physical/pixel space
	Vector2d origin;
	Vector2d end;
	Vector2d resolution;
	Vector2d pixel_origin;
	Vector2d pixel_end;
	Vector2d pixel_u;
	Vector2d pixel_v;
	
	// Frame and tunnel for rendering
	Frame2d frame;
	FrameTunnel tunnel;
} ViewportRegion;

// Viewport regions for left panel, game region, and right panel.
extern ViewportRegion game_viewport;
extern ViewportRegion lpanel_viewport;
extern ViewportRegion rpanel_viewport;
extern ViewportRegion entity_panel_viewport;
extern ViewportRegion utility_panel_viewport;

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
