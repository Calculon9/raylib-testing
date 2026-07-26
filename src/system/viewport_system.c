#include "system/viewport_system.h"

#include "raylib.h"
#include "camera/camera.h"
#include "common/common.h"
#include "system/world_system.h"
#include "system/universe_system.h"

// ============================================================================
// Configuration & Constants
// ============================================================================
static float viewport_left_panel_ratio = 0.275f;
static float viewport_right_panel_ratio = 0.275f;
static float viewport_target_game_logical_height = 9.0f;
static int viewport_ui_pixels_per_unit_override = 0;
static const float viewport_min_target_logical_height = 8.0f;
static const float viewport_max_target_logical_height = 400.0f;
static const float viewport_min_panel_ratio = 0.05f;
static const float viewport_max_panel_ratio = 0.45f;
static const float viewport_max_combined_panel_ratio = 0.90f;
static int viewport_debug_grid_enabled = 0;
static const float viewport_basis_min_magnitude = 0.0001f;

#ifndef VIEWPORT_VERBOSE_LOGGING
#define VIEWPORT_VERBOSE_LOGGING 0
#endif

// ============================================================================
// Viewport Regions
// ============================================================================
ViewportRegion game_viewport = {0};
ViewportRegion lpanel_viewport = {0};
ViewportRegion rpanel_viewport = {0};

// ============================================================================
// Viewport Basis Vectors (used by SetViewportSpaceBasis / ResetViewportSpaceBasis)
// ============================================================================
static Vector2d lpanel_u = {1, 0};
static Vector2d lpanel_v = {0, 1};
static Vector2d rpanel_u = {1, 0};
static Vector2d rpanel_v = {0, 1};

static bool game_viewport_basis_override_enabled = false;
static Vector2d game_viewport_basis_override_u = {1, 0};
static Vector2d game_viewport_basis_override_v = {0, 1};

static void DrawViewportRegionGrid(Vector2d origin, Vector2d basis_u, Vector2d basis_v, Vector2d local_resolution,
                                   Color line_color, Color border_color)
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

bool SetViewportSpaceBasis(ViewportSpaceId space_id, Vector2d basis_u, Vector2d basis_v)
{
    if (VectorMagnitude_2d(basis_u) < viewport_basis_min_magnitude ||
        VectorMagnitude_2d(basis_v) < viewport_basis_min_magnitude)
    {
        return false;
    }

    switch (space_id)
    {
    case VIEWPORT_SPACE_LPANEL:
        lpanel_u = basis_u;
        lpanel_v = basis_v;
        return true;
    case VIEWPORT_SPACE_GAME:
        game_viewport_basis_override_enabled = true;
        game_viewport_basis_override_u = basis_u;
        game_viewport_basis_override_v = basis_v;
        return true;
    case VIEWPORT_SPACE_RPANEL:
        rpanel_u = basis_u;
        rpanel_v = basis_v;
        return true;
    default:
        return false;
    }
}

void ResetViewportSpaceBasis(ViewportSpaceId space_id)
{
    switch (space_id)
    {
    case VIEWPORT_SPACE_LPANEL:
        lpanel_u = (Vector2d){1.0f, 0.0f};
        lpanel_v = (Vector2d){0.0f, 1.0f};
        break;
    case VIEWPORT_SPACE_GAME:
        game_viewport_basis_override_enabled = false;
        game_viewport_basis_override_u = (Vector2d){1.0f, 0.0f};
        game_viewport_basis_override_v = (Vector2d){0.0f, 1.0f};
        break;
    case VIEWPORT_SPACE_RPANEL:
        rpanel_u = (Vector2d){1.0f, 0.0f};
        rpanel_v = (Vector2d){0.0f, 1.0f};
        break;
    default:
        break;
    }
}

void DrawViewportDebugGrid(void)
{
    if (!viewport_debug_grid_enabled)
    {
        return;
    }

    DrawViewportRegionGrid(lpanel_viewport.pixel_origin, lpanel_viewport.pixel_u, lpanel_viewport.pixel_v, lpanel_viewport.resolution,
                           (Color){110, 170, 255, 28}, (Color){110, 170, 255, 90});

    DrawViewportRegionGrid(game_viewport.pixel_origin, game_viewport.pixel_u, game_viewport.pixel_v, game_viewport.resolution,
                           (Color){160, 240, 160, 26}, (Color){160, 240, 160, 90});

    DrawViewportRegionGrid(rpanel_viewport.pixel_origin, rpanel_viewport.pixel_u, rpanel_viewport.pixel_v, rpanel_viewport.resolution,
                           (Color){255, 180, 110, 28}, (Color){255, 180, 110, 90});
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
        VectorScale_2d(game_viewport.pixel_u, game_viewport.resolution.x),
        VectorScale_2d(game_viewport.pixel_v, game_viewport.resolution.y));
    return VectorSum_2d(game_viewport.pixel_origin, VectorScale_2d(game_viewport_pixel_dimensions, 0.5f));
}

