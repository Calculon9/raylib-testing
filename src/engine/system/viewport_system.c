#include "system/viewport_system.h"

#include "raylib.h"
#include "camera/camera.h"
#include "common/common.h"
#include "math/helpers.h"

// ============================================================================
// Configuration & Constants
// ============================================================================

// Screen-partition layout configuration. Gathers ALL of the hard-coded screen-partition
// literals that were previously scattered through InitViewportLayout (and the clamp bounds in
// SetViewportTargetLogicalHeight / SetViewportPanelRatios) into one declared data struct. This
// is a pure extraction of magic numbers: every field below holds exactly the literal it
// replaces, so the computed viewport regions are byte-identical to before. The relative math,
// region order and clamp semantics are unchanged; only the SOURCE of each number moved here.
typedef struct ViewportLayoutConfig
{
    float default_left_panel_ratio;    // Initial left-panel width as a fraction of logical width (was 0.175f).
    float default_right_panel_ratio;   // Initial right-panel width as a fraction of logical width (was 0.175f).
    float min_target_logical_height;   // Lower clamp on the target game logical height (was 8.0f).
    float max_target_logical_height;   // Upper clamp on the target game logical height (was 400.0f).
    float min_panel_ratio;             // Lower clamp on each individual panel ratio (was 0.05f).
    float max_panel_ratio;             // Upper clamp on each individual panel ratio (was 0.45f).
    float max_combined_panel_ratio;    // Upper clamp on the combined left+right panel ratio (was 0.90f).
    float info_strip_logical_height;   // Logical (game-unit) height of the top info-strip band (was 0.9f).
    float entity_panel_height_factor;  // Bottom entity-panel height as a fraction of logical height (was inline 0.275f).
    float min_band_logical;            // Minimum logical height of a carved band (was inline fmaxf(1.0f, ...)).
    float band_edge_gap_logical;       // Logical gap reserved so a band never swallows its column (was the inline -1.0f gaps).
} ViewportLayoutConfig;

// Single file-scope instance of the layout configuration. Values confirmed against the former
// inline literals; see the field comments above for the one-to-one mapping.
static ViewportLayoutConfig g_viewport_layout_config = {
    .default_left_panel_ratio   = 0.175f,
    .default_right_panel_ratio  = 0.175f,
    .min_target_logical_height  = 8.0f,
    .max_target_logical_height  = 400.0f,
    .min_panel_ratio            = 0.05f,
    .max_panel_ratio            = 0.45f,
    .max_combined_panel_ratio   = 0.90f,
    .info_strip_logical_height  = 0.9f,
    .entity_panel_height_factor = 0.275f,
    .min_band_logical           = 1.0f,
    .band_edge_gap_logical      = 1.0f,
};

// Logical width (game units) of the far-left toolbar band. Deliberately DERIVED from the
// info-strip band height so the toolbar's width stays exactly equal to the strip's height, with
// no separate stored value that could drift. This preserves the former
// `toolbar_logical_width = info_strip_logical_height` alias. Both extents live in the same
// logical (game-unit) viewport space, so the equality is genuine.
static inline float ViewportLayoutConfig_ToolbarLogicalWidth(const ViewportLayoutConfig *cfg)
{
    return cfg->info_strip_logical_height;
}

// Live, user-adjustable panel ratios (runtime state mutated by SetViewportPanelRatios). They are
// initialised from the config defaults; InitViewportLayout reads the CURRENT values, not the
// defaults.
static float left_panel_ratio = 0.175f;
static float right_panel_ratio = 0.175f;
static float target_game_logical_height = 9.0f;
static int ui_ppu_override = 0;
static int debug_grid_enabled = 0;

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
ViewportRegion info_strip_viewport = {0};
ViewportRegion toolbar_viewport = {0};

// ============================================================================
// Viewport Basis Vectors (used by SetViewportSpaceBasis / ResetViewportSpaceBasis)
// ============================================================================
// Consolidated viewport basis vectors - stores all (u, v) pairs for each viewport space
static struct {
    Vector2d u, v;
} viewport_basis_data[VIEWPORT_SPACE_COUNT] = {
    [VIEWPORT_SPACE_LPANEL] = {{1, 0}, {0, 1}},
    [VIEWPORT_SPACE_RPANEL] = {{1, 0}, {0, 1}},
    [VIEWPORT_SPACE_ENTITY_PANEL] = {{1, 0}, {0, 1}},
};

typedef struct ViewportBasisPair
{
    Vector2d *u;
    Vector2d *v;
} ViewportBasisPair;

// Pointers to basis vector pairs indexed by viewport space
static const ViewportBasisPair viewport_basis_pairs[VIEWPORT_SPACE_COUNT] = {
    [VIEWPORT_SPACE_LPANEL] = {&viewport_basis_data[VIEWPORT_SPACE_LPANEL].u, &viewport_basis_data[VIEWPORT_SPACE_LPANEL].v},
    [VIEWPORT_SPACE_RPANEL] = {&viewport_basis_data[VIEWPORT_SPACE_RPANEL].u, &viewport_basis_data[VIEWPORT_SPACE_RPANEL].v},
    [VIEWPORT_SPACE_ENTITY_PANEL] = {&viewport_basis_data[VIEWPORT_SPACE_ENTITY_PANEL].u, &viewport_basis_data[VIEWPORT_SPACE_ENTITY_PANEL].v},
};

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

    region->resolution = VectorDiff_2d(region->local_end, region->local_origin);
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

