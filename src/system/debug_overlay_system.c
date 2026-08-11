#include "system/debug_overlay_system.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#include "raylib.h"
#include "camera/camera.h"
#include "system/ui_system.h"
#include "world/world.h"
#include "world/universe.h"
#include "system/viewport_system.h"
#include "system/ui/lpanel_system.h"
#include "system/ui/rpanel_system.h"
#include "ui/cfont.h"
#include "ui/text_region.h"

typedef enum DebugBasisTargetId
{
    DEBUG_BASIS_TARGET_LPANEL_VIEWPORT = 0,
    DEBUG_BASIS_TARGET_GAME_VIEWPORT,
    DEBUG_BASIS_TARGET_RPANEL_VIEWPORT,
    DEBUG_BASIS_TARGET_UNIVERSE_SPACE,
    DEBUG_BASIS_TARGET_LPANEL_SPACE,
    DEBUG_BASIS_TARGET_RPANEL_SPACE,
    DEBUG_BASIS_TARGET_COUNT
} DebugBasisTargetId;

typedef struct DebugBasisTargetOps
{
    Frame2d *(*get_frame_fn)(void);
    bool (*apply_basis_fn)(Basis2d basis);
    void (*reset_fn)(void);
    const char *name;
} DebugBasisTargetOps;

static Frame2d *GetLPanelViewportFrame(void) { return &lpanel_viewport.frame; }
static Frame2d *GetGameViewportFrame(void) { return &game_viewport.frame; }
static Frame2d *GetRPanelViewportFrame(void) { return &rpanel_viewport.frame; }
static Frame2d *GetUniverseSpaceFrame(void) { return &G_Universe.camera.frame; }
static Frame2d *GetLPanelSpaceFrameTarget(void) { return GetLPanelSpaceFrame(); }
static Frame2d *GetRPanelSpaceFrameTarget(void) { return GetRPanelSpaceFrame(); }

static bool ApplyLPanelViewportBasis(Basis2d basis) { return SetViewportSpaceBasis(VIEWPORT_SPACE_LPANEL, basis.u, basis.v); }
static bool ApplyGameViewportBasis(Basis2d basis) { return SetViewportSpaceBasis(VIEWPORT_SPACE_GAME, basis.u, basis.v); }
static bool ApplyRPanelViewportBasis(Basis2d basis) { return SetViewportSpaceBasis(VIEWPORT_SPACE_RPANEL, basis.u, basis.v); }
static bool ApplyUniverseSpaceBasis(Basis2d basis)
{
    SetUniverseCameraBasis(basis);
    return true;
}
static bool ApplyLPanelSpaceBasis(Basis2d basis) { return SetLPanelSpaceBasis(basis.u, basis.v); }
static bool ApplyRPanelSpaceBasis(Basis2d basis) { return SetRPanelSpaceBasis(basis.u, basis.v); }

static void ResetLPanelViewportBasis(void) { ResetViewportSpaceBasis(VIEWPORT_SPACE_LPANEL); }
static void ResetGameViewportBasis(void) { ResetViewportSpaceBasis(VIEWPORT_SPACE_GAME); }
static void ResetRPanelViewportBasis(void) { ResetViewportSpaceBasis(VIEWPORT_SPACE_RPANEL); }
static void ResetUniverseSpaceBasis(void) { SetUniverseCameraBasis(IDENTITY_BASIS_2D); }
static void ResetLPanelSpaceBasisTarget(void) { ResetLPanelSpaceBasis(); }
static void ResetRPanelSpaceBasisTarget(void) { ResetRPanelSpaceBasis(); }

