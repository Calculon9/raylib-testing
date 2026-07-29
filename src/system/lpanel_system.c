#include "system/lpanel_system.h"

#include "raylib.h"
#include <stdint.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "camera/camera.h"
#include "system/viewport_system.h"
#include "ui/ui.h"
#include "ui/text_region.h"
#include "ui/ui_renderer.h"
#include "system/ui_system.h"
#include "system/panel_ui_helpers.h"
#include "system/panel_system.h"
#include "system/utility_system.h"
#include "world/world.h"
#include "system/systems.h"

// ============================================================================
// Panel System
// ============================================================================
static PanelSystem *lpanel = NULL;

// ============================================================================
// Action Codes
// ============================================================================
static int btn_action_create_entity = BUTTON_ACTION_CREATE_ENTITY;
static int btn_action_delete_entity = BUTTON_ACTION_DELETE_ENTITY;
static int btn_action_enumerate = 0;

// ============================================================================
// View Storage
// ============================================================================
static View lpanel_state_view_storage = {0};
static View lpanel_edit_entity_view_storage = {0};

// ============================================================================
// Visual Style Properties
// ============================================================================
static ColourRgba lpanel_fill_colour = {40, 54, 24, 255};
static Vector2d lpanel_tfield_padding = {0.03f, 0.03f};
static Size lpanel_title_tfield_size = {{6.0f, 0.5f}, SIZE_FIXED};
static Size lpanel_row_tfield_size = {{6.0f, 0.5f}, SIZE_FIXED};

// ============================================================================
// UI Element Pointers
// ============================================================================
View *lpanel_state_view = {0};
View *lpanel_edit_entity_view = {0};
UIElement *lpanel_state_view_cont = {0};
UIElement *lpanel_edit_view_cont = {0};
UIElement *lpanel_state_stats_tcont = {0};
UIElement *lpanel_state_entity_tcont = {0};
UIElement *lpanel_state_cell_tcont = {0};
UIElement *lpanel_btn_toggle_view_cont = {0};
UIElement *lpanel_btn_create_entity_cont = {0};
UIElement *lpanel_btn_delete_entity_cont = {0};
UIElement *lpanel_edit_entity_tcont = {0};

// ============================================================================
// Root Layout
// ============================================================================
Spacing lpanel_root_child_spacing = {{0, 0.0}, PERCENT, SPACING_NORMAL};

// ============================================================================
// Toggle Button Layout
// ============================================================================
Size lpanel_btn_cont_size = {{1.0f, 0.08f}, SIZE_PERCENT};
Spacing lpanel_btn_child_spacing = {{0.0f, 0.03f}, NONE, SPACING_STACKED};

// ============================================================================
// State View Layout
// ============================================================================
Offset lpanel_state_view_cont_offset = {{0, 0.08}, OFFSET_PERCENT};
Size lpanel_state_view_cont_size = {{1.0f, 0.92f}, SIZE_PERCENT};
Offset lpanel_state_stats_tcont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Size lpanel_state_stats_tcont_size = {{1.0f, 0.24f}, SIZE_PERCENT};
Offset lpanel_state_entity_tcont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Size lpanel_state_entity_tcont_size = {{1.0f, 0.40f}, SIZE_PERCENT};
Offset lpanel_state_cell_tcont_offset = {{0, 0.0}, OFFSET_PERCENT};
Size lpanel_state_cell_tcont_size = {{1.0f, 0.24f}, SIZE_PERCENT};

// ============================================================================
// Edit View Layout
// ============================================================================
Offset lpanel_edit_view_cont_offset = {{0, 0.08}, OFFSET_PERCENT};
Size lpanel_edit_view_cont_size = {{1.0f, 0.92f}, SIZE_PERCENT};
Offset lpanel_edit_entity_tcont_offset = {{0, 0.0}, OFFSET_PERCENT};
Size lpanel_edit_entity_tcont_size = {{1.0f, 1.0f}, SIZE_PERCENT};
Offset lpanel_btn_create_entity_cont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_btn_delete_entity_cont_offset = {{0.0, 0.0}, OFFSET_PERCENT};

void InitPanelStateView(void);
void InitPanelEditView(void);
void InitPanelToggleButtons(void);
void InitStatsContainer(void);
void InitCellStateContainer(void);
void InitEntityStateContainer(void);
void InitEntityEditorContainer(void);

void InitLPanel()
{
    // Create panel system
    lpanel = PanelSystem_Create(&lpanel_viewport, 1.0f, (Vector2d){0.1f, 0.1f},
                                lpanel_fill_colour, lpanel_root_child_spacing);
    if (!lpanel)
    {
        return;
    }
    
    // Initialize views array
    PanelSystem_InitViews(lpanel, 3);
    
    // Initialize root UI structure
    PanelSystem_InitRoot(lpanel);
    
    // Setup view storage
    lpanel_state_view = &lpanel_state_view_storage;
    lpanel_edit_entity_view = &lpanel_edit_entity_view_storage;
    
    // Build panel-specific UI
    InitPanelToggleButtons();
    InitPanelStateView();
    InitPanelEditView();
    
    // Initial layout update
    UpdateUISpace(lpanel->root, lpanel->seed_box);
}

