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
static Vector2d lpanel_tfield_padding = {0.03f, 0.03f};
static Size lpanel_title_tfield_size = {{6.0f, 0.5f}, SIZE_FIXED};

// ============================================================================
// UI Element Pointers
// ============================================================================
View *lpanel_state_view = {0};
View *lpanel_edit_entity_view = {0};
UIElement *lpanel_state_view_cont = {0};
UIElement *lpanel_edit_view_cont = {0};
UIElement *lpanel_btn_toggle_view_cont = {0};
UIElement *lpanel_btn_create_entity_cont = {0};
UIElement *lpanel_edit_entity_tcont = {0};
static ViewSelector *lpanel_view_selector = NULL;

// ============================================================================
// Root Layout
// ============================================================================
Spacing lpanel_root_child_spacing = {{0.0f, 0.01f}, PERCENT, SPACING_STACKED};

// ============================================================================
// Toggle Button Layout
// ============================================================================
Size lpanel_btn_cont_size = {{1.0f, 0.03f}, SIZE_PERCENT};
Size lpanel_toggle_button_size = {{0.5f, 1.0f}, SIZE_PERCENT};
Spacing lpanel_btn_child_spacing = {{0.0f, 0.0f}, NONE, SPACING_INLINE};

// ============================================================================
// State View Layout
// ============================================================================
Offset lpanel_state_view_cont_offset = {{0, 0.0f}, OFFSET_PERCENT};
Size lpanel_state_view_cont_size = {{1.0f, 1.0f}, SIZE_FILL};

// ============================================================================
// Edit View Layout
// ============================================================================
Offset lpanel_edit_view_cont_offset = {{0, 0.0f}, OFFSET_PERCENT};
Size lpanel_edit_view_cont_size = {{1.0f, 1.0f}, SIZE_FILL};
Offset lpanel_edit_entity_tcont_offset = {{0, 0.0}, OFFSET_PERCENT};
Size lpanel_edit_entity_tcont_size = {{1.0f, 0.50f}, SIZE_PERCENT};
Offset lpanel_btn_create_entity_cont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_btn_delete_entity_cont_offset = {{0.0, 0.0}, OFFSET_PERCENT};

void InitPanelStateView(void);
void InitPanelEditView(void);
void InitPanelToggleButtons(void);
void InitEntityEditorContainer(void);

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
                                &ui_default_palette, lpanel_root_child_spacing);
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
    InitPanelToggleButtons();
    InitPanelStateView();
    InitPanelEditView();
    PanelSystem_SelectView(lpanel_view_selector, 0);

    // Initial layout update
    UpdateUISpace(lpanel->root, lpanel->seed_box);
}

void InitPanelStateView(void)
{
    lpanel_state_view_cont = CreateUIContainer(lpanel->root, lpanel_state_view_cont_size,
                                                  lpanel_state_view_cont_offset, ZERO_VECTOR_2D,
                                                  lpanel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
                                                  cont_default_child_spacing, false, true);
    PanelSystem_AddView(lpanel, lpanel_state_view, lpanel_state_view_cont, LPANEL_STATE_VIEW);
}

void InitPanelEditView(void)
{
    lpanel_edit_view_cont = CreateUIContainer(lpanel->root, lpanel_edit_view_cont_size,
                                                 lpanel_edit_view_cont_offset, ZERO_VECTOR_2D,
                                                 lpanel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
                                                 cont_default_child_spacing, false, false);
    PanelSystem_AddView(lpanel, lpanel_edit_entity_view, lpanel_edit_view_cont, LPANEL_EDIT_ENTITY_VIEW);
    InitEntityEditorContainer();
}

void InitPanelToggleButtons(void)
{
    lpanel_btn_toggle_view_cont = CreateUIContainer(lpanel->root, lpanel_btn_cont_size,
                                                       (Offset){{0.0, 0.0}, OFFSET_PERCENT},
                                                       ZERO_VECTOR_2D, lpanel->palette,
                                                       UI_PALETTE_SURFACE_TRANSPARENT,
                                                       lpanel_btn_child_spacing,
                                                       false, true);
    lpanel_btn_toggle_view_cont->colour_border = lpanel->palette->container_border;
    const char *labels[] = {"STATE", "CREATE"};
    lpanel_view_selector = PanelSystem_CreateViewSelector(
        lpanel, lpanel_btn_toggle_view_cont, lpanel_toggle_button_size,
        labels, sizeof(labels) / sizeof(labels[0]), HandleLPanelViewSelected);
}

void InitEntityEditorContainer(void)
{
    lpanel_edit_entity_tcont = CreateUIContainer(
        lpanel_edit_view_cont, lpanel_edit_entity_tcont_size,
        lpanel_edit_entity_tcont_offset, tcont_default_padding,
        lpanel->palette, UI_PALETTE_SURFACE_CONTAINER,
        tcont_default_child_spacing, true, true);

    CreateUILabelTitleDefault(lpanel_edit_entity_tcont, "Object Create",
                         lpanel_title_tfield_size, lpanel_tfield_padding,
                         lpanel->palette);

    const UIFieldSpec edit_specs[] = {
        {"Vertices", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, INT, &G_UIState.lpanel_entity_edit_vertice_count_tbox, NULL},
        {"Width", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, INT, &G_UIState.lpanel_entity_edit_width_tbox, NULL},
        {"Height", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, FLOAT, &G_UIState.lpanel_entity_edit_height_tbox, NULL},
        {"Mass", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, FLOAT, &G_UIState.lpanel_entity_edit_mass_tbox, NULL},
        {"Pos.c", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, FLOAT, &G_UIState.lpanel_entity_edit_pos_c_tbox, NULL},
        {"Vel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, VECTOR2D, &G_UIState.lpanel_entity_edit_vel_tbox, NULL},
        {"Acc", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, VECTOR2D, &G_UIState.lpanel_entity_edit_accel_tbox, NULL},
        {"Moment", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, VECTOR2D, &G_UIState.lpanel_entity_edit_moment_tbox, NULL},
    };
    InitUIFields(lpanel_edit_entity_tcont, edit_specs,
                    sizeof(edit_specs) / sizeof(edit_specs[0]), lpanel_tfield_padding,
                    lpanel->palette);

    lpanel_btn_create_entity_cont = CreateUIContainer(
        lpanel_edit_entity_tcont, ui_default_control_size,
        lpanel_btn_create_entity_cont_offset, ZERO_VECTOR_2D,
        lpanel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
        lpanel_btn_child_spacing, false, true);

    CreateUIButtonDefault(lpanel_btn_create_entity_cont, UI_ELEMENT_BUTTON_SUBMIT,
                             "CREATE", btn_default_size, btn_default_padding,
                             lpanel->palette, HandleBtnSubmitClick,
                             &btn_action_create_entity, NULL);
}

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
