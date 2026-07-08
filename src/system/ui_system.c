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
UIState G_UIState = (UIState){0};
// ------------------TOTAL SCREEN-------------------------
// Logical->pixel-space conversion properties

// Default UI Properties
Offset tbox_tlabel_default_offset = {{0.03, 0}, OFFSET_FIXED};
Vector2d tbox_default_padding = {0.00, 0.00};
Vector2d tlabel_default_padding = {0.00, 0.00};
Vector2d tfield_default_padding = {0.01, 0.01};
Vector2d tcont_default_padding = {0.06, 0.06};
Vector2d btn_default_padding = {0.025, 0.025};
ColourRgba btn_default_colour_border = COLOUR_PANEL_DARK_2;
ColourRgba btn_default_colour_fill = COLOUR_PANEL_LIGHT_1;
ColourRgba tbox_default_colour_border = COLOUR_PANEL_DARK_1; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
ColourRgba tbox_default_colour_fill = COLOUR_PANEL_LIGHT_3;  // COLOUR_PANEL_DARK_1;
ColourRgba tcont_default_colour_fill = COLOUR_PANEL_LIGHT_1;
ColourRgba tcont_default_colour_border = COLOUR_PANEL_DARK_2; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
ColourRgba tfield_default_colour_fill = COLOURLESS_RGBA;
Size tfield_default_size = {{6, 0.5}, SIZE_FIXED};
// Give labels more room in inline text fields (label width = 1.0 - textbox width).
Size tbox_default_size = {{0.45, 1}, SIZE_PERCENT};
Size btn_default_size = {{6, 0.5}, SIZE_FIXED};    // Assuming it's in a container
//Size btn_default_size = {{1, 1}, SIZE_PERCENT};    // Assuming it's in a container
Size tcont_default_size = {{1, 0.5}, SIZE_PERCENT};
Spacing tcont_default_child_spacing = {{0, 0.015}, PERCENT, SPACING_STACKED};
Spacing cont_default_child_spacing = {{0, 0.015}, PERCENT, SPACING_STACKED};
Spacing btn_cont_default_child_spacing = {{0.0, 0.0}, PERCENT, SPACING_NONE};

// UI Elements

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------

void UpdatePanelRegion(int mouse_x, int mouse_y, bool cursor_in_panel);
void UpdateGlobalUIState();

static void BindTextbox(UIElement *textbox, void *data_bind)
{
    if (!textbox)
    {
        return;
    }

    textbox->data.textbox.data_bind = data_bind;
}

static void ClearTextbox(UIElement *textbox)
{
    if (!textbox)
    {
        return;
    }

    textbox->data.textbox.text.string[0] = '\0';
}

static void ClearAndUnbindTextbox(UIElement *textbox)
{
    ClearTextbox(textbox);
    BindTextbox(textbox, NULL);
}

static void WriteNumberIfUnfocused(UIElement *textbox, float value, int precision)
{
    if (!textbox || textbox->is_focused)
    {
        return;
    }

    PipelineNumberToText(value, precision, textbox->data.textbox.text.string, sizeof(String64));
}

static void WriteVectorIfUnfocused(UIElement *textbox, Vector2d value)
{
    if (!textbox || textbox->is_focused)
    {
        return;
    }

    PipelineVectorToText(value, textbox->data.textbox.text.string, sizeof(String64));
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
    InitRPanel();
    // G_UIState.active_panel_view = LPANEL_STATE_VIEW;
}

void UpdateUISystem(int mouse_x, int mouse_y)
{
    bool cursor_in_lpanel = mouse_x >= lpanel_pixel_origin.x && mouse_x <= (lpanel_pixel_origin.x + (lpanel_pixel_u.x * lpanel_resolution.x)) &&
                            mouse_y >= lpanel_pixel_origin.y && mouse_y <= (lpanel_pixel_origin.y + (lpanel_pixel_v.y * lpanel_resolution.y));
    bool cursor_in_rpanel = mouse_x >= rpanel_pixel_origin.x && mouse_x <= (rpanel_pixel_origin.x + (rpanel_pixel_u.x * rpanel_resolution.x)) &&
                            mouse_y >= rpanel_pixel_origin.y && mouse_y <= (rpanel_pixel_origin.y + (rpanel_pixel_v.y * rpanel_resolution.y));
    bool cursor_in_ui = cursor_in_lpanel || cursor_in_rpanel;

    // Send useful data to the Dispatcher for it triage and process/update affected elements
    ProcessUIInput(mouse_x, mouse_y, cursor_in_ui);
    UpdateGlobalUIState();
}

