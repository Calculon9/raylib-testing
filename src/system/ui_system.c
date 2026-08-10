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
#include "system/ui_system.h"
#include "system/ui/lpanel_system.h"
#include "system/ui/state_manager_system.h"
#include "system/ui/utility_panel_system.h"
#include "system/ui/popup_menu.h"
#include "world/world.h"
#include "input/drag_interaction.h"
#include "system/systems.h"
#include "system/utility_system.h"
#include "system/viewport_system.h"
#include "system/debug_overlay_system.h"

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
const Size ui_small_horizontal_button_size = {{1.91f, 0.5f}, SIZE_FIXED};
const Size ui_fill_button_size = {{1.0f, 1.0f}, SIZE_FILL};
const Size ui_standard_container_size = {{1, 0.5}, SIZE_PERCENT};
const Size ui_fill_container_size = {{1.0f, 1.0f}, SIZE_FILL};
const Size ui_standard_selector_container_size = {{1.0f, 0.03f}, SIZE_PERCENT};
const Size ui_standard_selector_button_size = {{0.5f, 1.0f}, SIZE_PERCENT};
const Spacing ui_standard_stack_spacing = {{0.0f, 0.06f}, SIZE_FIXED, SPACING_STACKED};
const Spacing ui_compact_stack_spacing = {{0.0f, 0.025f}, SIZE_FIXED, SPACING_STACKED};
const Spacing ui_standard_wrap_spacing = {{0.06f, 0.06f}, SIZE_FIXED, SPACING_STACKED_WRAP};
const Spacing ui_compact_wrap_spacing = {{0.025f, 0.025f}, SIZE_FIXED, SPACING_STACKED_WRAP};
const Spacing ui_zero_horizontal_wrap_spacing = {{0.0f, 0.06f}, SIZE_FIXED, SPACING_STACKED_WRAP};
const Spacing ui_standard_inline_spacing = {{0.06f, 0.0f}, SIZE_FIXED, SPACING_INLINE};
const Spacing ui_zero_inline_spacing = {{0.0f, 0.0f}, NONE, SPACING_INLINE};

const UIPalette ui_classic_palette = {
    .panel_background = COLOUR_UI_INK_RGBA,
    .container_border = COLOUR_UI_TERRACOTTA_RGBA,
    .container_fill = COLOUR_UI_SAGE_RGBA,
    .field_row_border = WHITE_RGBA,
    .field_row_fill = COLOURLESS_RGBA,
    .input_border = COLOUR_UI_INK_RGBA,
    .input_fill = COLOUR_UI_PAPER_RGBA,
    .button_border = COLOUR_UI_INK_RGBA,
    .button_fill = COLOUR_UI_GOLD_RGBA,
    .text = COLOUR_UI_INK_RGBA,
    .label_text = WHITE_RGBA,
    .text_on_dark = COLOUR_UI_PAPER_RGBA,
    .error = RED_RGBA,
    .warning = YELLOW_WARNING_RGBA
};

const UIPalette ui_earth_palette = {
    .panel_background = COLOUR_UI_EARTH_TEAL_RGBA,
    .container_border = COLOUR_UI_EARTH_RUST_RGBA,
    .container_fill = COLOUR_UI_EARTH_OLIVE_RGBA,
    .field_row_border = COLOUR_UI_EARTH_BROWN_RGBA,
    .field_row_fill = COLOURLESS_RGBA,
    .input_border = COLOUR_UI_EARTH_TEAL_RGBA,
    .input_fill = COLOUR_UI_EARTH_CREAM_RGBA,
    .button_border = COLOUR_UI_EARTH_TEAL_RGBA,
    .button_fill = COLOUR_UI_EARTH_BROWN_RGBA,
    .text = COLOUR_UI_EARTH_TEAL_RGBA,
    .label_text = COLOUR_UI_EARTH_BROWN_RGBA,
    .text_on_dark = COLOUR_UI_EARTH_CREAM_RGBA,
    .error = RED_RGBA,
    .warning = YELLOW_WARNING_RGBA
};

