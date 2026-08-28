/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 **********************************************************************************************/
#include "raylib.h"
#include <stdint.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "camera/camera.h"
#include "ui/ui.h"
#include "ui/text_region.h"
#include "ui/ui_renderer.h"
#include "ui/ui_input.h"
#include "system/ui_system.h"
#include "system/ui/lpanel_system.h"
#include "system/ui/rpanel_system.h"
#include "system/ui/state_manager_system.h"
#include "system/panel_system.h"
#include "system/ui/utility_panel_system.h"
#include "system/ui/popup_menu.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "world/universe.h"
#include "input/drag_interaction.h"
#include "system/systems.h"
#include "system/utility_system.h"
#include "system/viewport_system.h"
#include "system/debug_overlay_system.h"
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
UIState G_UIState = {0};
bool ui_borders_enabled = false;
// ------------------TOTAL SCREEN-------------------------
// Logical->pixel-space conversion properties

// Default UI Properties
//Vector2d tfield_default_padding = {0.0, 0.0};
const Vector2d ui_standard_container_padding = {0.06, 0.06};
const Vector2d ui_standard_field_padding = {0.02f, 0.0f};
const Vector2d ui_standard_button_padding = {0.025, 0.025};
const Size ui_standard_control_size = {{4.65, 0.50}, SIZE_FIXED};
//const Size ui_standard_textbox_size = {{4.55f, 0.50f}, SIZE_FIXED};
// Give labels more room in inline text fields (label width = 1.0 - textbox width).
const Size ui_standard_textfield_input_size = {{0.62f, 1.0f}, SIZE_PERCENT};
const Size ui_standard_button_size = {{3.22f, 0.5f}, SIZE_FIXED};
const Size ui_wide_button_size = {{4.65f, 0.5f}, SIZE_FIXED};
const Size ui_small_horizontal_button_size = {{1.91f, 0.5f}, SIZE_FIXED};
const Size ui_fill_button_size = {{1.0f, 1.0f}, SIZE_FILL};
const Size ui_standard_container_size = {{1, 0.5}, SIZE_PERCENT};
const Size ui_fill_container_size = {{1.0f, 1.0f}, SIZE_FILL};
const Size ui_standard_selector_container_size = UI_SIZE_CONTENT;
const Size ui_standard_selector_button_size = {{1.75f, 0.5f}, SIZE_FIXED};
const Spacing ui_standard_stack_spacing = {{0.0f, 0.06f}, SIZE_FIXED, SPACING_STACKED};
const Spacing ui_compact_stack_spacing = {{0.0f, 0.025f}, SIZE_FIXED, SPACING_STACKED};
const Spacing ui_standard_stack_wrap_spacing = {{0.06f, 0.06f}, SIZE_FIXED, SPACING_STACKED_WRAP};
const Spacing ui_compact_stack_wrap_spacing = {{0.025f, 0.025f}, SIZE_FIXED, SPACING_STACKED_WRAP};
const Spacing ui_zero_x_stack_wrap_spacing = {{0.0f, 0.06f}, SIZE_FIXED, SPACING_STACKED_WRAP};
const Spacing ui_standard_inline_wrap_spacing = {{0.06f, 0.06f}, SIZE_FIXED, SPACING_INLINE_WRAP};
const Spacing ui_zero_x_inline_wrap_spacing = {{0.0f, 0.06f}, SIZE_FIXED, SPACING_INLINE_WRAP};
const Spacing ui_standard_inline_spacing = {{0.06f, 0.0f}, SIZE_FIXED, SPACING_INLINE};
const Spacing ui_zero_inline_spacing = {{0.0f, 0.0f}, NONE, SPACING_INLINE};

UIPalette ui_classic_palette = {0};
UIPalette ui_earth_palette = {0};
UIPalette ui_harbor_palette = {0};
UIPalette ui_meadow_palette = {0};
UIPalette ui_default_palette = {0};

static bool ui_palettes_initialized = false;
static UIStateSelectionChangedCallback ui_selection_changed_callback = NULL;
static void *ui_selection_changed_user_data = NULL;

static ColourRgba MakeColour(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    ColourRgba c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
    return c;
}

