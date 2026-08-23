#include "system/ui/lpanel_system.h"

#include "system/viewport_system.h"
#include "ui/ui.h"
#include "system/ui_system.h"
#include "system/debug_overlay_system.h"
#include "ui/ui_constructors.h"
#include "system/panel_system.h"
#include "system/systems.h"

// ============================================================================
// Panel System
// ============================================================================
static PanelSystem *lpanel = NULL;

// ============================================================================
// Action Codes
// ============================================================================
static int btn_action_create_entity = BUTTON_ACTION_CREATE_ENTITY;

// ============================================================================
// Visual Style Properties
// ============================================================================
static Size debug_section_size = UI_SIZE_CONTENT_FILL;

// ============================================================================
// UI Element Pointers
// ============================================================================
UIElement *lpanel_state_view_cont = {0};
UIElement *lpanel_edit_view_cont = {0};
UIElement *lpanel_view_selector_cont = {0};
UIElement *lpanel_edit_entity_tcont = {0};
static ViewSelector *lpanel_view_selector = NULL;

typedef struct
{
    DebugOverlayId id;
    const char *label;
} LPanelDebugToggle;

static const LPanelDebugToggle lpanel_debug_general_toggles[] = {
    {DEBUG_DASHBOARD, "Dashboard"},
};

static const LPanelDebugToggle lpanel_debug_viewport_toggles[] = {
    {DEBUG_VIEWPORT_GRID, "Viewport Grid"},
};

static const LPanelDebugToggle lpanel_debug_world_toggles[] = {
    {DEBUG_WORLD_GRID, "World Grid"},
    {DEBUG_WORLD_GRID_LABELS, "World Grid Labels"},
    {DEBUG_UNIVERSE_GRID_LABELS, "Universe Grid Labels"},
};

static const LPanelDebugToggle lpanel_debug_ui_toggles[] = {
    {DEBUG_UI_BORDERS, "UI Borders"},
};

static const LPanelDebugToggle lpanel_debug_object_toggles[] = {
    {DEBUG_OBJECT_AXES, "Object Axes"},
};

static void HandleLPanelDebugToggleClick(UIElement *button)
{
    if (!button || !button->data.button.user_data)
    {
        return;
    }

    const LPanelDebugToggle *toggle = (const LPanelDebugToggle *)button->data.button.user_data;
    ToggleDebug(toggle->id);
    UpdateString64(button->data.button.label.string, "%s: %s", toggle->label,
                   IsDebugEnabled(toggle->id) ? "ON" : "OFF");
}

// Recalculate the left-panel UI tree immediately for layout debugging.
static void HandleLPanelUIRefreshClick(UIElement *button)
{
    (void)button;

    if (lpanel && lpanel->root)
    {
        UpdateUISpace(lpanel->root, lpanel->seed_box);
    }
}

// ============================================================================
// Root Layout
// ============================================================================
// ============================================================================
// Toggle Button Layout
// ============================================================================
// ============================================================================
// State View Layout
// ============================================================================s
// ============================================================================
// Edit View Layout
// ============================================================================
Size create_entity_section_size = UI_SIZE_CONTENT_FILL;

void InitLPanelStateView(void);
void InitLPanelEditView(void);

static void InitEntityCreateDefaults(void)
{
    if (!G_UIState.newtonoid_params)
    {
        return;
    }

    Newtonoid2dParams *params = G_UIState.newtonoid_params;
    params->shape_type = SHAPE_AUTO;
    params->vertice_count = 4;
    params->width = 1.0f;
    params->height = 1.0f;
    params->mass = 1.0f;
    params->anchor_position = ZERO_VECTOR_2D;
    params->velocity = ZERO_VECTOR_2D;

    WriteTextboxInt(G_UIState.edit_vertice_count_tbox, params->vertice_count);
    WriteTextboxFloat(G_UIState.edit_width_tbox, params->width, 2);
    WriteTextboxFloat(G_UIState.edit_height_tbox, params->height, 2);
    WriteTextboxFloat(G_UIState.edit_mass_tbox, params->mass, 2);
    WriteTextboxVectorPair(G_UIState.edit_pos_c_tbox, params->anchor_position);
    WriteTextboxVectorPair(G_UIState.edit_vel_tbox, params->velocity);
}

