#include "system/viewport_system.h"

#include "raylib.h"
#include "camera/camera.h"
#include "common/common.h"
#include "system/world_system.h"
#include "system/universe_system.h"

// Screen-space size expressed in game logical units (after game pixel scaling).
static float viewport_left_panel_ratio = 0.20f;
static float viewport_right_panel_ratio = 0.20f;
static float viewport_target_game_logical_height = 9.0f;
// Optional UI pixels-per-unit override; <= 0 follows the game pixels-per-unit.
static int viewport_ui_pixels_per_unit_override = 0;
static const float viewport_min_target_logical_height = 8.0f;
static const float viewport_max_target_logical_height = 400.0f;
static const float viewport_min_panel_ratio = 0.05f;
static const float viewport_max_panel_ratio = 0.45f;
static const float viewport_max_combined_panel_ratio = 0.90f;
static int viewport_debug_grid_enabled = 0;

#ifndef VIEWPORT_VERBOSE_LOGGING
#define VIEWPORT_VERBOSE_LOGGING 0
#endif

Vector2d game_viewport_local_origin = {0};
Vector2d game_viewport_local_end = {0};
Vector2d game_viewport_local_resolution = {0};
Vector2d game_viewport_resolution = {0};
Vector2d game_viewport_pixel_origin = {0};
Vector2d game_viewport_pixel_end = {0};
Vector2d game_viewport_pixel_u = {0};
Vector2d game_viewport_pixel_v = {0};

Vector2d lpanel_u = {1, 0};
Vector2d lpanel_v = {0, 1};
Vector2d lpanel_viewport_local_origin = {0};
Vector2d lpanel_viewport_local_end = {0};
Vector2d lpanel_viewport_resolution = {0};
Vector2d lpanel_origin = {0};
Vector2d lpanel_end = {0};
Vector2d lpanel_resolution = {0};
Vector2d lpanel_pixel_origin = {0};
Vector2d lpanel_pixel_u = {0};
Vector2d lpanel_pixel_v = {0};
Vector2d local_to_lpanel_scale = {0};
Vector2d lpanel_to_local_scale = {0};
//Camera2d camera_lpanel = {0};

Vector2d rpanel_viewport_local_origin = {0};
Vector2d rpanel_viewport_local_end = {0};
Vector2d rpanel_viewport_resolution = {0};
Vector2d rpanel_origin = {0};
Vector2d rpanel_end = {0};
Vector2d rpanel_resolution = {0};
Vector2d rpanel_pixel_origin = {0};
Vector2d rpanel_pixel_u = {0};
Vector2d rpanel_pixel_v = {0};

Frame2d viewport_frame = {0};
Frame2d screen_frame = {0};
FrameTunnel viewport_tunnel = {0};
Frame2d lpanel_viewport_frame = {0};
Frame2d rpanel_viewport_frame = {0};
Frame2d game_viewport_frame = {0};
FrameTunnel lpanel_tunnel = {0};
FrameTunnel rpanel_tunnel = {0};
FrameTunnel game_viewport_tunnel = {0};

static void DrawViewportRegionGrid(Vector2d origin,
                                   Vector2d basis_u,
                                   Vector2d basis_v,
                                   Vector2d local_resolution,
                                   Color line_color,
                                   Color border_color)
{
    int cols = (int)floorf(local_resolution.x);
    int rows = (int)floorf(local_resolution.y);

    for (int x = 0; x <= cols; x++)
    {
        Vector2d line_start = VectorSum_2d(origin, VectorScale_2d(basis_u, (float)x));
        Vector2d line_end = VectorSum_2d(line_start, VectorScale_2d(basis_v, local_resolution.y));
        DrawLineEx((Vector2){(float)line_start.x, (float)line_start.y},
                   (Vector2){(float)line_end.x, (float)line_end.y},
                   1.0f,
                   line_color);
    }

    for (int y = 0; y <= rows; y++)
    {
        Vector2d line_start = VectorSum_2d(origin, VectorScale_2d(basis_v, (float)y));
        Vector2d line_end = VectorSum_2d(line_start, VectorScale_2d(basis_u, local_resolution.x));
        DrawLineEx((Vector2){(float)line_start.x, (float)line_start.y},
                   (Vector2){(float)line_end.x, (float)line_end.y},
                   1.0f,
                   line_color);
    }

    Vector2d p00 = origin;
    Vector2d p10 = VectorSum_2d(origin, VectorScale_2d(basis_u, local_resolution.x));
    Vector2d p11 = VectorSum_2d(p10, VectorScale_2d(basis_v, local_resolution.y));
    Vector2d p01 = VectorSum_2d(origin, VectorScale_2d(basis_v, local_resolution.y));

    DrawLineEx((Vector2){(float)p00.x, (float)p00.y}, (Vector2){(float)p10.x, (float)p10.y}, 2.0f, border_color);
    DrawLineEx((Vector2){(float)p10.x, (float)p10.y}, (Vector2){(float)p11.x, (float)p11.y}, 2.0f, border_color);
    DrawLineEx((Vector2){(float)p11.x, (float)p11.y}, (Vector2){(float)p01.x, (float)p01.y}, 2.0f, border_color);
    DrawLineEx((Vector2){(float)p01.x, (float)p01.y}, (Vector2){(float)p00.x, (float)p00.y}, 2.0f, border_color);
}

