/**********************************************************************************************
*
DEBUG OVERLAY SYSTEM MODULE
*
**********************************************************************************************/
#ifndef DEBUG_OVERLAY_SYSTEM_H
#define DEBUG_OVERLAY_SYSTEM_H

#include "math/cvectors.h"
#include "system/viewport_system.h"

typedef enum DebugOverlayId
{
    DEBUG_DASHBOARD = 0,
    DEBUG_VIEWPORT_GRID,
    DEBUG_WORLD_GRID,
    DEBUG_WORLD_GRID_LABELS,
    DEBUG_UNIVERSE_GRID_LABELS,
    DEBUG_UI_BORDERS,
    DEBUG_OBJECT_AXES,
    DEBUG_COUNT
} DebugOverlayId;

extern bool ui_borders_enabled;

void ToggleDebug(DebugOverlayId overlay_id);
int IsDebugEnabled(DebugOverlayId overlay_id);
void UpdateDebugOverlayHotkeys(int screen_width,int screen_height, int screen_resolution_scalar,
                               float *viewport_target_game_logical_height, int *viewport_ui_pixels_per_unit_override);

// Draw overlays that depend on the universe/world transform chain.
void DrawUniverseDebugOverlays(Matrix3x3 root_world_to_pixel_mtx);

// Draw overlays that are global screen/viewport overlays.
void DrawGlobalDebugOverlays(void);

#endif