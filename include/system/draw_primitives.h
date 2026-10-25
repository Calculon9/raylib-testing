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
// As DrawTransformedLineEx, but transforms the endpoints to pixel space, clips the
// resulting pixel-space segment to the axis-aligned rectangle [clip_min, clip_max]
// (Liang-Barsky), and draws only the surviving portion. Clipping happens AFTER the
// transform, so it is correct under camera rotation and zoom. Lines fully outside the
// rectangle are skipped.
void DrawTransformedLineClippedEx(Vector2d start, Vector2d end, Matrix3x3 tfrm_mtx,
                                  float line_width, ColourRgba line_colour,
                                  Vector2d clip_min, Vector2d clip_max);
// Thin wrapper over DrawTransformedLineClippedEx with a 1.0f line width, mirroring
// DrawTransformedLineV for the grid draw sites.
void DrawTransformedLineClippedV(Vector2d start, Vector2d end, Matrix3x3 tfrm_mtx,
                                 ColourRgba line_colour,
                                 Vector2d clip_min, Vector2d clip_max);
void DrawTransformedArrowV(Vector2d start, Vector2d end, Matrix3x3 world_to_pixel_mtx, ColourRgba line_colour);

#endif