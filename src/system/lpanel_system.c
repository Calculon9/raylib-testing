#include "system/lpanel_system.h"

#include "raylib.h"
#include <stdint.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "camera/camera.h"
#include "ui/ui.h"
#include "ui/text_region.h"
#include "ui/ui_renderer.h"
#include "system/ui_system.h"
#include "system/panel_ui_helpers.h"
#include "system/str_helpers.h"
#include "world/world.h"
#include "system/systems.h"

// Small static action codes to avoid heap allocations for immutable button tags
static int btn_action_create_entity = BUTTON_ACTION_CREATE_ENTITY;
static int btn_action_delete_entity = BUTTON_ACTION_DELETE_ENTITY;
static int btn_action_enumerate = 0;

// Static storage for a couple of small, long-lived Views to avoid heap allocs
static View lpanel_state_view_storage = {0};
static View lpanel_edit_entity_view_storage = {0};

// ----------LEFT PANEL SCREEN----------
// Visual Properties
static ColourRgba lpanel_text_colour = COLOUR_PANEL_DARK_1;
static ColourRgba lpanel_fill_colour = COLOUR_PANEL_DARK_1;
static Vector2d lpanel_default_padding = {0.1, 0.1};
static Vector2d lpanel_tfield_padding = {0.03f, 0.03f};
static Size lpanel_title_tfield_size = {{6.0f, 0.45f}, SIZE_FIXED};
static Size lpanel_row_tfield_size = {{6.0f, 0.5f}, SIZE_FIXED};

// Coordinate Space Properties
static CoordSpace2d lpanel_space = {0};

UIBox seed_box = {0};
UIElement *lpanel_root = {0};
View *lpanel_state_view = {0};
UIElement *lpanel_state_view_cont = {0};
UIElement *lpanel_edit_view_cont = {0};
UIElement *lpanel_btn_toggle_view_cont = {0};
UIElement *lpanel_btn_create_entity_cont = {0};
UIElement *lpanel_btn_delete_entity_cont = {0};
UIElement *lpanel_state_stats_tcont = {0};
UIElement *lpanel_state_entity_tcont = {0};
UIElement *lpanel_state_cell_tcont = {0};
View *lpanel_edit_entity_view = {0};
UIElement *lpanel_edit_entity_tcont = {0};

LArray lpanel_views = {0};

// ----Root Layout----
Offset lpanel_btn_toggle_view_cont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_btn_create_entity_cont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_btn_delete_entity_cont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_edit_view_cont_offset = {{0, 0.05}, OFFSET_PERCENT};
Offset lpanel_edit_entity_tcont_offset = {{0, 0.0}, OFFSET_PERCENT};
Offset lpanel_state_view_cont_offset = {{0, 0.05}, OFFSET_PERCENT};
Offset lpanel_state_cell_tcont_offset = {{0, 0.0}, OFFSET_PERCENT};
Offset lpanel_state_stats_tcont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_state_entity_tcont_offset = {{0.0, 0.0}, OFFSET_PERCENT};

Spacing lpanel_root_child_spacing = {{0, 0.0}, PERCENT, SPACING_NORMAL};

Size lpanel_root_size = {{1, 1}, SIZE_PERCENT};
Size lpanel_btn_cont_size = {{1.0f, 0.08f}, SIZE_PERCENT};
Size lpanel_state_view_cont_size = {{1.0f, 0.92f}, SIZE_PERCENT};
Size lpanel_state_stats_tcont_size = {{1.0f, 0.20f}, SIZE_PERCENT};
Size lpanel_state_entity_tcont_size = {{1.0f, 0.36f}, SIZE_PERCENT};
Size lpanel_state_cell_tcont_size = {{1.0f, 0.20f}, SIZE_PERCENT};
Size lpanel_edit_view_cont_size = {{1.0f, 0.92f}, SIZE_PERCENT};
Size lpanel_edit_entity_tcont_size = {{1.0f, 1.0f}, SIZE_PERCENT};

void InitPanelRoot(void);
void InitPanelStateView(void);
void InitPanelEditView(void);
void InitPanelToggleButtons(void);
void InitStatsContainer(void);
void InitCellStateContainer(void);
void InitEntityStateContainer(void);
void InitEntityEditorContainer(void);

static UIElement *CreateTitleLabel(UIElement *parent, const char *text)
{
    return CreatePanelTitleLabel(parent,
                                 text,
                                 lpanel_title_tfield_size,
                                 lpanel_tfield_padding,
                                 FONT_BASIC,
                                 COLOURLESS_RGBA,
                                 COLOURLESS_RGBA);
}