void DrawUI()
{
    DrawLPanel();
}

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
                 G_UIState.lpanel_stats_fps_str->string,
                 G_UIState.lpanel_stats_ftime_str->string,
                 G_UIState.lpanel_stats_mem_str->string,
                 G_UIState.lpanel_stats_polygs_str->string);
    }
    // ----DEBUG //

    // COLLECT & UPDATE "SELECTED ENTITY" PROPERTIES
    Newtonoid2d *obj = G_WorldState.selected_object;
    if (obj)
    {
        // Bind selected_object data to the Object Properties TextBoxes
        BindTextbox(G_UIState.lpanel_entity_state_id_tbox, &obj->id);
        BindTextbox(G_UIState.lpanel_entity_state_mass_tbox, &obj->mass);
        BindTextbox(G_UIState.lpanel_entity_state_pos_tl_tbox, &obj->coords_origin);
        BindTextbox(G_UIState.lpanel_entity_state_pos_c_tbox, &obj->coords_center);
        BindTextbox(G_UIState.lpanel_entity_state_vel_tbox, &obj->velocity);
        BindTextbox(G_UIState.lpanel_entity_state_accel_tbox, &obj->acceleration);
        BindTextbox(G_UIState.lpanel_entity_state_moment_tbox, &obj->momentum);

        // PIPELINE data to text only when the element is NOT focused
        // so that editing of the text by the user doesn't keep getting overwritten with the value stored in the object
        WriteNumberIfUnfocused(G_UIState.lpanel_entity_state_id_tbox, (float)obj->id, 0);
        WriteNumberIfUnfocused(G_UIState.lpanel_entity_state_mass_tbox, obj->mass, 2);
        WriteVectorIfUnfocused(G_UIState.lpanel_entity_state_pos_tl_tbox, obj->coords_origin);
        WriteVectorIfUnfocused(G_UIState.lpanel_entity_state_pos_c_tbox, obj->coords_center);
        WriteVectorIfUnfocused(G_UIState.lpanel_entity_state_vel_tbox, obj->velocity);
        WriteVectorIfUnfocused(G_UIState.lpanel_entity_state_accel_tbox, obj->acceleration);
        WriteVectorIfUnfocused(G_UIState.lpanel_entity_state_moment_tbox, obj->momentum);
    }
    else // Reset the bounded textbox output buffers AND unbind
    {
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_state_id_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_state_mass_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_state_pos_tl_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_state_pos_c_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_state_vel_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_state_accel_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_state_moment_tbox);
    }

    // COLLECT & UPDATE SELECTED CELL PROPERTIES
    Cell *cell = G_WorldState.selected_cell;
    if (cell)
    {
        int index = GetIndexFromCoords(&G_WorldState.world->coord_space_grid.coord_space, cell->coords_center);
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
        //G_UIState.lpanel_entity_edit_edge_count_tbox->data.textbox.data_bind = &params->edge_count;
        BindTextbox(G_UIState.lpanel_entity_edit_vertice_count_tbox, &params->vertice_count);
        BindTextbox(G_UIState.lpanel_entity_edit_width_tbox, &params->width);
        BindTextbox(G_UIState.lpanel_entity_edit_height_tbox, &params->height);
        BindTextbox(G_UIState.lpanel_entity_edit_mass_tbox, &params->mass);
        BindTextbox(G_UIState.lpanel_entity_edit_pos_c_tbox, &params->coords_center);
        BindTextbox(G_UIState.lpanel_entity_edit_vel_tbox, &params->velocity);
        BindTextbox(G_UIState.lpanel_entity_edit_accel_tbox, &params->acceleration);
        BindTextbox(G_UIState.lpanel_entity_edit_moment_tbox, &params->momentum);

        // PIPELINE data to text only when the element is NOT focused
        // so that editing of the text by the user doesn't keep getting overwritten with the value stored in the object
        // if (!G_UIState.lpanel_entity_edit_edge_count_tbox->is_focused)
        //     PipelineNumberToText(params->edge_count, 0, G_UIState.lpanel_entity_edit_edge_count_tbox->data.textbox.text.string, str_64);
        WriteNumberIfUnfocused(G_UIState.lpanel_entity_edit_vertice_count_tbox, params->vertice_count, 0);
        WriteNumberIfUnfocused(G_UIState.lpanel_entity_edit_width_tbox, params->width, 2);
        WriteNumberIfUnfocused(G_UIState.lpanel_entity_edit_height_tbox, params->height, 2);
        WriteNumberIfUnfocused(G_UIState.lpanel_entity_edit_mass_tbox, params->mass, 2);
        WriteVectorIfUnfocused(G_UIState.lpanel_entity_edit_pos_c_tbox, params->coords_center);
        WriteVectorIfUnfocused(G_UIState.lpanel_entity_edit_vel_tbox, params->velocity);
        WriteVectorIfUnfocused(G_UIState.lpanel_entity_edit_accel_tbox, params->acceleration);
        WriteVectorIfUnfocused(G_UIState.lpanel_entity_edit_moment_tbox, params->momentum);
    }
    else
    {
        // Reset the bounded textbox output buffers
        //G_UIState.lpanel_entity_edit_edge_count_tbox->data.textbox.text.string[0] = '\0';
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_edit_vertice_count_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_edit_width_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_edit_height_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_edit_mass_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_edit_pos_c_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_edit_vel_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_edit_accel_tbox);
        ClearAndUnbindTextbox(G_UIState.lpanel_entity_edit_moment_tbox);

        // Unbind data
        //G_UIState.lpanel_entity_edit_edge_count_tbox->data.textbox.data_bind = NULL;
        // Unbind handled by ClearAndUnbindTextbox calls above.
    }
}

// Gameplay Screen Stage Update logic
void UpdatePanelRegion(int mouse_x, int mouse_y, bool cursor_in_region)
{
    (void)mouse_x;
    (void)mouse_y;
    (void)cursor_in_region;
}