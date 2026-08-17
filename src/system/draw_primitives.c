/**********************************************************************************************
*
DRAW PRIMITIVES MODULE
*
**********************************************************************************************/
#include "system/draw_primitives.h"

#include "raylib.h"
#include "camera/camera.h"

static Color ToRaylibColor(ColourRgba colour)
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

void DrawTransformedLineV(Vector2d start, Vector2d end, Matrix3x3 tfrm_mtx, ColourRgba line_colour)
{
    Vector2d line_pixel_origin = {0};
    Vector2d line_pixel_end = {0};
    TransformLineEndpoints(start, end, tfrm_mtx, &line_pixel_origin, &line_pixel_end);

    DrawLineV((Vector2){line_pixel_origin.x, line_pixel_origin.y},
              (Vector2){line_pixel_end.x, line_pixel_end.y},
              ToRaylibColor(line_colour));
}

void DrawTransformedArrowV(Vector2d start, Vector2d end, Matrix3x3 tfrm_mtx, ColourRgba line_colour)
{
    Vector2d line_pixel_origin = {0};
    Vector2d line_pixel_end = {0};
    TransformLineEndpoints(start, end, tfrm_mtx, &line_pixel_origin, &line_pixel_end);

    // Build the arrow geometry in screen space so it stays visible at any zoom.
    Vector2d delta = VectorSum_2d(line_pixel_end, (Vector2d){-line_pixel_origin.x, -line_pixel_origin.y});
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