static UIElement *CreateLabeledTextField(UIElement *parent, const char *label_text)
{
    return CreatePanelLabeledField(parent,
                                   label_text,
                                   UI_ELEMENT_TEXTBOX_SAFE_IO,
                                   lpanel_row_tfield_size,
                                   tbox_default_size,
                                   lpanel_tfield_padding,
                                   tbox_tlabel_default_offset.offset,
                                   WHITE_RGBA,
                                   COLOURLESS_RGBA,
                                   tbox_default_padding,
                                   tbox_default_colour_border,
                                   tbox_default_colour_fill,
                                   FONT_BASIC);
}

void InitPanel()
{
    lpanel_views = MakeLArray(3, sizeof(UIElement *));
    lpanel_state_view = &lpanel_state_view_storage;
    lpanel_edit_entity_view = &lpanel_edit_entity_view_storage;
    InitPanelRoot();
    InitPanelToggleButtons();
    InitPanelStateView();
    InitPanelEditView();
}

void InitPanelStateView(void)
{
    lpanel_state_view_cont = CreatePanelContainer(lpanel_root,
                                                  lpanel_state_view_cont_size,
                                                  lpanel_state_view_cont_offset,
                                                  ZERO_VECTOR_2D,
                                                  COLOURLESS_RGBA,
                                                  COLOURLESS_RGBA,
                                                  cont_default_child_spacing,
                                                  false,
                                                  true);
    lpanel_state_view->container = lpanel_state_view_cont;
    lpanel_state_view->type = LPANEL_STATE_VIEW;
    InitStatsContainer();
    InitEntityStateContainer();
    InitCellStateContainer();

    LArray_Push(&lpanel_views, &lpanel_state_view);
}

void InitPanelEditView(void)
{
    lpanel_edit_view_cont = CreatePanelContainer(lpanel_root,
                                                 lpanel_edit_view_cont_size,
                                                 lpanel_edit_view_cont_offset,
                                                 ZERO_VECTOR_2D,
                                                 COLOURLESS_RGBA,
                                                 COLOURLESS_RGBA,
                                                 cont_default_child_spacing,
                                                 false,
                                                 false);
    lpanel_edit_entity_view->container = lpanel_edit_view_cont;
    lpanel_edit_entity_view->type = LPANEL_EDIT_ENTITY_VIEW;
    InitEntityEditorContainer();

    LArray_Push(&lpanel_views, &lpanel_edit_entity_view);
}

void InitPanelRoot(void)
{
    Basis2d lpanel_basis = (Basis2d){lpanel_u, lpanel_v};
    Basis2d lpanel_pixel_basis = (Basis2d){lpanel_pixel_u, lpanel_pixel_v};
    camera_lpanel = CreateCamera2d(lpanel_pixel_basis, lpanel_basis, lpanel_pixel_origin, lpanel_origin, 1, 0);

    lpanel_space = NewCoordSpace2d(lpanel_origin, lpanel_resolution, lpanel_basis);
    lpanel_root = CreateUIElement(UI_ELEMENT_ROOT, lpanel_root_size, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, lpanel_default_padding, COLOURLESS_RGBA, lpanel_fill_colour);
    lpanel_root->data.root.coord_space = lpanel_space;
    lpanel_root->child_spacing = lpanel_root_child_spacing;

    Vector2d basis_scale = BasisTransform_2d_Scale(camera_lpanel.source_basis, camera_lpanel.destination_basis);
    seed_box.coords = (Vector2d){lpanel_pixel_origin.x, lpanel_pixel_origin.y};
    seed_box.dimensions = (Vector2d){lpanel_resolution.x * basis_scale.x, lpanel_resolution.y * basis_scale.y};
}

void InitPanelToggleButtons(void)
{
    lpanel_btn_toggle_view_cont = CreatePanelContainer(lpanel_root,
                                                       lpanel_btn_cont_size,
                                                       lpanel_btn_toggle_view_cont_offset,
                                                       ZERO_VECTOR_2D,
                                                       COLOURLESS_RGBA,
                                                       COLOURLESS_RGBA,
                                                       btn_cont_default_child_spacing,
                                                       false,
                                                       true);
    char *btn_labels[] = {"STATE -- UTIL"};

    for (int i = 0; i < 1; i++)
    {
        btn_action_enumerate = i;
        CreatePanelButton(lpanel_btn_toggle_view_cont,
                          UI_ELEMENT_BUTTON_ENUMERATE,
                          btn_labels[i],
                          btn_default_size,
                          btn_default_padding,
                          btn_default_colour_border,
                          btn_default_colour_fill,
                          FONT_BASIC,
                          HandleBtnEnumerateClick,
                          &btn_action_enumerate,
                          &lpanel_views);
    }
}

