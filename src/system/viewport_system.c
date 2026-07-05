#include "system/viewport_system.h"

#include "raylib.h"
#include "common/common.h"
#include "system/ui_system.h"
#include "system/world_system.h"

// Local derived resolution for the full screen in logical units.
static Vector2d resolution = {0};
static float viewport_left_panel_ratio = 0.20f;
static float viewport_right_panel_ratio = 0.20f;
static float viewport_target_world_logical_height = 9.0f;
static int viewport_ui_scale_scalar_override = 0;

Vector2d lpanel_u = {1, 0};
Vector2d lpanel_v = {0, 1};
Vector2d lpanel_origin = {0};
Vector2d lpanel_end = {0};
Vector2d lpanel_resolution = {0};
Vector2d lpanel_pixel_origin = {0};
Vector2d lpanel_pixel_end = {0};
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

void SetViewportTargetLogicalHeight(float logical_height)
{
    if (logical_height < 8.0f)
    {
        logical_height = 8.0f;
    }
    if (logical_height > 400.0f)
    {
        logical_height = 400.0f;
    }

    viewport_target_world_logical_height = logical_height;
}

void SetViewportUIScaleScalar(int ui_resolution_scalar)
{
    viewport_ui_scale_scalar_override = ui_resolution_scalar;
}

void SetViewportPanelRatios(float left_panel_ratio, float right_panel_ratio)
{
    float left = left_panel_ratio;
    float right = right_panel_ratio;

    if (left < 0.05f)
        left = 0.05f;
    if (right < 0.05f)
        right = 0.05f;
    if (left > 0.45f)
        left = 0.45f;
    if (right > 0.45f)
        right = 0.45f;

    // Keep enough width for the world region.
    if ((left + right) > 0.90f)
    {
        float scale = 0.90f / (left + right);
        left *= scale;
        right *= scale;
    }

    viewport_left_panel_ratio = left;
    viewport_right_panel_ratio = right;
}

