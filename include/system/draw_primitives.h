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
#include "system/texture_registry.h" // TextureId

Color ToRaylibColor(ColourRgba colour);

// Draw a whole texture as a world-space quad. `world_centre` and `world_size` are in WORLD units;
// `space_to_pixel_mtx` is the SAME world->pixel matrix the surrounding draw uses (so the texture
// tracks camera pan/zoom/rotation identically to the vector geometry). `tint` modulates the
// texture ({255,255,255,255} => untinted). No-op if `texture_id` resolves to no texture.
//
// A3 LIMITATION: this decomposes the affine matrix into DrawTexturePro's position + per-axis size +
// single rotation model. That is EXACT only for a non-sheared, per-axis-uniform world->pixel
// transform (pan/zoom/camera-rotation are fine). It CANNOT represent shear/skew; if a sheared
// world basis ever reaches the viewport the texture will not track the geometry exactly. The
// documented escape hatch is a rotated-quad via rlgl (transform the 4 corners through the matrix +
// textured triangles), which composes with ANY affine matrix. That fallback is deliberately NOT
// implemented — keep this primitive to the single DrawTexturePro call until runtime shows skew
// artefacts (the spike validated pan/zoom/rotation tracking with no shear, so it is not needed).
void DrawTextureInWorld(TextureId texture_id, Vector2d world_centre, Vector2d world_size,
                        Matrix3x3 space_to_pixel_mtx, ColourRgba tint);
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