void InitEntityStateContainer(void)
{
    lpanel_state_entity_tcont = CreatePanelContainer(lpanel_state_view->container,
                                                     lpanel_state_entity_tcont_size,
                                                     lpanel_state_entity_tcont_offset,
                                                     tcont_default_padding,
                                                     tcont_default_colour_border,
                                                     tcont_default_colour_fill,
                                                     tcont_default_child_spacing,
                                                     true,
                                                     true);

    char *tbox_labels[] = {"OBJECT PROPERTIES", "ID", "MASS", "POS.TL", "POS.C", "VEL", "ACCEL", "MOMENT"};
    UIElement **state_map_tbox[] = {NULL, &G_UIState.lpanel_entity_state_id_tbox, &G_UIState.lpanel_entity_state_mass_tbox, &G_UIState.lpanel_entity_state_pos_tl_tbox, &G_UIState.lpanel_entity_state_pos_c_tbox,
                                    &G_UIState.lpanel_entity_state_vel_tbox, &G_UIState.lpanel_entity_state_accel_tbox, &G_UIState.lpanel_entity_state_moment_tbox};

    CreateTitleLabel(lpanel_state_entity_tcont, tbox_labels[0]);

    for (int i = 1; i < 8; i++)
    {
        UIElement *input_child = CreateLabeledTextField(lpanel_state_entity_tcont, tbox_labels[i]);
        if (input_child)
        {
            input_child->type = UI_ELEMENT_TEXTBOX_SAFE_IO;

            if (i == 1)
            {
                input_child->type = UI_ELEMENT_TEXTBOX_O;
                input_child->data.textbox.data_type = FLOAT;
            }
            if (i == 3)
                input_child->type = UI_ELEMENT_TEXTBOX_O;
            if (i > 2)
                input_child->data.textbox.data_type = VECTOR2D;
            else
                input_child->data.textbox.data_type = FLOAT;

            UIElement **global_ptr_address = state_map_tbox[i];
            if (global_ptr_address != NULL)
            {
                *global_ptr_address = input_child;
            }
        }
    }

    lpanel_btn_delete_entity_cont = CreatePanelContainer(lpanel_state_entity_tcont,
                                                         lpanel_btn_cont_size,
                                                         lpanel_btn_delete_entity_cont_offset,
                                                         ZERO_VECTOR_2D,
                                                         COLOURLESS_RGBA,
                                                         COLOURLESS_RGBA,
                                                         btn_cont_default_child_spacing,
                                                         false,
                                                         true);

    CreatePanelButton(lpanel_btn_delete_entity_cont,
                      UI_ELEMENT_BUTTON_SUBMIT,
                      "DELETE",
                      btn_default_size,
                      btn_default_padding,
                      btn_default_colour_border,
                      btn_default_colour_fill,
                      FONT_BASIC,
                      HandleBtnSubmitClick,
                      &btn_action_delete_entity,
                      NULL);
}

void InitCellStateContainer(void)
{
    lpanel_state_cell_tcont = CreatePanelContainer(lpanel_state_view->container,
                                                   lpanel_state_cell_tcont_size,
                                                   lpanel_state_cell_tcont_offset,
                                                   tcont_default_padding,
                                                   tcont_default_colour_border,
                                                   tcont_default_colour_fill,
                                                   tcont_default_child_spacing,
                                                   true,
                                                   true);

    char *tbox_labels[] = {"CELL STATE", "INDEX", "OCCU", "VALUE", "FILL"};
    String64 **state_map_str[] = {NULL, &G_UIState.lpanel_cell_state_id_str, &G_UIState.lpanel_cell_state_occu_str, &G_UIState.lpanel_cell_state_value_str, &G_UIState.lpanel_cell_state_fill_str};

    CreateTitleLabel(lpanel_state_cell_tcont, tbox_labels[0]);

    for (int i = 1; i < 5; i++)
    {
        UIElement *input_child = CreateLabeledTextField(lpanel_state_cell_tcont, tbox_labels[i]);
        if (input_child)
        {
            input_child->type = UI_ELEMENT_TEXTBOX_O;
            String64 **global_ptr_address = state_map_str[i];
            if (global_ptr_address != NULL)
            {
                *global_ptr_address = &input_child->data.textbox.text;
            }
        }
    }
}

