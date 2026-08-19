/**********************************************************************************************
*
UNIVERSE RENDERER MODULE
*
**********************************************************************************************/
#include "raylib.h"
#include "common/common.h"
#include "math/cvectors.h"
#include "system/debug_overlay_system.h"
#include "system/draw_primitives.h"
#include "ui/cfont.h"
#include "world/universe_renderer.h"

static int universe_grid_cells_x = 60;
static int universe_grid_cells_y = 60;
static float universe_grid_cell_size = 1.0f;

extern ColourRgba camera_marker_colour;

Vector2d UniverseRenderer_GetResolution(void)
{
    if (universe_grid_cell_size <= 0.0f)
    {
        universe_grid_cell_size = 1.0f;
    }

    return (Vector2d){
        (float)universe_grid_cells_x * universe_grid_cell_size,
        (float)universe_grid_cells_y * universe_grid_cell_size};
}

static void DrawUniverseCameraMarker(const Universe *universe, Matrix3x3 camera_to_pixel_mtx)
{
    Vector2d camera_world_pos = universe->camera.source_focus_coords;
    Vector2d pixel_origin = TransformCoordinates(camera_to_pixel_mtx, camera_world_pos);
    float marker_pixel_size = 24.0f * (float)universe->camera.zoom;
    float half_size = marker_pixel_size * 0.5f;

    Vector2 position = {
        (float)(pixel_origin.x - half_size),
        (float)(pixel_origin.y - half_size)};
    Vector2 size = {(float)marker_pixel_size, (float)marker_pixel_size};
    Color color = ToRaylibColor(camera_marker_colour);

    DrawRectangleV(position, size, color);
}

static void DrawUniverseGrid(const Universe *universe, CameraViewBox camera_view, Matrix3x3 camera_to_pixel_mtx)
{
    float grid_cell_size = universe_grid_cell_size;

    float uni_half_w = universe->resolution.x * 0.5f;
    float uni_half_h = universe->resolution.y * 0.5f;
    float lens_min_x = camera_view.origin.x;
    float lens_max_x = camera_view.origin.x + camera_view.dimensions.x;
    float lens_min_y = camera_view.origin.y;
    float lens_max_y = camera_view.origin.y + camera_view.dimensions.y;

    float world_min_x = fmaxf(lens_min_x, -uni_half_w);
    float world_max_x = fminf(lens_max_x, uni_half_w);
    float world_min_y = fmaxf(lens_min_y, -uni_half_h);
    float world_max_y = fminf(lens_max_y, uni_half_h);
    float start_x = floorf(world_min_x / grid_cell_size) * grid_cell_size;
    float start_y = floorf(world_min_y / grid_cell_size) * grid_cell_size;

    for (float x = start_x; x <= world_max_x; x += grid_cell_size)
    {
        Vector2d line_start = {x, -uni_half_h};
        Vector2d line_end = {x, uni_half_h};
        DrawTransformedLineV(line_start, line_end, camera_to_pixel_mtx, COLOUR_GAME_OLIVE_RGBA);
    }

    for (float y = start_y; y <= world_max_y; y += grid_cell_size)
    {
        Vector2d line_start = {-uni_half_w, y};
        Vector2d line_end = {uni_half_w, y};
        DrawTransformedLineV(line_start, line_end, camera_to_pixel_mtx, COLOUR_GAME_OLIVE_RGBA);
    }

    DrawTransformedLineEx((Vector2d){world_min_x, 0.0f}, (Vector2d){world_max_x, 0.0f},
                          camera_to_pixel_mtx, 2.5f, COLOUR_GAME_AXIS_X_RGBA);
    DrawTransformedLineEx((Vector2d){0.0f, world_min_y}, (Vector2d){0.0f, world_max_y},
                          camera_to_pixel_mtx, 2.5f, COLOUR_GAME_AXIS_Y_RGBA);

    if (!IsDebugEnabled(DEBUG_UNIVERSE_GRID_LABELS))
    {
        return;
    }

    Vector2d p00 = TransformCoordinates(camera_to_pixel_mtx, ZERO_VECTOR_2D);
    Vector2d p10 = TransformCoordinates(camera_to_pixel_mtx, (Vector2d){grid_cell_size, 0.0f});
    float cell_px_w = (float)VectorMagnitude_2d((Vector2d){p10.x - p00.x, p10.y - p00.y});
    if (cell_px_w < 40.0f)
    {
        return;
    }

    Color text_colour = {
        COLOUR_GAME_PARCHMENT_RGBA.r,
        COLOUR_GAME_PARCHMENT_RGBA.g,
        COLOUR_GAME_PARCHMENT_RGBA.b,
        220};
    for (float y = start_y; y < world_max_y; y += grid_cell_size)
    {
        for (float x = start_x; x < world_max_x; x += grid_cell_size)
        {
            Vector2d cell_origin = {x, y};
            Vector2d cell_pixel = TransformCoordinates(camera_to_pixel_mtx, cell_origin);
            int ix = (int)floorf((x + uni_half_w) / grid_cell_size);
            int iy = (int)floorf((y + uni_half_h) / grid_cell_size);

            if (ix < 0) ix = 0;
            if (ix >= universe_grid_cells_x) ix = universe_grid_cells_x - 1;
            if (iy < 0) iy = 0;
            if (iy >= universe_grid_cells_y) iy = universe_grid_cells_y - 1;

            int cell_index = (iy * universe_grid_cells_x) + ix;
            const char *display_text = TextFormat("%d\n(%.0f,%.0f)", cell_index, cell_origin.x, cell_origin.y);
            DrawTextEx(font, display_text,
                       (Vector2){(float)cell_pixel.x + 2, (float)cell_pixel.y + 2},
                       18, 1, text_colour);
        }
    }
}

void UniverseRenderer_Draw(Universe *universe, CameraViewBox camera_view, Matrix3x3 camera_to_pixel_mtx)
{
    if (!universe)
    {
        return;
    }

    DrawUniverseGrid(universe, camera_view, camera_to_pixel_mtx);
    Universe_Draw(universe);
    DrawUniverseCameraMarker(universe, camera_to_pixel_mtx);
    DrawUniverseDebugOverlays();
}
