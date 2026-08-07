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

#endif
