/**********************************************************************************************
*
DRAW PRIMITIVES MODULE
*
**********************************************************************************************/
#ifndef DRAW_PRIMITIVES_H
#define DRAW_PRIMITIVES_H

#include "common/common.h"
#include "camera/camera.h"
#include "math/cvectors.h"

void DrawTransformedLineV(Vector2d start, Vector2d end, Matrix3x3 world_to_pixel_mtx, ColourRgba line_colour);
void DrawTransformedArrowV(Vector2d start, Vector2d end, Matrix3x3 world_to_pixel_mtx, ColourRgba line_colour);

#endif