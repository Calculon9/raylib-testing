#include "system/debug_overlay_system.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#include "raylib.h"
#include "camera/camera.h"
#include "system/ui_system.h"
#include "system/world_system.h"
#include "system/universe_system.h"
#include "system/viewport_system.h"
#include "system/lpanel_system.h"
#include "system/rpanel_system.h"
#include "ui/cfont.h"
#include "ui/text_region.h"
#include "world/universe.h"

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

static Frame2d *GetDebugBasisTargetFrame(DebugBasisTargetId target_id)
{
    switch (target_id)
    {
    case DEBUG_BASIS_TARGET_LPANEL_VIEWPORT:
        return &lpanel_viewport.frame;
    case DEBUG_BASIS_TARGET_GAME_VIEWPORT:
        return &game_viewport.frame;
    case DEBUG_BASIS_TARGET_RPANEL_VIEWPORT:
        return &rpanel_viewport.frame;
    case DEBUG_BASIS_TARGET_UNIVERSE_SPACE:
        return &G_Universe.camera.frame;
    case DEBUG_BASIS_TARGET_LPANEL_SPACE:
        return GetLPanelSpaceFrame();
    case DEBUG_BASIS_TARGET_RPANEL_SPACE:
        return GetRPanelSpaceFrame();
    default:
        return NULL;
    }
}

static void ApplyDebugBasisTarget(DebugBasisTargetId target_id, const Frame2d *frame)
{
    if (!frame)
    {
        return;
    }

    switch (target_id)
    {
    case DEBUG_BASIS_TARGET_LPANEL_VIEWPORT:
        SetViewportSpaceBasis(VIEWPORT_SPACE_LPANEL, frame->basis.u, frame->basis.v);
        break;
    case DEBUG_BASIS_TARGET_GAME_VIEWPORT:
        SetViewportSpaceBasis(VIEWPORT_SPACE_GAME, frame->basis.u, frame->basis.v);
        break;
    case DEBUG_BASIS_TARGET_RPANEL_VIEWPORT:
        SetViewportSpaceBasis(VIEWPORT_SPACE_RPANEL, frame->basis.u, frame->basis.v);
        break;
    case DEBUG_BASIS_TARGET_UNIVERSE_SPACE:
        SetUniverseCameraBasis(frame->basis);
        break;
    case DEBUG_BASIS_TARGET_LPANEL_SPACE:
        SetLPanelSpaceBasis(frame->basis.u, frame->basis.v);
        break;
    case DEBUG_BASIS_TARGET_RPANEL_SPACE:
        SetRPanelSpaceBasis(frame->basis.u, frame->basis.v);
        break;
    default:
        break;
    }
}

static void ResetDebugBasisTarget(DebugBasisTargetId target_id)
{
    switch (target_id)
    {
    case DEBUG_BASIS_TARGET_LPANEL_VIEWPORT:
        ResetViewportSpaceBasis(VIEWPORT_SPACE_LPANEL);
        break;
    case DEBUG_BASIS_TARGET_GAME_VIEWPORT:
        ResetViewportSpaceBasis(VIEWPORT_SPACE_GAME);
        break;
    case DEBUG_BASIS_TARGET_RPANEL_VIEWPORT:
        ResetViewportSpaceBasis(VIEWPORT_SPACE_RPANEL);
        break;
    case DEBUG_BASIS_TARGET_UNIVERSE_SPACE:
        SetUniverseCameraBasis(IDENTITY_BASIS_2D);
        break;
    case DEBUG_BASIS_TARGET_LPANEL_SPACE:
        ResetLPanelSpaceBasis();
        break;
    case DEBUG_BASIS_TARGET_RPANEL_SPACE:
        ResetRPanelSpaceBasis();
        break;
    default:
        break;
    }
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
    float viewport_width_px;
    float viewport_height_px;
    int selected_index;
    int hovered_world_index;
    int target_world_index;
    int world_cell_index;
    float camera_zoom;
    float camera_rotation;
    Vector2d camera_focus;
} UniverseDebugSnapshot;