static void InitUIPalettes(void)
{
    if (ui_palettes_initialized)
    {
        return;
    }

    ui_classic_palette.panel_background = MakeColour(32, 42, 37, 255);
    ui_classic_palette.container_border = MakeColour(194, 105, 83, 255);
    ui_classic_palette.container_fill = MakeColour(111, 137, 119, 255);
    ui_classic_palette.field_row_border = MakeColour(255, 255, 255, 255);
    ui_classic_palette.field_row_fill = MakeColour(0, 0, 0, 0);
    ui_classic_palette.input_border = MakeColour(32, 42, 37, 255);
    ui_classic_palette.input_fill = MakeColour(245, 241, 230, 255);
    ui_classic_palette.button_border = MakeColour(32, 42, 37, 255);
    ui_classic_palette.button_fill = MakeColour(230, 194, 119, 255);
    ui_classic_palette.text = MakeColour(32, 42, 37, 255);
    ui_classic_palette.label_text = MakeColour(255, 255, 255, 255);
    ui_classic_palette.text_on_dark = MakeColour(245, 241, 230, 255);
    ui_classic_palette.error = MakeColour(230, 41, 55, 255);
    ui_classic_palette.warning = MakeColour(255, 255, 176, 255);

    ui_earth_palette.panel_background = MakeColour(38, 101, 115, 255);
    ui_earth_palette.container_border = MakeColour(191, 78, 36, 255);
    ui_earth_palette.container_fill = MakeColour(121, 120, 49, 255);
    ui_earth_palette.field_row_border = MakeColour(166, 109, 60, 255);
    ui_earth_palette.field_row_fill = MakeColour(0, 0, 0, 0);
    ui_earth_palette.input_border = MakeColour(38, 101, 115, 255);
    ui_earth_palette.input_fill = MakeColour(242, 211, 172, 255);
    ui_earth_palette.button_border = MakeColour(38, 101, 115, 255);
    ui_earth_palette.button_fill = MakeColour(166, 109, 60, 255);
    ui_earth_palette.text = MakeColour(38, 101, 115, 255);
    ui_earth_palette.label_text = MakeColour(166, 109, 60, 255);
    ui_earth_palette.text_on_dark = MakeColour(242, 211, 172, 255);
    ui_earth_palette.error = MakeColour(230, 41, 55, 255);
    ui_earth_palette.warning = MakeColour(255, 255, 176, 255);

    ui_harbor_palette.panel_background = MakeColour(28, 58, 67, 255);
    ui_harbor_palette.container_border = MakeColour(218, 117, 96, 255);
    ui_harbor_palette.container_fill = MakeColour(86, 139, 127, 255);
    ui_harbor_palette.field_row_border = MakeColour(220, 158, 70, 255);
    ui_harbor_palette.field_row_fill = MakeColour(0, 0, 0, 0);
    ui_harbor_palette.input_border = MakeColour(28, 58, 67, 255);
    ui_harbor_palette.input_fill = MakeColour(239, 221, 184, 255);
    ui_harbor_palette.button_border = MakeColour(28, 58, 67, 255);
    ui_harbor_palette.button_fill = MakeColour(220, 158, 70, 255);
    ui_harbor_palette.text = MakeColour(28, 58, 67, 255);
    ui_harbor_palette.label_text = MakeColour(220, 158, 70, 255);
    ui_harbor_palette.text_on_dark = MakeColour(239, 221, 184, 255);
    ui_harbor_palette.error = MakeColour(230, 41, 55, 255);
    ui_harbor_palette.warning = MakeColour(255, 255, 176, 255);

    ui_meadow_palette.panel_background = MakeColour(121, 120, 49, 255);
    ui_meadow_palette.container_border = MakeColour(230, 194, 119, 255);
    ui_meadow_palette.container_fill = MakeColour(111, 137, 119, 255);
    ui_meadow_palette.field_row_border = MakeColour(38, 101, 115, 255);
    ui_meadow_palette.field_row_fill = MakeColour(0, 0, 0, 0);
    ui_meadow_palette.input_border = MakeColour(38, 101, 115, 255);
    ui_meadow_palette.input_fill = MakeColour(245, 241, 230, 255);
    ui_meadow_palette.button_border = MakeColour(38, 101, 115, 255);
    ui_meadow_palette.button_fill = MakeColour(230, 194, 119, 255);
    ui_meadow_palette.text = MakeColour(32, 42, 37, 255);
    ui_meadow_palette.label_text = MakeColour(38, 101, 115, 255);
    ui_meadow_palette.text_on_dark = MakeColour(245, 241, 230, 255);
    ui_meadow_palette.error = MakeColour(230, 41, 55, 255);
    ui_meadow_palette.warning = MakeColour(255, 255, 176, 255);

    ui_default_palette = ui_harbor_palette;
    ui_palettes_initialized = true;
}