static const DebugBasisTargetOps debug_basis_target_ops[DEBUG_BASIS_TARGET_COUNT] = {
    [DEBUG_BASIS_TARGET_LPANEL_VIEWPORT] = {
        .get_frame_fn = GetLPanelViewportFrame,
        .apply_basis_fn = ApplyLPanelViewportBasis,
        .reset_fn = ResetLPanelViewportBasis,
        .name = "LPANEL_VIEWPORT",
    },
    [DEBUG_BASIS_TARGET_GAME_VIEWPORT] = {
        .get_frame_fn = GetGameViewportFrame,
        .apply_basis_fn = ApplyGameViewportBasis,
        .reset_fn = ResetGameViewportBasis,
        .name = "GAME_VIEWPORT",
    },
    [DEBUG_BASIS_TARGET_RPANEL_VIEWPORT] = {
        .get_frame_fn = GetRPanelViewportFrame,
        .apply_basis_fn = ApplyRPanelViewportBasis,
        .reset_fn = ResetRPanelViewportBasis,
        .name = "RPANEL_VIEWPORT",
    },
    [DEBUG_BASIS_TARGET_UNIVERSE_SPACE] = {
        .get_frame_fn = GetUniverseSpaceFrame,
        .apply_basis_fn = ApplyUniverseSpaceBasis,
        .reset_fn = ResetUniverseSpaceBasis,
        .name = "UNIVERSE_SPACE",
    },
    [DEBUG_BASIS_TARGET_LPANEL_SPACE] = {
        .get_frame_fn = GetLPanelSpaceFrameTarget,
        .apply_basis_fn = ApplyLPanelSpaceBasis,
        .reset_fn = ResetLPanelSpaceBasisTarget,
        .name = "LPANEL_SPACE",
    },
    [DEBUG_BASIS_TARGET_RPANEL_SPACE] = {
        .get_frame_fn = GetRPanelSpaceFrameTarget,
        .apply_basis_fn = ApplyRPanelSpaceBasis,
        .reset_fn = ResetRPanelSpaceBasisTarget,
        .name = "RPANEL_SPACE",
    },
};

static const DebugBasisTargetOps *GetDebugBasisTargetOps(DebugBasisTargetId target_id)
{
    if (target_id < DEBUG_BASIS_TARGET_LPANEL_VIEWPORT || target_id >= DEBUG_BASIS_TARGET_COUNT)
    {
        return NULL;
    }

    return &debug_basis_target_ops[(int)target_id];
}

static Frame2d *GetDebugBasisTargetFrame(DebugBasisTargetId target_id)
{
    const DebugBasisTargetOps *ops = GetDebugBasisTargetOps(target_id);
    if (!ops || !ops->get_frame_fn)
    {
        return NULL;
    }

    return ops->get_frame_fn();
}

static bool ApplyDebugBasisTarget(DebugBasisTargetId target_id, const Frame2d *frame)
{
    if (!frame)
    {
        return false;
    }

    const DebugBasisTargetOps *ops = GetDebugBasisTargetOps(target_id);
    if (!ops || !ops->apply_basis_fn)
    {
        return false;
    }

    return ops->apply_basis_fn(frame->basis);
}

static void ResetDebugBasisTarget(DebugBasisTargetId target_id)
{
    const DebugBasisTargetOps *ops = GetDebugBasisTargetOps(target_id);
    if (!ops || !ops->reset_fn)
    {
        return;
    }

    ops->reset_fn();
}

typedef struct UniverseDebugSnapshot
{
    bool valid;
    bool cursor_in_game_viewport;
    bool has_child_local;
    Vector2d pixel;
    Vector2d parent_local;
    Vector2d child_origin_in_parent;
    Vector2d child_local;
    Vector2d viewport_pixel_center;
    Vector2d viewport_local_center;
    float viewport_ppu_u;
    float viewport_ppu_v;
    int selected_index;
    int hovered_world_index;
    int target_world_index;
    int world_cell_index;
    float camera_zoom;
    float camera_rotation;
    Vector2d camera_focus;
} UniverseDebugSnapshot;

static bool dashboard_overlay_enabled = false;
static bool grid_labels_overlay_enabled = false;
static bool world_grid_overlay_enabled = true;
static bool basis_editor_enabled = false;
static bool basis_editor_editing_u = true;
static DebugBasisTargetId basis_editor_target = DEBUG_BASIS_TARGET_LPANEL_VIEWPORT;
static UniverseDebugSnapshot debug_snapshot = {0};

static void RefreshViewportBase(int screen_width, int screen_height, int screen_resolution_scalar,
                               float viewport_target_game_logical_height,
                               int viewport_ui_pixels_per_unit_override)
{
    SetViewportTargetLogicalHeight(viewport_target_game_logical_height);
    SetViewportUIScaleScalar(viewport_ui_pixels_per_unit_override);
    InitViewportLayout(screen_width, screen_height, screen_resolution_scalar);
    SyncUniverseCameraToViewport();
}

static float ClampViewportLogicalHeight(float height)
{
    if (height < 8.0f)
    {
        return 8.0f;
    }
    if (height > 400.0f)
    {
        return 400.0f;
    }

    return height;
}

static void RefreshViewportForBasisEdit(int screen_width, int screen_height, int screen_resolution_scalar,
                                        float viewport_target_game_logical_height,
                                        int viewport_ui_pixels_per_unit_override)
{
    RefreshViewportBase(screen_width, screen_height, screen_resolution_scalar,
                        viewport_target_game_logical_height,
                        viewport_ui_pixels_per_unit_override);
    InitUI();
}