static bool dashboard_overlay_enabled = false;
static bool universe_grid_labels_overlay_enabled = true;
static int basis_editor_enabled = 0;
static int basis_editor_editing_u = 1;
static DebugBasisTargetId basis_editor_target = DEBUG_BASIS_TARGET_LPANEL_VIEWPORT;
static UniverseDebugSnapshot universe_debug_snapshot = {0};

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
    SetViewportTargetLogicalHeight(viewport_target_game_logical_height);
    SetViewportUIScaleScalar(viewport_ui_pixels_per_unit_override);
    InitViewportLayout(screen_width, screen_height, screen_resolution_scalar);
    SyncUniverseCameraToViewport();
    InitUI();
}

static void RefreshViewportAndDependentSystems(int screen_width, int screen_height, int screen_resolution_scalar,
                                               float viewport_target_game_logical_height,
                                               int viewport_ui_pixels_per_unit_override,
                                               bool viewport_scale_changed,
                                               bool ui_scale_changed)
{
    SetViewportTargetLogicalHeight(viewport_target_game_logical_height);
    SetViewportUIScaleScalar(viewport_ui_pixels_per_unit_override);
    InitViewportLayout(screen_width, screen_height, screen_resolution_scalar);
    SyncUniverseCameraToViewport();

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
    switch (target_id)
    {
    case DEBUG_BASIS_TARGET_LPANEL_VIEWPORT:
        return "LPANEL_VIEWPORT";
    case DEBUG_BASIS_TARGET_GAME_VIEWPORT:
        return "GAME_VIEWPORT";
    case DEBUG_BASIS_TARGET_RPANEL_VIEWPORT:
        return "RPANEL_VIEWPORT";
    case DEBUG_BASIS_TARGET_UNIVERSE_SPACE:
        return "UNIVERSE_SPACE";
    case DEBUG_BASIS_TARGET_LPANEL_SPACE:
        return "LPANEL_SPACE";
    case DEBUG_BASIS_TARGET_RPANEL_SPACE:
        return "RPANEL_SPACE";
    default:
        return "UNKNOWN";
    }
}

static const char *GetOnOffLabel(int enabled)
{
    return enabled ? "ON" : "OFF";
}

