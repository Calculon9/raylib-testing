#include "system/diagnostics.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "raylib.h"
#include "system/debug_overlay_system.h"
#include "system/universe_system.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "world/universe.h"
#include "system/viewport_system.h"
#include "system/ui/lpanel_system.h"
#include "system/ui/rpanel_system.h"
#include "ui/cfont.h"
#include "ui/text_region.h"

// --- Basis-target machinery (shared by the dashboard and the basis editor) ---

// Identifies a single basis-edit target: the viewport/space whose basis frame is read or mutated.
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

// Dispatch table entry: how to read, apply, reset, and name a basis target.
typedef struct DebugBasisTargetOps
{
    Frame2d *(*get_frame_fn)(void);
    bool (*apply_basis_fn)(Basis2d basis);
    void (*reset_fn)(void);
    const char *name;
} DebugBasisTargetOps;

// Frame getters: return a pointer to the live basis frame for each target.
static Frame2d *GetLPanelViewportFrame(void) { return &lpanel_viewport.frame; }
static Frame2d *GetGameViewportFrame(void) { return &game_viewport.frame; }
static Frame2d *GetRPanelViewportFrame(void) { return &rpanel_viewport.frame; }
static Frame2d *GetUniverseSpaceFrame(void) { return &G_Universe.camera.frame; }

// Basis-apply callbacks: push an edited basis back into the owning subsystem.
static bool ApplyLPanelViewportBasis(Basis2d basis) { return SetViewportSpaceBasis(VIEWPORT_SPACE_LPANEL, basis.u, basis.v); }
static bool ApplyGameViewportBasis(Basis2d basis) { return SetViewportSpaceBasis(VIEWPORT_SPACE_GAME, basis.u, basis.v); }
static bool ApplyRPanelViewportBasis(Basis2d basis) { return SetViewportSpaceBasis(VIEWPORT_SPACE_RPANEL, basis.u, basis.v); }
static bool ApplyUniverseSpaceBasis(Basis2d basis)
{
    return SetUniverseCameraBasis(basis);
}
static bool ApplyLPanelSpaceBasis(Basis2d basis) { return SetLPanelSpaceBasis(basis.u, basis.v); }
static bool ApplyRPanelSpaceBasis(Basis2d basis) { return SetRPanelSpaceBasis(basis.u, basis.v); }

// Reset callbacks: restore the target's basis to its identity/default.
static void ResetLPanelViewportBasis(void) { ResetViewportSpaceBasis(VIEWPORT_SPACE_LPANEL); }
static void ResetGameViewportBasis(void) { ResetViewportSpaceBasis(VIEWPORT_SPACE_GAME); }
static void ResetRPanelViewportBasis(void) { ResetViewportSpaceBasis(VIEWPORT_SPACE_RPANEL); }
static void ResetUniverseSpaceBasis(void) { SetUniverseCameraBasis(IDENTITY_BASIS_2D); }

// The shared basis-target registry: single source of truth for both diagnostics halves.
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
        .get_frame_fn = GetLPanelSpaceFrame,
        .apply_basis_fn = ApplyLPanelSpaceBasis,
        .reset_fn = ResetLPanelSpaceBasis,
        .name = "LPANEL_SPACE",
    },
    [DEBUG_BASIS_TARGET_RPANEL_SPACE] = {
        .get_frame_fn = GetRPanelSpaceFrame,
        .apply_basis_fn = ApplyRPanelSpaceBasis,
        .reset_fn = ResetRPanelSpaceBasis,
        .name = "RPANEL_SPACE",
    },
};

// Return the ops entry for a target id, or NULL when the id is out of range.
static const DebugBasisTargetOps *GetDebugBasisTargetOps(DebugBasisTargetId target_id)
{
    if (target_id < DEBUG_BASIS_TARGET_LPANEL_VIEWPORT || target_id >= DEBUG_BASIS_TARGET_COUNT)
    {
        return NULL;
    }

    return &debug_basis_target_ops[(int)target_id];
}

// Return the live basis frame for a target, or NULL when unavailable.
static Frame2d *GetDebugBasisTargetFrame(DebugBasisTargetId target_id)
{
    const DebugBasisTargetOps *ops = GetDebugBasisTargetOps(target_id);
    if (!ops || !ops->get_frame_fn)
    {
        return NULL;
    }

    return ops->get_frame_fn();
}

// Apply a frame's basis to its target subsystem; returns false on invalid target/frame or apply failure.
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

// Reset a target's basis to its default; a no-op for invalid targets.
static void ResetDebugBasisTarget(DebugBasisTargetId target_id)
{
    const DebugBasisTargetOps *ops = GetDebugBasisTargetOps(target_id);
    if (!ops || !ops->reset_fn)
    {
        return;
    }

    ops->reset_fn();
}