static void RefreshViewportAndDependentSystems(int screen_width, int screen_height, int screen_resolution_scalar,
                                               float viewport_target_game_logical_height, int viewport_ui_pixels_per_unit_override,
                                               bool viewport_scale_changed,  bool ui_scale_changed)
{
    RefreshViewportBase(screen_width, screen_height, screen_resolution_scalar,
                        viewport_target_game_logical_height,
                        viewport_ui_pixels_per_unit_override);

    if (viewport_scale_changed)
    {
        InitWorldSystem();
        InitUI();
        printf("[Viewport] Target game logical height set to %.1f. UI px-per-unit override: %d\n",
               viewport_target_game_logical_height, viewport_ui_pixels_per_unit_override);
        return;
    }

    if (ui_scale_changed)
    {
        InitUI();
        printf("[Viewport] UI px-per-unit override set to %d (0 = follow game viewport scale).\n",
               viewport_ui_pixels_per_unit_override);
    }
}

static const char *GetDebugBasisTargetName(DebugBasisTargetId target_id)
{
    const DebugBasisTargetOps *ops = GetDebugBasisTargetOps(target_id);
    if (!ops || !ops->name)
    {
        return "UNKNOWN";
    }

    return ops->name;
}

static const char *GetOnOffLabel(int enabled)
{
    return enabled ? "ON" : "OFF";
}

static void DrawDashboardLine(const char *text, int x, int y, ColourRgba color)
{
    DrawTextCustom(text, (Vector2d){(float)x, (float)y}, FONT_BASIC.scale, FONT_BASIC, color);
}

typedef struct DashboardBasisData
{
    Vector2d lpanel_viewport_basis_u;
    Vector2d lpanel_viewport_basis_v;
    Vector2d game_viewport_basis_u;
    Vector2d game_viewport_basis_v;
    Vector2d rpanel_viewport_basis_u;
    Vector2d rpanel_viewport_basis_v;
    Vector2d lpanel_space_basis_u;
    Vector2d lpanel_space_basis_v;
    Vector2d rpanel_space_basis_u;
    Vector2d rpanel_space_basis_v;
} DashboardBasisData;

static DashboardBasisData ResolveDashboardBasisData(void)
{
    DashboardBasisData data = {0};

    Frame2d *lpanel_viewport_frame = GetDebugBasisTargetFrame(DEBUG_BASIS_TARGET_LPANEL_VIEWPORT);
    Frame2d *game_viewport_frame = GetDebugBasisTargetFrame(DEBUG_BASIS_TARGET_GAME_VIEWPORT);
    Frame2d *rpanel_viewport_frame = GetDebugBasisTargetFrame(DEBUG_BASIS_TARGET_RPANEL_VIEWPORT);
    Frame2d *lpanel_space_frame = GetDebugBasisTargetFrame(DEBUG_BASIS_TARGET_LPANEL_SPACE);
    Frame2d *rpanel_space_frame = GetDebugBasisTargetFrame(DEBUG_BASIS_TARGET_RPANEL_SPACE);

    data.lpanel_viewport_basis_u = lpanel_viewport_frame ? lpanel_viewport_frame->basis.u : ZERO_VECTOR_2D;
    data.lpanel_viewport_basis_v = lpanel_viewport_frame ? lpanel_viewport_frame->basis.v : ZERO_VECTOR_2D;
    data.game_viewport_basis_u = game_viewport_frame ? game_viewport_frame->basis.u : ZERO_VECTOR_2D;
    data.game_viewport_basis_v = game_viewport_frame ? game_viewport_frame->basis.v : ZERO_VECTOR_2D;
    data.rpanel_viewport_basis_u = rpanel_viewport_frame ? rpanel_viewport_frame->basis.u : ZERO_VECTOR_2D;
    data.rpanel_viewport_basis_v = rpanel_viewport_frame ? rpanel_viewport_frame->basis.v : ZERO_VECTOR_2D;
    data.lpanel_space_basis_u = lpanel_space_frame ? lpanel_space_frame->basis.u : ZERO_VECTOR_2D;
    data.lpanel_space_basis_v = lpanel_space_frame ? lpanel_space_frame->basis.v : ZERO_VECTOR_2D;
    data.rpanel_space_basis_u = rpanel_space_frame ? rpanel_space_frame->basis.u : ZERO_VECTOR_2D;
    data.rpanel_space_basis_v = rpanel_space_frame ? rpanel_space_frame->basis.v : ZERO_VECTOR_2D;

    return data;
}

