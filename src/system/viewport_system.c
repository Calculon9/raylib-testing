#include "system/viewport_system.h"

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

#ifndef VIEWPORT_VERBOSE_LOGGING
#define VIEWPORT_VERBOSE_LOGGING 0
#endif

Vector2d lpanel_u = {1, 0};
Vector2d lpanel_v = {0, 1};
Vector2d lpanel_origin = {0};
Vector2d lpanel_end = {0};
Vector2d lpanel_resolution = {0};
Vector2d lpanel_pixel_origin = {0};
Vector2d lpanel_pixel_u = {0};
Vector2d lpanel_pixel_v = {0};
Vector2d local_to_lpanel_scale = {0};
Vector2d lpanel_to_local_scale = {0};
Camera2d camera_lpanel = {0};

Vector2d rpanel_origin = {0};
Vector2d rpanel_end = {0};
Vector2d rpanel_resolution = {0};
Vector2d rpanel_pixel_origin = {0};
Vector2d rpanel_pixel_u = {0};
Vector2d rpanel_pixel_v = {0};

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
    viewport_target_game_logical_height = ClampFloat(logical_height,
                                                     viewport_min_target_logical_height,
                                                     viewport_max_target_logical_height);
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
    lpanel_origin = ZERO_VECTOR_2D;
    lpanel_end.x = floorf(lpanel_origin.x + (viewport_left_panel_ratio * logical_screen_size.x));
    lpanel_end.y = logical_screen_size.y;
    lpanel_resolution = VectorSum_2d(VectorScale_2d(lpanel_origin, -1), lpanel_end);
    float lpanel_logical_area = VectorBox_2d(lpanel_resolution);

    rpanel_end.x = logical_screen_size.x;
    rpanel_end.y = logical_screen_size.y;
    rpanel_origin.x = floorf(rpanel_end.x - (viewport_right_panel_ratio * logical_screen_size.x));
    rpanel_origin.y = 0.0f;
    rpanel_resolution = VectorSum_2d(VectorScale_2d(rpanel_origin, -1), rpanel_end);
    float rpanel_logical_area = VectorBox_2d(rpanel_resolution);

    game_region_resolution = (Vector2d){logical_screen_size.x - lpanel_resolution.x - rpanel_resolution.x, logical_screen_size.y};
    game_region_origin = (Vector2d){lpanel_end.x, 0.0f};
    game_region_end = (Vector2d){game_region_origin.x + game_region_resolution.x, game_region_origin.y + game_region_resolution.y};
    float game_region_logical_area = VectorBox_2d(game_region_resolution);

    // Resolve pixel-space basis vectors for each region.
    Vector2d *world_u = GetNextWorldBasisUPtr();
    Vector2d *world_v = GetNextWorldBasisVPtr();
    Vector2d resolved_world_u = (world_u && VectorMagnitude_2d(*world_u) > 0.0f) ? *world_u : (Vector2d){1.0f, 0.0f};
    Vector2d resolved_world_v = (world_v && VectorMagnitude_2d(*world_v) > 0.0f) ? *world_v : (Vector2d){0.0f, 1.0f};
    game_viewport_u = VectorScale_2d(resolved_world_u, (float)screen_pixels_per_unit);
    game_viewport_v = VectorScale_2d(resolved_world_v, (float)screen_pixels_per_unit);
    lpanel_pixel_u = VectorScale_2d(lpanel_u, (float)ui_pixels_per_unit);
    lpanel_pixel_v = VectorScale_2d(lpanel_v, (float)ui_pixels_per_unit);
    rpanel_pixel_u = lpanel_pixel_u;
    rpanel_pixel_v = lpanel_pixel_v;

    // Save basis scale factors for panel coordinate conversions.
    Basis2d lpanel_basis = (Basis2d){lpanel_u, lpanel_v};
    Basis2d lpanel_pixel_basis = (Basis2d){lpanel_pixel_u, lpanel_pixel_v};
    local_to_lpanel_scale = BasisTransform_2d_Scale(lpanel_basis, lpanel_pixel_basis);
    lpanel_to_local_scale = BasisTransform_2d_Scale(lpanel_pixel_basis, lpanel_basis);

    // Calculate screen pixel-space origins for each region.
    lpanel_pixel_origin.x = (lpanel_pixel_u.x + lpanel_pixel_v.x) * lpanel_origin.x;
    lpanel_pixel_origin.y = (lpanel_pixel_u.y + lpanel_pixel_v.y) * lpanel_origin.y;
    game_viewport_origin.x = (game_viewport_u.x + game_viewport_v.x) * game_region_origin.x;
    game_viewport_origin.y = (game_viewport_u.y + game_viewport_v.y) * game_region_origin.y;
    game_viewport_end.x = (game_viewport_u.x + game_viewport_v.x) * game_region_end.x;
    game_viewport_end.y = (game_viewport_u.y + game_viewport_v.y) * game_region_end.y;
    rpanel_pixel_origin.x = (rpanel_pixel_u.x + rpanel_pixel_v.x) * rpanel_origin.x;
    rpanel_pixel_origin.y = (rpanel_pixel_u.y + rpanel_pixel_v.y) * rpanel_origin.y;

    // Debug check: region widths/heights should reconstruct the total logical screen size.
    float reconstructed_logical_width = lpanel_resolution.x + game_region_resolution.x + rpanel_resolution.x;
    float reconstructed_logical_height = lpanel_resolution.y + game_region_resolution.y;
    float reconstructed_logical_area = reconstructed_logical_width * reconstructed_logical_height;

    #if VIEWPORT_VERBOSE_LOGGING
        printf("LOGICAL REGIONS --> SCREEN(AREA:%0.1f SIZE:%0.1f,%0.1f); LPANEL(AREA:%0.1f SIZE:%0.1f,%0.1f); GAME(AREA:%0.1f SIZE:%0.1f,%0.1f); RPANEL(AREA:%0.1f SIZE:%0.1f,%0.1f); RECONSTRUCTED_AREA(%0.1f);\n",
            total_logical_area, logical_screen_size.x, logical_screen_size.y,
            lpanel_logical_area, lpanel_resolution.x, lpanel_resolution.y,
            game_region_logical_area, game_region_resolution.x, game_region_resolution.y,
            rpanel_logical_area, rpanel_resolution.x, rpanel_resolution.y,
            reconstructed_logical_area);
        printf("PIXEL ORIGINS --> LPANEL(%0.1f,%0.1f); GAME_VIEWPORT (%0.1f,%0.1f); RPANEL(%0.1f,%0.1f);\n",
            lpanel_pixel_origin.x, lpanel_pixel_origin.y,
            game_viewport_origin.x, game_viewport_origin.y,
            rpanel_pixel_origin.x, rpanel_pixel_origin.y);
        printf("VIEWPORT SCALE --> GAME_PX_PER_UNIT:%d UI_PX_PER_UNIT:%d TARGET_GAME_H:%.1f\n",
            screen_pixels_per_unit, ui_pixels_per_unit, viewport_target_game_logical_height);
    #else
        (void)total_logical_area;
        (void)lpanel_logical_area;
        (void)game_region_logical_area;
        (void)rpanel_logical_area;
        (void)reconstructed_logical_area;
    #endif
}