static void ConfigureViewportRegion(ViewportRegion *region, Vector2d local_origin, Vector2d local_end,
                                    Vector2d basis_u, Vector2d basis_v,
                                    float pixels_per_unit, Frame2d *screen_frame)
{
    if (!region)
    {
        return;
    }

    region->local_origin = local_origin;
    region->local_end = local_end;
    FinalizeViewportRegion(region, basis_u, basis_v, pixels_per_unit, screen_frame);
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

void SetViewportTargetLogicalHeight(float logical_height)
{
    target_game_logical_height = ClampFloat(logical_height,
                                            g_viewport_layout_config.min_target_logical_height,
                                            g_viewport_layout_config.max_target_logical_height);
}

void SetViewportUIScaleScalar(int ui_pixels_per_unit_override)
{
    ui_ppu_override = ui_pixels_per_unit_override;
}

void SetViewportPanelRatios(float left_panel_ratio, float right_panel_ratio)
{
    float left = ClampFloat(left_panel_ratio,
                            g_viewport_layout_config.min_panel_ratio,
                            g_viewport_layout_config.max_panel_ratio);
    float right = ClampFloat(right_panel_ratio,
                             g_viewport_layout_config.min_panel_ratio,
                             g_viewport_layout_config.max_panel_ratio);

    // Keep enough width for the world region.
    if ((left + right) > g_viewport_layout_config.max_combined_panel_ratio)
    {
        float scale = g_viewport_layout_config.max_combined_panel_ratio / (left + right);
        left *= scale;
        right *= scale;
    }

    left_panel_ratio = left;
    right_panel_ratio = right;
}

bool ViewportRegion_ContainsPixel(const ViewportRegion *region, Vector2d pixel_coords)
{
    if (!region)
    {
        return false;
    }

    // Resolve through the region tunnel so containment follows the active basis.
    Vector2d local_coords = TransformCoordinates(region->tunnel.dest_to_source_mtx, pixel_coords);
    return local_coords.x >= region->frame.local_min.x &&
           local_coords.x <= region->frame.local_max.x &&
           local_coords.y >= region->frame.local_min.y &&
           local_coords.y <= region->frame.local_max.y;
}

bool SetViewportSpaceBasis(ViewportSpaceId space_id, Vector2d basis_u, Vector2d basis_v)
{
    Basis2d normalised_basis;
    if (!Basis2d_NormaliseAndValidate((Basis2d){basis_u, basis_v}, &normalised_basis))
    {
        return false;
    }

    if (space_id == VIEWPORT_SPACE_GAME)
    {
        game_viewport_basis_override_enabled = true;
        game_viewport_basis_override_u = normalised_basis.u;
        game_viewport_basis_override_v = normalised_basis.v;
        return true;
    }

    if (space_id < VIEWPORT_SPACE_LPANEL || space_id >= VIEWPORT_SPACE_COUNT)
    {
        return false;
    }

    const ViewportBasisPair pair = viewport_basis_pairs[(int)space_id];
    if (!pair.u || !pair.v)
    {
        return false;
    }

    *pair.u = normalised_basis.u;
    *pair.v = normalised_basis.v;

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
    if (!debug_grid_enabled)
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
    debug_grid_enabled = !debug_grid_enabled;
}

int IsViewportDebugGridEnabled(void)
{
    return debug_grid_enabled;
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

    // Read the partition literals from the single layout configuration (see ViewportLayoutConfig).
    const ViewportLayoutConfig *cfg = &g_viewport_layout_config;

    int screen_pixels_per_unit = game_pixels_per_unit_override;
    if (screen_pixels_per_unit <= 0)
    {
        screen_pixels_per_unit = (int)roundf((float)screen_height / target_game_logical_height);
        if (screen_pixels_per_unit < 1)
            screen_pixels_per_unit = 1;
    }

    int ui_pixels_per_unit = ui_ppu_override > 0 ? ui_ppu_override : screen_pixels_per_unit;

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

    // LEFT PANEL: origin at the far left, width by ratio, full height (its original placement;
    // the toolbar now lives on the INNER edge of the game viewport, not outside the left panel).
    ConfigureViewportRegion(&lpanel_viewport,
                            ZERO_VECTOR_2D,
                            (Vector2d){floorf(left_panel_ratio * logical_screen.x), logical_screen.y},
                            viewport_basis_data[VIEWPORT_SPACE_LPANEL].u, viewport_basis_data[VIEWPORT_SPACE_LPANEL].v,
                            (float)ui_pixels_per_unit,
                            &screen_frame);

    // RIGHT PANEL: mirror of left panel at the right edge
    ConfigureViewportRegion(&rpanel_viewport,
                            (Vector2d){logical_screen.x - lpanel_viewport.resolution.x, 0.0f},
                            logical_screen,
                            viewport_basis_data[VIEWPORT_SPACE_RPANEL].u, viewport_basis_data[VIEWPORT_SPACE_RPANEL].v,
                            (float)ui_pixels_per_unit,
                            &screen_frame);

    // INFO STRIP: a thin horizontal band across the top of the central column (the gap
    // between the left and right panels). Carved out FIRST so the game viewport below can
    // account for its height and the strip never overlaps the world render region. The band
    // height is clamped so it can never consume the whole central column on very short windows.
    float strip_height = fminf(cfg->info_strip_logical_height, fmaxf(0.0f, logical_screen.y - cfg->band_edge_gap_logical));

    // TOOLBAR BAND: a thin vertical tool strip carved from the LEFT EDGE of the central column,
    // flush against the left panel's right edge and spanning the game viewport's vertical extent
    // (below the info strip, above the bottom entity panel). It hugs the world it operates on,
    // like a classic tool palette, rather than being an outermost band. The info strip and the
    // game viewport below start at the toolbar's right edge so nothing overlaps. Width is clamped
    // so the band can never swallow the whole central column on narrow windows.
    //
    // The toolbar spans from the TOP of the central column (y=0) down to the bottom entity panel,
    // so it also covers the top-left corner square that would otherwise be left uncovered between
    // the info strip (which starts to the toolbar's right) and the toolbar (if it started below
    // the strip). The info strip's left edge then aligns with the game viewport's left edge below
    // it, giving a clean vertical line.
    float entity_panel_height = fmaxf(cfg->min_band_logical, floorf(logical_screen.y * cfg->entity_panel_height_factor));
    float central_left = lpanel_viewport.local_end.x;
    float toolbar_w = fminf(ViewportLayoutConfig_ToolbarLogicalWidth(cfg), fmaxf(0.0f, rpanel_viewport.local_origin.x - central_left - cfg->band_edge_gap_logical));
    ConfigureViewportRegion(&toolbar_viewport,
                            (Vector2d){central_left, 0.0f},
                            (Vector2d){central_left + toolbar_w, logical_screen.y - entity_panel_height},
                            viewport_basis_data[VIEWPORT_SPACE_LPANEL].u, viewport_basis_data[VIEWPORT_SPACE_LPANEL].v,
                            (float)ui_pixels_per_unit,
                            &screen_frame);

    // The central column's content (info strip + game viewport) begins to the RIGHT of the
    // toolbar band.
    float central_content_left = toolbar_viewport.local_end.x;

    // INFO STRIP: thin horizontal band across the top of the central column, starting right of
    // the toolbar so the two never overlap.
    ConfigureViewportRegion(&info_strip_viewport,
                            (Vector2d){central_content_left, 0.0f},
                            (Vector2d){rpanel_viewport.local_origin.x, strip_height},
                            viewport_basis_data[VIEWPORT_SPACE_LPANEL].u, viewport_basis_data[VIEWPORT_SPACE_LPANEL].v,
                            (float)ui_pixels_per_unit,
                            &screen_frame);

    // GAME VIEWPORT: fills the central column right of the toolbar, below the info strip and
    // above the bottom entity panel, so toolbar / strip / world never overlap.
    ConfigureViewportRegion(&game_viewport,
                            (Vector2d){central_content_left, strip_height},
                            (Vector2d){rpanel_viewport.local_origin.x, logical_screen.y - entity_panel_height},
                            resolved_u, resolved_v,
                            (float)screen_pixels_per_unit,
                            &screen_frame);
    game_viewport.local_resolution = game_viewport.resolution;

    // BOTTOM ENTITY PANEL: horizontal panel occupying the area left of the utility panel.
    float utility_width = fminf(lpanel_viewport.resolution.x, logical_screen.x);
    ConfigureViewportRegion(&entity_panel_viewport,
                            (Vector2d){0.0f, game_viewport.local_end.y},
                            (Vector2d){logical_screen.x - utility_width, logical_screen.y},
                            viewport_basis_data[VIEWPORT_SPACE_ENTITY_PANEL].u, viewport_basis_data[VIEWPORT_SPACE_ENTITY_PANEL].v,
                            (float)ui_pixels_per_unit,
                            &screen_frame);

    // UTILITY PANEL: rightmost section of the bottom panel band.
    ConfigureViewportRegion(&utility_panel_viewport,
                            (Vector2d){logical_screen.x - utility_width, game_viewport.local_end.y},
                            logical_screen,
                            viewport_basis_data[VIEWPORT_SPACE_ENTITY_PANEL].u, viewport_basis_data[VIEWPORT_SPACE_ENTITY_PANEL].v,
                            (float)ui_pixels_per_unit,
                            &screen_frame);

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
           screen_pixels_per_unit, ui_pixels_per_unit, target_game_logical_height);
}