static void DrawDashboardLine(const char *text, int x, int y, ColourRgba color)
{
    DrawTextCustom(text, (Vector2d){(float)x, (float)y}, FONT_BASIC.scale, FONT_BASIC, color);
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

    Frame2d *lpanel_viewport_frame = GetDebugBasisTargetFrame(DEBUG_BASIS_TARGET_LPANEL_VIEWPORT);
    Frame2d *game_viewport_frame = GetDebugBasisTargetFrame(DEBUG_BASIS_TARGET_GAME_VIEWPORT);
    Frame2d *rpanel_viewport_frame = GetDebugBasisTargetFrame(DEBUG_BASIS_TARGET_RPANEL_VIEWPORT);
    Frame2d *lpanel_space_frame = GetDebugBasisTargetFrame(DEBUG_BASIS_TARGET_LPANEL_SPACE);
    Frame2d *rpanel_space_frame = GetDebugBasisTargetFrame(DEBUG_BASIS_TARGET_RPANEL_SPACE);

    Vector2d lpanel_viewport_basis_u = lpanel_viewport_frame ? lpanel_viewport_frame->basis.u : ZERO_VECTOR_2D;
    Vector2d lpanel_viewport_basis_v = lpanel_viewport_frame ? lpanel_viewport_frame->basis.v : ZERO_VECTOR_2D;
    Vector2d game_viewport_basis_u = game_viewport_frame ? game_viewport_frame->basis.u : ZERO_VECTOR_2D;
    Vector2d game_viewport_basis_v = game_viewport_frame ? game_viewport_frame->basis.v : ZERO_VECTOR_2D;
    Vector2d rpanel_viewport_basis_u = rpanel_viewport_frame ? rpanel_viewport_frame->basis.u : ZERO_VECTOR_2D;
    Vector2d rpanel_viewport_basis_v = rpanel_viewport_frame ? rpanel_viewport_frame->basis.v : ZERO_VECTOR_2D;
    Vector2d lpanel_space_basis_u = lpanel_space_frame ? lpanel_space_frame->basis.u : ZERO_VECTOR_2D;
    Vector2d lpanel_space_basis_v = lpanel_space_frame ? lpanel_space_frame->basis.v : ZERO_VECTOR_2D;
    Vector2d rpanel_space_basis_u = rpanel_space_frame ? rpanel_space_frame->basis.u : ZERO_VECTOR_2D;
    Vector2d rpanel_space_basis_v = rpanel_space_frame ? rpanel_space_frame->basis.v : ZERO_VECTOR_2D;

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){8, 12, 18, 90});
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){210, 228, 245, 180});

    DrawDashboardLine("DEBUG DASHBOARD", col1_x, row_y_left, title_color);
    row_y_left += row_step * 2;
    row_y_right += row_step * 2;

    DrawDashboardLine("Controls", col1_x, row_y_left, accent_color);
    row_y_left += row_step;

    char line[256] = {0};
    snprintf(line, sizeof(line), "F11 Dashboard: %s", GetOnOffLabel(dashboard_overlay_enabled));
    DrawDashboardLine(line, col1_x, row_y_left, body_color);
    row_y_left += row_step;
    snprintf(line, sizeof(line), "F5 Basis editor: %s", GetOnOffLabel(basis_editor_enabled));
    DrawDashboardLine(line, col1_x, row_y_left, body_color);
    row_y_left += row_step;
    snprintf(line, sizeof(line), "F6 Viewport grid: %s", GetOnOffLabel(IsDebugOverlayEnabled(DEBUG_OVERLAY_VIEWPORT_GRID)));
    DrawDashboardLine(line, col1_x, row_y_left, body_color);
    row_y_left += row_step;
    snprintf(line, sizeof(line), "F12 Universe grid labels: %s", GetOnOffLabel(IsDebugOverlayEnabled(DEBUG_OVERLAY_UNIVERSE_GRID_LABELS)));
    DrawDashboardLine(line, col1_x, row_y_left, body_color);
    row_y_left += row_step;
    DrawDashboardLine("F7/F8 Logical height   F9/F10 UI scale", col1_x, row_y_left, body_color);
    row_y_left += row_step;
    DrawDashboardLine("TAB Space   U/V Vector   I/J/K/L Nudge", col1_x, row_y_left, body_color);
    row_y_left += row_step;
    DrawDashboardLine("O/P Scale   BACKSPACE Reset", col1_x, row_y_left, body_color);
    row_y_left += row_step * 2;

    DrawDashboardLine("Universe", col1_x, row_y_left, accent_color);
    row_y_left += row_step;

    if (universe_debug_snapshot.valid)
    {
        snprintf(line, sizeof(line), "Cursor px: (%.1f, %.1f) [%s]", universe_debug_snapshot.pixel.x, universe_debug_snapshot.pixel.y,
                 universe_debug_snapshot.cursor_in_game_viewport ? "in viewport" : "outside viewport");
        DrawDashboardLine(line, col1_x, row_y_left, body_color);
        row_y_left += row_step;

        snprintf(line, sizeof(line), "Universe coords: (%.3f, %.3f)", universe_debug_snapshot.parent_local.x, universe_debug_snapshot.parent_local.y);
        DrawDashboardLine(line, col1_x, row_y_left, body_color);
        row_y_left += row_step;

        snprintf(line, sizeof(line), "Selected world: %d   Hovered world: %d", universe_debug_snapshot.selected_index, universe_debug_snapshot.hovered_world_index);
        DrawDashboardLine(line, col1_x, row_y_left, body_color);
        row_y_left += row_step;

        if (universe_debug_snapshot.has_child_local)
        {
            snprintf(line, sizeof(line), "Child local[%d]: (%.3f, %.3f)", universe_debug_snapshot.target_world_index,
                     universe_debug_snapshot.child_local.x, universe_debug_snapshot.child_local.y);
            DrawDashboardLine(line, col1_x, row_y_left, body_color);
            row_y_left += row_step;

            snprintf(line, sizeof(line), "Child origin: (%.3f, %.3f)", universe_debug_snapshot.child_origin_in_parent.x,
                     universe_debug_snapshot.child_origin_in_parent.y);
            DrawDashboardLine(line, col1_x, row_y_left, body_color);
            row_y_left += row_step;

            snprintf(line, sizeof(line), "Child cell index: %d", universe_debug_snapshot.world_cell_index);
            DrawDashboardLine(line, col1_x, row_y_left, body_color);
            row_y_left += row_step;
        }
        else
        {
            DrawDashboardLine("Child local: n/a", col1_x, row_y_left, body_color);
            row_y_left += row_step;
        }

        snprintf(line, sizeof(line), "Camera focus(%.2f, %.2f) zoom=%.3f rot=%.3f",
                 universe_debug_snapshot.camera_focus.x, universe_debug_snapshot.camera_focus.y,
                 universe_debug_snapshot.camera_zoom, universe_debug_snapshot.camera_rotation);
        DrawDashboardLine(line, col1_x, row_y_left, body_color);
    }
    else
    {
        DrawDashboardLine("Universe diagnostics unavailable for current frame.", col1_x, row_y_left, body_color);
    }

    DrawDashboardLine("Game Viewport", col2_x, row_y_right, accent_color);
    row_y_right += row_step;

    snprintf(line, sizeof(line), "Global Viewport bounds: (%.1f, %.1f) -> (%.1f, %.1f)",
             game_viewport.local_origin.x, game_viewport.local_origin.y,
             game_viewport.local_end.x, game_viewport.local_end.y);
    DrawDashboardLine(line, col2_x, row_y_right, body_color);
    row_y_right += row_step;
    snprintf(line, sizeof(line), "Pixel bounds: (%.1f, %.1f) -> (%.1f, %.1f)",
             game_viewport.pixel_origin.x, game_viewport.pixel_origin.y,
             game_viewport.pixel_end.x, game_viewport.pixel_end.y);
    DrawDashboardLine(line, col2_x, row_y_right, body_color);
    row_y_right += row_step;
    snprintf(line, sizeof(line), "Region center local(%.2f, %.2f) pixel(%.1f, %.1f)",
             universe_debug_snapshot.viewport_local_center.x, universe_debug_snapshot.viewport_local_center.y,
             universe_debug_snapshot.viewport_pixel_center.x, universe_debug_snapshot.viewport_pixel_center.y);
    DrawDashboardLine(line, col2_x, row_y_right, body_color);
    row_y_right += row_step;
    snprintf(line, sizeof(line), "Px/unit U=%.2f V=%.2f", VectorMagnitude_2d(game_viewport.pixel_u), VectorMagnitude_2d(game_viewport.pixel_v));
    DrawDashboardLine(line, col2_x, row_y_right, body_color);
    row_y_right += row_step;
    //snprintf(line, sizeof(line), "Viewport size %.1f x %.1f", universe_debug_snapshot.viewport_width_px, universe_debug_snapshot.viewport_height_px);
    //DrawDashboardLine(line, col2_x, row_y_right, body_color);
    //row_y_right += row_step * 2;

    DrawDashboardLine("Basis", col2_x, row_y_right, accent_color);
    row_y_right += row_step;

    snprintf(line, sizeof(line), "Editor target: %s.%s", GetDebugBasisTargetName(basis_editor_target), basis_editor_editing_u ? "U" : "V");
    DrawDashboardLine(line, col2_x, row_y_right, body_color);
    row_y_right += row_step;
    DrawDashboardLine("Viewport Spaces", col2_x, row_y_right, accent_color);
    row_y_right += row_step;
    snprintf(line, sizeof(line), "LPANEL_VIEWPORT U(%.2f, %.2f) V(%.2f, %.2f)", lpanel_viewport_basis_u.x, lpanel_viewport_basis_u.y, lpanel_viewport_basis_v.x, lpanel_viewport_basis_v.y);
    DrawDashboardLine(line, col2_x, row_y_right, body_color);
    row_y_right += row_step;
    snprintf(line, sizeof(line), "GAME_VIEWPORT   U(%.2f, %.2f) V(%.2f, %.2f)", game_viewport_basis_u.x, game_viewport_basis_u.y, game_viewport_basis_v.x, game_viewport_basis_v.y);
    DrawDashboardLine(line, col2_x, row_y_right, body_color);
    row_y_right += row_step;
    snprintf(line, sizeof(line), "RPANEL_VIEWPORT U(%.2f, %.2f) V(%.2f, %.2f)", rpanel_viewport_basis_u.x, rpanel_viewport_basis_u.y, rpanel_viewport_basis_v.x, rpanel_viewport_basis_v.y);
    DrawDashboardLine(line, col2_x, row_y_right, body_color);
    row_y_right += row_step * 2;
    DrawDashboardLine("Universe Space", col2_x, row_y_right, accent_color);
    row_y_right += row_step;
    snprintf(line, sizeof(line), "UNIVERSE_SPACE  U(%.2f, %.2f) V(%.2f, %.2f)",
             G_Universe.camera.frame.basis.u.x, G_Universe.camera.frame.basis.u.y,
             G_Universe.camera.frame.basis.v.x, G_Universe.camera.frame.basis.v.y);
    DrawDashboardLine(line, col2_x, row_y_right, body_color);
    row_y_right += row_step;
    DrawDashboardLine("Panel Local Spaces", col2_x, row_y_right, accent_color);
    row_y_right += row_step;
    snprintf(line, sizeof(line), "LPANEL_SPACE U(%.2f, %.2f) V(%.2f, %.2f)", lpanel_space_basis_u.x, lpanel_space_basis_u.y, lpanel_space_basis_v.x, lpanel_space_basis_v.y);
    DrawDashboardLine(line, col2_x, row_y_right, body_color);
    row_y_right += row_step;
    snprintf(line, sizeof(line), "RPANEL_SPACE U(%.2f, %.2f) V(%.2f, %.2f)", rpanel_space_basis_u.x, rpanel_space_basis_u.y, rpanel_space_basis_v.x, rpanel_space_basis_v.y);
    DrawDashboardLine(line, col2_x, row_y_right, body_color);
}

