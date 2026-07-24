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
#include "system/lpanel_system.h"
#include "world/world.h"
#include "system/systems.h"
#include "system/utility_system.h"
#include "system/viewport_system.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
UIState G_UIState = {0};
// ------------------TOTAL SCREEN-------------------------
// Logical->pixel-space conversion properties

// Default UI Properties
Offset tbox_tlabel_default_offset = {{0.03, 0}, OFFSET_FIXED};
Vector2d tbox_default_padding = {0.00, 0.00};
Vector2d tlabel_default_padding = {0.00, 0.00};
Vector2d tfield_default_padding = {0.01, 0.01};
Vector2d tcont_default_padding = {0.06, 0.06};
Vector2d btn_default_padding = {0.025, 0.025};
ColourRgba btn_default_colour_border = {188, 108, 37, 255};
ColourRgba btn_default_colour_fill = {96, 108, 56, 255};
ColourRgba tbox_default_colour_border = {40, 54, 24, 255}; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
ColourRgba tbox_default_colour_fill = {254, 250, 224, 255};  // COLOUR_PANEL_DARK_1;
ColourRgba tcont_default_colour_fill = {96, 108, 56, 255};
ColourRgba tcont_default_colour_border = {188, 108, 37, 255}; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
ColourRgba tfield_default_colour_fill = {0, 0, 0, 0};
Size tfield_default_size = {{6, 0.5}, SIZE_FIXED};
// Give labels more room in inline text fields (label width = 1.0 - textbox width).
Size tbox_default_size = {{0.45, 1}, SIZE_PERCENT};
Size btn_default_size = {{6, 0.5}, SIZE_FIXED}; // Assuming it's in a container
// Size btn_default_size = {{1, 1}, SIZE_PERCENT};    // Assuming it's in a container
Size tcont_default_size = {{1, 0.5}, SIZE_PERCENT};
Spacing tcont_default_child_spacing = {{0, 0.015}, PERCENT, SPACING_STACKED};
Spacing cont_default_child_spacing = {{0, 0.015}, PERCENT, SPACING_STACKED};
Spacing btn_cont_default_child_spacing = {{0.0, 0.0}, PERCENT, SPACING_NONE};

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

void InitUI(void)
{
    // Init Global UI State
    G_UIState.focused_element = NULL;
    InitLPanel();
    //InitRPanel();
    // G_UIState.active_panel_view = LPANEL_STATE_VIEW;
}

void UpdateUISystem(int mouse_x, int mouse_y)
{
    bool cursor_in_lpanel = mouse_x >= lpanel_pixel_origin.x && mouse_x <= (lpanel_pixel_origin.x + (lpanel_pixel_u.x * lpanel_viewport_resolution.x)) &&
                            mouse_y >= lpanel_pixel_origin.y && mouse_y <= (lpanel_pixel_origin.y + (lpanel_pixel_v.y * lpanel_viewport_resolution.y));
    bool cursor_in_rpanel = mouse_x >= rpanel_pixel_origin.x && mouse_x <= (rpanel_pixel_origin.x + (rpanel_pixel_u.x * rpanel_viewport_resolution.x)) &&
                            mouse_y >= rpanel_pixel_origin.y && mouse_y <= (rpanel_pixel_origin.y + (rpanel_pixel_v.y * rpanel_viewport_resolution.y));
    bool cursor_in_ui = cursor_in_lpanel || cursor_in_rpanel;

    // Send useful data to the Dispatcher for it triage and process/update affected elements
    ProcessUIInput(mouse_x, mouse_y, cursor_in_ui);
    UpdateGlobalUIState();
}

void DrawUI()
{
    DrawLPanel();
}

void UpdateUILogicalSpace()
{
    // Update Left Panel space
    
}

void UpdateUISpace(UIElement *root_element, UIBox seed_box)
{
    // Resolve logical units
    if (!root_element)
    {
        return;
    }

    // Resolve space position and dimensions of panel element
    // UIBox box = ResolveElementBox(root_element, seed_box);
    // root_element->cached_box = box;

    DistributeChildrenRecursiveResolved(root_element, seed_box);

    // frame_counter.total_frames % 800 == 0 ? printf("DREW [%s] Pos: (%.1f, %.1f) | Size: (%.1f, %.1f)\n", GetElementTypeName(root_element->type), box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y) : (void)0;

    // Recursively draw children
    // UIElement *child = root_element->first_child;
    // while (child)
    // {
    //     UpdateUISpaceElement(child, seed_box);
    //     child = child->next_sibling;
    // }
}

// static void UpdateUISpaceElement(UIElement *e, UIBox parent_box)
// {
//     // Resolve logical units
//     if (!e)
//     {
//         return;
//     }

//     // Need to convert world coordinates --> viewport --> screen coordinates
//     Space2d panel_space = e->data.root.space;
//     //Vector2d origin = panel_space.frame.origin_in_parent;    