void UIPalette_GetSurfaceColours(const UIPalette *palette, UIPaletteSurface surface,
                                 ColourRgba *out_border, ColourRgba *out_fill)
{
    ColourRgba border = COLOURLESS_RGBA;
    ColourRgba fill = COLOURLESS_RGBA;

    if (!palette)
    {
        palette = &ui_default_palette;
    }

    switch (surface)
    {
        case UI_PALETTE_SURFACE_CONTAINER:
            border = palette->container_border;
            fill = palette->container_fill;
            break;
        case UI_PALETTE_SURFACE_FIELD_ROW:
            border = palette->field_row_border;
            fill = palette->field_row_fill;
            break;
        case UI_PALETTE_SURFACE_INPUT:
            border = palette->input_border;
            fill = palette->input_fill;
            break;
        case UI_PALETTE_SURFACE_BUTTON:
            border = palette->button_border;
            fill = palette->button_fill;
            break;
        case UI_PALETTE_SURFACE_TRANSPARENT:
        default:
            break;
    }

    if (out_border)
    {
        *out_border = border;
    }
    if (out_fill)
    {
        *out_fill = fill;
    }
}

// UI Elements

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------

void UpdateGlobalUIState();

static void ClearString64(String64 *value)
{
    if (!value)
    {
        return;
    }

    value->string[0] = '\0';
}

void UIState_SetSelectedObject(Newtonoid2d *object)
{
    // Centralise object selection writes so other systems stop mutating the UI state directly.
    UIState_SetSelection(object, G_UIState.selected_cell, G_UIState.selected_cell_index);
}

void UIState_SetSelectedObjectById(EntityId entity_id)
{
    // Resolve by stable ID at write time to avoid leaking cross-module pointer lookup details.
    if (entity_id == INVALID_ENTITY_ID)
    {
        UIState_SetSelectedObject(NULL);
        return;
    }

    Newtonoid2d *selected_object = Universe_GetEntityByID(&G_Universe, entity_id, NULL);
    UIState_SetSelectedObject(selected_object);
}

void UIState_SetSelection(Newtonoid2d *object, Cell *cell, int cell_index)
{
    // Keep object and cell selection updates in one write path to avoid partial state updates.
    EntityId prev_selected_object_id = G_UIState.selected_object_id;
    Newtonoid2d *prev_selected_object = G_UIState.selected_object;
    Cell *prev_selected_cell = G_UIState.selected_cell;
    int prev_selected_cell_index = G_UIState.selected_cell_index;

    G_UIState.selected_object_id = object ? object->id : INVALID_ENTITY_ID;
    G_UIState.selected_object = object;
    G_UIState.selected_cell = cell;
    G_UIState.selected_cell_index = cell_index;

    bool selection_changed = prev_selected_object_id != G_UIState.selected_object_id ||
                            prev_selected_object != G_UIState.selected_object ||
                            prev_selected_cell != G_UIState.selected_cell ||
                            prev_selected_cell_index != G_UIState.selected_cell_index;
    if (selection_changed && ui_selection_changed_callback)
    {
        ui_selection_changed_callback(G_UIState.selected_object_id,
                                      G_UIState.selected_object,
                                      G_UIState.selected_cell,
                                      G_UIState.selected_cell_index,
                                      ui_selection_changed_user_data);
    }
}

void UIState_SetSelectionChangedCallback(UIStateSelectionChangedCallback callback, void *user_data)
{
    ui_selection_changed_callback = callback;
    ui_selection_changed_user_data = user_data;
}

void UIState_ClearSelectedObject(void)
{
    UIState_SetSelectedObject(NULL);
}

void UIState_ValidateSelection(void)
{
    // Keep pointer state synchronised with the selected ID to prevent stale references after deletes/moves.
    if (G_UIState.selected_object_id == INVALID_ENTITY_ID)
    {
        if (G_UIState.selected_object)
        {
            UIState_SetSelectedObject(NULL);
        }
        return;
    }

    Newtonoid2d *selected_object = Universe_GetEntityByID(&G_Universe, G_UIState.selected_object_id, NULL);
    if (!selected_object)
    {
        UIState_SetSelectedObject(NULL);
        return;
    }

    if (selected_object != G_UIState.selected_object)
    {
        UIState_SetSelectedObject(selected_object);
    }
}

Newtonoid2d *UIState_GetSelectedObject(void)
{
    // Always validate before exposing the pointer to callers.
    UIState_ValidateSelection();
    return G_UIState.selected_object;
}

