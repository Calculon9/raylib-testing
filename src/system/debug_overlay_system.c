#include "system/debug_overlay_system.h"

#include <math.h>
#include <stdio.h>

#include "raylib.h"
#include "camera/camera.h"
#include "system/viewport_system.h"
#include "ui/cfont.h"
#include "ui/text_region.h"
#include "world/universe.h"

static bool coordinate_overlay_enabled = false;

void ToggleDebugOverlay(DebugOverlayId overlay_id)
{
    switch (overlay_id)
    {
    case DEBUG_OVERLAY_COORDINATE_SPACE:
        coordinate_overlay_enabled = !coordinate_overlay_enabled;
        break;
    case DEBUG_OVERLAY_VIEWPORT_GRID:
        ToggleViewportDebugGrid();
        break;
    default:
        break;
    }
}

int IsDebugOverlayEnabled(DebugOverlayId overlay_id)
{
    switch (overlay_id)
    {
    case DEBUG_OVERLAY_COORDINATE_SPACE:
        return coordinate_overlay_enabled;
    case DEBUG_OVERLAY_VIEWPORT_GRID:
        return IsViewportDebugGridEnabled();
    default:
        return 0;
    }
}

void DrawGlobalDebugOverlays(void)
{
    DrawViewportDebugGrid();
}