void InitLPanel()
{
    const char *labels[] = {"STATE", "DRAW"};
    lpanel = PanelSystem_CreateStandard(&lpanel_viewport, 2, labels, ARRAY_COUNT(labels),
                                        PanelSystem_HandleViewSelected,
                                        &ui_default_palette, ui_standard_stack_spacing);
    if (!lpanel)
    {
        return;
    }

    // Build panel-specific UI
    InitLPanelStateView();
    InitLPanelEditView();

    // Select the initial view after both panel views have been registered.
    if (lpanel->selectors.count > 0)
    {
        lpanel_view_selector = *((ViewSelector **)LArray_Get(&lpanel->selectors, 0));
        PanelSystem_SelectView(lpanel_view_selector, 0);
    }

    // Initial layout update
    // UpdateUISpace(lpanel->root, lpanel->seed_box);
}

void InitLPanelStateView(void)
{
    View *view = PanelSystem_CreateView(lpanel, LPANEL_STATE_VIEW);
    lpanel_state_view_cont = view ? view->container : NULL;

    if (!lpanel_state_view_cont)
    {
        return;
    }

    lpanel_state_view_cont->colour_border = lpanel->palette->container_border;
    lpanel_state_view_cont->colour_fill = lpanel->palette->container_fill;
    lpanel_state_view_cont->is_draggable = true;

    // Keep every debug feature in STATE, grouped by the system it visualises.
    const struct
    {
        const char *title;
        const LPanelDebugToggle *toggles;
        size_t count;
    } debug_sections[] = {
        {"General", lpanel_debug_general_toggles, ARRAY_COUNT(lpanel_debug_general_toggles)},
        {"Viewport", lpanel_debug_viewport_toggles, ARRAY_COUNT(lpanel_debug_viewport_toggles)},
        {"World", lpanel_debug_world_toggles, ARRAY_COUNT(lpanel_debug_world_toggles)},
        {"UI", lpanel_debug_ui_toggles, ARRAY_COUNT(lpanel_debug_ui_toggles)},
        {"Objects", lpanel_debug_object_toggles, ARRAY_COUNT(lpanel_debug_object_toggles)},
    };

    for (size_t section_index = 0; section_index < ARRAY_COUNT(debug_sections); section_index++)
    {
        UIElement *section = CreateViewSection_Stack(
            lpanel_state_view_cont, debug_sections[section_index].title,
            debug_section_size, lpanel->palette);

        for (size_t toggle_index = 0; toggle_index < debug_sections[section_index].count; toggle_index++)
        {
            const LPanelDebugToggle *toggle = &debug_sections[section_index].toggles[toggle_index];
            String64 label = {0};
            UpdateString64(label.string, "%s: %s", toggle->label, IsDebugEnabled(toggle->id) ? "ON" : "OFF");
            CreateUIButtonDefault(section, UI_ELEMENT_BUTTON_SIMPLE, label.string,
                                  ui_wide_button_size, ui_standard_button_padding,
                                  lpanel->palette, HandleLPanelDebugToggleClick, (void *)toggle, NULL);
        }
    }
}

