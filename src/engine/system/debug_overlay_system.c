#include "system/debug_overlay_system.h"

#include <stdio.h>

#include "raylib.h"
#include "camera/camera.h"
#include "system/diagnostics.h"
#include "system/ui_system.h"
#include "system/universe_system.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "world/universe.h"
#include "world/object_gizmos.h"
#include "system/viewport_system.h"

// World grid (world_grid_overlay_enabled) and universe grid labels (universe_grid_labels_enabled)
// are view/display settings now owned by the world layer (world_system.c); the DEBUG_WORLD_GRID
// and DEBUG_UNIVERSE_GRID_LABELS facade cases below delegate to those globals.
// Object gizmo (axes/hull/AABB) enabled-state now lives in the world-layer object_gizmos module;
// the DEBUG_OBJECT_* facade cases below delegate to it, mirroring how viewport-grid / UI-borders
// delegate to their owning subsystems.
// The debug dashboard and the viewport-space basis editor (and their shared basis-target
// machinery) now live in the diagnostics module; the DEBUG_DASHBOARD facade case and the
// dashboard/basis-editor entry points below delegate to it.

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
    // Rebuild through the teardown path so the previous UI tree is not orphaned.
    ResetUI();
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
        // Rebuild through the teardown path so the previous UI tree is not orphaned.
        ResetUI();
        printf("[Viewport] Target game logical height set to %.1f. UI px-per-unit override: %d\n",
               viewport_target_game_logical_height, viewport_ui_pixels_per_unit_override);
        return;
    }

    if (ui_scale_changed)
    {
        // Rebuild through the teardown path so the previous UI tree is not orphaned.
        ResetUI();
        printf("[Viewport] UI px-per-unit override set to %d (0 = follow game viewport scale).\n",
               viewport_ui_pixels_per_unit_override);
    }
}

void ToggleDebug(DebugOverlayId overlay_id)
{
    switch (overlay_id)
    {
    case DEBUG_DASHBOARD:
        Diagnostics_ToggleDashboard(); // delegate to the diagnostics owner
        break;
    case DEBUG_VIEWPORT_GRID:
        ToggleViewportDebugGrid();
        break;
    case DEBUG_WORLD_GRID:
        world_grid_overlay_enabled = !world_grid_overlay_enabled; // world-owned view flag
        break;
    case DEBUG_WORLD_GRID_LABELS:
        world_grid_debug_labels_enabled = !world_grid_debug_labels_enabled; // world-owned
        break;
    case DEBUG_UNIVERSE_GRID_LABELS:
        universe_grid_labels_enabled = !universe_grid_labels_enabled; // universe-owned view flag
        break;
    case DEBUG_UI_BORDERS:
        ui_borders_enabled = !ui_borders_enabled;
        break;
    case DEBUG_OBJECT_AXES:
        ToggleGizmo(GIZMO_AXES); // delegate to world-layer gizmo owner
        break;
    case DEBUG_OBJECT_HULL:
        ToggleGizmo(GIZMO_HULL);
        break;
    case DEBUG_OBJECT_AABB:
        ToggleGizmo(GIZMO_AABB);
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
        return Diagnostics_IsDashboardEnabled();
    case DEBUG_VIEWPORT_GRID:
        return IsViewportDebugGridEnabled();
    case DEBUG_WORLD_GRID:
        return world_grid_overlay_enabled;
    case DEBUG_WORLD_GRID_LABELS:
        return world_grid_debug_labels_enabled;
    case DEBUG_UNIVERSE_GRID_LABELS:
        return universe_grid_labels_enabled;
    case DEBUG_UI_BORDERS:
        return ui_borders_enabled;
    case DEBUG_OBJECT_AXES:
        return IsGizmoEnabled(GIZMO_AXES); // delegate to world-layer gizmo owner
    case DEBUG_OBJECT_HULL:
        return IsGizmoEnabled(GIZMO_HULL);
    case DEBUG_OBJECT_AABB:
        return IsGizmoEnabled(GIZMO_AABB);
    default:
        return 0;
    }
}

static void HandleDebugToggleHotkeys(void)
{
    bool ctrl_down = IsKeyDown(KEY_LEFT_CONTROL);

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

    if (IsKeyPressed(KEY_F1))
    {
        if (ctrl_down)
        {
            size_t bytes_before = GetCurrentMemoryAllocated();
            ResetUI();
            size_t bytes_after = GetCurrentMemoryAllocated();
            printf("[UI] UI state reset: %.2f kB -> %.2f kB (delta %+0.2f kB)\n",
                   (double)bytes_before / 1024.0,
                   (double)bytes_after / 1024.0,
                   ((double)bytes_after - (double)bytes_before) / 1024.0);
        }
        else
        {
            ToggleDebug(DEBUG_OBJECT_AXES);
            printf("[World] Object axes: %s\n", IsDebugEnabled(DEBUG_OBJECT_AXES) ? "ON" : "OFF");
        }
    }

    if (IsKeyPressed(KEY_F12))
    {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
        {
            ToggleDebug(DEBUG_OBJECT_AABB);
            printf("[World] Object AABB: %s\n", IsDebugEnabled(DEBUG_OBJECT_AABB) ? "ON" : "OFF");
        }
        else
        {
            ToggleDebug(DEBUG_OBJECT_HULL);
            printf("[World] Object hull: %s\n", IsDebugEnabled(DEBUG_OBJECT_HULL) ? "ON" : "OFF");
        }
    }
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
        if (Diagnostics_ActiveBasisTargetIsUniverseSpace())
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
    basis_changed = Diagnostics_HandleBasisEditorHotkeys();
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

    if (Diagnostics_IsDashboardEnabled())
    {
        Diagnostics_DrawDashboard();
    }
}

void DrawUniverseDebugOverlays(void)
{
    // The dashboard's per-frame universe snapshot now lives in the diagnostics module.
    Diagnostics_UpdateSnapshot();
}