void DrawUniverseDebugOverlays(Matrix3x3 root_world_to_pixel_mtx)
{
    if (!coordinate_overlay_enabled)
    {
        return;
    }

    int mouse_x = GetMouseX();
    int mouse_y = GetMouseY();
    Vector2d pixel = {(float)mouse_x, (float)mouse_y};

    bool cursor_in_game_viewport = mouse_x >= game_viewport_pixel_origin.x && mouse_x <= game_viewport_pixel_end.x &&
                                   mouse_y >= game_viewport_pixel_origin.y && mouse_y <= game_viewport_pixel_end.y;

    Matrix3x3 pixel_to_world_mtx = MatrixInvert_3x3(root_world_to_pixel_mtx);
    Vector2d parent_local = TransformCoordinates(pixel_to_world_mtx, pixel);

    int selected_index = G_Universe.selected_world_index;
    int hovered_world_index = Universe_FindWorldAt(&G_Universe, parent_local);
    int target_world_index = (hovered_world_index >= 0) ? hovered_world_index : selected_index;

    bool has_child_local = false;
    Vector2d child_origin_in_parent = ZERO_VECTOR_2D;
    Vector2d child_local = ZERO_VECTOR_2D;
    int world_cell_index = -1;

    if (target_world_index >= 0 && target_world_index < G_Universe.world_count)
    {
        World2d *world = &G_Universe.worlds[target_world_index];
        Vector2d world_resolution = {(float)world->grid_space.space.columns, (float)world->grid_space.space.rows};

        Matrix3x3 universe_to_world_mtx = world->camera.tunnel.dest_to_source_mtx;
        has_child_local = true;
        child_local = TransformCoordinates(universe_to_world_mtx, parent_local);

        child_origin_in_parent.x = world->grid_space.space.frame.origin_in_parent.x + (world_resolution.x * 0.5f);
        child_origin_in_parent.y = world->grid_space.space.frame.origin_in_parent.y + (world_resolution.y * 0.5f);

        if (child_local.x >= 0.0f && child_local.y >= 0.0f &&
            child_local.x < world_resolution.x && child_local.y < world_resolution.y)
        {
            int cell_x = (int)floorf(child_local.x);
            int cell_y = (int)floorf(child_local.y);
            world_cell_index = (cell_y * (int)world_resolution.x) + cell_x;
        }
    }

    Vector2d viewport_pixel_center = ResolveGameViewportPixelCenter();
    Vector2d viewport_local_center = ResolveGameViewportLocalCenter();
    float viewport_ppu_u = VectorMagnitude_2d(game_viewport_pixel_u);
    float viewport_ppu_v = VectorMagnitude_2d(game_viewport_pixel_v);
    float viewport_width_px = game_viewport_pixel_end.x - game_viewport_pixel_origin.x;
    float viewport_height_px = game_viewport_pixel_end.y - game_viewport_pixel_origin.y;

    const int panel_x = (int)game_viewport_pixel_origin.x + 6;
    const int panel_y = (int)game_viewport_pixel_origin.y + 6;
    const int panel_w = 840;
    const int panel_h = 330;

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){12, 16, 24, 210});
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){210, 230, 255, 180});

    char lines[11][256] = {0};
    snprintf(lines[0], sizeof(lines[0]), "Cursor px: (%.1f, %.1f) [%s]", pixel.x, pixel.y, cursor_in_game_viewport ? "in viewport" : "outside viewport");
    snprintf(lines[1], sizeof(lines[1]), "Parent coords (Universe): (%.3f, %.3f)", parent_local.x, parent_local.y);

    if (has_child_local)
    {
        snprintf(lines[2], sizeof(lines[2]), "Child local [world %d]: (%.3f, %.3f)", target_world_index, child_local.x, child_local.y);
        snprintf(lines[3], sizeof(lines[3]), "Child origin in parent: (%.3f, %.3f)", child_origin_in_parent.x, child_origin_in_parent.y);
        snprintf(lines[4], sizeof(lines[4]), "Child cell index: %d", world_cell_index);
    }
    else
    {
        snprintf(lines[2], sizeof(lines[2]), "Child local: n/a (no selected/hovered world)");
        snprintf(lines[3], sizeof(lines[3]), "Child origin in parent: n/a");
        snprintf(lines[4], sizeof(lines[4]), "Child cell index: n/a");
    }

    snprintf(lines[5], sizeof(lines[5]), "Selected world: %d | Hovered world: %d | Toggle: F11", selected_index, hovered_world_index);
    snprintf(lines[6], sizeof(lines[6]), "Viewport local: origin(%.2f, %.2f) end(%.2f, %.2f) res(%.2f, %.2f)",
             game_viewport_local_origin.x, game_viewport_local_origin.y,
             game_viewport_local_end.x, game_viewport_local_end.y,
             game_viewport_resolution.x, game_viewport_resolution.y);
    snprintf(lines[7], sizeof(lines[7]), "Viewport pixel: origin(%.1f, %.1f) end(%.1f, %.1f) size(%.1f x %.1f)",
             game_viewport_pixel_origin.x, game_viewport_pixel_origin.y,
             game_viewport_pixel_end.x, game_viewport_pixel_end.y,
             viewport_width_px, viewport_height_px);
    snprintf(lines[8], sizeof(lines[8]), "Viewport basis: U(%.1f, %.1f)|%.2fpx V(%.1f, %.1f)|%.2fpx",
             game_viewport_pixel_u.x, game_viewport_pixel_u.y, viewport_ppu_u,
             game_viewport_pixel_v.x, game_viewport_pixel_v.y, viewport_ppu_v);
    snprintf(lines[9], sizeof(lines[9]), "Viewport center: local(%.2f, %.2f) pixel(%.1f, %.1f)",
             viewport_local_center.x, viewport_local_center.y,
             viewport_pixel_center.x, viewport_pixel_center.y);
    snprintf(lines[10], sizeof(lines[10]), "Camera: focus(%.2f, %.2f) zoom=%.3f rot=%.3f", G_Universe.camera.source_focus_coords.x,
             G_Universe.camera.source_focus_coords.y, G_Universe.camera.zoom, G_Universe.camera.rotation);

    Bitmap_Font overlay_font = FONT_BASIC;
    for (int i = 0; i < 11; i++)
    {
        ColourRgba line_color = i < 6 ? (ColourRgba){240, 246, 255, 230} : (ColourRgba){190, 220, 255, 230};
        DrawTextCustom(lines[i], (Vector2d){(float)panel_x + 10, (float)panel_y + 10 + (i * 28)}, overlay_font.scale, overlay_font, line_color);
    }
}