static void DrawDashboardControlsSection(int col_x, int *row_y, int row_step,
                                         ColourRgba accent_color, ColourRgba body_color,
                                         char *line, size_t line_size)
{
    if (!row_y || !line)
    {
        return;
    }

    DrawDashboardLine("Controls", col_x, *row_y, accent_color);
    *row_y += row_step;

    snprintf(line, line_size, "F11 Dashboard: %s", GetOnOffLabel(dashboard_overlay_enabled));
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "F5 Basis editor: %s", GetOnOffLabel(basis_editor_enabled));
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "F6 Viewport grid: %s", GetOnOffLabel(IsDebugEnabled(DEBUG_VIEWPORT_GRID)));
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "F2 World grid labels: %s", GetOnOffLabel(IsDebugEnabled(DEBUG_WORLD_GRID_LABELS)));
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "F3 UI borders: %s", GetOnOffLabel(IsDebugEnabled(DEBUG_UI_BORDERS)));
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "F4 Universe grid labels: %s", GetOnOffLabel(IsDebugEnabled(DEBUG_UNIVERSE_GRID_LABELS)));
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    DrawDashboardLine("F7/F8 Logical height   F9/F10 UI scale", col_x, *row_y, body_color);
    *row_y += row_step;
    DrawDashboardLine("TAB Space   U/V Vector   I/J/K/L Nudge", col_x, *row_y, body_color);
    *row_y += row_step;
    DrawDashboardLine("O/P Scale   BACKSPACE Reset", col_x, *row_y, body_color);
    *row_y += row_step * 2;
}

static void DrawDashboardUniverseSection(int col_x, int *row_y, int row_step,
                                         ColourRgba accent_color, ColourRgba body_color,
                                         char *line, size_t line_size)
{
    if (!row_y || !line)
    {
        return;
    }

    DrawDashboardLine("Universe", col_x, *row_y, accent_color);
    *row_y += row_step;

    if (debug_snapshot.valid)
    {
        snprintf(line, line_size, "Cursor px: (%.1f, %.1f) [%s]", debug_snapshot.pixel.x, debug_snapshot.pixel.y,
                 debug_snapshot.cursor_in_game_viewport ? "in viewport" : "outside viewport");
        DrawDashboardLine(line, col_x, *row_y, body_color);
        *row_y += row_step;

        snprintf(line, line_size, "Universe coords: (%.3f, %.3f)", debug_snapshot.parent_local.x, debug_snapshot.parent_local.y);
        DrawDashboardLine(line, col_x, *row_y, body_color);
        *row_y += row_step;

        snprintf(line, line_size, "Selected world: %d   Hovered world: %d", debug_snapshot.selected_index, debug_snapshot.hovered_world_index);
        DrawDashboardLine(line, col_x, *row_y, body_color);
        *row_y += row_step;

        if (debug_snapshot.has_child_local)
        {
            snprintf(line, line_size, "Child local[%d]: (%.3f, %.3f)", debug_snapshot.target_world_index,
                     debug_snapshot.child_local.x, debug_snapshot.child_local.y);
            DrawDashboardLine(line, col_x, *row_y, body_color);
            *row_y += row_step;

            snprintf(line, line_size, "Child origin: (%.3f, %.3f)", debug_snapshot.child_origin_in_parent.x,
                     debug_snapshot.child_origin_in_parent.y);
            DrawDashboardLine(line, col_x, *row_y, body_color);
            *row_y += row_step;

            snprintf(line, line_size, "Child cell index: %d", debug_snapshot.world_cell_index);
            DrawDashboardLine(line, col_x, *row_y, body_color);
            *row_y += row_step;
        }
        else
        {
            DrawDashboardLine("Child local: n/a", col_x, *row_y, body_color);
            *row_y += row_step;
        }

        snprintf(line, line_size, "Camera focus(%.2f, %.2f) zoom=%.3f rot=%.3f",
                 debug_snapshot.camera_focus.x, debug_snapshot.camera_focus.y,
                 debug_snapshot.camera_zoom, debug_snapshot.camera_rotation);
        DrawDashboardLine(line, col_x, *row_y, body_color);
    }
    else
    {
        DrawDashboardLine("Universe diagnostics unavailable for current frame.", col_x, *row_y, body_color);
    }
}