static float ClampFloat(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

void SetViewportTargetLogicalHeight(float logical_height)
{
    viewport_target_game_logical_height = ClampFloat(logical_height, viewport_min_target_logical_height, viewport_max_target_logical_height);
}

void SetViewportUIScaleScalar(int ui_pixels_per_unit_override)
{
    viewport_ui_pixels_per_unit_override = ui_pixels_per_unit_override;
}

void SetViewportPanelRatios(float left_panel_ratio, float right_panel_ratio)
{
    float left = ClampFloat(left_panel_ratio, viewport_min_panel_ratio, viewport_max_panel_ratio);
    float right = ClampFloat(right_panel_ratio, viewport_min_panel_ratio, viewport_max_panel_ratio);

    // Keep enough width for the world region.
    if ((left + right) > viewport_max_combined_panel_ratio)
    {
        float scale = viewport_max_combined_panel_ratio / (left + right);
        left *= scale;
        right *= scale;
    }

    viewport_left_panel_ratio = left;
    viewport_right_panel_ratio = right;
}

void DrawViewportDebugGrid(void)
{
    if (!viewport_debug_grid_enabled)
    {
        return;
    }

    DrawViewportRegionGrid(lpanel_pixel_origin,
                           lpanel_pixel_u,
                           lpanel_pixel_v,
                           lpanel_viewport_resolution,
                           (Color){110, 170, 255, 28},
                           (Color){110, 170, 255, 90});

    DrawViewportRegionGrid(game_viewport_pixel_origin,
                           game_viewport_pixel_u,
                           game_viewport_pixel_v,
                           game_viewport_resolution,
                           (Color){160, 240, 160, 26},
                           (Color){160, 240, 160, 90});

    DrawViewportRegionGrid(rpanel_pixel_origin,
                           rpanel_pixel_u,
                           rpanel_pixel_v,
                           rpanel_viewport_resolution,
                           (Color){255, 180, 110, 28},
                           (Color){255, 180, 110, 90});
}

void ToggleViewportDebugGrid(void)
{
    viewport_debug_grid_enabled = !viewport_debug_grid_enabled;
}

int IsViewportDebugGridEnabled(void)
{
    return viewport_debug_grid_enabled;
}

Vector2d ResolveGameViewportPixelCenter()
{
    Vector2d game_viewport_pixel_dimensions = VectorSum_2d(
        VectorScale_2d(game_viewport_pixel_u, game_viewport_resolution.x),
        VectorScale_2d(game_viewport_pixel_v, game_viewport_resolution.y));
    return VectorSum_2d(game_viewport_pixel_origin, VectorScale_2d(game_viewport_pixel_dimensions, 0.5f));
}

Vector2d ResolveGameViewportLocalCenter()
{
    Vector2d game_viewport_pixel_dimensions = VectorSum_2d(
        VectorScale_2d(game_viewport_pixel_u, game_viewport_resolution.x),
        VectorScale_2d(game_viewport_pixel_v, game_viewport_resolution.y));
    return VectorSum_2d(game_viewport_local_origin, VectorScale_2d(game_viewport_resolution, 0.5f));
}

void InitViewportLayout(int screen_width, int screen_height, int game_pixels_per_unit_override)
{
    int screen_pixels_per_unit = game_pixels_per_unit_override;
    if (screen_pixels_per_unit <= 0)
    {
        screen_pixels_per_unit = (int)roundf((float)screen_height / viewport_target_game_logical_height);
        if (screen_pixels_per_unit < 1)
        {
            screen_pixels_per_unit = 1;
        }
    }

    int ui_pixels_per_unit = viewport_ui_pixels_per_unit_override > 0 ? viewport_ui_pixels_per_unit_override : screen_pixels_per_unit;

    // 0. Resolve total screen size in game logical units.
    Vector2d logical_screen_size = VectorScale_2d((Vector2d){(float)screen_width, (float)screen_height}, 1.0f / (float)screen_pixels_per_unit);
    logical_screen_size.x = floorf(logical_screen_size.x);
    logical_screen_size.y = floorf(logical_screen_size.y);
    float total_logical_area = logical_screen_size.x * logical_screen_size.y;

    // Resolve logical regions (left panel, game region, right panel).
    lpanel_viewport_local_origin = ZERO_VECTOR_2D;
    lpanel_viewport_local_end.x = floorf(lpanel_viewport_local_origin.x + (viewport_left_panel_ratio * logical_screen_size.x));
    lpanel_viewport_local_end.y = logical_screen_size.y;
    lpanel_viewport_resolution = VectorSum_2d(VectorScale_2d(lpanel_viewport_local_origin, -1), lpanel_viewport_local_end);
    float lpanel_logical_area = VectorBox_2d(lpanel_viewport_resolution);

    rpanel_viewport_local_end.x = logical_screen_size.x;
    rpanel_viewport_local_end.y = logical_screen_size.y;
    rpanel_viewport_local_origin.x = floorf(rpanel_viewport_local_end.x - (viewport_right_panel_ratio * logical_screen_size.x));
    rpanel_viewport_local_origin.y = 0.0f;
    rpanel_viewport_resolution = VectorSum_2d(VectorScale_2d(rpanel_viewport_local_origin, -1), rpanel_viewport_local_end);
    float rpanel_logical_area = VectorBox_2d(rpanel_viewport_resolution);

    // Keep legacy names in sync while viewport rename migration is in progress.
    lpanel_origin = lpanel_viewport_local_origin;
    lpanel_end = lpanel_viewport_local_end;
    lpanel_resolution = lpanel_viewport_resolution;
    rpanel_origin = rpanel_viewport_local_origin;
    rpanel_end = rpanel_viewport_local_end;
    rpanel_resolution = rpanel_viewport_resolution;

    game_viewport_resolution = (Vector2d){logical_screen_size.x - lpanel_viewport_resolution.x - rpanel_viewport_resolution.x, logical_screen_size.y};
    game_viewport_local_resolution = game_viewport_resolution;
    game_viewport_local_origin = (Vector2d){lpanel_viewport_local_end.x, 0.0f};
    game_viewport_local_end = (Vector2d){game_viewport_local_origin.x + game_viewport_resolution.x, game_viewport_local_origin.y + game_viewport_resolution.y};
    float game_viewport_local_area = VectorBox_2d(game_viewport_resolution);

    // Resolve pixel-space basis vectors for each region.
    Vector2d *world_u = GetNextWorldBasisUPtr();
    Vector2d *world_v = GetNextWorldBasisVPtr();
    Vector2d resolved_world_u = (world_u && VectorMagnitude_2d(*world_u) > 0.0f) ? *world_u : (Vector2d){1.0f, 0.0f};
    Vector2d resolved_world_v = (world_v && VectorMagnitude_2d(*world_v) > 0.0f) ? *world_v : (Vector2d){0.0f, 1.0f};
    game_viewport_pixel_u = VectorScale_2d(resolved_world_u, (float)screen_pixels_per_unit);
    game_viewport_pixel_v = VectorScale_2d(resolved_world_v, (float)screen_pixels_per_unit);
    lpanel_pixel_u = VectorScale_2d(lpanel_u, (float)ui_pixels_per_unit);
    lpanel_pixel_v = VectorScale_2d(lpanel_v, (float)ui_pixels_per_unit);
    rpanel_pixel_u = lpanel_pixel_u;
    rpanel_pixel_v = lpanel_pixel_v;

    // Save basis scale factors for panel coordinate conversions.
    Basis2d lpanel_basis = (Basis2d){lpanel_u, lpanel_v};
    Basis2d lpanel_pixel_basis = (Basis2d){lpanel_pixel_u, lpanel_pixel_v};
    local_to_lpanel_scale = Frame_GetBasisScaling(lpanel_basis, lpanel_pixel_basis);
    lpanel_to_local_scale = Frame_GetBasisScaling(lpanel_pixel_basis, lpanel_basis);

    // Calculate screen pixel-space origins for each region.
    lpanel_pixel_origin.x = (lpanel_pixel_u.x + lpanel_pixel_v.x) * lpanel_viewport_local_origin.x;
    lpanel_pixel_origin.y = (lpanel_pixel_u.y + lpanel_pixel_v.y) * lpanel_viewport_local_origin.y;
    game_viewport_pixel_origin.x = (game_viewport_pixel_u.x + game_viewport_pixel_v.x) * game_viewport_local_origin.x;
    game_viewport_pixel_origin.y = (game_viewport_pixel_u.y + game_viewport_pixel_v.y) * game_viewport_local_origin.y;
    game_viewport_pixel_end.x = (game_viewport_pixel_u.x + game_viewport_pixel_v.x) * game_viewport_local_end.x;
    game_viewport_pixel_end.y = (game_viewport_pixel_u.y + game_viewport_pixel_v.y) * game_viewport_local_end.y;
    rpanel_pixel_origin.x = (rpanel_pixel_u.x + rpanel_pixel_v.x) * rpanel_viewport_local_origin.x;
    rpanel_pixel_origin.y = (rpanel_pixel_u.y + rpanel_pixel_v.y) * rpanel_viewport_local_origin.y;

    // Debug check: region widths/heights should reconstruct the total logical screen size.
    float reconstructed_logical_width = lpanel_viewport_resolution.x + game_viewport_resolution.x + rpanel_viewport_resolution.x;
    float reconstructed_logical_height = lpanel_viewport_resolution.y + game_viewport_resolution.y;
    float reconstructed_logical_area = reconstructed_logical_width * reconstructed_logical_height;

    // Create a Frame for viewport space to be used for Universe camera construction.
    // The viewport frame is defined in pixel-space, with its origin at the top-left of the screen.
    // ==========================================
    // COMPLETE LAYOUT HOOKS & TUNNEL BUILD
    // ==========================================

    // Core Window Boundaries
    screen_frame = CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, (Vector2d){(float)screen_width, (float)screen_height});
    // Viewport frame represents the entire screen canvas in logical game layout units
    viewport_frame = CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, logical_screen_size);

    // Initialize Master Viewport-to-Screen Root Tunnel
    viewport_tunnel.source_frame = &viewport_frame;
    viewport_tunnel.destination_frame = &screen_frame;
    // Build the structural matrix transformation bypassing manual hacks
    viewport_tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*viewport_tunnel.source_frame);

    // Construct Frame2d Instances for Each Unique Region
    // These frames translate from regional local viewport spaces straight into Screen parent coordinates
    
    // Left UI Panel Frame
    //Basis2d lpanel_pixel_basis = (Basis2d){lpanel_pixel_u, lpanel_pixel_v};
    lpanel_viewport_frame = CreateFrame2d(lpanel_pixel_basis, lpanel_pixel_origin, lpanel_viewport_resolution);
    
    // Right UI Panel Frame
    Basis2d rpanel_pixel_basis = (Basis2d){rpanel_pixel_u, rpanel_pixel_v};
    rpanel_viewport_frame = CreateFrame2d(rpanel_pixel_basis, rpanel_pixel_origin, rpanel_viewport_resolution);

    // Central Game Viewport Frame
    Basis2d game_pixel_basis = (Basis2d){game_viewport_pixel_u, game_viewport_pixel_v};
    game_viewport_frame = CreateFrame2d(game_pixel_basis, game_viewport_pixel_origin, game_viewport_resolution);

    game_viewport_tunnel.source_frame = &game_viewport_frame;
    game_viewport_tunnel.destination_frame = &screen_frame;
    game_viewport_tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*game_viewport_tunnel.source_frame);

    // Initialize the Viewport -> Screen Display Layout Tunnels
    lpanel_tunnel.source_frame = &lpanel_viewport_frame;
    lpanel_tunnel.destination_frame = &screen_frame;
    lpanel_tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*lpanel_tunnel.source_frame);

    rpanel_tunnel.source_frame = &rpanel_viewport_frame;
    rpanel_tunnel.destination_frame = &screen_frame;
    rpanel_tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*rpanel_tunnel.source_frame);

    game_viewport_tunnel.source_frame = &game_viewport_frame;
    game_viewport_tunnel.destination_frame = &screen_frame;
    game_viewport_tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*game_viewport_tunnel.source_frame);


    printf("LOGICAL REGIONS --> SCREEN(AREA:%0.1f SIZE:%0.1f,%0.1f); LPANEL(AREA:%0.1f SIZE:%0.1f,%0.1f); GAME(AREA:%0.1f SIZE:%0.1f,%0.1f); RPANEL(AREA:%0.1f SIZE:%0.1f,%0.1f); RECONSTRUCTED_AREA(%0.1f);\n",
           total_logical_area, logical_screen_size.x, logical_screen_size.y,
           lpanel_logical_area, lpanel_viewport_resolution.x, lpanel_viewport_resolution.y,
           game_viewport_local_area, game_viewport_resolution.x, game_viewport_resolution.y,
           rpanel_logical_area, rpanel_viewport_resolution.x, rpanel_viewport_resolution.y,
           reconstructed_logical_area);
    printf("PIXEL ORIGINS --> LPANEL(%0.1f,%0.1f); GAME_VIEWPORT (%0.1f,%0.1f); RPANEL(%0.1f,%0.1f);\n",
           lpanel_pixel_origin.x, lpanel_pixel_origin.y,
            game_viewport_pixel_origin.x, game_viewport_pixel_origin.y,
           rpanel_pixel_origin.x, rpanel_pixel_origin.y);
    printf("VIEWPORT SCALE --> GAME_PX_PER_UNIT:%d UI_PX_PER_UNIT:%d TARGET_GAME_H:%.1f\n",
           screen_pixels_per_unit, ui_pixels_per_unit, viewport_target_game_logical_height);

}

