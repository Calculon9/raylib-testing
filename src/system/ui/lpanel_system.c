#include "system/ui/lpanel_system.h"

#include "system/viewport_system.h"
#include "ui/ui.h"
#include "system/ui_system.h"
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

// ============================================================================
// UI Element Pointers
// ============================================================================
View *lpanel_state_view = {0};
View *lpanel_edit_entity_view = {0};
UIElement *lpanel_state_view_cont = {0};
UIElement *lpanel_edit_view_cont = {0};
UIElement *lpanel_view_selector_cont = {0};
UIElement *lpanel_edit_entity_tcont = {0};
static ViewSelector *lpanel_view_selector = NULL;

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
    params->coords_center = ZERO_VECTOR_2D;
    params->velocity = ZERO_VECTOR_2D;

    WriteTextboxInt(G_UIState.edit_vertice_count_tbox, params->vertice_count);
    WriteTextboxFloat(G_UIState.edit_width_tbox, params->width, 2);
    WriteTextboxFloat(G_UIState.edit_height_tbox, params->height, 2);
    WriteTextboxFloat(G_UIState.edit_mass_tbox, params->mass, 2);
    WriteTextboxVectorPair(G_UIState.edit_pos_c_tbox, params->coords_center);
    WriteTextboxVectorPair(G_UIState.edit_vel_tbox, params->velocity);
}

static void HandleLPanelViewSelected(View *view)
{
    if (view)
    {
        G_UIState.active_panel_view = view->type;
    }
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
}

void InitLPanelEditView(void)
{
    // Create View's container & register the View
    lpanel_edit_view_cont = CreateUIContainer(lpanel->root, ui_fill_container_size,
                                                 lpanel_edit_view_cont_offset, ZERO_VECTOR_2D,
                                                 lpanel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
                                                 ui_standard_stack_spacing, false, false);
    PanelSystem_AddView(lpanel, lpanel_edit_entity_view, lpanel_edit_view_cont, LPANEL_EDIT_ENTITY_VIEW);

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
        {"Pos.c", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.edit_pos_c_tbox, NULL},
        {"Vel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.edit_vel_tbox, NULL},
        {"Acc", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.edit_accel_tbox, NULL},
        {"Moment", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.edit_moment_tbox, NULL},
    };
    InitUIFields(lpanel_edit_entity_tcont, edit_specs,
                    sizeof(edit_specs) / sizeof(edit_specs[0]), ui_standard_field_padding,
                    lpanel->palette);

    BindTextboxData(G_UIState.edit_vertice_count_tbox, INT, &G_UIState.newtonoid_params->vertice_count);
    BindTextboxData(G_UIState.edit_width_tbox, FLOAT, &G_UIState.newtonoid_params->width);
    BindTextboxData(G_UIState.edit_height_tbox, FLOAT, &G_UIState.newtonoid_params->height);
    BindTextboxData(G_UIState.edit_mass_tbox, FLOAT, &G_UIState.newtonoid_params->mass);
    BindTextboxData(G_UIState.edit_pos_c_tbox, VECTOR2D, &G_UIState.newtonoid_params->coords_center);
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
    const char *labels[] = {"STATE", "CREATE"};
    lpanel_view_selector = PanelSystem_CreateViewSelector(
        lpanel, lpanel_view_selector_cont, ui_standard_selector_button_size,
        labels, sizeof(labels) / sizeof(labels[0]), HandleLPanelViewSelected);
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