static void DrawDashboardViewportBasisSection(int col_x, int *row_y, int row_step,
                                              ColourRgba accent_color, ColourRgba body_color,
                                              char *line, size_t line_size,
                                              const DashboardBasisData *basis)
{
    if (!row_y || !line || !basis)
    {
        return;
    }

    DrawDashboardLine("Game Viewport", col_x, *row_y, accent_color);
    *row_y += row_step;

    snprintf(line, line_size, "Global Viewport bounds: (%.1f, %.1f) -> (%.1f, %.1f)",
             game_viewport.local_origin.x, game_viewport.local_origin.y,
             game_viewport.local_end.x, game_viewport.local_end.y);
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "Pixel bounds: (%.1f, %.1f) -> (%.1f, %.1f)",
             game_viewport.pixel_origin.x, game_viewport.pixel_origin.y,
             game_viewport.pixel_end.x, game_viewport.pixel_end.y);
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "Region center local(%.2f, %.2f) pixel(%.1f, %.1f)",
             debug_snapshot.viewport_local_center.x, debug_snapshot.viewport_local_center.y,
             debug_snapshot.viewport_pixel_center.x, debug_snapshot.viewport_pixel_center.y);
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "Px/unit U=%.2f V=%.2f", VectorMagnitude_2d(game_viewport.pixel_u), VectorMagnitude_2d(game_viewport.pixel_v));
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "Viewport px: %.0f x %.0f",
             game_viewport.pixel_end.x - game_viewport.pixel_origin.x,
             game_viewport.pixel_end.y - game_viewport.pixel_origin.y);
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;

    DrawDashboardLine("Basis", col_x, *row_y, accent_color);
    *row_y += row_step;

    snprintf(line, line_size, "Editor target: %s.%s", GetDebugBasisTargetName(basis_editor_target), basis_editor_editing_u ? "U" : "V");
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    DrawDashboardLine("Viewport Spaces", col_x, *row_y, accent_color);
    *row_y += row_step;
    snprintf(line, line_size, "LPANEL_VIEWPORT U(%.2f, %.2f) V(%.2f, %.2f)", basis->lpanel_viewport_basis_u.x, basis->lpanel_viewport_basis_u.y, basis->lpanel_viewport_basis_v.x, basis->lpanel_viewport_basis_v.y);
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "GAME_VIEWPORT   U(%.2f, %.2f) V(%.2f, %.2f)", basis->game_viewport_basis_u.x, basis->game_viewport_basis_u.y, basis->game_viewport_basis_v.x, basis->game_viewport_basis_v.y);
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "RPANEL_VIEWPORT U(%.2f, %.2f) V(%.2f, %.2f)", basis->rpanel_viewport_basis_u.x, basis->rpanel_viewport_basis_u.y, basis->rpanel_viewport_basis_v.x, basis->rpanel_viewport_basis_v.y);
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step * 2;
    DrawDashboardLine("Universe Space", col_x, *row_y, accent_color);
    *row_y += row_step;
    snprintf(line, line_size, "UNIVERSE_SPACE  U(%.2f, %.2f) V(%.2f, %.2f)",
             G_Universe.camera.frame.basis.u.x, G_Universe.camera.frame.basis.u.y,
             G_Universe.camera.frame.basis.v.x, G_Universe.camera.frame.basis.v.y);
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    DrawDashboardLine("Panel Local Spaces", col_x, *row_y, accent_color);
    *row_y += row_step;
    snprintf(line, line_size, "LPANEL_SPACE U(%.2f, %.2f) V(%.2f, %.2f)", basis->lpanel_space_basis_u.x, basis->lpanel_space_basis_u.y, basis->lpanel_space_basis_v.x, basis->lpanel_space_basis_v.y);
    DrawDashboardLine(line, col_x, *row_y, body_color);
    *row_y += row_step;
    snprintf(line, line_size, "RPANEL_SPACE U(%.2f, %.2f) V(%.2f, %.2f)", basis->rpanel_space_basis_u.x, basis->rpanel_space_basis_u.y, basis->rpanel_space_basis_v.x, basis->rpanel_space_basis_v.y);
    DrawDashboardLine(line, col_x, *row_y, body_color);
}

static void DrawDebugDashboard(void)
{
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();
    int panel_x = 16;
    int panel_y = 16;
    int panel_w = screen_width - 32;
    int panel_h = screen_height - 32;
    int col1_x = panel_x + 18;
    int col2_x = panel_x + (panel_w / 2);
    int row_y_left = panel_y + 18;
    int row_y_right = panel_y + 18;
    int row_step = 24;
    ColourRgba title_color = (ColourRgba){255, 244, 200, 240};
    ColourRgba body_color = (ColourRgba){232, 240, 250, 200};
    ColourRgba accent_color = (ColourRgba){170, 220, 255, 230};
    DashboardBasisData basis = ResolveDashboardBasisData();

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){8, 12, 18, 90});
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){210, 228, 245, 180});

    DrawDashboardLine("DEBUG DASHBOARD", col1_x, row_y_left, title_color);
    row_y_left += row_step * 2;
    row_y_right += row_step * 2;

    char line[256] = {0};

    DrawDashboardControlsSection(col1_x, &row_y_left, row_step, accent_color, body_color, line, sizeof(line));
    DrawDashboardUniverseSection(col1_x, &row_y_left, row_step, accent_color, body_color, line, sizeof(line));
    DrawDashboardViewportBasisSection(col2_x, &row_y_right, row_step, accent_color, body_color, line, sizeof(line), &basis);
}