void InitViewportLayout(int screen_width, int screen_height, int resolution_scalar)
{
    int world_scale_scalar = resolution_scalar;
    if (world_scale_scalar <= 0)
    {
        world_scale_scalar = (int)roundf((float)screen_height / viewport_target_world_logical_height);
        if (world_scale_scalar < 1)
        {
            world_scale_scalar = 1;
        }
    }

    int ui_scale_scalar = viewport_ui_scale_scalar_override > 0 ? viewport_ui_scale_scalar_override : world_scale_scalar;

    // 0. Calculate logical/local resolution from screen pixel resolution.
    resolution = VectorScale_2d((Vector2d){(float)screen_width, (float)screen_height}, 1.0f / (float)world_scale_scalar);
    resolution.x = floorf(resolution.x);
    resolution.y = floorf(resolution.y);
    float total_space_measure = resolution.x * resolution.y;

    // 1. Define and calculate logical regions (left panel, world, right panel).
    lpanel_origin = ZERO_VECTOR_2D;
    lpanel_end.x = floorf(lpanel_origin.x + (viewport_left_panel_ratio * resolution.x));
    lpanel_end.y = resolution.y;
    lpanel_resolution = VectorSum_2d(VectorScale_2d(lpanel_origin, -1), lpanel_end);
    float lpanel_space_measure = VectorBox_2d(lpanel_resolution);

    rpanel_end.x = resolution.x;
    rpanel_end.y = resolution.y;
    rpanel_origin.x = floorf(rpanel_end.x - (viewport_right_panel_ratio * resolution.x));
    rpanel_origin.y = 0.0f;
    rpanel_resolution = VectorSum_2d(VectorScale_2d(rpanel_origin, -1), rpanel_end);
    float rpanel_space_measure = VectorBox_2d(rpanel_resolution);

    world_resolution = (Vector2d){resolution.x - lpanel_resolution.x - rpanel_resolution.x, resolution.y};
    world_origin = (Vector2d){lpanel_end.x, 0.0f};
    world_end = (Vector2d){world_origin.x + world_resolution.x, world_origin.y + world_resolution.y};
    float world_space_measure = VectorBox_2d(world_resolution);

    // 2. Back-calculate screen pixel-space basis vectors for each region.
    universe_viewport_u = VectorScale_2d(world_u, (float)world_scale_scalar);
    universe_viewport_v = VectorScale_2d(world_v, (float)world_scale_scalar);
    lpanel_pixel_u = VectorScale_2d(lpanel_u, (float)ui_scale_scalar);
    lpanel_pixel_v = VectorScale_2d(lpanel_v, (float)ui_scale_scalar);
    rpanel_pixel_u = lpanel_pixel_u;
    rpanel_pixel_v = lpanel_pixel_v;

    // 2.2 Save basis scale factors for later coordinate conversions.
    Basis2d lpanel_basis = (Basis2d){lpanel_u, lpanel_v};
    Basis2d lpanel_pixel_basis = (Basis2d){lpanel_pixel_u, lpanel_pixel_v};
    Basis2d world_basis = (Basis2d){world_u, world_v};
    Basis2d universe_viewport_basis = (Basis2d){universe_viewport_u, universe_viewport_v};
    local_to_lpanel_scale = BasisTransform_2d_Scale(lpanel_basis, lpanel_pixel_basis);
    lpanel_to_local_scale = BasisTransform_2d_Scale(lpanel_pixel_basis, lpanel_basis);
    local_to_world_scale = BasisTransform_2d_Scale(world_basis, universe_viewport_basis);
    world_to_local_scale = BasisTransform_2d_Scale(universe_viewport_basis, world_basis);

    // 3. Calculate screen pixel-space origins for each region.
    lpanel_pixel_origin.x = (lpanel_pixel_u.x + lpanel_pixel_v.x) * lpanel_origin.x;
    lpanel_pixel_origin.y = (lpanel_pixel_u.y + lpanel_pixel_v.y) * lpanel_origin.y;
    universe_viewport_origin.x = (universe_viewport_u.x + universe_viewport_v.x) * world_origin.x;
    universe_viewport_origin.y = (universe_viewport_u.y + universe_viewport_v.y) * world_origin.y;
    rpanel_pixel_origin.x = (rpanel_pixel_u.x + rpanel_pixel_v.x) * rpanel_origin.x;
    rpanel_pixel_origin.y = (rpanel_pixel_u.y + rpanel_pixel_v.y) * rpanel_origin.y;

    // Debug check: regions should reconstruct full logical resolution.
    float resolution_recalc_x = lpanel_resolution.x + world_resolution.x + rpanel_resolution.x;
    float resolution_recalc_y = lpanel_resolution.y + world_resolution.y;
    float res_recalc_measure = resolution_recalc_x * resolution_recalc_y;

    printf("LOCAL RESOLUTIONS --> TOTAL_LOCAL(%0.1f)(%0.1f,%0.1f); LPANEL_LOCAL(%0.1f)(%0.1f,%0.1f); WORLD_LOCAL(%0.1f)(%0.1f,%0.1f); RPANEL_LOCAL(%0.1f)(%0.1f,%0.1f); TOTAL_LOCAL_RECALC_MEASURE(%0.1f);\n",
           total_space_measure, resolution.x, resolution.y,
           lpanel_space_measure, lpanel_resolution.x, lpanel_resolution.y,
           world_space_measure, world_resolution.x, world_resolution.y,
           rpanel_space_measure, rpanel_resolution.x, rpanel_resolution.y,
           res_recalc_measure);
    printf("PIXEL ORIGINS --> LPANEL(%0.1f,%0.1f); GAME_WORLD (%0.1f,%0.1f); RPANEL(%0.1f,%0.1f);\n",
           lpanel_pixel_origin.x, lpanel_pixel_origin.y,
           universe_viewport_origin.x, universe_viewport_origin.y,
           rpanel_pixel_origin.x, rpanel_pixel_origin.y);
        printf("VIEWPORT SCALE --> WORLD:%d UI:%d TARGET_WORLD_H:%.1f\n",
            world_scale_scalar, ui_scale_scalar, viewport_target_world_logical_height);
}

void DrawViewportOverlays(void)
{
    DrawRectangle((int)rpanel_pixel_origin.x,
                  (int)rpanel_pixel_origin.y,
                  (int)(rpanel_pixel_u.x * rpanel_resolution.x),
                  (int)(rpanel_pixel_v.y * rpanel_resolution.y),
                  (Color){70, 78, 66, 255});
}
