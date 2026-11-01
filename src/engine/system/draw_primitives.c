/**********************************************************************************************
*
DRAW PRIMITIVES MODULE
*
**********************************************************************************************/
#include "system/draw_primitives.h"

#include <math.h> // atan2f

#include "raylib.h"
#include "camera/camera.h"
#include "math/geometry.h"
#include "system/texture_registry.h" // TextureRegistry_Get

Color ToRaylibColor(ColourRgba colour)
{
    return (Color){colour.r, colour.g, colour.b, colour.a};
}

static void TransformLineEndpoints(Vector2d start, Vector2d end, Matrix3x3 tfrm_mtx, Vector2d *out_start, Vector2d *out_end)
{
    // Convert the segment endpoints into pixel space once.
    if (out_start != NULL)
    {
        *out_start = TransformCoordinates(tfrm_mtx, start);
    }

    if (out_end != NULL)
    {
        *out_end = TransformCoordinates(tfrm_mtx, end);
    }
}

void DrawTransformedLineEx(Vector2d start, Vector2d end, Matrix3x3 tfrm_mtx, float line_width, ColourRgba line_colour)
{
    Vector2d line_pixel_origin = {0};
    Vector2d line_pixel_end = {0};
    TransformLineEndpoints(start, end, tfrm_mtx, &line_pixel_origin, &line_pixel_end);

    DrawLineEx((Vector2){line_pixel_origin.x, line_pixel_origin.y},
               (Vector2){line_pixel_end.x, line_pixel_end.y},line_width,
               ToRaylibColor(line_colour));
}

void DrawTransformedLineV(Vector2d start, Vector2d end, Matrix3x3 tfrm_mtx, ColourRgba line_colour)
{
    DrawTransformedLineEx(start, end, tfrm_mtx, 1.0f, line_colour);
}

// As DrawTransformedLineEx, but clips the transformed pixel-space segment to the
// axis-aligned rectangle [clip_min, clip_max] before rasterising, so the line never
// bleeds outside the game viewport. Clipping after the transform keeps it correct under
// camera rotation and zoom; segments fully outside the rectangle are skipped.
void DrawTransformedLineClippedEx(Vector2d start, Vector2d end, Matrix3x3 tfrm_mtx,
                                  float line_width, ColourRgba line_colour,
                                  Vector2d clip_min, Vector2d clip_max)
{
    // Transform both endpoints to pixel space (same path as DrawTransformedLineEx).
    Vector2d line_pixel_origin = {0};
    Vector2d line_pixel_end = {0};
    TransformLineEndpoints(start, end, tfrm_mtx, &line_pixel_origin, &line_pixel_end);

    // Clip the pixel-space segment to the axis-aligned viewport rectangle. Correct
    // under camera rotation because the clip rect is axis-aligned in pixel space.
    if (!ClipSegmentToRect(&line_pixel_origin, &line_pixel_end, clip_min, clip_max))
    {
        return; // Segment lies entirely outside the viewport; nothing to draw.
    }

    DrawLineEx((Vector2){line_pixel_origin.x, line_pixel_origin.y},
               (Vector2){line_pixel_end.x, line_pixel_end.y}, line_width,
               ToRaylibColor(line_colour));
}

// Thin wrapper over DrawTransformedLineClippedEx with a 1.0f line width, mirroring
// DrawTransformedLineV for the grid draw sites.
void DrawTransformedLineClippedV(Vector2d start, Vector2d end, Matrix3x3 tfrm_mtx,
                                 ColourRgba line_colour,
                                 Vector2d clip_min, Vector2d clip_max)
{
    DrawTransformedLineClippedEx(start, end, tfrm_mtx, 1.0f, line_colour,
                                 clip_min, clip_max);
}

