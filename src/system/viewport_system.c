#include "system/viewport_system.h"

#include "raylib.h"
#include "camera/camera.h"
#include "common/common.h"

// ============================================================================
// Configuration & Constants
// ============================================================================
static float viewport_left_panel_ratio = 0.15f;
static float viewport_right_panel_ratio = 0.15f;
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
ViewportRegion entity_panel_viewport = {0};
ViewportRegion utility_panel_viewport = {0};

// ============================================================================
// Viewport Basis Vectors (used by SetViewportSpaceBasis / ResetViewportSpaceBasis)
// ============================================================================
static Vector2d lpanel_u = {1, 0};
static Vector2d lpanel_v = {0, 1};
static Vector2d rpanel_u = {1, 0};
static Vector2d rpanel_v = {0, 1};
static Vector2d entity_panel_u = {1, 0};
static Vector2d entity_panel_v = {0, 1};

static bool game_viewport_basis_override_enabled = false;
static Vector2d game_viewport_basis_override_u = {1, 0};
static Vector2d game_viewport_basis_override_v = {0, 1};

static void FinalizeViewportRegion(ViewportRegion *region, Vector2d basis_u, Vector2d basis_v,
                                   float pixels_per_unit, Frame2d *screen_frame)
{
    if (!region || !screen_frame)
    {
        return;
    }

    region->resolution = VectorSum_2d(VectorScale_2d(region->local_origin, -1.0f), region->local_end);
    region->pixel_u = VectorScale_2d(basis_u, pixels_per_unit);
    region->pixel_v = VectorScale_2d(basis_v, pixels_per_unit);
    region->pixel_origin.x = region->local_origin.x * region->pixel_u.x + region->local_origin.y * region->pixel_v.x;
    region->pixel_origin.y = region->local_origin.x * region->pixel_u.y + region->local_origin.y * region->pixel_v.y;

    Vector2d extent = VectorSum_2d(VectorScale_2d(region->pixel_u, region->resolution.x),
                                   VectorScale_2d(region->pixel_v, region->resolution.y));
    region->pixel_end.x = region->pixel_origin.x + extent.x;
    region->pixel_end.y = region->pixel_origin.y + extent.y;

    region->frame = CreateFrame2d((Basis2d){region->pixel_u, region->pixel_v},
                                  region->pixel_origin, region->resolution);
    region->tunnel.source_frame = &region->frame;
    region->tunnel.destination_frame = screen_frame;
    region->tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*region->tunnel.source_frame);
    region->tunnel.dest_to_source_mtx = MatrixInvert_3x3(region->tunnel.source_to_dest_mtx);
}

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

    Vector2d *target_u = NULL;
    Vector2d *target_v = NULL;

    switch (space_id)
    {
    case VIEWPORT_SPACE_LPANEL:
        target_u = &lpanel_u;
        target_v = &lpanel_v;
        break;
    case VIEWPORT_SPACE_GAME:
        game_viewport_basis_override_enabled = true;
        game_viewport_basis_override_u = basis_u;
        game_viewport_basis_override_v = basis_v;
        return true;
    case VIEWPORT_SPACE_RPANEL:
        target_u = &rpanel_u;
        target_v = &rpanel_v;
        break;
    case VIEWPORT_SPACE_ENTITY_PANEL:
        target_u = &entity_panel_u;
        target_v = &entity_panel_v;
        break;
    default:
        return false;
    }

    if (!target_u || !target_v)
    {
        return false;
    }

    *target_u = basis_u;
    *target_v = basis_v;

    return true;
}

void ResetViewportSpaceBasis(ViewportSpaceId space_id)
{
    if (space_id == VIEWPORT_SPACE_GAME)
    {
        game_viewport_basis_override_enabled = false;
        game_viewport_basis_override_u = (Vector2d){1.0f, 0.0f};
        game_viewport_basis_override_v = (Vector2d){0.0f, 1.0f};
        return;
    }

    (void)SetViewportSpaceBasis(space_id, (Vector2d){1.0f, 0.0f}, (Vector2d){0.0f, 1.0f});
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
        if (screen_pixels_per_unit < 1)
            screen_pixels_per_unit = 1;
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

    // LEFT PANEL: origin at 0, width by ratio, full height
    lpanel_viewport.local_origin = ZERO_VECTOR_2D;
    lpanel_viewport.local_end = (Vector2d){floorf(viewport_left_panel_ratio * logical_screen.x), logical_screen.y};
    FinalizeViewportRegion(&lpanel_viewport, lpanel_u, lpanel_v, (float)ui_pixels_per_unit, &screen_frame);

    // RIGHT PANEL: mirror of left panel at the right edge
    rpanel_viewport.local_end = logical_screen;
    rpanel_viewport.local_origin = (Vector2d){logical_screen.x - lpanel_viewport.resolution.x, 0.0f};
    FinalizeViewportRegion(&rpanel_viewport, rpanel_u, rpanel_v, (float)ui_pixels_per_unit, &screen_frame);

    // GAME VIEWPORT: fills the gap between left and right panels
    game_viewport.local_origin = (Vector2d){lpanel_viewport.local_end.x, 0.0f};
    float entity_panel_height = fmaxf(1.0f, floorf(logical_screen.y * 0.275f));
    game_viewport.local_end = (Vector2d){rpanel_viewport.local_origin.x, logical_screen.y - entity_panel_height};
    FinalizeViewportRegion(&game_viewport, resolved_u, resolved_v, (float)screen_pixels_per_unit, &screen_frame);
    game_viewport.local_resolution = game_viewport.resolution;

    // BOTTOM ENTITY PANEL: horizontal panel spanning the full screen viewport.
    entity_panel_viewport.local_origin = (Vector2d){0.0f, game_viewport.local_end.y};
    entity_panel_viewport.local_end = (Vector2d){logical_screen.x, logical_screen.y};
    FinalizeViewportRegion(&entity_panel_viewport, entity_panel_u, entity_panel_v, (float)ui_pixels_per_unit, &screen_frame);

    // UTILITY PANEL: rightmost section of the full-width bottom state-manager band.
    float utility_width = fminf(lpanel_viewport.resolution.x, logical_screen.x);
    utility_panel_viewport.local_origin = (Vector2d){
        logical_screen.x - utility_width,
        game_viewport.local_end.y};
    utility_panel_viewport.local_end = logical_screen;
    FinalizeViewportRegion(&utility_panel_viewport, entity_panel_u, entity_panel_v, (float)ui_pixels_per_unit, &screen_frame);

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