// Return a target's display name, or "UNKNOWN" for invalid targets.
static const char *GetDebugBasisTargetName(DebugBasisTargetId target_id)
{
    const DebugBasisTargetOps *ops = GetDebugBasisTargetOps(target_id);
    if (!ops || !ops->name)
    {
        return "UNKNOWN";
    }

    return ops->name;
}

// --- Dashboard snapshot + state ---

// Per-frame universe diagnostics captured for the dashboard's Universe section.
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

// Dashboard enabled-state, owned here; the facade flips/reads it via Diagnostics_Toggle/IsDashboard.
static bool dashboard_overlay_enabled = false;
// Basis-editor state, owned here and observed by the facade only through the universe-space predicate.
static bool basis_editor_enabled = false;
static bool basis_editor_editing_u = true;
static DebugBasisTargetId basis_editor_target = DEBUG_BASIS_TARGET_LPANEL_VIEWPORT;
static UniverseDebugSnapshot debug_snapshot = {0};

// Return "ON"/"OFF" for a boolean enabled flag.
static const char *GetOnOffLabel(int enabled)
{
    return enabled ? "ON" : "OFF";
}

// Draw a single dashboard text line at (x, y) in the given colour.
static void DrawDashboardLine(const char *text, int x, int y, ColourRgba color)
{
    DrawTextCustom(text, (Vector2d){(float)x, (float)y}, FONT_BASIC.scale, FONT_BASIC, color);
}

// Formats and draws a single dashboard row, advancing the row pointer.
static void DrawDashboardRowf(int x, int *row_y, int row_step, ColourRgba color,
                              char *line, size_t line_size, const char *fmt, ...)
{
    if (!row_y || !line || !fmt)
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(line, line_size, fmt, args);
    va_end(args);

    DrawDashboardLine(line, x, *row_y, color);
    *row_y += row_step;
}

// Cached basis vectors for each space, resolved once per dashboard frame.
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

// Resolve every basis target's current U/V vectors for the dashboard's Basis section.
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

// Draw the dashboard's Controls section: toggle states and the hotkey legend.
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

    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "F11 Dashboard: %s", GetOnOffLabel(dashboard_overlay_enabled));
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "F5 Basis editor: %s", GetOnOffLabel(basis_editor_enabled));
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "F6 Viewport grid: %s", GetOnOffLabel(IsDebugEnabled(DEBUG_VIEWPORT_GRID)));
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "F2 World grid labels: %s", GetOnOffLabel(IsDebugEnabled(DEBUG_WORLD_GRID_LABELS)));
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "F3 UI borders: %s", GetOnOffLabel(IsDebugEnabled(DEBUG_UI_BORDERS)));
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "F4 Universe grid labels: %s", GetOnOffLabel(IsDebugEnabled(DEBUG_UNIVERSE_GRID_LABELS)));
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "F1 Object axes: %s", GetOnOffLabel(IsDebugEnabled(DEBUG_OBJECT_AXES)));
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "F12 Object hull: %s", GetOnOffLabel(IsDebugEnabled(DEBUG_OBJECT_HULL)));
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "Shift+F12 Object AABB: %s", GetOnOffLabel(IsDebugEnabled(DEBUG_OBJECT_AABB)));
    DrawDashboardLine("F7/F8 Logical height   F9/F10 UI scale", col_x, *row_y, body_color);
    *row_y += row_step;
    DrawDashboardLine("TAB Space   U/V Vector   I/J/K/L Nudge", col_x, *row_y, body_color);
    *row_y += row_step;
    DrawDashboardLine("O/P Scale   BACKSPACE Reset", col_x, *row_y, body_color);
    *row_y += row_step * 2;
}

// Draw the dashboard's Universe section from the current frame's snapshot.
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

    if (!debug_snapshot.valid)
    {
        DrawDashboardLine("Universe diagnostics unavailable for current frame.", col_x, *row_y, body_color);
        return;
    }

    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "Cursor px: (%.1f, %.1f) [%s]", debug_snapshot.pixel.x, debug_snapshot.pixel.y,
                      debug_snapshot.cursor_in_game_viewport ? "in viewport" : "outside viewport");
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "Universe coords: (%.3f, %.3f)",
                      debug_snapshot.parent_local.x, debug_snapshot.parent_local.y);
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "Selected world: %d   Hovered world: %d",
                      debug_snapshot.selected_index, debug_snapshot.hovered_world_index);

    if (debug_snapshot.has_child_local)
    {
        DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                          "Child local[%d]: (%.3f, %.3f)", debug_snapshot.target_world_index,
                          debug_snapshot.child_local.x, debug_snapshot.child_local.y);
        DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                          "Child origin: (%.3f, %.3f)",
                          debug_snapshot.child_origin_in_parent.x,
                          debug_snapshot.child_origin_in_parent.y);
        DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                          "Child cell index: %d", debug_snapshot.world_cell_index);
    }
    else
    {
        DrawDashboardLine("Child local: n/a", col_x, *row_y, body_color);
        *row_y += row_step;
    }

    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "Camera focus(%.2f, %.2f) zoom=%.3f rot=%.3f",
                      debug_snapshot.camera_focus.x, debug_snapshot.camera_focus.y,
                      debug_snapshot.camera_zoom, debug_snapshot.camera_rotation);
}