void ToggleDebug(DebugOverlayId overlay_id)
{
    switch (overlay_id)
    {
    case DEBUG_DASHBOARD:
        dashboard_overlay_enabled = !dashboard_overlay_enabled;
        break;
    case DEBUG_VIEWPORT_GRID:
        ToggleViewportDebugGrid();
        break;
    case DEBUG_WORLD_GRID:
        world_grid_overlay_enabled = !world_grid_overlay_enabled;
        break;
    case DEBUG_WORLD_GRID_LABELS:
        world_grid_debug_labels_enabled = !world_grid_debug_labels_enabled;
        break;
    case DEBUG_UNIVERSE_GRID_LABELS:
        grid_labels_overlay_enabled = !grid_labels_overlay_enabled;
        break;
    case DEBUG_UI_BORDERS:
        ui_borders_enabled = !ui_borders_enabled;
        break;
    default:
        break;
    }
}

int IsDebugEnabled(DebugOverlayId overlay_id)
{
    switch (overlay_id)
    {
    case DEBUG_DASHBOARD:
        return dashboard_overlay_enabled;
    case DEBUG_VIEWPORT_GRID:
        return IsViewportDebugGridEnabled();
    case DEBUG_WORLD_GRID:
        return world_grid_overlay_enabled;
    case DEBUG_WORLD_GRID_LABELS:
        return world_grid_debug_labels_enabled;
    case DEBUG_UNIVERSE_GRID_LABELS:
        return grid_labels_overlay_enabled;
    case DEBUG_UI_BORDERS:
        return ui_borders_enabled;
    default:
        return 0;
    }
}

static void HandleDebugToggleHotkeys(void)
{
    if (IsKeyPressed(KEY_F2))
    {
        ToggleDebug(DEBUG_WORLD_GRID_LABELS);
        printf("[World] Grid debug labels: %s\n", IsDebugEnabled(DEBUG_WORLD_GRID_LABELS) ? "ON" : "OFF");
    }

    if (IsKeyPressed(KEY_F6))
    {
        ToggleDebug(DEBUG_VIEWPORT_GRID);
        printf("[Viewport] Debug region grid: %s\n", IsDebugEnabled(DEBUG_VIEWPORT_GRID) ? "ON" : "OFF");
    }

    if (IsKeyPressed(KEY_F3))
    {
        ToggleDebug(DEBUG_UI_BORDERS);
        printf("[UI] Element borders: %s\n", IsDebugEnabled(DEBUG_UI_BORDERS) ? "ON" : "OFF");
    }

    if (IsKeyPressed(KEY_F11))
    {
        ToggleDebug(DEBUG_DASHBOARD);
        printf("[Debug] Dashboard: %s\n", IsDebugEnabled(DEBUG_DASHBOARD) ? "ON" : "OFF");
    }

    if (IsKeyPressed(KEY_F4))
    {
        ToggleDebug(DEBUG_UNIVERSE_GRID_LABELS);
        printf("[Universe] Grid debug labels: %s\n", IsDebugEnabled(DEBUG_UNIVERSE_GRID_LABELS) ? "ON" : "OFF");
    }
}

static bool HandleBasisVectorMutation(Vector2d *target)
{
    if (!target)
    {
        return false;
    }

    bool mutated = false;
    const float nudge = 0.05f;
    const float scale_down = 1.0f / 1.05f;
    const float scale_up = 1.05f;

    if (IsKeyPressed(KEY_J))
    {
        target->x -= nudge;
        mutated = true;
    }
    if (IsKeyPressed(KEY_L))
    {
        target->x += nudge;
        mutated = true;
    }
    if (IsKeyPressed(KEY_I))
    {
        target->y -= nudge;
        mutated = true;
    }
    if (IsKeyPressed(KEY_K))
    {
        target->y += nudge;
        mutated = true;
    }
    if (IsKeyPressed(KEY_O))
    {
        *target = VectorScale_2d(*target, scale_down);
        mutated = true;
    }
    if (IsKeyPressed(KEY_P))
    {
        *target = VectorScale_2d(*target, scale_up);
        mutated = true;
    }

    return mutated;
}