//     // Resolve rendered position and dimensions of panel element
//     UIBox box = ResolveElementBox(e, seed_box);
//     e->cached_box = box;

//     DistributeChildrenRecursiveResolved(e, box);

//     // frame_counter.total_frames % 800 == 0 ? printf("DREW [%s] Pos: (%.1f, %.1f) | Size: (%.1f, %.1f)\n", GetElementTypeName(root_element->type), box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y) : (void)0;

//     // Recursively draw children
//     UIElement *child = e->first_child;
//     while (child)
//     {
//         UpdateLPanelSpace(child, box);
//         child = child->next_sibling;
//     }
// }

void UpdateGlobalUIState()
{
    // UPDATE STATISTICS
    float fps = frame_counter.fps;
    float ftime = frame_counter.delta_time * 1000;
    float bytes = GetCurrentMemoryAllocated() / 1024.0f;
    int polygs = GetNewtonoidCount();

    // Only update every 20 frames, unnecessary to do every frame
    if (frame_counter.total_frames % 20 == 0)
    {
        snprintf(G_UIState.lpanel_stats_fps_str->string, sizeof(String64), "%.1f", fps);
        snprintf(G_UIState.lpanel_stats_mem_str->string, sizeof(String64), "%.1f", bytes);
        snprintf(G_UIState.lpanel_stats_polygs_str->string, sizeof(String64), "%d", polygs);
        snprintf(G_UIState.lpanel_stats_ftime_str->string, sizeof(String64), "%.1f", ftime);
    }

    // DEBUG----
    if (frame_counter.total_frames % 900 == 0)
    {
        LOG_INFO("[Telemetry Update] FPS: %s  | F.TIME: %s | MEM: %sKB | POLY: %s\n",
                 G_UIState.lpanel_stats_fps_str->string, G_UIState.lpanel_stats_ftime_str->string,
                 G_UIState.lpanel_stats_mem_str->string, G_UIState.lpanel_stats_polygs_str->string);
    }
    // ----DEBUG //

    // COLLECT & UPDATE "SELECTED ENTITY" PROPERTIES
    Newtonoid2d *obj = G_WorldState.selected_object;
    if (obj)
    {
        // Bind selected_object data to the Object Properties TextBoxes
        UIElement *state_boxes[] = {
            G_UIState.lpanel_entity_state_id_tbox,
            G_UIState.lpanel_entity_state_mass_tbox,
            G_UIState.lpanel_entity_state_pos_tl_tbox,
            G_UIState.lpanel_entity_state_pos_c_tbox,
            G_UIState.lpanel_entity_state_vel_tbox,
            G_UIState.lpanel_entity_state_accel_tbox,
            G_UIState.lpanel_entity_state_moment_tbox,
        };
        void *state_bindings[] = {
            &obj->id,
            &obj->mass,
            &obj->coords_origin,
            &obj->coords_center,
            &obj->velocity,
            &obj->acceleration,
            &obj->momentum,
        };
        BindTextboxGroup(state_boxes, state_bindings, sizeof(state_boxes) / sizeof(state_boxes[0]));

        // PIPELINE data to text only when the element is NOT focused
        // so that editing of the text by the user doesn't keep getting overwritten with the value stored in the object
        WriteTextboxNumberIfUnfocused(G_UIState.lpanel_entity_state_id_tbox, (float)obj->id, 0);
        WriteTextboxNumberIfUnfocused(G_UIState.lpanel_entity_state_mass_tbox, obj->mass, 2);
        WriteTextboxVectorIfUnfocused(G_UIState.lpanel_entity_state_pos_tl_tbox, obj->coords_origin);
        WriteTextboxVectorIfUnfocused(G_UIState.lpanel_entity_state_pos_c_tbox, obj->coords_center);
        WriteTextboxVectorIfUnfocused(G_UIState.lpanel_entity_state_vel_tbox, obj->velocity);
        WriteTextboxVectorIfUnfocused(G_UIState.lpanel_entity_state_accel_tbox, obj->acceleration);
        WriteTextboxVectorIfUnfocused(G_UIState.lpanel_entity_state_moment_tbox, obj->momentum);
    }
    else // Reset the bounded textbox output buffers AND unbind
    {
        UIElement *state_boxes[] = {
            G_UIState.lpanel_entity_state_id_tbox,
            G_UIState.lpanel_entity_state_mass_tbox,
            G_UIState.lpanel_entity_state_pos_tl_tbox,
            G_UIState.lpanel_entity_state_pos_c_tbox,
            G_UIState.lpanel_entity_state_vel_tbox,
            G_UIState.lpanel_entity_state_accel_tbox,
            G_UIState.lpanel_entity_state_moment_tbox,
        };
        ClearAndUnbindTextboxGroup(state_boxes, sizeof(state_boxes) / sizeof(state_boxes[0]));
    }

    // COLLECT & UPDATE SELECTED CELL PROPERTIES
    Cell *cell = G_WorldState.selected_cell;
    if (cell)
    {
        int index = G_WorldState.selected_cell_index;
        int occu = cell->occupancy;
        float val = cell->value;
        float fill = 0; // set to 0 for now

        // Write to selected_cell data to the Cell State TextBoxes via Global State
        snprintf(G_UIState.lpanel_cell_state_id_str->string, sizeof(String64), "%d", index);
        snprintf(G_UIState.lpanel_cell_state_occu_str->string, sizeof(String64), "%d", occu);
        snprintf(G_UIState.lpanel_cell_state_value_str->string, sizeof(String64), "%0.1f", val);
        snprintf(G_UIState.lpanel_cell_state_fill_str->string, sizeof(String64), "%0.1f", fill);
    }
    else
    {
        ClearString64(G_UIState.lpanel_cell_state_id_str);
        ClearString64(G_UIState.lpanel_cell_state_occu_str);
        ClearString64(G_UIState.lpanel_cell_state_value_str);
        ClearString64(G_UIState.lpanel_cell_state_fill_str);
    }

    // COLLECT & UPDATE EDITING ENTITY PROPERTIES
    // Determine if the Edit View is active.
    Newtonoid2dParams *params = G_WorldState.newtonoid_params;
    if (G_UIState.active_panel_view == LPANEL_EDIT_ENTITY_VIEW && params)
    {
        // Bind selected_object data to the Object Properties TextBoxes
        // G_UIState.lpanel_entity_edit_edge_count_tbox->data.textbox.data_bind = &params->edge_count;
        UIElement *edit_boxes[] = {
            G_UIState.lpanel_entity_edit_vertice_count_tbox,
            G_UIState.lpanel_entity_edit_width_tbox,
            G_UIState.lpanel_entity_edit_height_tbox,
            G_UIState.lpanel_entity_edit_mass_tbox,
            G_UIState.lpanel_entity_edit_pos_c_tbox,
            G_UIState.lpanel_entity_edit_vel_tbox,
            G_UIState.lpanel_entity_edit_accel_tbox,
            G_UIState.lpanel_entity_edit_moment_tbox,
        };
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
        BindTextboxGroup(edit_boxes, edit_bindings, sizeof(edit_boxes) / sizeof(edit_boxes[0]));

        // PIPELINE data to text only when the element is NOT focused
        // so that editing of the text by the user doesn't keep getting overwritten with the value stored in the object
        // if (!G_UIState.lpanel_entity_edit_edge_count_tbox->is_focused)
        //     PipelineNumberToText(params->edge_count, 0, G_UIState.lpanel_entity_edit_edge_count_tbox->data.textbox.text.string, str_64);
        WriteTextboxNumberIfUnfocused(G_UIState.lpanel_entity_edit_vertice_count_tbox, params->vertice_count, 0);
        WriteTextboxNumberIfUnfocused(G_UIState.lpanel_entity_edit_width_tbox, params->width, 2);
        WriteTextboxNumberIfUnfocused(G_UIState.lpanel_entity_edit_height_tbox, params->height, 2);
        WriteTextboxNumberIfUnfocused(G_UIState.lpanel_entity_edit_mass_tbox, params->mass, 2);
        WriteTextboxVectorIfUnfocused(G_UIState.lpanel_entity_edit_pos_c_tbox, params->coords_center);
        WriteTextboxVectorIfUnfocused(G_UIState.lpanel_entity_edit_vel_tbox, params->velocity);
        WriteTextboxVectorIfUnfocused(G_UIState.lpanel_entity_edit_accel_tbox, params->acceleration);
        WriteTextboxVectorIfUnfocused(G_UIState.lpanel_entity_edit_moment_tbox, params->momentum);
    }
    else
    {
        // Reset the bounded textbox output buffers
        // G_UIState.lpanel_entity_edit_edge_count_tbox->data.textbox.text.string[0] = '\0';
        UIElement *edit_boxes[] = {
            G_UIState.lpanel_entity_edit_vertice_count_tbox,
            G_UIState.lpanel_entity_edit_width_tbox,
            G_UIState.lpanel_entity_edit_height_tbox,
            G_UIState.lpanel_entity_edit_mass_tbox,
            G_UIState.lpanel_entity_edit_pos_c_tbox,
            G_UIState.lpanel_entity_edit_vel_tbox,
            G_UIState.lpanel_entity_edit_accel_tbox,
            G_UIState.lpanel_entity_edit_moment_tbox,
        };
        ClearAndUnbindTextboxGroup(edit_boxes, sizeof(edit_boxes) / sizeof(edit_boxes[0]));

        // Unbind data
        // G_UIState.lpanel_entity_edit_edge_count_tbox->data.textbox.data_bind = NULL;
        // Unbind handled by ClearAndUnbindTextbox calls above.
    }
}


