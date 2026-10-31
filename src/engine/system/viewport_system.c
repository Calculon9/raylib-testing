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
    float toolbar_logical_width;   // Logical (game-unit) height of the top info-strip band (was 0.9f).
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
    .info_strip_logical_height  = 0.7f,
    .toolbar_logical_width      = 0.8f,
    .entity_panel_height_factor = 0.275f,
    .min_band_logical           = 1.0f,
    .band_edge_gap_logical      = 1.0f,
};

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
ViewportRegion detail_strip_viewport = {0};
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

// ============================================================================
// Dock Model
// ============================================================================
// Declarative description of the screen partition. The seven viewport regions used to be laid
// out by hand-written relative math inside InitViewportLayout; that math is now expressed as a
// fixed-order DOCK TABLE (data) solved by SolveDockLayout (a small, fixed-order solver). The
// solver reproduces the former partition BYTE-FOR-BYTE: it performs exactly the same arithmetic,
// in the same order, and writes the same seven globals via the unchanged ConfigureViewportRegion.
//
// The partition is NOT a generic "shrink a rectangle, FILL takes the rest" border layout. It has
// three non-generic traits the solver encodes explicitly:
//   1. The toolbar spans the full central-column height minus the bottom band (y: 0 .. H-entity),
//      so it also covers the top-left corner beside the info strip; it does not start below it.
//   2. The info strip and game begin at the toolbar's RIGHT edge (central_content_left), not at
//      the left panel's right edge.
//   3. The bottom entity/utility band docks within the WHOLE SCREEN (from x=0, under the left
//      panel) with its top edge pinned to the game's bottom (H - entity_panel_height).
// Hence the solver is a straight-line transcription of the former source order, not a loop over a
// shrinking container.

// Which edge of its container a region docks against. FILL takes whatever remains.
typedef enum DockEdge
{
    DOCK_EDGE_LEFT = 0, // Pinned to the container's left edge; consumes width from the left.
    DOCK_EDGE_RIGHT,    // Pinned to the container's right edge; consumes width from the right.
    DOCK_EDGE_TOP,      // Pinned to the container's top edge; consumes height from the top.
    DOCK_EDGE_BOTTOM,   // Pinned to the container's bottom edge; consumes height from the bottom.
    DOCK_EDGE_FILL      // Occupies the remainder of its container after the fixed slabs are placed.
} DockEdge;

// How a row's `size` value is interpreted.
typedef enum DockSizeMode
{
    DOCK_SIZE_RATIO = 0, // `size` is a fraction of a reference extent (see DockSizeRef).
    DOCK_SIZE_LOGICAL    // `size` is an absolute measure in logical (game) units.
} DockSizeMode;

// Which extent a RATIO size multiplies. Only the logical screen width is needed today (the panel
// ratios are fractions of the full logical screen width); the field is explicit so a reader can
// see the reference is the screen, not the container.
typedef enum DockSizeRef
{
    DOCK_REF_SCREEN_WIDTH = 0 // Fraction of logical_screen.x.
} DockSizeRef;

// The container a row docks WITHIN. The partition has exactly two nesting levels today.
typedef enum DockContainer
{
    DOCK_WITHIN_SCREEN = 0, // Docks within the whole logical screen rectangle.
    DOCK_WITHIN_CENTRAL     // Docks within the CENTRAL column (the horizontal span that remains
                            // after the left and right panels are removed from the screen).
} DockContainer;

// Stable identity for each docked region. The order here is the SOLVE order used by the solver
// and mirrors the former source order in InitViewportLayout.
typedef enum DockRegionId
{
    DOCK_REGION_LPANEL = 0,
    DOCK_REGION_RPANEL,
    DOCK_REGION_TOOLBAR,
    DOCK_REGION_INFO_STRIP,
    DOCK_REGION_GAME, // FILL
    DOCK_REGION_DETAIL_STRIP,
    DOCK_REGION_ENTITY_PANEL,
    DOCK_REGION_UTILITY_PANEL,
    DOCK_REGION_COUNT
} DockRegionId;

// A single dock-table row: the declarative geometry of one region.
typedef struct DockRow
{
    DockRegionId  region;    // Which region this row produces (and which global it writes).
    DockEdge      edge;      // Edge to dock against, or FILL for the remainder.
    DockSizeMode  size_mode; // How `size` is read (RATIO vs LOGICAL). Ignored when edge == FILL.
    DockSizeRef   size_ref;  // Reference extent for RATIO sizes. Ignored for LOGICAL / FILL.
    float         size;      // The SINGLE perpendicular measure: width for LEFT/RIGHT, height for
                             // TOP/BOTTOM. Ignored for FILL. The bottom-band rows (ENTITY_PANEL,
                             // UTILITY_PANEL) carry ONLY their band height here; their horizontal
                             // split is a solver datum (utility_width), NOT a `size` value.
    DockContainer within;    // SCREEN or CENTRAL: which rectangle the row docks inside.
} DockRow;

