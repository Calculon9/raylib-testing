/**********************************************************************************************
*
DRAW PRIMITIVES MODULE
*
**********************************************************************************************/
#ifndef DRAW_PRIMITIVES_H
#define DRAW_PRIMITIVES_H

#include "raylib.h"
#include "common/common.h"
#include "camera/camera.h"
#include "math/cvectors.h"

Color ToRaylibColor(ColourRgba colour);
void DrawTransformedLineEx(Vector2d start, Vector2d end, Matrix3x3 world_to_pixel_mtx, float line_width, ColourRgba line_colour);
void DrawTransformedLineV(Vector2d start, Vector2d end, Matrix3x3 world_to_pixel_mtx, ColourRgba line_colour);
void DrawTransformedArrowV(Vector2d start, Vector2d end, Matrix3x3 world_to_pixel_mtx, ColourRgba line_colour);

#endif