void InitLPanelEditView(void)
{
    // Create View's container & register the View
    View *view = PanelSystem_CreateView(lpanel, LPANEL_DRAW_VIEW);
    lpanel_edit_view_cont = view ? view->container : NULL;
    if (!lpanel_edit_view_cont)
    {
        return;
    }

    // The edit view starts disabled until selected.
    DisableElement(lpanel_edit_view_cont);

    // Build the editable object controls as a standard stacked ViewSection.
    lpanel_edit_entity_tcont = CreateViewSection_Stack(
        lpanel_edit_view_cont, "Object Create", create_entity_section_size,
        lpanel->palette);

    const UIFieldSpec edit_specs[] = {
        {"Vertices", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, INT, &G_UIState.edit_vertice_count_tbox, NULL},
        {"Width", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.edit_width_tbox, NULL},
        {"Height", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.edit_height_tbox, NULL},
        {"Mass", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.edit_mass_tbox, NULL},
        {"Anchor", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.edit_pos_c_tbox, NULL},
        {"Vel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.edit_vel_tbox, NULL},
        {"Acc", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.edit_accel_tbox, NULL},
        {"Moment", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.edit_moment_tbox, NULL},
    };
    InitUIFields(lpanel_edit_entity_tcont, edit_specs,
                 ARRAY_COUNT(edit_specs), ui_standard_field_padding,
                 lpanel->palette);

    BindTextboxData(G_UIState.edit_vertice_count_tbox, INT, &G_UIState.newtonoid_params->vertice_count);
    BindTextboxData(G_UIState.edit_width_tbox, FLOAT, &G_UIState.newtonoid_params->width);
    BindTextboxData(G_UIState.edit_height_tbox, FLOAT, &G_UIState.newtonoid_params->height);
    BindTextboxData(G_UIState.edit_mass_tbox, FLOAT, &G_UIState.newtonoid_params->mass);
    BindTextboxData(G_UIState.edit_pos_c_tbox, VECTOR2D, &G_UIState.newtonoid_params->anchor_position);
    BindTextboxData(G_UIState.edit_vel_tbox, VECTOR2D, &G_UIState.newtonoid_params->velocity);
    InitEntityCreateDefaults();

    CreateUIButtonDefault(lpanel_edit_entity_tcont, UI_ELEMENT_BUTTON_SUBMIT,
                          "CREATE", ui_standard_button_size, ui_standard_button_padding,
                          lpanel->palette, HandleBtnSubmitClick,
                          &btn_action_create_entity, NULL);
}

// void InitEntityEditorContainer(void)
// {

// }

void DrawLPanel(void)
{
    if (!lpanel)
    {
        return;
    }

    PanelSystem_Draw(lpanel);
}

Frame2d *GetLPanelSpaceFrame(void)
{
    return PanelSystem_GetSpaceFrame(lpanel);
}

bool SetLPanelSpaceBasis(Vector2d basis_u, Vector2d basis_v)
{
    return PanelSystem_SetSpaceBasis(lpanel, basis_u, basis_v);
}

void ResetLPanelSpaceBasis(void)
{
    PanelSystem_ResetSpaceBasis(lpanel);
}

UIElement *GetLPanelRoot(void)
{
    return lpanel ? lpanel->root : NULL;
}

PanelSystem *GetLPanelSystem(void)
{
    return lpanel;
}

// Destroy the left panel and clear its cached UI references.
void DestroyLPanel(void)
{
    PanelSystem *panel = lpanel;
    lpanel = NULL;
    PanelSystem_Destroy(panel);

    lpanel_state_view_cont = NULL;
    lpanel_edit_view_cont = NULL;
    lpanel_edit_entity_tcont = NULL;
    lpanel_view_selector_cont = NULL;
    lpanel_view_selector = NULL;
    G_UIState.lpanel_views = NULL;

    G_UIState.edit_edge_count_tbox = NULL;
    G_UIState.edit_vertice_count_tbox = NULL;
    G_UIState.edit_width_tbox = NULL;
    G_UIState.edit_height_tbox = NULL;
    G_UIState.edit_mass_tbox = NULL;
    G_UIState.edit_pos_c_tbox = NULL;
    G_UIState.edit_vel_tbox = NULL;
    G_UIState.edit_accel_tbox = NULL;
    G_UIState.edit_moment_tbox = NULL;
    G_UIState.edit_edge_count_str = NULL;
    G_UIState.edit_vertice_count_str = NULL;
    G_UIState.edit_width_str = NULL;
    G_UIState.edit_height_str = NULL;
    G_UIState.edit_mass_str = NULL;
    G_UIState.edit_pos_c_str = NULL;
    G_UIState.edit_vel_str = NULL;
    G_UIState.edit_accel_str = NULL;
    G_UIState.edit_moment_str = NULL;
}