static bool HandleBasisEditorHotkeys(void)
{
    bool basis_changed = false;

    if (IsKeyPressed(KEY_F5))
    {
        basis_editor_enabled = !basis_editor_enabled;
        printf("[Viewport Basis] Editor: %s (TAB=space, U/V=vector, I/J/K/L=xy, O/P=scale, BACKSPACE=reset)\n",
               basis_editor_enabled ? "ON" : "OFF");
    }

    if (!basis_editor_enabled)
    {
        return false;
    }

    if (IsKeyPressed(KEY_TAB))
    {
        basis_editor_target = (DebugBasisTargetId)(((int)basis_editor_target + 1) % DEBUG_BASIS_TARGET_COUNT);
        printf("[Basis Editor] Target: %s\n", GetDebugBasisTargetName(basis_editor_target));
    }

    if (IsKeyPressed(KEY_U))
    {
        basis_editor_editing_u = true;
        printf("[Viewport Basis] Editing vector: U\n");
    }
    if (IsKeyPressed(KEY_V))
    {
        basis_editor_editing_u = false;
        printf("[Viewport Basis] Editing vector: V\n");
    }

    if (IsKeyPressed(KEY_BACKSPACE))
    {
        ResetDebugBasisTarget(basis_editor_target);
        basis_changed = true;
    }
    else
    {
        Frame2d *basis_frame = GetDebugBasisTargetFrame(basis_editor_target);
        if (basis_frame)
        {
            const Basis2d previous_basis = basis_frame->basis;
            Vector2d *target = basis_editor_editing_u ? &basis_frame->basis.u : &basis_frame->basis.v;
            if (HandleBasisVectorMutation(target))
            {
                if (ApplyDebugBasisTarget(basis_editor_target, basis_frame))
                {
                    basis_changed = true;
                }
                else
                {
                    basis_frame->basis = previous_basis;
                    printf("[Basis Editor] Failed to apply basis for %s\n",
                           GetDebugBasisTargetName(basis_editor_target));
                }
            }
        }
    }

    if (basis_changed)
    {
        Frame2d *basis_frame = GetDebugBasisTargetFrame(basis_editor_target);
        if (basis_frame)
        {
            printf("[Basis Editor] %s U(%.2f, %.2f) V(%.2f, %.2f)\n",
                   GetDebugBasisTargetName(basis_editor_target),
                   basis_frame->basis.u.x, basis_frame->basis.u.y,
                   basis_frame->basis.v.x, basis_frame->basis.v.y);
        }
    }

    return basis_changed;
}

static void HandleViewportScaleHotkeys(float *viewport_target_game_logical_height,
                                       int *viewport_ui_pixels_per_unit_override,
                                       bool *viewport_scale_changed,
                                       bool *ui_scale_changed)
{
    if (!viewport_target_game_logical_height || !viewport_ui_pixels_per_unit_override ||
        !viewport_scale_changed || !ui_scale_changed)
    {
        return;
    }

    if (IsKeyPressed(KEY_F7))
    {
        *viewport_target_game_logical_height = ClampViewportLogicalHeight(*viewport_target_game_logical_height - 1.0f);
        *viewport_scale_changed = true;
    }
    if (IsKeyPressed(KEY_F8))
    {
        *viewport_target_game_logical_height = ClampViewportLogicalHeight(*viewport_target_game_logical_height + 1.0f);
        *viewport_scale_changed = true;
    }

    if (IsKeyPressed(KEY_F9))
    {
        if (*viewport_ui_pixels_per_unit_override <= 1)
        {
            *viewport_ui_pixels_per_unit_override = 0;
        }
        else
        {
            *viewport_ui_pixels_per_unit_override -= 1;
        }
        *ui_scale_changed = true;
    }
    if (IsKeyPressed(KEY_F10))
    {
        if (*viewport_ui_pixels_per_unit_override == 0)
        {
            *viewport_ui_pixels_per_unit_override = 10;
        }
        else
        {
            *viewport_ui_pixels_per_unit_override += 1;
        }
        *ui_scale_changed = true;
    }
}