// Draw the dashboard's Game Viewport + Basis sections from live viewport/basis state.
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

    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "Editor target: %s.%s",
                      GetDebugBasisTargetName(basis_editor_target),
                      basis_editor_editing_u ? "U" : "V");
    DrawDashboardLine("Viewport Spaces", col_x, *row_y, accent_color);
    *row_y += row_step;
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "LPANEL_VIEWPORT U(%.2f, %.2f) V(%.2f, %.2f)",
                      basis->lpanel_viewport_basis_u.x, basis->lpanel_viewport_basis_u.y,
                      basis->lpanel_viewport_basis_v.x, basis->lpanel_viewport_basis_v.y);
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "GAME_VIEWPORT   U(%.2f, %.2f) V(%.2f, %.2f)",
                      basis->game_viewport_basis_u.x, basis->game_viewport_basis_u.y,
                      basis->game_viewport_basis_v.x, basis->game_viewport_basis_v.y);
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "RPANEL_VIEWPORT U(%.2f, %.2f) V(%.2f, %.2f)",
                      basis->rpanel_viewport_basis_u.x, basis->rpanel_viewport_basis_u.y,
                      basis->rpanel_viewport_basis_v.x, basis->rpanel_viewport_basis_v.y);
    *row_y += row_step;
    DrawDashboardLine("Universe Space", col_x, *row_y, accent_color);
    *row_y += row_step;
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "UNIVERSE_SPACE  U(%.2f, %.2f) V(%.2f, %.2f)",
                      G_Universe.camera.frame.basis.u.x, G_Universe.camera.frame.basis.u.y,
                      G_Universe.camera.frame.basis.v.x, G_Universe.camera.frame.basis.v.y);
    DrawDashboardLine("Panel Local Spaces", col_x, *row_y, accent_color);
    *row_y += row_step;
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "LPANEL_SPACE U(%.2f, %.2f) V(%.2f, %.2f)",
                      basis->lpanel_space_basis_u.x, basis->lpanel_space_basis_u.y,
                      basis->lpanel_space_basis_v.x, basis->lpanel_space_basis_v.y);
    DrawDashboardRowf(col_x, row_y, row_step, body_color, line, line_size,
                      "RPANEL_SPACE U(%.2f, %.2f) V(%.2f, %.2f)",
                      basis->rpanel_space_basis_u.x, basis->rpanel_space_basis_u.y,
                      basis->rpanel_space_basis_v.x, basis->rpanel_space_basis_v.y);
}

// Compose and draw the full-screen debug dashboard from its sections.
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

// --- Basis editor ---

// Nudge/scale a single basis vector from I/J/K/L and O/P input; returns true when it changed.
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

// --- Public API (facade delegates to these) ---

// Flip the debug dashboard's enabled state.
void Diagnostics_ToggleDashboard(void)
{
    dashboard_overlay_enabled = !dashboard_overlay_enabled;
}

// Return true when the debug dashboard is enabled, false otherwise.
bool Diagnostics_IsDashboardEnabled(void)
{
    return dashboard_overlay_enabled;
}

// Draw the full-screen debug dashboard (caller guards on the enabled state).
void Diagnostics_DrawDashboard(void)
{
    DrawDebugDashboard();
}

// Recompute the universe diagnostics snapshot for the current frame.
void Diagnostics_UpdateSnapshot(void)
{
    debug_snapshot.valid = false;

    if (!dashboard_overlay_enabled)
    {
        return;
    }

    int mouse_x = GetMouseX();
    int mouse_y = GetMouseY();
    Vector2d pixel = {(float)mouse_x, (float)mouse_y};

    bool cursor_in_game_viewport = ViewportRegion_ContainsPixel(&game_viewport, pixel);

    Vector2d parent_local = ResolvePixelToWorldFrame(&G_Universe.root_world, pixel);

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

        has_child_local = true;
        child_local = ResolvePixelToWorldFrame(world, pixel);

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

// Process basis-editor hotkeys; returns true when the active basis target changed this frame.
bool Diagnostics_HandleBasisEditorHotkeys(void)
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

// Return non-zero when the active basis-editor target is universe space, zero otherwise.
int Diagnostics_ActiveBasisTargetIsUniverseSpace(void)
{
    return basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE;
}
