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
#include "system/ui_palette.h"
#include "system/viewport_system.h"
#include "ui/cfont.h"
#include "world/universe_renderer.h"

static int universe_grid_cells_x = 500;
static int universe_grid_cells_y = 500;
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

// Fill the world viewport rectangle with the palette's world background (the root world's empty
// space). Only the game viewport is filled: the band below it keeps the panel background cleared
// by DrawGameplayScreen, and UI panels draw over this afterwards.
static void DrawWorldBackground(const UIWorldColours *world_colours)
{
    Vector2 origin = {(float)game_viewport.pixel_origin.x, (float)game_viewport.pixel_origin.y};
    Vector2 size = {(float)(game_viewport.pixel_end.x - game_viewport.pixel_origin.x),
                    (float)(game_viewport.pixel_end.y - game_viewport.pixel_origin.y)};
    DrawRectangleV(origin, size, ToRaylibColor(world_colours->background));
}

// Draw the universe grid lines, axes and optional cell labels, all in palette world colours.
static void DrawUniverseGrid(const Universe *universe, CameraViewBox camera_view, Matrix3x3 camera_to_pixel_mtx,
                             const UIWorldColours *colours)
{
    const UIWorldColours world_colours = *colours;
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
        // Clip to the game viewport pixel rect so grid lines do not bleed into panels.
        DrawTransformedLineClippedV(line_start, line_end, camera_to_pixel_mtx,
                                    world_colours.grid_line,
                                    game_viewport.pixel_origin, game_viewport.pixel_end);
    }

    for (float y = start_y; y <= world_max_y; y += grid_cell_size)
    {
        Vector2d line_start = {-uni_half_w, y};
        Vector2d line_end = {uni_half_w, y};
        // Clip to the game viewport pixel rect so grid lines do not bleed into panels.
        DrawTransformedLineClippedV(line_start, line_end, camera_to_pixel_mtx,
                                    world_colours.grid_line,
                                    game_viewport.pixel_origin, game_viewport.pixel_end);
    }

    DrawTransformedLineEx((Vector2d){world_min_x, 0.0f}, (Vector2d){world_max_x, 0.0f},
                          camera_to_pixel_mtx, 2.5f, world_colours.axis_x);
    DrawTransformedLineEx((Vector2d){0.0f, world_min_y}, (Vector2d){0.0f, world_max_y},
                          camera_to_pixel_mtx, 2.5f, world_colours.axis_y);

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

    // Debug label ink: the default cell fill colour (light on the dark world background).
    Color text_colour = {
        world_colours.cell_fill.r,
        world_colours.cell_fill.g,
        world_colours.cell_fill.b,
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

    // Resolve the world colours once per frame; background first so everything else draws over it.
    const UIWorldColours world_colours = UIPalette_GetWorldColours(NULL);
    DrawWorldBackground(&world_colours);
    DrawUniverseGrid(universe, camera_view, camera_to_pixel_mtx, &world_colours);
    Universe_Draw(universe);
    DrawUniverseCameraMarker(universe, camera_to_pixel_mtx);
    DrawUniverseDebugOverlays();
}