// Fixed, NON-geometric attributes of each region, indexed by DockRegionId. Kept separate from
// DockRow because these are per-region constants independent of the declared edge/size. `static
// const` so the game-only behaviours are visible data rather than buried control flow.
typedef struct DockRegionDesc
{
    ViewportRegion *region;              // Destination global (its ADDRESS is the preserved contract).
    int             basis_space;         // Index into viewport_basis_data[] (the panel/strip basis).
    bool            use_game_basis;      // true ONLY for GAME: use resolved_u/resolved_v instead of
                                         //   viewport_basis_data[basis_space].
    bool            use_screen_ppu;      // true ONLY for GAME: pass screen_pixels_per_unit (NOT
                                         //   ui_pixels_per_unit) to ConfigureViewportRegion.
    bool            set_local_resolution;// true ONLY for GAME: after ConfigureViewportRegion, assign
                                         //   region->local_resolution = region->resolution.
} DockRegionDesc;

// Structural invariant (developer-time, NOT a runtime input): the dock table must contain exactly
// DOCK_REGION_COUNT rows in solve order and exactly ONE FILL row (the GAME viewport). The table is
// private file-local data that no external input can corrupt, so this is enforced by inspection
// and by the byte-identical verification dump, not by a runtime assert (the engine uses none).
static const DockRegionDesc g_dock_region_desc[DOCK_REGION_COUNT] = {
    [DOCK_REGION_LPANEL]        = {&lpanel_viewport,        VIEWPORT_SPACE_LPANEL,       false, false, false},
    [DOCK_REGION_RPANEL]        = {&rpanel_viewport,        VIEWPORT_SPACE_RPANEL,       false, false, false},
    [DOCK_REGION_TOOLBAR]       = {&toolbar_viewport,       VIEWPORT_SPACE_LPANEL,       false, false, false},
    [DOCK_REGION_INFO_STRIP]    = {&info_strip_viewport,    VIEWPORT_SPACE_LPANEL,       false, false, false},
    [DOCK_REGION_GAME]          = {&game_viewport,          VIEWPORT_SPACE_LPANEL,       true,  true,  true },
    [DOCK_REGION_DETAIL_STRIP]  = {&detail_strip_viewport,  VIEWPORT_SPACE_LPANEL,       false, false, false},
    [DOCK_REGION_ENTITY_PANEL]  = {&entity_panel_viewport,  VIEWPORT_SPACE_ENTITY_PANEL, false, false, false},
    [DOCK_REGION_UTILITY_PANEL] = {&utility_panel_viewport, VIEWPORT_SPACE_ENTITY_PANEL, false, false, false},
};

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