void InitStatsContainer(void)
{
    lpanel_state_stats_tcont = CreatePanelContainer(lpanel_state_view->container,
                                                    lpanel_state_stats_tcont_size,
                                                    lpanel_state_stats_tcont_offset,
                                                    tcont_default_padding,
                                                    tcont_default_colour_border,
                                                    tcont_default_colour_fill,
                                                    tcont_default_child_spacing,
                                                    true,
                                                    true);

    char *tbox_labels[] = {"STATISTICS", "POLYOIDS", "MEM", "FPS", "F.TIME"};
    String64 **state_map_str[] = {NULL, &G_UIState.lpanel_stats_polygs_str, &G_UIState.lpanel_stats_mem_str, &G_UIState.lpanel_stats_fps_str, &G_UIState.lpanel_stats_ftime_str};

    CreateTitleLabel(lpanel_state_stats_tcont, tbox_labels[0]);

    for (int i = 1; i < 5; i++)
    {
        UIElement *input_child = CreateLabeledTextField(lpanel_state_stats_tcont, tbox_labels[i]);
        if (input_child)
        {
            input_child->type = UI_ELEMENT_TEXTBOX_O;
            String64 **global_ptr_address = state_map_str[i];
            if (global_ptr_address != NULL)
            {
                *global_ptr_address = &input_child->data.textbox.text;
            }
        }
    }
}

void InitEntityEditorContainer(void)
{
    lpanel_edit_entity_tcont = CreatePanelContainer(lpanel_edit_view_cont,
                                                    lpanel_edit_entity_tcont_size,
                                                    lpanel_edit_entity_tcont_offset,
                                                    tcont_default_padding,
                                                    tcont_default_colour_border,
                                                    tcont_default_colour_fill,
                                                    tcont_default_child_spacing,
                                                    true,
                                                    true);

    char *tbox_labels[] = {"ENTITY EDIT", "VERT.CNT", "WIDTH", "HEIGHT", "MASS", "POS.C", "VEL", "ACCEL", "MOMENT"};
    UIElement **map_tbox[] = {NULL, &G_UIState.lpanel_entity_edit_vertice_count_tbox, &G_UIState.lpanel_entity_edit_width_tbox, &G_UIState.lpanel_entity_edit_height_tbox,
                              &G_UIState.lpanel_entity_edit_mass_tbox, &G_UIState.lpanel_entity_edit_pos_c_tbox, &G_UIState.lpanel_entity_edit_vel_tbox, &G_UIState.lpanel_entity_edit_accel_tbox, &G_UIState.lpanel_entity_edit_moment_tbox};

    CreateTitleLabel(lpanel_edit_entity_view->container, tbox_labels[0]);

    for (int i = 1; i < 9; i++)
    {
        UIElement *input_child = CreateLabeledTextField(lpanel_edit_entity_tcont, tbox_labels[i]);
        if (input_child)
        {
            input_child->type = UI_ELEMENT_TEXTBOX_SAFE_IO;
            UIElement **global_ptr_address = map_tbox[i];
            if (global_ptr_address != NULL)
            {
                *global_ptr_address = input_child;
            }
            if (i == 1 || i == 2)
                input_child->data.textbox.data_type = INT;
            else if (i < 6)
                input_child->data.textbox.data_type = FLOAT;
            else
                input_child->data.textbox.data_type = VECTOR2D;
        }
    }

    lpanel_btn_create_entity_cont = CreatePanelContainer(lpanel_edit_entity_tcont,
                                                         lpanel_btn_cont_size,
                                                         lpanel_btn_create_entity_cont_offset,
                                                         ZERO_VECTOR_2D,
                                                         COLOURLESS_RGBA,
                                                         COLOURLESS_RGBA,
                                                         btn_cont_default_child_spacing,
                                                         false,
                                                         true);

    CreatePanelButton(lpanel_btn_create_entity_cont,
                      UI_ELEMENT_BUTTON_SUBMIT,
                      "CREATE",
                      btn_default_size,
                      btn_default_padding,
                      btn_default_colour_border,
                      btn_default_colour_fill,
                      FONT_BASIC,
                      HandleBtnSubmitClick,
                      &btn_action_create_entity,
                      NULL);
}

void InitLPanel(void)
{
    InitPanel();
}

void UpdateLPanel(int mouse_x, int mouse_y)
{
    (void)mouse_x;
    (void)mouse_y;
}

void DrawLPanel(void)
{
    DrawRootUIElement(lpanel_root, seed_box, camera_lpanel);
}