static void ApplyDebugHotkeyChanges(int screen_width, int screen_height, int screen_resolution_scalar,
                                    float viewport_target_game_logical_height,
                                    int viewport_ui_pixels_per_unit_override,bool basis_changed,
                                    bool viewport_scale_changed, bool ui_scale_changed)
{
    if (basis_changed)
    {
        if (basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE)
        {
            UpdateCameraFull(&G_Universe.camera);
        }
        else
        {
            RefreshViewportForBasisEdit(screen_width, screen_height, screen_resolution_scalar,
                                        viewport_target_game_logical_height,
                                        viewport_ui_pixels_per_unit_override);
        }
    }

    if (viewport_scale_changed || ui_scale_changed)
    {
        RefreshViewportAndDependentSystems(screen_width, screen_height, screen_resolution_scalar,
                                           viewport_target_game_logical_height, viewport_ui_pixels_per_unit_override,
                                           viewport_scale_changed, ui_scale_changed);
    }
}

void UpdateDebugOverlayHotkeys(int screen_width, int screen_height, int screen_resolution_scalar,
                               float *viewport_target_game_logical_height, int *viewport_ui_pixels_per_unit_override)
{
    bool viewport_scale_changed = false;
    bool ui_scale_changed = false;
    bool basis_changed = false;

    if (!viewport_target_game_logical_height || !viewport_ui_pixels_per_unit_override)
    {
        return;
    }

    HandleDebugToggleHotkeys();
    basis_changed = HandleBasisEditorHotkeys();
    HandleViewportScaleHotkeys(viewport_target_game_logical_height,
                               viewport_ui_pixels_per_unit_override,
                               &viewport_scale_changed,
                               &ui_scale_changed);

    ApplyDebugHotkeyChanges(screen_width, screen_height, screen_resolution_scalar,
                            *viewport_target_game_logical_height,
                            *viewport_ui_pixels_per_unit_override,
                            basis_changed,
                            viewport_scale_changed,
                            ui_scale_changed);
}

void DrawGlobalDebugOverlays(void)
{
    DrawViewportDebugGrid();

    if (dashboard_overlay_enabled)
    {
        DrawDebugDashboard();
    }
}

void DrawUniverseDebugOverlays(Matrix3x3 root_world_to_pixel_mtx)
{
    debug_snapshot.valid = false;

    if (!dashboard_overlay_enabled)
    {
        return;
    }

    int mouse_x = GetMouseX();
    int mouse_y = GetMouseY();
    Vector2d pixel = {(float)mouse_x, (float)mouse_y};

    bool cursor_in_game_viewport = mouse_x >= game_viewport.pixel_origin.x && mouse_x <= game_viewport.pixel_end.x &&
                                   mouse_y >= game_viewport.pixel_origin.y && mouse_y <= game_viewport.pixel_end.y;

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

        Matrix3x3 universe_to_world_mtx = world->tunnel.dest_to_source_mtx;
        has_child_local = true;
        child_local = TransformCoordinates(universe_to_world_mtx, parent_local);

        child_origin_in_parent.x = world->grid_space.space.frame.origin_in_parent.x;// + (world_resolution.x * 0.5f);
        child_origin_in_parent.y = world->grid_space.space.frame.origin_in_parent.y;// + (world_resolution.y * 0.5f);

        if (child_local.x >= 0.0f && child_local.y >= 0.0f &&
            child_local.x < world_resolution.x && child_local.y < world_resolution.y)
        {
            int cell_x = (int)floorf(child_local.x);
            int cell_y = (int)floorf(child_local.y);
            world_cell_index = (cell_y * (int)world_resolution.x) + cell_x;
        }
    }

    memset(&debug_snapshot, 0, sizeof(debug_snapshot));
    debug_snapshot.valid = true;
    debug_snapshot.cursor_in_game_viewport = cursor_in_game_viewport;
    debug_snapshot.has_child_local = has_child_local;
    debug_snapshot.pixel = pixel;
    debug_snapshot.parent_local = parent_local;
    debug_snapshot.child_origin_in_parent = child_origin_in_parent;
    debug_snapshot.child_local = child_local;
    debug_snapshot.viewport_pixel_center = ResolveGameViewportPixelCenter();
    debug_snapshot.viewport_local_center = ResolveGameViewportLocalCenter();
    debug_snapshot.viewport_ppu_u = VectorMagnitude_2d(game_viewport.pixel_u);
    debug_snapshot.viewport_ppu_v = VectorMagnitude_2d(game_viewport.pixel_v);
    debug_snapshot.selected_index = selected_index;
    debug_snapshot.hovered_world_index = hovered_world_index;
    debug_snapshot.target_world_index = target_world_index;
    debug_snapshot.world_cell_index = world_cell_index;
    debug_snapshot.camera_zoom = G_Universe.camera.zoom;
    debug_snapshot.camera_rotation = G_Universe.camera.rotation;
    debug_snapshot.camera_focus = G_Universe.camera.source_focus_coords;
}