const UIPalette ui_harbor_palette = {
    .panel_background = COLOUR_UI_HARBOR_NAVY_RGBA,
    .container_border = COLOUR_UI_HARBOR_CORAL_RGBA,
    .container_fill = COLOUR_UI_HARBOR_SEAFOAM_RGBA,
    .field_row_border = COLOUR_UI_HARBOR_AMBER_RGBA,
    .field_row_fill = COLOURLESS_RGBA,
    .input_border = COLOUR_UI_HARBOR_NAVY_RGBA,
    .input_fill = COLOUR_UI_HARBOR_SAND_RGBA,
    .button_border = COLOUR_UI_HARBOR_NAVY_RGBA,
    .button_fill = COLOUR_UI_HARBOR_AMBER_RGBA,
    .text = COLOUR_UI_HARBOR_NAVY_RGBA,
    .label_text = COLOUR_UI_HARBOR_AMBER_RGBA,
    .text_on_dark = COLOUR_UI_HARBOR_SAND_RGBA,
    .error = RED_RGBA,
    .warning = YELLOW_WARNING_RGBA
};

const UIPalette ui_meadow_palette = {
    .panel_background = COLOUR_UI_EARTH_OLIVE_RGBA,
    .container_border = COLOUR_UI_GOLD_RGBA,
    .container_fill = COLOUR_UI_SAGE_RGBA,
    .field_row_border = COLOUR_UI_EARTH_TEAL_RGBA,
    .field_row_fill = COLOURLESS_RGBA,
    .input_border = COLOUR_UI_EARTH_TEAL_RGBA,
    .input_fill = COLOUR_UI_PAPER_RGBA,
    .button_border = COLOUR_UI_EARTH_TEAL_RGBA,
    .button_fill = COLOUR_UI_GOLD_RGBA,
    .text = COLOUR_UI_INK_RGBA,
    .label_text = COLOUR_UI_EARTH_TEAL_RGBA,
    .text_on_dark = COLOUR_UI_PAPER_RGBA,
    .error = RED_RGBA,
    .warning = YELLOW_WARNING_RGBA
};

const UIPalette ui_default_palette = {
    .panel_background = COLOUR_UI_HARBOR_NAVY_RGBA,
    .container_border = COLOUR_UI_HARBOR_CORAL_RGBA,
    .container_fill = COLOUR_UI_HARBOR_SEAFOAM_RGBA,
    .field_row_border = COLOUR_UI_HARBOR_AMBER_RGBA,
    .field_row_fill = COLOURLESS_RGBA,
    .input_border = COLOUR_UI_HARBOR_NAVY_RGBA,
    .input_fill = COLOUR_UI_HARBOR_SAND_RGBA,
    .button_border = COLOUR_UI_HARBOR_NAVY_RGBA,
    .button_fill = COLOUR_UI_HARBOR_AMBER_RGBA,
    .text = COLOUR_UI_HARBOR_NAVY_RGBA,
    .label_text = COLOUR_UI_HARBOR_AMBER_RGBA,
    .text_on_dark = COLOUR_UI_HARBOR_SAND_RGBA,
    .error = RED_RGBA,
    .warning = YELLOW_WARNING_RGBA
};

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

static bool IsPointInViewportRegion(const ViewportRegion *region, int mouse_x, int mouse_y)
{
    if (!region)
    {
        return false;
    }

    return mouse_x >= region->pixel_origin.x &&
           mouse_x <= (region->pixel_origin.x + (region->pixel_u.x * region->resolution.x)) &&
           mouse_y >= region->pixel_origin.y &&
           mouse_y <= (region->pixel_origin.y + (region->pixel_v.y * region->resolution.y));
}

static void ClearString64(String64 *value)
{
    if (!value)
    {
        return;
    }

    value->string[0] = '\0';
}

