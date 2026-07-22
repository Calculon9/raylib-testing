/**********************************************************************************************
*
DEBUG OVERLAY SYSTEM MODULE
*
**********************************************************************************************/
#ifndef DEBUG_OVERLAY_SYSTEM_H
#define DEBUG_OVERLAY_SYSTEM_H

#include "math/cvectors.h"

typedef enum DebugOverlayId
{
    DEBUG_OVERLAY_COORDINATE_SPACE = 0,
    DEBUG_OVERLAY_VIEWPORT_GRID,
    DEBUG_OVERLAY_COUNT
} DebugOverlayId;

void ToggleDebugOverlay(DebugOverlayId overlay_id);
int IsDebugOverlayEnabled(DebugOverlayId overlay_id);

// Draw overlays that depend on the universe/world transform chain.
void DrawUniverseDebugOverlays(Matrix3x3 root_world_to_pixel_mtx);

// Draw overlays that are global screen/viewport overlays.
void DrawGlobalDebugOverlays(void);

#endif