EntityId UIState_GetSelectedObjectId(void)
{
    return G_UIState.selected_object_id;
}

void UIState_SetSelectedCell(Cell *cell, int cell_index)
{
    // Keep cell pointer and index updates atomic from the caller's perspective.
    UIState_SetSelection(G_UIState.selected_object, cell, cell_index);
}

void UIState_ClearSelectedCell(void)
{
    UIState_SetSelectedCell(NULL, -1);
}

Cell *UIState_GetSelectedCell(void)
{
    return G_UIState.selected_cell;
}

int UIState_GetSelectedCellIndex(void)
{
    return G_UIState.selected_cell_index;
}

void InitUI(void)
{
    InitUIPalettes();

    // Init Global UI State
    UIState_SetSelection(NULL, NULL, -1);
    G_UIState.focused_element = NULL;
    InitLPanel();
    InitUtilityPanel();
    InitRPanel();
    InitStateManagerSystem();
    InitPopupMenu();
    G_UIState.active_panel_view = LPANEL_STATE_VIEW;
}

// Tears down every UI tree, panel, transient input state and drag capture.
void DestroyUI(void)
{
    // Remove text/focus state before any elements are freed to avoid stale pointers.
    ResetUIInputState();

    // Drop any in-flight drag capture so freed elements are not referenced.
    DragInteraction_ResetContext(DRAG_CONTEXT_UI);
    DragInteraction_ResetContext(DRAG_CONTEXT_GAME);

    // Clear popup visibility and submenu state while its elements are still valid.
    HidePopupMenu();

    // Each panel owns and disposes its root, views, selectors, and cached pointers.
    DestroyLPanel();
    DestroyRPanel();
    DestroyUtilityPanel();
    DestroyPopupMenu();
    DestroyStateManagerSystem();

    // Destroy the global element pool so the next allocation creates a clean pool.
    Pool *ui_element_pool = GetUIElementPool();
    if (ui_element_pool)
    {
        PoolDestroy(ui_element_pool);
        SetUIElementPool(NULL);
    }

}

// Rebuilds the complete UI from the same initialisation path used at startup.
void ResetUI(void)
{
    // Capture memory snapshots around each phase so reset-time growth can be attributed precisely.
    size_t bytes_before = GetCurrentMemoryAllocated();

    DestroyUI();

    size_t bytes_after_destroy = GetCurrentMemoryAllocated();
    printf("[UI] Reset destroy phase: %.2f kB -> %.2f kB (delta %+0.2f kB)\n",
           (double)bytes_before / 1024.0,
           (double)bytes_after_destroy / 1024.0,
           ((double)bytes_after_destroy - (double)bytes_before) / 1024.0);

    InitUI();

    size_t bytes_after_init = GetCurrentMemoryAllocated();
    printf("[UI] Reset init phase: %.2f kB -> %.2f kB (delta %+0.2f kB)\n",
           (double)bytes_after_destroy / 1024.0,
           (double)bytes_after_init / 1024.0,
           ((double)bytes_after_init - (double)bytes_after_destroy) / 1024.0);
}

InputRouteResult UpdateUISystem(const InputFrame *input)
{
    if (!input)
    {
        return INPUT_ROUTE_IGNORED;
    }

    int mouse_x = (int)input->pointer_position.x;
    int mouse_y = (int)input->pointer_position.y;
    UIElement *popup_root = GetPopupMenuRoot();
    bool cursor_in_ui = ViewportRegion_ContainsPixel(&lpanel_viewport, input->pointer_position) ||
                        ViewportRegion_ContainsPixel(&rpanel_viewport, input->pointer_position) ||
                        ViewportRegion_ContainsPixel(&entity_panel_viewport, input->pointer_position) ||
                        ViewportRegion_ContainsPixel(&utility_panel_viewport, input->pointer_position) ||
                        (IsPopupMenuVisible() && popup_root &&
                         IsMouseOverElement(popup_root, (Vector2d){(float)mouse_x, (float)mouse_y}));
    bool cursor_in_game_viewport = ViewportRegion_ContainsPixel(&game_viewport, input->pointer_position);

    // Any click outside the UI should drop UI keyboard focus so world/game actions can proceed.
    if (!cursor_in_ui && (input->left_pressed || input->right_pressed))
    {
        ClearUIFocus();
    }

    if (cursor_in_game_viewport && input->right_pressed)
    {
        if (IsPopupMenuVisible())
        {
            HidePopupMenu();
        }
        else
        {
            Vector2d popup_position = ResolvePixelToWorldFrame(
                &G_Universe.root_world,
                (Vector2d){(float)mouse_x, (float)mouse_y});
            ShowPopupMenu(popup_position);
        }
        cursor_in_ui = true;
    }

    DragInteractionState *drag_ctx = DragInteraction_BeginContextFrame(DRAG_CONTEXT_UI, input);
    ProcessUIInput(input, cursor_in_ui);
    InputRouteResult result = INPUT_ROUTE_IGNORED;
    if (drag_ctx->has_capture && (input->left_down || input->left_released))
    {
        result = INPUT_ROUTE_CAPTURED;
    }
    else if (cursor_in_ui)
    {
        result = INPUT_ROUTE_BLOCKED;
    }
    DragInteraction_EndContextFrame(DRAG_CONTEXT_UI, input);
    UpdateGlobalUIState();
    return result;
}

