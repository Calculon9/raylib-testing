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
// View Storage
// ============================================================================
static View lpanel_state_view_storage = {0};
static View lpanel_edit_entity_view_storage = {0};

// ============================================================================
// Visual Style Properties
// ============================================================================
static Size lpanel_title_tfield_size = {{6.0f, 0.5f}, SIZE_FIXED};
static Size lpanel_debug_section_size = {{0.0f, 0.0f}, SIZE_CONTENT_FILL};

// ============================================================================
// UI Element Pointers
// ============================================================================
View *lpanel_state_view = {0};
View *lpanel_edit_entity_view = {0};
UIElement *lpanel_state_view_cont = {0};
static UIElement *lpanel_state_debug_cont = NULL;
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

// ============================================================================
// Root Layout
// ============================================================================
// ============================================================================
// Toggle Button Layout
// ============================================================================
// ============================================================================
// State View Layout
// ============================================================================
Offset lpanel_state_view_cont_offset = {{0, 0.0f}, OFFSET_PERCENT};
// ============================================================================
// Edit View Layout
// ============================================================================
Offset lpanel_edit_view_cont_offset = {{0, 0.0f}, OFFSET_PERCENT};
Offset lpanel_edit_entity_tcont_offset = {{0, 0.0}, OFFSET_PERCENT};
Size lpanel_edit_entity_tcont_size = {{1.0f, 0.50f}, SIZE_PERCENT};

void InitLPanelStateView(void);
void InitLPanelEditView(void);
void InitLPanelViewSelector(void);
void InitEntityEditorContainer(void);

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
    // Create panel system
    lpanel = PanelSystem_Create(&lpanel_viewport, 1.0f, (Vector2d){0.1f, 0.1f},
                                &ui_default_palette, ui_standard_stack_spacing);
    if (!lpanel)
    {
        return;
    }
    
    // Initialize views array
    PanelSystem_InitViews(lpanel, 2);
    
    // Initialize root UI structure
    PanelSystem_InitRoot(lpanel);
    
    // Setup view storage
    lpanel_state_view = &lpanel_state_view_storage;
    lpanel_edit_entity_view = &lpanel_edit_entity_view_storage;
    
    // Build panel-specific UI
    InitLPanelViewSelector();
    InitLPanelStateView();
    InitLPanelEditView();
    PanelSystem_SelectView(lpanel_view_selector, 0);

    // Initial layout update
    UpdateUISpace(lpanel->root, lpanel->seed_box);
}

void InitLPanelStateView(void)
{
    lpanel_state_view_cont = CreateUIContainer(lpanel->root, ui_fill_container_size,
                                                  lpanel_state_view_cont_offset, ZERO_VECTOR_2D,
                                                  lpanel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
                                                  ui_standard_stack_spacing, false, true);
    PanelSystem_AddView(lpanel, lpanel_state_view, lpanel_state_view_cont, LPANEL_STATE_VIEW);

    // Match rpanel and StateManager by placing view content in a padded surface container.
    lpanel_state_debug_cont = CreateUIContainer(
        lpanel_state_view_cont, ui_fill_container_size,
        (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, ui_standard_container_padding,
        lpanel->palette, UI_PALETTE_SURFACE_CONTAINER,
        ui_standard_stack_spacing, true, true);

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

    for (size_t section_index = 0;
         section_index < ARRAY_COUNT(debug_sections);
         section_index++)
    {
        UIElement *section = CreateViewSection_Stack(
            lpanel_state_debug_cont, debug_sections[section_index].title,
            lpanel_debug_section_size, lpanel->palette);

        for (size_t toggle_index = 0;
             toggle_index < debug_sections[section_index].count;
             toggle_index++)
        {
            const LPanelDebugToggle *toggle = &debug_sections[section_index].toggles[toggle_index];
            String64 label = {0};
            UpdateString64(label.string, "%s: %s", toggle->label,
                           IsDebugEnabled(toggle->id) ? "ON" : "OFF");
            CreateUIButtonDefault(section, UI_ELEMENT_BUTTON_SIMPLE, label.string,
                                  ui_wide_button_size, ui_standard_button_padding,
                                  lpanel->palette, HandleLPanelDebugToggleClick,
                                  (void *)toggle, NULL);
        }
    }
}

void InitLPanelEditView(void)
{
    // Create View's container & register the View
    lpanel_edit_view_cont = CreateUIContainer(lpanel->root, ui_fill_container_size,
                                                 lpanel_edit_view_cont_offset, ZERO_VECTOR_2D,
                                                 lpanel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
                                                 ui_standard_stack_spacing, false, false);
    PanelSystem_AddView(lpanel, lpanel_edit_entity_view, lpanel_edit_view_cont, LPANEL_DRAW_VIEW);

    // Customise the View
    lpanel_edit_entity_tcont = CreateUIContainer(
        lpanel_edit_view_cont, lpanel_edit_entity_tcont_size,
        lpanel_edit_entity_tcont_offset, ui_standard_container_padding,
        lpanel->palette, UI_PALETTE_SURFACE_CONTAINER,
        ui_standard_stack_spacing, true, true);

    CreateUILabelTitleDefault(lpanel_edit_entity_tcont, "Object Create",
                         lpanel_title_tfield_size, ui_standard_field_padding,
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

void InitLPanelViewSelector(void)
{
    lpanel_view_selector_cont = CreateUIContainer(lpanel->root, ui_standard_selector_container_size,
                                                       (Offset){{0.0, 0.0}, OFFSET_PERCENT},
                                                       ZERO_VECTOR_2D, lpanel->palette,
                                                       UI_PALETTE_SURFACE_TRANSPARENT,
                                                       ui_zero_inline_spacing,
                                                       false, true);
    lpanel_view_selector_cont->colour_border = lpanel->palette->container_border;
    const char *labels[] = {"STATE", "DRAW"};
    lpanel_view_selector = PanelSystem_CreateViewSelector(
        lpanel, lpanel_view_selector_cont, ui_standard_selector_button_size,
        labels, ARRAY_COUNT(labels), PanelSystem_HandleViewSelected);
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

UIElement* GetLPanelRoot(void)
{
    return lpanel ? lpanel->root : NULL;
}