void InitUI(void)
{
    // Init Global UI State
    G_UIState.focused_element = NULL;
    InitLPanel();
    InitUtilityPanel();
    InitRPanel();
    InitStateManagerSystem();
    InitPopupMenu();
    G_UIState.active_panel_view = LPANEL_STATE_VIEW;
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
    bool cursor_in_ui = IsPointInViewportRegion(&lpanel_viewport, mouse_x, mouse_y) ||
                        IsPointInViewportRegion(&rpanel_viewport, mouse_x, mouse_y) ||
                        IsPointInViewportRegion(&entity_panel_viewport, mouse_x, mouse_y) ||
                        IsPointInViewportRegion(&utility_panel_viewport, mouse_x, mouse_y) ||
                        (IsPopupMenuVisible() && popup_root &&
                         IsMouseOverElement(popup_root, (Vector2d){(float)mouse_x, (float)mouse_y}));
    bool cursor_in_game_viewport = IsPointInViewportRegion(&game_viewport, mouse_x, mouse_y);

    if (cursor_in_game_viewport && input->right_pressed)
    {
        if (IsPopupMenuVisible())
        {
            HidePopupMenu();
        }
        else
        {
            Vector2d popup_position = TransformCoordinates(
                game_viewport.tunnel.dest_to_source_mtx,
                (Vector2d){(float)mouse_x, (float)mouse_y});
            ShowPopupMenu(popup_position);
        }
        cursor_in_ui = true;
    }

    DragInteractionState *drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_UI);
    DragInteraction_BeginFrame(drag_ctx, input);
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
    DragInteraction_EndFrame(drag_ctx, input);
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

void UpdateUILogicalSpace()
{
}

void UpdateUISpace(UIElement *root_element, UIBox seed_box)
{
    if (!root_element)
    {
        return;
    }

    UI_LayoutSubtree(root_element, seed_box);
}