void DrawUI()
{
    DrawLPanel();
    DrawRPanel();
    DrawStateManagerSystem();
    DrawUtilityPanel();
    DrawPopupMenu();
}

void UpdateUISpace(UIElement *root_element, UIBox seed_box)
{
    if (!root_element)
    {
        return;
    }

    UI_LayoutSubtree(root_element, seed_box);
}

// Write telemetry strings for visible stats labels without assuming every label exists.
static void UpdateTelemetryStats(float fps, float frame_time_ms, float kib_allocated,
                                 int newtonoid_count)
{
    // Only update every 20 frames to avoid unnecessary string formatting churn.
    if (frame_counter.total_frames % 20 != 0)
    {
        return;
    }

    if (G_UIState.stats_fps_str)
    {
        UpdateString64(G_UIState.stats_fps_str->string, "%.1f", fps);
    }
    if (G_UIState.stats_mem_str)
    {
        UpdateString64(G_UIState.stats_mem_str->string, "%.1f", kib_allocated);
    }
    if (G_UIState.stats_polygs_str)
    {
        UpdateString64(G_UIState.stats_polygs_str->string, "%d", newtonoid_count);
    }
    if (G_UIState.stats_ftime_str)
    {
        UpdateString64(G_UIState.stats_ftime_str->string, "%.2f", frame_time_ms);
    }
}

// Sync entity readout textboxes from the current selected object or clear when none is selected.
static void RefreshSelectedObjectFields(const Newtonoid2d *selected_object)
{
    TextboxField state_fields[] = {
        {G_UIState.state_id_tbox, INT, selected_object ? (void *)&selected_object->id : NULL, 0, NULL},
        {G_UIState.state_mass_tbox, FLOAT, selected_object ? (void *)&selected_object->mass : NULL, 2, NULL},
        {G_UIState.state_pos_tl_tbox, VECTOR2D, selected_object ? (void *)&selected_object->bounds_origin : NULL, 0, NULL},
        {G_UIState.state_pos_c_tbox, VECTOR2D, selected_object ? (void *)&selected_object->anchor_position : NULL, 0, NULL},
        {G_UIState.state_vel_tbox, VECTOR2D, selected_object ? (void *)&selected_object->velocity : NULL, 0, NULL},
        {G_UIState.state_accel_tbox, VECTOR2D, selected_object ? (void *)&selected_object->acceleration : NULL, 0, NULL},
        {G_UIState.state_moment_tbox, VECTOR2D, selected_object ? (void *)&selected_object->momentum : NULL, 0, NULL},
        {G_UIState.state_angular_velocity_tbox, FLOAT, selected_object ? (void *)&selected_object->angular_velocity : NULL, 2, NULL},
        {G_UIState.state_angular_acceleration_tbox, FLOAT, selected_object ? (void *)&selected_object->angular_acceleration : NULL, 2, NULL},
        {G_UIState.state_health_tbox, FLOAT, selected_object ? (void *)&selected_object->health : NULL, 2, NULL},
        {G_UIState.state_max_health_tbox, FLOAT, selected_object ? (void *)&selected_object->max_health : NULL, 2, NULL},
        {G_UIState.state_damage_tbox, FLOAT, selected_object ? (void *)&selected_object->damage : NULL, 2, NULL},
    };

    RefreshTextboxFields(state_fields, ARRAY_COUNT(state_fields));
}