void InitPanelStateView(void)
{
    lpanel_state_view_cont = CreatePanelContainer(lpanel->root, lpanel_state_view_cont_size,
                                                  lpanel_state_view_cont_offset, ZERO_VECTOR_2D,
                                                  COLOURLESS_RGBA, COLOURLESS_RGBA,
                                                  cont_default_child_spacing, false, true);
    lpanel_state_view->container = lpanel_state_view_cont;
    lpanel_state_view->type = LPANEL_STATE_VIEW;
    InitStatsContainer();
    InitEntityStateContainer();
    InitCellStateContainer();

    LArray_Push(&lpanel->views, &lpanel_state_view);
}

void InitPanelEditView(void)
{
    lpanel_edit_view_cont = CreatePanelContainer(lpanel->root, lpanel_edit_view_cont_size,
                                                 lpanel_edit_view_cont_offset, ZERO_VECTOR_2D,
                                                 COLOURLESS_RGBA, COLOURLESS_RGBA,
                                                 cont_default_child_spacing, false, false);
    lpanel_edit_entity_view->container = lpanel_edit_view_cont;
    lpanel_edit_entity_view->type = LPANEL_EDIT_ENTITY_VIEW;
    InitEntityEditorContainer();

    LArray_Push(&lpanel->views, &lpanel_edit_entity_view);
}

void InitPanelToggleButtons(void)
{
    lpanel_btn_toggle_view_cont = CreatePanelContainer(lpanel->root, lpanel_btn_cont_size,
                                                       (Offset){{0.0, 0.0}, OFFSET_PERCENT},
                                                       ZERO_VECTOR_2D, COLOURLESS_RGBA,
                                                       COLOURLESS_RGBA, lpanel_btn_child_spacing,
                                                       false, true);
    btn_action_enumerate = 0;
    CreatePanelButtonDefault(lpanel_btn_toggle_view_cont, UI_ELEMENT_BUTTON_ENUMERATE,
                             "STATE -- UTIL", btn_default_size, btn_default_padding,
                             HandleBtnEnumerateClick, &btn_action_enumerate, &lpanel->views);
}

void InitEntityStateContainer(void)
{
    lpanel_state_entity_tcont = CreatePanelContainer(
        lpanel_state_view->container, lpanel_state_entity_tcont_size,
        lpanel_state_entity_tcont_offset, tcont_default_padding,
        tcont_default_colour_border, tcont_default_colour_fill,
        tcont_default_child_spacing, true, true);

    CreatePanelTitleLabelDefault(lpanel_state_entity_tcont, "SELECTED ENTITY", lpanel_title_tfield_size, lpanel_tfield_padding);

    const PanelFieldSpec state_specs[] = {
        {"ID", UI_ELEMENT_TEXTBOX_O, lpanel_row_tfield_size, FLOAT, &G_UIState.lpanel_entity_state_id_tbox, NULL},
        {"MASS", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, FLOAT, &G_UIState.lpanel_entity_state_mass_tbox, NULL},
        {"POS.TL", UI_ELEMENT_TEXTBOX_O, lpanel_row_tfield_size, VECTOR2D, &G_UIState.lpanel_entity_state_pos_tl_tbox, NULL},
        {"POS.C", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, VECTOR2D, &G_UIState.lpanel_entity_state_pos_c_tbox, NULL},
        {"VEL", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, VECTOR2D, &G_UIState.lpanel_entity_state_vel_tbox, NULL},
        {"ACCEL", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, VECTOR2D, &G_UIState.lpanel_entity_state_accel_tbox, NULL},
        {"MOMENT", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, VECTOR2D, &G_UIState.lpanel_entity_state_moment_tbox, NULL},
    };
    InitPanelFields(lpanel_state_entity_tcont, state_specs,
                    sizeof(state_specs) / sizeof(state_specs[0]), lpanel_tfield_padding,
                    WHITE_RGBA, COLOURLESS_RGBA);

    lpanel_btn_delete_entity_cont = CreatePanelContainer(
        lpanel_state_entity_tcont, lpanel_btn_cont_size,
        lpanel_btn_delete_entity_cont_offset, ZERO_VECTOR_2D,
        COLOURLESS_RGBA, COLOURLESS_RGBA,
        lpanel_btn_child_spacing, false, true);

    CreatePanelButtonDefault(lpanel_btn_delete_entity_cont, UI_ELEMENT_BUTTON_SUBMIT,
                             "DELETE", btn_default_size, btn_default_padding,
                             HandleBtnSubmitClick, &btn_action_delete_entity, NULL);
}