void ToggleDebugOverlay(DebugOverlayId overlay_id)
{
    switch (overlay_id)
    {
    case DEBUG_OVERLAY_DASHBOARD:
        dashboard_overlay_enabled = !dashboard_overlay_enabled;
        break;
    case DEBUG_OVERLAY_VIEWPORT_GRID:
        ToggleViewportDebugGrid();
        break;
    case DEBUG_OVERLAY_UNIVERSE_GRID_LABELS:
        universe_grid_labels_overlay_enabled = !universe_grid_labels_overlay_enabled;
        break;
    default:
        break;
    }
}

int IsDebugOverlayEnabled(DebugOverlayId overlay_id)
{
    switch (overlay_id)
    {
    case DEBUG_OVERLAY_DASHBOARD:
        return dashboard_overlay_enabled;
    case DEBUG_OVERLAY_VIEWPORT_GRID:
        return IsViewportDebugGridEnabled();
    case DEBUG_OVERLAY_UNIVERSE_GRID_LABELS:
        return universe_grid_labels_overlay_enabled;
    default:
        return 0;
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

    if (IsKeyPressed(KEY_F5))
    {
        basis_editor_enabled = !basis_editor_enabled;
        printf("[Viewport Basis] Editor: %s (TAB=space, U/V=vector, I/J/K/L=xy, O/P=scale, BACKSPACE=reset)\n",
               basis_editor_enabled ? "ON" : "OFF");
    }

    if (IsKeyPressed(KEY_F6))
    {
        ToggleDebugOverlay(DEBUG_OVERLAY_VIEWPORT_GRID);
        printf("[Viewport] Debug region grid: %s\n", IsDebugOverlayEnabled(DEBUG_OVERLAY_VIEWPORT_GRID) ? "ON" : "OFF");
    }

    if (IsKeyPressed(KEY_F11))
    {
        ToggleDebugOverlay(DEBUG_OVERLAY_DASHBOARD);
        printf("[Debug] Dashboard: %s\n", IsDebugOverlayEnabled(DEBUG_OVERLAY_DASHBOARD) ? "ON" : "OFF");
    }

    if (IsKeyPressed(KEY_F12))
    {
        ToggleDebugOverlay(DEBUG_OVERLAY_UNIVERSE_GRID_LABELS);
        printf("[Universe] Grid debug labels: %s\n", IsDebugOverlayEnabled(DEBUG_OVERLAY_UNIVERSE_GRID_LABELS) ? "ON" : "OFF");
    }

    if (basis_editor_enabled)
    {
        if (IsKeyPressed(KEY_TAB))
        {
            basis_editor_target = (DebugBasisTargetId)(((int)basis_editor_target + 1) % DEBUG_BASIS_TARGET_COUNT);
            printf("[Basis Editor] Target: %s\n", GetDebugBasisTargetName(basis_editor_target));
        }

        if (IsKeyPressed(KEY_U))
        {
            basis_editor_editing_u = 1;
            printf("[Viewport Basis] Editing vector: U\n");
        }
        if (IsKeyPressed(KEY_V))
        {
            basis_editor_editing_u = 0;
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
                Vector2d *target = basis_editor_editing_u ? &basis_frame->basis.u : &basis_frame->basis.v;
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

                if (mutated)
                {
                    ApplyDebugBasisTarget(basis_editor_target, basis_frame);
                    basis_changed = true;
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
    }

    if (IsKeyPressed(KEY_F7))
    {
        *viewport_target_game_logical_height = ClampViewportLogicalHeight(*viewport_target_game_logical_height - 1.0f);
        viewport_scale_changed = true;
    }
    if (IsKeyPressed(KEY_F8))
    {
        *viewport_target_game_logical_height = ClampViewportLogicalHeight(*viewport_target_game_logical_height + 1.0f);
        viewport_scale_changed = true;
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
        ui_scale_changed = true;
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
        ui_scale_changed = true;
    }

    if (basis_changed)
    {
        if (basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE)
        {
            UpdateCameraFull(&G_Universe.camera);
        }
        else
        {
            RefreshViewportForBasisEdit(screen_width, screen_height, screen_resolution_scalar,
                                        *viewport_target_game_logical_height,
                                        *viewport_ui_pixels_per_unit_override);
        }
    }

    if (viewport_scale_changed || ui_scale_changed)
    {
        RefreshViewportAndDependentSystems(screen_width, screen_height, screen_resolution_scalar,
                                           *viewport_target_game_logical_height,
                                           *viewport_ui_pixels_per_unit_override,
                                           viewport_scale_changed,
                                           ui_scale_changed);
    }
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
    universe_debug_snapshot.valid = false;

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

    memset(&universe_debug_snapshot, 0, sizeof(universe_debug_snapshot));
    universe_debug_snapshot.valid = true;
    universe_debug_snapshot.cursor_in_game_viewport = cursor_in_game_viewport;
    universe_debug_snapshot.has_child_local = has_child_local;
    universe_debug_snapshot.pixel = pixel;
    universe_debug_snapshot.parent_local = parent_local;
    universe_debug_snapshot.child_origin_in_parent = child_origin_in_parent;
    universe_debug_snapshot.child_local = child_local;
    universe_debug_snapshot.viewport_pixel_center = ResolveGameViewportPixelCenter();
    universe_debug_snapshot.viewport_local_center = ResolveGameViewportLocalCenter();
    universe_debug_snapshot.viewport_ppu_u = VectorMagnitude_2d(game_viewport.pixel_u);
    universe_debug_snapshot.viewport_ppu_v = VectorMagnitude_2d(game_viewport.pixel_v);
    universe_debug_snapshot.viewport_width_px = game_viewport.pixel_end.x - game_viewport.pixel_origin.x;
    universe_debug_snapshot.viewport_height_px = game_viewport.pixel_end.y - game_viewport.pixel_origin.y;
    universe_debug_snapshot.selected_index = selected_index;
    universe_debug_snapshot.hovered_world_index = hovered_world_index;
    universe_debug_snapshot.target_world_index = target_world_index;
    universe_debug_snapshot.world_cell_index = world_cell_index;
    universe_debug_snapshot.camera_zoom = G_Universe.camera.zoom;
    universe_debug_snapshot.camera_rotation = G_Universe.camera.rotation;
    universe_debug_snapshot.camera_focus = G_Universe.camera.source_focus_coords;
}