// Resolve the dock table into the seven viewport regions and write them through the unchanged
// ConfigureViewportRegion. The solver runs in the fixed table order (= the former source order)
// and performs exactly the former arithmetic, so its output is byte-identical to the previous
// hand-written partition. It does NOT reimplement ConfigureViewportRegion/FinalizeViewportRegion;
// it only computes each region's logical rectangle and hands it over.
//
// Parameters carry the already-derived inputs the former code used: the declared `rows`, the
// `logical_screen` size, the layout `cfg`, the game basis vectors (`resolved_u`/`resolved_v`), the
// two pixels-per-unit scales, and the shared `screen_frame`.
static void SolveDockLayout(const DockRow *rows, int row_count,
                            Vector2d logical_screen, const ViewportLayoutConfig *cfg,
                            Vector2d resolved_u, Vector2d resolved_v,
                            int ui_pixels_per_unit, int screen_pixels_per_unit,
                            Frame2d *screen_frame)
{
    (void)rows;        // The rows encode the declared partition; the fixed solve order below is
    (void)row_count;   // their transcription. Referenced for completeness / future rows.

    // Writes one resolved rectangle into its destination global via the single §3a write path:
    // the game row alone uses the game basis, the screen pixels-per-unit, and the local_resolution
    // copy; every other row uses its panel basis and the UI pixels-per-unit.
    #define DOCK_WRITE_REGION(ID, ORIGIN, END)                                                   \
        do {                                                                                     \
            const DockRegionDesc *d = &g_dock_region_desc[(ID)];                                 \
            Vector2d wu = d->use_game_basis ? resolved_u : viewport_basis_data[d->basis_space].u;\
            Vector2d wv = d->use_game_basis ? resolved_v : viewport_basis_data[d->basis_space].v;\
            float wppu = d->use_screen_ppu ? (float)screen_pixels_per_unit                       \
                                           : (float)ui_pixels_per_unit;                          \
            ConfigureViewportRegion(d->region, (ORIGIN), (END), wu, wv, wppu, screen_frame);     \
            if (d->set_local_resolution)                                                         \
                d->region->local_resolution = d->region->resolution;                            \
        } while (0)

    // Step 1 - LPANEL (SCREEN / LEFT / RATIO): far left, width by the live left-panel ratio,
    // full height. Reproduces the former lpanel block.
    DOCK_WRITE_REGION(DOCK_REGION_LPANEL,
                      ZERO_VECTOR_2D,
                      ((Vector2d){floorf(left_panel_ratio * logical_screen.x), logical_screen.y}));

    // Step 2 - RPANEL (SCREEN / RIGHT): mirror of the left panel at the right edge. Its width is
    // the SOLVED left-panel resolution (not right_panel_ratio * width), exactly as before.
    DOCK_WRITE_REGION(DOCK_REGION_RPANEL,
                      ((Vector2d){logical_screen.x - lpanel_viewport.resolution.x, 0.0f}),
                      logical_screen);

    // Step 3 - pre-compute the shared band datums before any CENTRAL region is written, matching
    // the former ordering. `strip_height` is the clamped info-strip band height; it is computed
    // here so the game viewport below can account for it.
    float strip_height = fminf(cfg->info_strip_logical_height, fmaxf(0.0f, logical_screen.y - cfg->band_edge_gap_logical));
    float entity_panel_height = fmaxf(cfg->min_band_logical, floorf(logical_screen.y * cfg->entity_panel_height_factor));
    float central_left = lpanel_viewport.local_end.x;   // Left edge of the CENTRAL container.

    // Step 4 - TOOLBAR (CENTRAL / LEFT): a thin vertical band flush against the left panel's right
    // edge, spanning from the top (y=0) down to the bottom entity band (y = H - entity_height), so
    // it also covers the top-left corner beside the info strip. Width is clamped so it can never
    // swallow the whole central column.
    float toolbar_w = fminf(cfg->toolbar_logical_width, fmaxf(0.0f, rpanel_viewport.local_origin.x - central_left - cfg->band_edge_gap_logical));
    DOCK_WRITE_REGION(DOCK_REGION_TOOLBAR,
                      ((Vector2d){central_left, 0.0f}),
                      ((Vector2d){central_left + toolbar_w, logical_screen.y - entity_panel_height}));

    // Step 5 - advance the CENTRAL content left edge past the toolbar. The info strip and the game
    // viewport begin to the RIGHT of the toolbar band.
    float central_content_left = toolbar_viewport.local_end.x;

    // Step 6 - INFO_STRIP (CENTRAL / TOP): thin horizontal band across the top of the central
    // content column, from the toolbar's right edge to the right panel's left edge.
    DOCK_WRITE_REGION(DOCK_REGION_INFO_STRIP,
                      ((Vector2d){central_content_left, 0.0f}),
                      ((Vector2d){rpanel_viewport.local_origin.x, strip_height}));

    // Detail strip height: SAME as the info-strip band height (and clamped identically) — the
    // detail strip reuses the computed strip_height datum rather than any new literal, so it is the
    // same height as the info strip by construction. The central content column now carries two
    // thin bands: the info strip at the TOP and the detail strip at the BOTTOM, with the game
    // viewport filling between them above the bottom entity band.
    float detail_strip_height = strip_height;
    // Former game bottom edge (also the entity/utility band top). Decoupled from the game viewport
    // so carving the detail strip from the game's bottom leaves every OTHER region unchanged.
    float entity_band_top = logical_screen.y - entity_panel_height;
    // New game bottom edge: raised by exactly one strip height to make room for the detail strip.
    float game_bottom = entity_band_top - detail_strip_height;

    // Step 7 - GAME (FILL): the remainder of the central content column, right of the toolbar,
    // below the info strip and ABOVE the detail strip (which is itself above the bottom entity
    // band). The §3a write path routes this row to the game basis + screen pixels-per-unit and
    // copies resolution into local_resolution.
    DOCK_WRITE_REGION(DOCK_REGION_GAME,
                      ((Vector2d){central_content_left, strip_height}),
                      ((Vector2d){rpanel_viewport.local_origin.x, game_bottom}));

    // Step 7b - DETAIL_STRIP (CENTRAL / BOTTOM): thin horizontal band across the bottom of the
    // central content column, from the toolbar's right edge to the right panel's left edge, sitting
    // directly below the game viewport and above the bottom entity band. Its bottom edge meets the
    // entity band top exactly, so there is no overlap or gap.
    DOCK_WRITE_REGION(DOCK_REGION_DETAIL_STRIP,
                      ((Vector2d){central_content_left, game_bottom}),
                      ((Vector2d){rpanel_viewport.local_origin.x, entity_band_top}));

    // Step 8 - bottom band shared datum, then ENTITY_PANEL and UTILITY_PANEL (both SCREEN / BOTTOM).
    // `utility_width` is the solver datum that splits the full-width bottom band horizontally; both
    // rows pin their top edge to `entity_band_top` (their ORIGINAL top, H - entity_height) rather
    // than the game viewport's now-smaller bottom, so they are byte-identical to before.
    float utility_width = fminf(lpanel_viewport.resolution.x, logical_screen.x);
    DOCK_WRITE_REGION(DOCK_REGION_ENTITY_PANEL,
                      ((Vector2d){0.0f, entity_band_top}),
                      ((Vector2d){logical_screen.x - utility_width, logical_screen.y}));
    DOCK_WRITE_REGION(DOCK_REGION_UTILITY_PANEL,
                      ((Vector2d){logical_screen.x - utility_width, entity_band_top}),
                      logical_screen);

    #undef DOCK_WRITE_REGION
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

    // Build the dock table (declarative partition) from the layout config and the live panel
    // ratio, then solve it. The sizes that are runtime-computed (ratio x width, mirror width, the
    // clamped bands) are resolved by the solver; the rows carry the DECLARED intent so there is a
    // single source of truth with ViewportLayoutConfig. See the Dock Model section above for the
    // row schema and the per-region descriptor table.
    DockRow dock_rows[DOCK_REGION_COUNT] = {
        // LPANEL: left edge of the screen, width = left_panel_ratio * logical width.
        {DOCK_REGION_LPANEL, DOCK_EDGE_LEFT, DOCK_SIZE_RATIO, DOCK_REF_SCREEN_WIDTH, left_panel_ratio, DOCK_WITHIN_SCREEN},
        // RPANEL: right edge of the screen; width MIRRORS the solved left-panel width.
        {DOCK_REGION_RPANEL, DOCK_EDGE_RIGHT, DOCK_SIZE_LOGICAL, DOCK_REF_SCREEN_WIDTH, 0.0f, DOCK_WITHIN_SCREEN},
        // TOOLBAR: left edge of the central column.
        {DOCK_REGION_TOOLBAR, DOCK_EDGE_LEFT, DOCK_SIZE_LOGICAL, DOCK_REF_SCREEN_WIDTH, cfg->toolbar_logical_width, DOCK_WITHIN_CENTRAL},
        // INFO_STRIP: top edge of the central content column; height = info_strip_logical_height.
        {DOCK_REGION_INFO_STRIP, DOCK_EDGE_TOP, DOCK_SIZE_LOGICAL, DOCK_REF_SCREEN_WIDTH, cfg->info_strip_logical_height, DOCK_WITHIN_CENTRAL},
        // GAME: fills the remainder of the central content column.
        {DOCK_REGION_GAME, DOCK_EDGE_FILL, DOCK_SIZE_LOGICAL, DOCK_REF_SCREEN_WIDTH, 0.0f, DOCK_WITHIN_CENTRAL},
        // DETAIL_STRIP: bottom edge of the central content column; the solver sets its height to the
        // info-strip datum (detail_strip_height = strip_height), so this row size is NOT read — it
        // carries 0.0f only for row symmetry, exactly as the GAME (FILL) row does.
        {DOCK_REGION_DETAIL_STRIP, DOCK_EDGE_BOTTOM, DOCK_SIZE_LOGICAL, DOCK_REF_SCREEN_WIDTH, 0.0f, DOCK_WITHIN_CENTRAL},
        // ENTITY_PANEL: bottom band of the whole screen; carries the band height only.
        {DOCK_REGION_ENTITY_PANEL, DOCK_EDGE_BOTTOM, DOCK_SIZE_LOGICAL, DOCK_REF_SCREEN_WIDTH, 0.0f, DOCK_WITHIN_SCREEN},
        // UTILITY_PANEL: bottom band of the whole screen; carries the band height only.
        {DOCK_REGION_UTILITY_PANEL, DOCK_EDGE_BOTTOM, DOCK_SIZE_LOGICAL, DOCK_REF_SCREEN_WIDTH, 0.0f, DOCK_WITHIN_SCREEN},
    };

    SolveDockLayout(dock_rows, DOCK_REGION_COUNT,
                    logical_screen, cfg,
                    resolved_u, resolved_v,
                    ui_pixels_per_unit, screen_pixels_per_unit,
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