void InitCellStateContainer(void)
{
    lpanel_state_cell_tcont = CreatePanelContainer(
        lpanel_state_view->container, lpanel_state_cell_tcont_size,
        lpanel_state_cell_tcont_offset, tcont_default_padding,
        tcont_default_colour_border, tcont_default_colour_fill,
        tcont_default_child_spacing, true, true);

    CreatePanelTitleLabelDefault(lpanel_state_cell_tcont, "CELL STATE", lpanel_title_tfield_size, lpanel_tfield_padding);

    const PanelFieldSpec cell_specs[] = {
        {"INDEX", UI_ELEMENT_TEXTBOX_O, lpanel_row_tfield_size, FLOAT, NULL, &G_UIState.lpanel_cell_state_id_str},
        {"OCCU", UI_ELEMENT_TEXTBOX_O, lpanel_row_tfield_size, FLOAT, NULL, &G_UIState.lpanel_cell_state_occu_str},
        {"VALUE", UI_ELEMENT_TEXTBOX_O, lpanel_row_tfield_size, FLOAT, NULL, &G_UIState.lpanel_cell_state_value_str},
        {"FILL", UI_ELEMENT_TEXTBOX_O, lpanel_row_tfield_size, FLOAT, NULL, &G_UIState.lpanel_cell_state_fill_str},
    };
    InitPanelFields(lpanel_state_cell_tcont, cell_specs,
                    sizeof(cell_specs) / sizeof(cell_specs[0]), lpanel_tfield_padding,
                    WHITE_RGBA, COLOURLESS_RGBA);
}

void InitStatsContainer(void)
{
    lpanel_state_stats_tcont = CreatePanelContainer(
        lpanel_state_view->container, lpanel_state_stats_tcont_size,
        lpanel_state_stats_tcont_offset, tcont_default_padding,
        tcont_default_colour_border, tcont_default_colour_fill,
        tcont_default_child_spacing, true, true);

    CreatePanelTitleLabelDefault(lpanel_state_stats_tcont, "STATISTICS", lpanel_title_tfield_size, lpanel_tfield_padding);

    const PanelFieldSpec stats_specs[] = {
        {"POLYOIDS", UI_ELEMENT_TEXTBOX_O, lpanel_row_tfield_size, FLOAT, NULL, &G_UIState.lpanel_stats_polygs_str},
        {"MEM", UI_ELEMENT_TEXTBOX_O, lpanel_row_tfield_size, FLOAT, NULL, &G_UIState.lpanel_stats_mem_str},
        {"FPS", UI_ELEMENT_TEXTBOX_O, lpanel_row_tfield_size, FLOAT, NULL, &G_UIState.lpanel_stats_fps_str},
        {"F.TIME", UI_ELEMENT_TEXTBOX_O, lpanel_row_tfield_size, FLOAT, NULL, &G_UIState.lpanel_stats_ftime_str},
    };
    InitPanelFields(lpanel_state_stats_tcont, stats_specs,
                    sizeof(stats_specs) / sizeof(stats_specs[0]), lpanel_tfield_padding,
                    WHITE_RGBA, COLOURLESS_RGBA);
}

void InitEntityEditorContainer(void)
{
    lpanel_edit_entity_tcont = CreatePanelContainer(
        lpanel_edit_view_cont, lpanel_edit_entity_tcont_size,
        lpanel_edit_entity_tcont_offset, tcont_default_padding,
        tcont_default_colour_border, tcont_default_colour_fill,
        tcont_default_child_spacing, true, true);

    CreatePanelTitleLabelDefault(lpanel_edit_entity_view->container, "ENTITY EDIT", lpanel_title_tfield_size, lpanel_tfield_padding);

    const PanelFieldSpec edit_specs[] = {
        {"VERT.CNT", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, INT, &G_UIState.lpanel_entity_edit_vertice_count_tbox, NULL},
        {"WIDTH", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, INT, &G_UIState.lpanel_entity_edit_width_tbox, NULL},
        {"HEIGHT", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, FLOAT, &G_UIState.lpanel_entity_edit_height_tbox, NULL},
        {"MASS", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, FLOAT, &G_UIState.lpanel_entity_edit_mass_tbox, NULL},
        {"POS.C", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, FLOAT, &G_UIState.lpanel_entity_edit_pos_c_tbox, NULL},
        {"VEL", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, VECTOR2D, &G_UIState.lpanel_entity_edit_vel_tbox, NULL},
        {"ACCEL", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, VECTOR2D, &G_UIState.lpanel_entity_edit_accel_tbox, NULL},
        {"MOMENT", UI_ELEMENT_TEXTBOX_SAFE_IO, lpanel_row_tfield_size, VECTOR2D, &G_UIState.lpanel_entity_edit_moment_tbox, NULL},
    };
    InitPanelFields(lpanel_edit_entity_tcont, edit_specs,
                    sizeof(edit_specs) / sizeof(edit_specs[0]), lpanel_tfield_padding,
                    WHITE_RGBA, COLOURLESS_RGBA);

    lpanel_btn_create_entity_cont = CreatePanelContainer(
        lpanel_edit_entity_tcont, lpanel_btn_cont_size,
        lpanel_btn_create_entity_cont_offset, ZERO_VECTOR_2D,
        COLOURLESS_RGBA, COLOURLESS_RGBA,
        lpanel_btn_child_spacing, false, true);

    CreatePanelButtonDefault(lpanel_btn_create_entity_cont, UI_ELEMENT_BUTTON_SUBMIT,
                             "CREATE", btn_default_size, btn_default_padding,
                             HandleBtnSubmitClick, &btn_action_create_entity, NULL);
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