Vector2d ResolveGameViewportLocalCenter()
{
    Vector2d game_viewport_pixel_dimensions = VectorSum_2d(
        VectorScale_2d(game_viewport.pixel_u, game_viewport.resolution.x),
        VectorScale_2d(game_viewport.pixel_v, game_viewport.resolution.y));
    return VectorSum_2d(game_viewport.local_origin, VectorScale_2d(game_viewport.resolution, 0.5f));
}

void InitViewportLayout(int screen_width, int screen_height, int game_pixels_per_unit_override)
{
    // STATIC so its memory address stays valid after this function returns
    static Frame2d screen_frame;
    
    int screen_pixels_per_unit = game_pixels_per_unit_override;
    if (screen_pixels_per_unit <= 0)
    {
        screen_pixels_per_unit = (int)roundf((float)screen_height / viewport_target_game_logical_height);
        if (screen_pixels_per_unit < 1) screen_pixels_per_unit = 1;
    }

    int ui_pixels_per_unit = viewport_ui_pixels_per_unit_override > 0 ? viewport_ui_pixels_per_unit_override : screen_pixels_per_unit;

    // Calculate logical screen size
    Vector2d logical_screen = {floorf((float)screen_width / (float)screen_pixels_per_unit),
                               floorf((float)screen_height / (float)screen_pixels_per_unit)};
    
    screen_frame = CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, (Vector2d){(float)screen_width, (float)screen_height});

    // Resolve pixel basis vectors for game viewport
    Vector2d resolved_u = {1.0f, 0.0f}, resolved_v = {0.0f, 1.0f};
    if (game_viewport_basis_override_enabled)
    {
        resolved_u = game_viewport_basis_override_u;
        resolved_v = game_viewport_basis_override_v;
    }
    else
    {
        // Keep viewport projection canonical unless explicitly overridden.
        resolved_u = (Vector2d){1.0f, 0.0f};
        resolved_v = (Vector2d){0.0f, 1.0f};
    }

    // LEFT PANEL: origin at 0, width by ratio, full height
    lpanel_viewport.local_origin = ZERO_VECTOR_2D;
    lpanel_viewport.local_end = (Vector2d){floorf(viewport_left_panel_ratio * logical_screen.x), logical_screen.y};
    lpanel_viewport.resolution = VectorSum_2d(VectorScale_2d(lpanel_viewport.local_origin, -1.0f), lpanel_viewport.local_end);
    lpanel_viewport.pixel_u = VectorScale_2d(lpanel_u, (float)ui_pixels_per_unit);
    lpanel_viewport.pixel_v = VectorScale_2d(lpanel_v, (float)ui_pixels_per_unit);
    lpanel_viewport.pixel_origin.x = lpanel_viewport.local_origin.x * lpanel_viewport.pixel_u.x + lpanel_viewport.local_origin.y * lpanel_viewport.pixel_v.x;
    lpanel_viewport.pixel_origin.y = lpanel_viewport.local_origin.x * lpanel_viewport.pixel_u.y + lpanel_viewport.local_origin.y * lpanel_viewport.pixel_v.y;
    
    Vector2d lpanel_extent = VectorSum_2d(VectorScale_2d(lpanel_viewport.pixel_u, lpanel_viewport.resolution.x),
                                          VectorScale_2d(lpanel_viewport.pixel_v, lpanel_viewport.resolution.y));
    lpanel_viewport.pixel_end.x = lpanel_viewport.pixel_origin.x + lpanel_extent.x;
    lpanel_viewport.pixel_end.y = lpanel_viewport.pixel_origin.y + lpanel_extent.y;
    
    lpanel_viewport.frame = CreateFrame2d((Basis2d){lpanel_viewport.pixel_u, lpanel_viewport.pixel_v},
                                          lpanel_viewport.pixel_origin, lpanel_viewport.resolution);
    lpanel_viewport.tunnel.source_frame = &lpanel_viewport.frame;
    lpanel_viewport.tunnel.destination_frame = &screen_frame;
    lpanel_viewport.tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*lpanel_viewport.tunnel.source_frame);
    lpanel_viewport.tunnel.dest_to_source_mtx = MatrixInvert_3x3(lpanel_viewport.tunnel.source_to_dest_mtx);

    // RIGHT PANEL: mirror of left panel at the right edge
    rpanel_viewport.local_end = logical_screen;
    rpanel_viewport.local_origin = (Vector2d){logical_screen.x - lpanel_viewport.resolution.x, 0.0f};
    rpanel_viewport.resolution = VectorSum_2d(VectorScale_2d(rpanel_viewport.local_origin, -1.0f), rpanel_viewport.local_end);
    rpanel_viewport.pixel_u = VectorScale_2d(rpanel_u, (float)ui_pixels_per_unit);
    rpanel_viewport.pixel_v = VectorScale_2d(rpanel_v, (float)ui_pixels_per_unit);
    rpanel_viewport.pixel_origin.x = rpanel_viewport.local_origin.x * rpanel_viewport.pixel_u.x + rpanel_viewport.local_origin.y * rpanel_viewport.pixel_v.x;
    rpanel_viewport.pixel_origin.y = rpanel_viewport.local_origin.x * rpanel_viewport.pixel_u.y + rpanel_viewport.local_origin.y * rpanel_viewport.pixel_v.y;
    
    Vector2d rpanel_extent = VectorSum_2d(VectorScale_2d(rpanel_viewport.pixel_u, rpanel_viewport.resolution.x),
                                          VectorScale_2d(rpanel_viewport.pixel_v, rpanel_viewport.resolution.y));
    rpanel_viewport.pixel_end.x = rpanel_viewport.pixel_origin.x + rpanel_extent.x;
    rpanel_viewport.pixel_end.y = rpanel_viewport.pixel_origin.y + rpanel_extent.y;
    
    rpanel_viewport.frame = CreateFrame2d((Basis2d){rpanel_viewport.pixel_u, rpanel_viewport.pixel_v},
                                          rpanel_viewport.pixel_origin, rpanel_viewport.resolution);
    rpanel_viewport.tunnel.source_frame = &rpanel_viewport.frame;
    rpanel_viewport.tunnel.destination_frame = &screen_frame;
    rpanel_viewport.tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*rpanel_viewport.tunnel.source_frame);
    rpanel_viewport.tunnel.dest_to_source_mtx = MatrixInvert_3x3(rpanel_viewport.tunnel.source_to_dest_mtx);

    // GAME VIEWPORT: fills the gap between left and right panels
    game_viewport.local_origin = (Vector2d){lpanel_viewport.local_end.x, 0.0f};
    game_viewport.local_end = (Vector2d){rpanel_viewport.local_origin.x, logical_screen.y};
    game_viewport.resolution = VectorSum_2d(VectorScale_2d(game_viewport.local_origin, -1.0f), game_viewport.local_end);
    game_viewport.local_resolution = game_viewport.resolution;
    game_viewport.pixel_u = VectorScale_2d(resolved_u, (float)screen_pixels_per_unit);
    game_viewport.pixel_v = VectorScale_2d(resolved_v, (float)screen_pixels_per_unit);
    game_viewport.pixel_origin.x = game_viewport.local_origin.x * game_viewport.pixel_u.x + game_viewport.local_origin.y * game_viewport.pixel_v.x;
    game_viewport.pixel_origin.y = game_viewport.local_origin.x * game_viewport.pixel_u.y + game_viewport.local_origin.y * game_viewport.pixel_v.y;
    
    Vector2d game_extent = VectorSum_2d(VectorScale_2d(game_viewport.pixel_u, game_viewport.resolution.x),
                                        VectorScale_2d(game_viewport.pixel_v, game_viewport.resolution.y));
    game_viewport.pixel_end.x = game_viewport.pixel_origin.x + game_extent.x;
    game_viewport.pixel_end.y = game_viewport.pixel_origin.y + game_extent.y;
    
    game_viewport.frame = CreateFrame2d((Basis2d){game_viewport.pixel_u, game_viewport.pixel_v},
                                        game_viewport.pixel_origin, game_viewport.resolution);
    game_viewport.tunnel.source_frame = &game_viewport.frame;
    game_viewport.tunnel.destination_frame = &screen_frame;
    game_viewport.tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*game_viewport.tunnel.source_frame);
    game_viewport.tunnel.dest_to_source_mtx = MatrixInvert_3x3(game_viewport.tunnel.source_to_dest_mtx);

    // Debug output
    printf("LOGICAL REGIONS --> SCREEN(SIZE:%0.1f,%0.1f); LPANEL(SIZE:%0.1f,%0.1f); GAME(SIZE:%0.1f,%0.1f); RPANEL(SIZE:%0.1f,%0.1f)\n",
           logical_screen.x, logical_screen.y,
           lpanel_viewport.resolution.x, lpanel_viewport.resolution.y,
           game_viewport.resolution.x, game_viewport.resolution.y,
           rpanel_viewport.resolution.x, rpanel_viewport.resolution.y);
    printf("PIXEL ORIGINS --> LPANEL(%0.1f,%0.1f); GAME(%0.1f,%0.1f); RPANEL(%0.1f,%0.1f)\n",
           lpanel_viewport.pixel_origin.x, lpanel_viewport.pixel_origin.y,
           game_viewport.pixel_origin.x, game_viewport.pixel_origin.y,
           rpanel_viewport.pixel_origin.x, rpanel_viewport.pixel_origin.y);
    printf("VIEWPORT SCALE --> GAME_PX_PER_UNIT:%d UI_PX_PER_UNIT:%d TARGET_GAME_H:%.1f\n",
           screen_pixels_per_unit, ui_pixels_per_unit, viewport_target_game_logical_height);
}