void DrawTransformedArrowV(Vector2d start, Vector2d end, Matrix3x3 tfrm_mtx, ColourRgba line_colour)
{
    Vector2d line_pixel_origin = {0};
    Vector2d line_pixel_end = {0};
    TransformLineEndpoints(start, end, tfrm_mtx, &line_pixel_origin, &line_pixel_end);

    // Build the arrow geometry in screen space so it stays visible at any zoom.
    Vector2d delta = VectorDiff_2d(line_pixel_end, line_pixel_origin);
    float delta_length = VectorMagnitude_2d(delta);
    if (delta_length < 0.0001f)
    {
        DrawLineV((Vector2){line_pixel_origin.x, line_pixel_origin.y},
                  (Vector2){line_pixel_end.x, line_pixel_end.y},
                  ToRaylibColor(line_colour));
        return;
    }

    Vector2d unit_delta = {delta.x / delta_length, delta.y / delta_length};
    Vector2d perp_delta = {-unit_delta.y, unit_delta.x};

    float arrow_head_length = delta_length * 0.35f;
    if (arrow_head_length < 8.0f)
    {
        arrow_head_length = 8.0f;
    }
    if (arrow_head_length > 18.0f)
    {
        arrow_head_length = 18.0f;
    }

    float arrow_head_width = arrow_head_length * 0.65f;

    Vector2d arrow_base_pixel = VectorSum_2d(line_pixel_end, VectorScale_2d(unit_delta, -arrow_head_length));
    Vector2d arrow_left_pixel = VectorSum_2d(arrow_base_pixel, VectorScale_2d(perp_delta, arrow_head_width));
    Vector2d arrow_right_pixel = VectorSum_2d(arrow_base_pixel, VectorScale_2d(perp_delta, -arrow_head_width));

    Color color = ToRaylibColor(line_colour);
    DrawLineEx((Vector2){line_pixel_origin.x, line_pixel_origin.y},
               (Vector2){arrow_base_pixel.x, arrow_base_pixel.y},
               2.0f,
               color);
    DrawLineEx((Vector2){line_pixel_end.x, line_pixel_end.y},
               (Vector2){arrow_left_pixel.x, arrow_left_pixel.y},
               2.0f,
               color);
    DrawLineEx((Vector2){line_pixel_end.x, line_pixel_end.y},
               (Vector2){arrow_right_pixel.x, arrow_right_pixel.y},
               2.0f,
              color);
}

// Draw a whole texture as a world-space quad (see the A3-limitation note on the declaration in
// draw_primitives.h). Decomposes the affine world->pixel matrix into the position + per-axis size +
// single-rotation model DrawTexturePro understands, then issues one DrawTexturePro call. raylib is
// the floor here, exactly as DrawTriangle / DrawLineEx are for the vector primitives above.
void DrawTextureInWorld(TextureId texture_id, Vector2d world_centre, Vector2d world_size,
                        Matrix3x3 space_to_pixel_mtx, ColourRgba tint, float world_rotation_radians)
{
    // Resolve the borrowed texture; skip the draw entirely if the handle is unset/unknown. The load
    // failure (if any) was already logged once at LoadOrGet time, so the draw stays silent to avoid
    // per-frame log spam.
    const Texture2D *texture = TextureRegistry_Get(texture_id);
    if (texture == NULL)
    {
        return; // Nothing to draw (INVALID_TEXTURE_ID or not found) — silent, non-fatal.
    }

    // Map the world centre into pixel space with the SAME matrix the geometry uses.
    Vector2d pixel_centre = TransformCoordinates(space_to_pixel_mtx, world_centre);

    // Per-axis world-unit -> pixel scale = magnitude of each basis column (col1 = U basis,
    // col2 = V basis). This captures zoom and non-uniform per-axis scale; it does NOT capture
    // shear (see the A3 limitation on the declaration).
    Vector2d basis_u = {space_to_pixel_mtx.col1.x, space_to_pixel_mtx.col1.y};
    Vector2d basis_v = {space_to_pixel_mtx.col2.x, space_to_pixel_mtx.col2.y};
    float pixel_w = world_size.x * VectorMagnitude_2d(basis_u);
    float pixel_h = world_size.y * VectorMagnitude_2d(basis_v);

    // Total draw rotation = CAMERA/world-matrix rotation + the ENTITY's own rotation, mirroring the
    // vertex path which applies the entity basis BEFORE the world->pixel matrix. The matrix term is
    // the angle of the U basis column, atan2(U.y, U.x) — a CCW math angle that already tracks camera
    // pan/zoom/rotation. The entity term, world_rotation_radians, is Newtonoid2d.rotation, with the
    // SAME CCW convention (local_axis_x = (cos, sin)), so the two angles ADD directly: a positive
    // entity rotation spins the texture the SAME way its vertices spin. DrawTexturePro wants degrees.
    float angle_deg = (atan2f(basis_u.y, basis_u.x) + world_rotation_radians) * (180.0f / PI);

    // Whole texture as the source rectangle (no atlas/sub-rect in this foundation).
    Rectangle src = {0.0f, 0.0f, (float)texture->width, (float)texture->height};
    // Destination centred on the pixel centre; origin at half-size so rotation pivots on centre.
    Rectangle dst = {(float)pixel_centre.x, (float)pixel_centre.y, pixel_w, pixel_h};
    Vector2 origin = {pixel_w * 0.5f, pixel_h * 0.5f};

    DrawTexturePro(*texture, src, dst, origin, angle_deg, ToRaylibColor(tint));
}