void UpdateGlobalUIState()
{
    UIElement *state_boxes[] = {
        G_UIState.state_id_tbox,
        G_UIState.state_mass_tbox,
        G_UIState.state_pos_tl_tbox,
        G_UIState.state_pos_c_tbox,
        G_UIState.state_vel_tbox,
        G_UIState.state_accel_tbox,
        G_UIState.state_moment_tbox,
    };

    UIElement *edit_boxes[] = {
        G_UIState.edit_vertice_count_tbox,
        G_UIState.edit_width_tbox,
        G_UIState.edit_height_tbox,
        G_UIState.edit_mass_tbox,
        G_UIState.edit_pos_c_tbox,
        G_UIState.edit_vel_tbox,
        G_UIState.edit_accel_tbox,
        G_UIState.edit_moment_tbox,
    };
    size_t state_box_count = sizeof(state_boxes) / sizeof(state_boxes[0]);
    size_t edit_box_count = sizeof(edit_boxes) / sizeof(edit_boxes[0]);

    // UPDATE STATISTICS
    float fps = frame_counter.fps;
    float ftime = frame_counter.delta_time * 1000;
    float bytes = GetCurrentMemoryAllocated() / 1024.0f;
    int polygs = GetNewtonoidCount();

    // Only update every 20 frames, unnecessary to do every frame
    if (frame_counter.total_frames % 20 == 0)
    {
        UpdateString64(G_UIState.stats_fps_str->string, "%.1f", fps);
        UpdateString64(G_UIState.stats_mem_str->string, "%.1f", bytes);
        UpdateString64(G_UIState.stats_polygs_str->string, "%d", polygs);
        UpdateString64(G_UIState.stats_ftime_str->string, "%.2f", ftime);
    }

    // DEBUG----
    if (frame_counter.total_frames % 900 == 0)
    {
        LOG_INFO("[Telemetry Update] FPS: %s  | F.TIME: %s | MEM: %sKB | POLY: %s\n",
                 G_UIState.stats_fps_str->string, G_UIState.stats_ftime_str->string,
                 G_UIState.stats_mem_str->string, G_UIState.stats_polygs_str->string);
    }
    // ----DEBUG //

    // COLLECT & UPDATE "SELECTED ENTITY" PROPERTIES
    Newtonoid2d *obj = G_UIState.selected_object;
    if (obj)
    {
        // Bind selected_object data to the Object Properties TextBoxes
        void *state_bindings[] = {
            &obj->id,
            &obj->mass,
            &obj->coords_origin,
            &obj->coords_center,
            &obj->velocity,
            &obj->acceleration,
            &obj->momentum,
        };
        BindTextboxGroup(state_boxes, state_bindings, state_box_count);

        // PIPELINE data to text only when the element is NOT focused
        // so that editing of the text by the user doesn't keep getting overwritten with the value stored in the object
        WriteTextboxNumberIfUnfocused(G_UIState.state_id_tbox, (float)obj->id, 0);
        WriteTextboxNumberIfUnfocused(G_UIState.state_mass_tbox, obj->mass, 2);
        WriteTextboxVectorIfUnfocused(G_UIState.state_pos_tl_tbox, obj->coords_origin);
        WriteTextboxVectorIfUnfocused(G_UIState.state_pos_c_tbox, obj->coords_center);
        WriteTextboxVectorIfUnfocused(G_UIState.state_vel_tbox, obj->velocity);
        WriteTextboxVectorIfUnfocused(G_UIState.state_accel_tbox, obj->acceleration);
        WriteTextboxVectorIfUnfocused(G_UIState.state_moment_tbox, obj->momentum);
    }
    else // Reset the bounded textbox output buffers AND unbind
    {
        ClearAndUnbindTextboxGroup(state_boxes, state_box_count);
    }
    UpdateStateManagerSelectedObject();

    // COLLECT & UPDATE SELECTED CELL PROPERTIES
    Cell *cell = G_UIState.selected_cell;
    if (cell)
    {
        int index = G_UIState.selected_cell_index;
        int occu = cell->occupancy;
        float val = cell->value;
        float fill = 0; // set to 0 for now

        UpdateString64(G_UIState.cell_id_str->string, "%d", index);
        UpdateString64(G_UIState.cell_occu_str->string, "%d", occu);
        UpdateString64(G_UIState.cell_value_str->string, "%0.1f", val);
        UpdateString64(G_UIState.cell_fill_str->string, "%0.1f", fill);
    }
    else
    {
        ClearString64(G_UIState.cell_id_str);
        ClearString64(G_UIState.cell_occu_str);
        ClearString64(G_UIState.cell_value_str);
        ClearString64(G_UIState.cell_fill_str);
    }

    // COLLECT & UPDATE EDITING ENTITY PROPERTIES
    // Determine if the Edit View is active.
    Newtonoid2dParams *params = G_UIState.newtonoid_params;
    if (G_UIState.active_panel_view == LPANEL_EDIT_ENTITY_VIEW && params)
    {
        // Bind selected_object data to the Object Properties TextBoxes
        void *edit_bindings[] = {
            &params->vertice_count,
            &params->width,
            &params->height,
            &params->mass,
            &params->coords_center,
            &params->velocity,
            &params->acceleration,
            &params->momentum,
        };
        BindTextboxGroup(edit_boxes, edit_bindings, edit_box_count);

        // PIPELINE data to text only when the element is NOT focused
        // so that editing of the text by the user doesn't keep getting overwritten with the value stored in the object
        WriteTextboxNumberIfUnfocused(G_UIState.edit_vertice_count_tbox, params->vertice_count, 0);
        WriteTextboxNumberIfUnfocused(G_UIState.edit_width_tbox, params->width, 2);
        WriteTextboxNumberIfUnfocused(G_UIState.edit_height_tbox, params->height, 2);
        WriteTextboxNumberIfUnfocused(G_UIState.edit_mass_tbox, params->mass, 2);
        WriteTextboxVectorIfUnfocused(G_UIState.edit_pos_c_tbox, params->coords_center);
        WriteTextboxVectorIfUnfocused(G_UIState.edit_vel_tbox, params->velocity);
        WriteTextboxVectorIfUnfocused(G_UIState.edit_accel_tbox, params->acceleration);
        WriteTextboxVectorIfUnfocused(G_UIState.edit_moment_tbox, params->momentum);
    }
    else
    {
        ClearAndUnbindTextboxGroup(edit_boxes, edit_box_count);
    }
}


