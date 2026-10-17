/**********************************************************************************************
*
UNIVERSE RENDERER MODULE
*
**********************************************************************************************/
#ifndef UNIVERSE_RENDERER_H
#define UNIVERSE_RENDERER_H

#include "camera/camera.h"
#include "world/universe.h"

Vector2d UniverseRenderer_GetResolution(void);
void UniverseRenderer_Draw(Universe *universe, CameraViewBox camera_view, Matrix3x3 camera_to_pixel_mtx);

// Outline every member of the central selection set (entities by their AABB, worlds by their
// cached bounds). A world-space overlay pass that only reads the set + existing bounds.
void WorldRenderer_DrawSelectionHighlights(Universe *universe);

#endif