// Sync selected-cell labels and clear them when no cell is selected.
static void RefreshSelectedCellFields(const Cell *selected_cell)
{
    if (selected_cell)
    {
        int index = UIState_GetSelectedCellIndex();
        int occupancy = selected_cell->occupancy;
        float value = selected_cell->value;
        float fill = 0.0f;

        if (G_UIState.cell_id_str)
        {
            UpdateString64(G_UIState.cell_id_str->string, "%d", index);
        }
        if (G_UIState.cell_occu_str)
        {
            UpdateString64(G_UIState.cell_occu_str->string, "%d", occupancy);
        }
        if (G_UIState.cell_value_str)
        {
            UpdateString64(G_UIState.cell_value_str->string, "%0.1f", value);
        }
        if (G_UIState.cell_fill_str)
        {
            UpdateString64(G_UIState.cell_fill_str->string, "%0.1f", fill);
        }

        return;
    }

    ClearString64(G_UIState.cell_id_str);
    ClearString64(G_UIState.cell_occu_str);
    ClearString64(G_UIState.cell_value_str);
    ClearString64(G_UIState.cell_fill_str);
}

// Sync entity-creation editor textboxes when the draw view is active, otherwise clear them.
static void RefreshEntityEditorFields(bool editor_active, Newtonoid2dParams *params)
{
    TextboxField edit_fields[] = {
        {G_UIState.edit_vertice_count_tbox, INT, (editor_active && params) ? (void *)&params->vertice_count : NULL, 0, NULL},
        {G_UIState.edit_width_tbox, FLOAT, (editor_active && params) ? (void *)&params->width : NULL, 2, NULL},
        {G_UIState.edit_height_tbox, FLOAT, (editor_active && params) ? (void *)&params->height : NULL, 2, NULL},
        {G_UIState.edit_mass_tbox, FLOAT, (editor_active && params) ? (void *)&params->mass : NULL, 2, NULL},
        {G_UIState.edit_pos_c_tbox, VECTOR2D, (editor_active && params) ? (void *)&params->anchor_position : NULL, 0, NULL},
        {G_UIState.edit_vel_tbox, VECTOR2D, (editor_active && params) ? (void *)&params->velocity : NULL, 0, NULL},
        {G_UIState.edit_accel_tbox, VECTOR2D, (editor_active && params) ? (void *)&params->acceleration : NULL, 0, NULL},
        {G_UIState.edit_moment_tbox, VECTOR2D, (editor_active && params) ? (void *)&params->momentum : NULL, 0, NULL},
    };

    RefreshTextboxFields(edit_fields, ARRAY_COUNT(edit_fields));
}

void UpdateGlobalUIState()
{
    // UPDATE STATISTICS
    float fps = frame_counter.fps;
    float ftime = frame_counter.delta_time * 1000;
    float bytes = GetCurrentMemoryAllocated() / 1024.0f;
    int polygs = GetNewtonoidCount();

    UpdateTelemetryStats(fps, ftime, bytes, polygs);

    // DEBUG----
    if (frame_counter.total_frames % 900 == 0)
    {
        const char *fps_text = G_UIState.stats_fps_str ? G_UIState.stats_fps_str->string : "N/A";
        const char *ftime_text = G_UIState.stats_ftime_str ? G_UIState.stats_ftime_str->string : "N/A";
        const char *mem_text = G_UIState.stats_mem_str ? G_UIState.stats_mem_str->string : "N/A";
        const char *poly_text = G_UIState.stats_polygs_str ? G_UIState.stats_polygs_str->string : "N/A";

        LOG_INFO("[Telemetry Update] FPS: %s  | F.TIME: %s | MEM: %sKB | POLY: %s\n",
                 fps_text, ftime_text, mem_text, poly_text);
    }
    // ----DEBUG //

    // COLLECT & UPDATE "SELECTED ENTITY" PROPERTIES
    // Read via the validated accessor so bindings never use stale entity pointers.
    Newtonoid2d *obj = UIState_GetSelectedObject();
    RefreshSelectedObjectFields(obj);

    // COLLECT & UPDATE SELECTED CELL PROPERTIES
    Cell *cell = UIState_GetSelectedCell();
    RefreshSelectedCellFields(cell);

    // COLLECT & UPDATE EDITING ENTITY PROPERTIES
    // Determine if the Edit View is active.
    Newtonoid2dParams *params = G_UIState.newtonoid_params;
    bool edit_view_active = G_UIState.active_panel_view == LPANEL_DRAW_VIEW;
    RefreshEntityEditorFields(edit_view_active, params);
}


