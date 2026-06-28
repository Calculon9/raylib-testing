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
#include "world/world.h"
#include "system/systems.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//  ----------LEFT PANEL SCREEN----------
//   Visual Properties
static ColourRgba lpanel_text_colour = COLOUR_PANEL_DARK_1;
static ColourRgba lpanel_fill_colour = COLOUR_PANEL_DARK_1;
static Vector2d lpanel_default_padding = {0.1, 0.1};

// Coordinate Space Properties
static CoordSpace2d lpanel_space = {0};
Vector2d lpanel_origin, lpanel_end = {0}; // Dependent on the game world screen area
Vector2d lpanel_pixel_origin, lpanel_pixel_end = {0};
Vector2d lpanel_u = {1, 0};
Vector2d lpanel_v = {0, 1};
Vector2d lpanel_resolution = {0};
// Logical->pixel-space conversion properties
Camera2d camera_lpanel = {0};
Vector2d lpanel_pixel_u = {0};
Vector2d lpanel_pixel_v = {0};
Vector2d local_to_lpanel_scale = {0};
Vector2d lpanel_to_local_scale = {0};

UIBox seed_box = {0}; // This is the box that will be used as the parent box for the root element of the panel, and all other elements will calculate their positions and dimensions based on this box, which represents the entire panel area in pixel coordinates
UIElement *lpanel_root = {0};
View *lpanel_state_view = {0};
UIElement *lpanel_state_view_cont = {0};
UIElement *lpanel_edit_view_cont = {0};
UIElement *lpanel_btn_cont = {0};
UIElement *lpanel_state_stats_tcont = {0};
UIElement *lpanel_state_entity_tcont = {0};
UIElement *lpanel_state_cell_tcont = {0};
View *lpanel_edit_entity_view = {0};
UIElement *lpanel_edit_entity_tcont = {0};

LArray lpanel_views = {0}; // lpanel_state_views

// ----Root Layout----
Offset lpanel_btn_cont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_edit_view_cont_offset = {{0, 0.05}, OFFSET_PERCENT};
Offset lpanel_edit_entity_tcont_offset = {{0, 0.0}, OFFSET_PERCENT};
Offset lpanel_state_view_cont_offset = {{0, 0.05}, OFFSET_PERCENT};
Offset lpanel_state_cell_tcont_offset = {{0, 0.0}, OFFSET_PERCENT};
Offset lpanel_state_stats_tcont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_state_entity_tcont_offset = {{0.0, 0.0}, OFFSET_PERCENT};

Spacing lpanel_root_child_spacing = {{0, 0.0}, PERCENT, SPACING_NORMAL};

Size lpanel_root_size = {{1, 1}, SIZE_PERCENT};
Size lpanel_btn_cont_size = {{1, 0.05}, SIZE_PERCENT};
Size lpanel_state_view_cont_size = {{1, 0.95}, SIZE_PERCENT}; // Not 1,1 because we want to leave room for the buttons at the bottom of the panel
Size lpanel_state_stats_tcont_size = {{1, 0.275}, SIZE_PERCENT};
Size lpanel_state_entity_tcont_size = {{1, 0.45}, SIZE_PERCENT};
Size lpanel_state_cell_tcont_size = {{1, 0.275}, SIZE_PERCENT};
Size lpanel_edit_view_cont_size = {{1, 0.95}, SIZE_PERCENT}; // Not 1,1 because we want to leave room for the buttons at the bottom of the panel
Size lpanel_edit_entity_tcont_size = {{1, 1}, SIZE_PERCENT};

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
void InitPanelRoot(void);
void InitPanelStateView(void);
void InitPanelEditView(void);
void InitPanelToggleButtons(void);
void InitStatsContainer(void);
void InitCellStateContainer(void);
void InitEntityStateContainer(void);
void InitEntityEditorContainer(void);

void InitPanel()
{
    // Init memory for the panel UI elements
    lpanel_views = MakeLArray(3, sizeof(UIElement *));
    lpanel_state_view = AllocateBytes(sizeof(View));
    lpanel_edit_entity_view = AllocateBytes(sizeof(View));
    InitPanelRoot();
    InitPanelToggleButtons();
    InitPanelStateView();
    InitPanelEditView();

    // UIElement *btn = lpanel_btn_cont ? lpanel_btn_cont->first_child : NULL;
    // if (btn)
    // {
    //     btn->data.button.slave = lpanel_state_view_cont;
    // }
}

void InitPanelStateView(void)
{
    // Create the Container (The Parent)
    // Note: this is so we can move the entire state view container around the panel if we want to, and all the child elements will move with it.
    lpanel_state_view_cont = CreateUIElementInTree(UI_ELEMENT_CONTAINER, lpanel_state_view_cont_size, lpanel_root, lpanel_state_view_cont_offset, ZERO_VECTOR_2D, COLOURLESS_RGBA, COLOURLESS_RGBA);
    lpanel_state_view_cont->child_spacing = cont_default_child_spacing;
    lpanel_state_view->container = lpanel_state_view_cont;
    lpanel_state_view->type = LPANEL_STATE_VIEW;
    InitStatsContainer();
    InitEntityStateContainer();
    InitCellStateContainer();

    LArray_Push(&lpanel_views, &lpanel_state_view);
}

void InitPanelEditView(void)
{
    // Create the Container (The Parent)
    // Note: this is so we can move the entire state view container around the panel if we want to, and all the child elements will move with it.
    lpanel_edit_view_cont = CreateUIElementInTree(UI_ELEMENT_CONTAINER, lpanel_edit_view_cont_size, lpanel_root, lpanel_edit_view_cont_offset, ZERO_VECTOR_2D, COLOURLESS_RGBA, COLOURLESS_RGBA);
    lpanel_edit_view_cont->child_spacing = cont_default_child_spacing;
    lpanel_edit_view_cont->is_enabled = false;
    lpanel_edit_entity_view->container = lpanel_edit_view_cont;
    lpanel_edit_entity_view->type = LPANEL_EDIT_ENTITY_VIEW;
    InitEntityEditorContainer();

    LArray_Push(&lpanel_views, &lpanel_edit_entity_view);
}

void InitPanelRoot(void)
{
    // 0. Define the properties of the panel space and camera based on the overall screen properties defined in Step 0, and then use those properties to initialise the panel's camera, coordinate space and UI elements in the subsequent steps
    // INIT CAMERA using using the resolutions, sceen basis, origins etc. from Step 0
    Basis2d lpanel_basis = (Basis2d){lpanel_u, lpanel_v};
    Basis2d lpanel_pixel_basis = (Basis2d){lpanel_pixel_u, lpanel_pixel_v};
    camera_lpanel = CreateCamera2d(lpanel_pixel_basis, lpanel_basis, lpanel_pixel_origin, lpanel_origin, 1, 0);

    // INIT a LOCAL COORD SPACE for the SIDE PANEL using the resolutions, origins etc. from Step 0
    // This will be the Root UI Element for the panel, and all other UI elements within the panel will be children of this Root element and will use this coordinate space for their positioning and dimensions
    lpanel_space = NewCoordSpace2d(lpanel_origin, lpanel_resolution, lpanel_basis);
    lpanel_root = CreateUIElement(UI_ELEMENT_ROOT, (Size){lpanel_space.resolution_ixj, SIZE_FILL}, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, lpanel_default_padding, COLOURLESS_RGBA, lpanel_fill_colour);
    lpanel_root->data.root.coord_space = lpanel_space;
    lpanel_root->child_spacing = lpanel_root_child_spacing;

    Vector2d basis_scale = BasisTransform_2d_Scale(camera_lpanel.source_basis, camera_lpanel.destination_basis);
    seed_box.coords = (Vector2d){lpanel_pixel_origin.x * basis_scale.x, lpanel_pixel_origin.y * basis_scale.y};
    seed_box.dimensions = (Vector2d){lpanel_resolution.x * basis_scale.x, lpanel_resolution.y * basis_scale.y}; //

    // Init input buffers
}

void InitPanelToggleButtons(void)
{
    lpanel_btn_cont = CreateUIElementInTree(UI_ELEMENT_CONTAINER, lpanel_btn_cont_size, lpanel_root, lpanel_btn_cont_offset, ZERO_VECTOR_2D, COLOURLESS_RGBA, COLOURLESS_RGBA);
    lpanel_btn_cont->child_spacing = btn_cont_default_child_spacing;
    char *btn_labels[] = {"STATE -- UTIL"};

    for (int i = 0; i < 1; i++)
    {
        UIElement *btn = CreateUIElementInTree(UI_ELEMENT_BUTTON_ENUMERATE, btn_default_size, lpanel_btn_cont, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, btn_default_padding, COLOUR_PANEL_LIGHT_2, COLOUR_PANEL_LIGHT_3);
        strncpy(btn->data.button.label.string, btn_labels[i], MAX_LABEL_CHARS - 1);
        btn->data.button.label.string[MAX_LABEL_CHARS - 1] = '\0';
        btn->data.button.font = FONT_BASIC;
        btn->is_draggable = true;
        btn->data.button.data_bind = &lpanel_views;
        btn->data.button.on_click = HandleBtnEnumerateClick;
        btn->data.button.user_data = AllocateBytes(sizeof(int));
        *(int *)(btn->data.button.user_data) = 0;
        // btn->data.textbox.data_type = FLOAT;
    }
}

//----------------------------------------------------------------------------------
// STATE View Functions
//----------------------------------------------------------------------------------
void InitEntityStateContainer(void)
{
    // Create the Container (The Parent)
    // Note: We use an offset relative to lpanel_root, NOT an absolute origin.
    lpanel_state_entity_tcont = CreateUIElementInTree(UI_ELEMENT_CONTAINER, lpanel_state_entity_tcont_size, lpanel_state_view->container, lpanel_state_entity_tcont_offset, tcont_default_padding,
                                                      tcont_default_colour_border, tcont_default_colour_fill);
    // CreateTextFieldContainerInTree(lpanel_entity_state_tcont_size, lpanel_state_view_cont, lpanel_entity_state_tcont_offset, tcont_default_padding, tcont_default_child_spacing,
    // tcont_default_colour_border, tcont_default_colour_fill);
    lpanel_state_entity_tcont->is_draggable = true;
    lpanel_state_entity_tcont->child_spacing = tcont_default_child_spacing;

    char *tbox_labels[] = {"OBJECT PROPERTIES", "ID", "MASS", "POS.TL", "POS.C", "VEL", "ACCEL", "MOMENT"};
    // String64 **state_map_str[] = {NULL, &G_UIState.lpanel_entity_state_id_str, &G_UIState.lpanel_entity_state_mass_str, &G_UIState.lpanel_entity_state_pos_str, &G_UIState.lpanel_entity_state_vel_str, &G_UIState.lpanel_entity_state_accel_str, &G_UIState.lpanel_entity_state_moment_str};
    UIElement **state_map_tbox[] = {NULL, &G_UIState.lpanel_entity_state_id_tbox, &G_UIState.lpanel_entity_state_mass_tbox, &G_UIState.lpanel_entity_state_pos_tl_tbox, &G_UIState.lpanel_entity_state_pos_c_tbox,
                                    &G_UIState.lpanel_entity_state_vel_tbox, &G_UIState.lpanel_entity_state_accel_tbox, &G_UIState.lpanel_entity_state_moment_tbox};

    // Create Title (Label)
    UIElement *title = CreateUIElementInTree(UI_ELEMENT_LABEL, tfield_default_size, lpanel_state_entity_tcont, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, tfield_default_padding, COLOURLESS_RGBA, COLOURLESS_RGBA);
    strncpy(title->data.label.text.string, tbox_labels[0], MAX_LABEL_CHARS - 1);
    title->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';
    title->data.label.font = FONT_BASIC;
    title->is_draggable = true;

    for (int i = 1; i < 8; i++)
    {
        // Calculate the local offset for this TextField within the container
        // Formula: (Spacing + Height) * index
        // Create the TextFieldtbox_default_size
        UIElement *tfield = CreateTextFieldInTree(tfield_default_size, lpanel_state_entity_tcont, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, tbox_default_size, tfield_default_padding,
                                                  tbox_tlabel_default_offset.offset, COLOURLESS_RGBA, COLOURLESS_RGBA);
        tfield->is_draggable = true;

        // Access Children via the Tree
        UIElement *label_child = tfield->first_child;
        UIElement *input_child = (label_child) ? label_child->next_sibling : NULL;

        if (label_child && input_child)
        {
            // Configure Label
            label_child->padding = tbox_default_padding;
            label_child->colour_border = tbox_default_colour_border;
            label_child->colour_fill = tbox_default_colour_fill;
            label_child->data.label.font = FONT_BASIC;

            strncpy(label_child->data.label.text.string, tbox_labels[i], MAX_LABEL_CHARS - 1);
            label_child->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';

            // Configure TextBox
            input_child->padding = tbox_default_padding;
            input_child->colour_border = tbox_default_colour_border;
            input_child->colour_fill = tbox_default_colour_fill;
            input_child->data.textbox.font = FONT_BASIC;
            input_child->type = UI_ELEMENT_TEXTBOX_SAFE_IO; // Will only save over previous text if ENTER is pressed

            // ID will be output-only
            LOG_INFO("[DEBUG CONTEXT] Address: %p | Raw Value: %d\n", (void *)&i, i);
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

            // String64 **global_ptr_address = state_map_str[i];
            UIElement **global_ptr_address = state_map_tbox[i];
            if (global_ptr_address != NULL)
            {
                // Dereference once (*) to overwrite the actual pointer inside G_UIState
                *global_ptr_address = input_child;
            }
        }
    }
}

void InitCellStateContainer(void)
{
    // Create the Container (The Parent)
    // Note: We use an offset relative to lpanel_root, NOT an absolute origin.
    lpanel_state_cell_tcont = CreateUIElementInTree(UI_ELEMENT_CONTAINER, lpanel_state_cell_tcont_size, lpanel_state_view->container, lpanel_state_cell_tcont_offset, tcont_default_padding,
                                                    tcont_default_colour_border, tcont_default_colour_fill);
    lpanel_state_cell_tcont->is_draggable = true;
    lpanel_state_cell_tcont->child_spacing = tcont_default_child_spacing;

    char *tbox_labels[] = {"CELL STATE", "INDEX", "OCCU", "VALUE", "FILL"};
    String64 **state_map_str[] = {NULL, &G_UIState.lpanel_cell_state_id_str, &G_UIState.lpanel_cell_state_occu_str, &G_UIState.lpanel_cell_state_value_str, &G_UIState.lpanel_cell_state_fill_str};

    // Create Title (Label)
    UIElement *title = CreateUIElementInTree(UI_ELEMENT_LABEL, tfield_default_size, lpanel_state_cell_tcont, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, tfield_default_padding, COLOURLESS_RGBA, COLOURLESS_RGBA);
    strncpy(title->data.label.text.string, tbox_labels[0], MAX_LABEL_CHARS - 1);
    title->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';
    title->data.label.font = FONT_BASIC;
    title->is_draggable = true;

    for (int i = 1; i < 5; i++)
    {
        // Calculate the local offset for this TextField within the container
        // Formula: (Spacing + Height) * index
        // Create the TextField
        UIElement *tfield = CreateTextFieldInTree(tfield_default_size, lpanel_state_cell_tcont, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, tbox_default_size, tfield_default_padding,
                                                  tbox_tlabel_default_offset.offset, COLOURLESS_RGBA, COLOURLESS_RGBA);
        tfield->is_draggable = true;

        // Access Children via the Tree
        UIElement *label_child = tfield->first_child;
        UIElement *input_child = (label_child) ? label_child->next_sibling : NULL;

        if (label_child && input_child)
        {
            // Configure Label
            label_child->padding = tbox_default_padding;
            label_child->colour_border = tbox_default_colour_border;
            label_child->colour_fill = tbox_default_colour_fill;
            label_child->data.label.font = FONT_BASIC;

            strncpy(label_child->data.label.text.string, tbox_labels[i], MAX_LABEL_CHARS - 1);
            label_child->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';

            // Configure TextBox
            input_child->padding = tbox_default_padding;
            input_child->colour_border = tbox_default_colour_border;
            input_child->colour_fill = tbox_default_colour_fill;
            input_child->data.textbox.font = FONT_BASIC;
            input_child->type = UI_ELEMENT_TEXTBOX_O; // Will only save over previous text if ENTER is pressed
            String64 **global_ptr_address = state_map_str[i];
            // UIElement **global_ptr_address = state_map_str[i];
            if (global_ptr_address != NULL)
            {
                // Dereference once (*) to overwrite the actual pointer inside G_UIState
                *global_ptr_address = &input_child->data.textbox.text;
            }
        }
    }
}

void InitStatsContainer(void)
{
    // Create the Container (The Parent)
    // Note: We use an offset relative to lpanel_root, NOT an absolute origin.
    lpanel_state_stats_tcont = CreateUIElementInTree(UI_ELEMENT_CONTAINER, lpanel_state_stats_tcont_size, lpanel_state_view->container, lpanel_state_stats_tcont_offset, tcont_default_padding, tcont_default_colour_border,
                                                     tcont_default_colour_fill);
    lpanel_state_stats_tcont->is_draggable = true;
    lpanel_state_stats_tcont->child_spacing = tcont_default_child_spacing;

    char *tbox_labels[] = {"STATISTICS", "POLYOIDS", "MEM", "FPS", "F.TIME"};
    String64 **state_map_str[] = {NULL, &G_UIState.lpanel_stats_polygs_str, &G_UIState.lpanel_stats_mem_str, &G_UIState.lpanel_stats_fps_str, &G_UIState.lpanel_stats_ftime_str};

    // Create Title (Label)
    UIElement *title = CreateUIElementInTree(UI_ELEMENT_LABEL, tfield_default_size, lpanel_state_stats_tcont, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                             tfield_default_padding, COLOURLESS_RGBA, COLOURLESS_RGBA);
    strncpy(title->data.label.text.string, tbox_labels[0], MAX_LABEL_CHARS - 1);
    title->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';
    title->data.label.font = FONT_BASIC;
    title->is_draggable = true;

    for (int i = 1; i < 5; i++)
    {
        // Calculate the local offset for this TextField within the container
        // Formula: (Spacing + Height) * index
        // Create the TextField
        UIElement *tfield = CreateTextFieldInTree(tfield_default_size, lpanel_state_stats_tcont, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, tbox_default_size, tfield_default_padding,
                                                  tbox_tlabel_default_offset.offset,
                                                  COLOURLESS_RGBA, COLOURLESS_RGBA);
        tfield->is_draggable = true;

        // Access Children via the Tree
        UIElement *label_child = tfield->first_child;
        UIElement *input_child = (label_child) ? label_child->next_sibling : NULL;

        if (label_child && input_child)
        {
            // Configure Label
            label_child->padding = tbox_default_padding;
            label_child->colour_border = tbox_default_colour_border;
            label_child->colour_fill = tbox_default_colour_fill;
            label_child->data.label.font = FONT_BASIC;

            strncpy(label_child->data.label.text.string, tbox_labels[i], MAX_LABEL_CHARS - 1);
            label_child->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';

            // Configure TextBox
            input_child->padding = tbox_default_padding;
            input_child->colour_border = tbox_default_colour_border;
            input_child->colour_fill = tbox_default_colour_fill;
            input_child->data.textbox.font = FONT_BASIC;
            input_child->type = UI_ELEMENT_TEXTBOX_O; // Will only save over previous text if ENTER is pressed
            String64 **global_ptr_address = state_map_str[i];
            // UIElement **global_ptr_address = state_map_str[i];
            if (global_ptr_address != NULL)
            {
                // Dereference once (*) to overwrite the actual pointer inside G_UIState
                *global_ptr_address = &input_child->data.textbox.text;
            }
        }
    }
}

//----------------------------------------------------------------------------------
// EDIT View Functions
//----------------------------------------------------------------------------------
void InitEntityEditorContainer(void)
{
    // Create the Container (The Parent)
    // Note: We use an offset relative to lpanel_root, NOT an absolute origin.
    lpanel_edit_entity_tcont = CreateUIElementInTree(UI_ELEMENT_CONTAINER, lpanel_edit_entity_tcont_size, lpanel_edit_view_cont, lpanel_edit_entity_tcont_offset, tcont_default_padding,
                                                     tcont_default_colour_border, tcont_default_colour_fill);
    lpanel_edit_entity_tcont->is_draggable = true;
    lpanel_edit_entity_tcont->child_spacing = tcont_default_child_spacing;

    char *tbox_labels[] = {"ENTITY EDIT", "EDGE.CNT", "VERT.CNT", "WIDTH", "HEIGHT", "MASS", "POS.C", "VEL", "ACCEL", "MOMENT"};
    UIElement **map_tbox[] = {NULL, &G_UIState.lpanel_entity_edit_edge_count_tbox, &G_UIState.lpanel_entity_edit_vertice_count_tbox, &G_UIState.lpanel_entity_edit_width_tbox, &G_UIState.lpanel_entity_edit_height_tbox,
                              &G_UIState.lpanel_entity_edit_mass_tbox, &G_UIState.lpanel_entity_edit_pos_c_tbox, &G_UIState.lpanel_entity_edit_vel_tbox, &G_UIState.lpanel_entity_edit_accel_tbox, &G_UIState.lpanel_entity_edit_moment_tbox};
    // String64 **map_str[] = {NULL, &G_UIState.lpanel_entity_edit_edge_count_str, &G_UIState.lpanel_entity_edit_vertice_count_str, &G_UIState.lpanel_entity_edit_width_str, &G_UIState.lpanel_entity_edit_height_str,
    //                       &G_UIState.lpanel_entity_edit_mass_str, &G_UIState.lpanel_entity_edit_pos_c_str, &G_UIState.lpanel_entity_edit_vel_str, &G_UIState.lpanel_entity_edit_accel_str, &G_UIState.lpanel_entity_edit_moment_str};

    // Create Title (Label)
    UIElement *title = CreateUIElementInTree(UI_ELEMENT_LABEL, tfield_default_size, lpanel_edit_entity_view->container, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, tfield_default_padding, COLOURLESS_RGBA, COLOURLESS_RGBA);
    strncpy(title->data.label.text.string, tbox_labels[0], MAX_LABEL_CHARS - 1);
    title->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';
    title->data.label.font = FONT_BASIC;
    title->is_draggable = true;

    for (int i = 1; i < 10; i++)
    {
        // Calculate the local offset for this TextField within the container
        // Formula: (Spacing + Height) * index
        // Create the TextField
        UIElement *tfield = CreateTextFieldInTree(tfield_default_size, lpanel_edit_entity_tcont, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, tbox_default_size, tfield_default_padding,
                                                  tbox_tlabel_default_offset.offset, COLOURLESS_RGBA, COLOURLESS_RGBA);
        tfield->is_draggable = true;

        // Access Children via the Tree
        UIElement *label_child = tfield->first_child;
        UIElement *input_child = (label_child) ? label_child->next_sibling : NULL;

        if (label_child && input_child)
        {
            // Configure Label
            label_child->padding = tbox_default_padding;
            label_child->colour_border = tbox_default_colour_border;
            label_child->colour_fill = tbox_default_colour_fill;
            label_child->data.label.font = FONT_BASIC;

            strncpy(label_child->data.label.text.string, tbox_labels[i], MAX_LABEL_CHARS - 1);
            label_child->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';

            // Configure TextBox
            input_child->padding = tbox_default_padding;
            input_child->colour_border = tbox_default_colour_border;
            input_child->colour_fill = tbox_default_colour_fill;
            input_child->data.textbox.font = FONT_BASIC;
            input_child->type = UI_ELEMENT_TEXTBOX_SAFE_IO; // Will only save over previous text if ENTER is pressed
            UIElement **global_ptr_address = map_tbox[i];
            // UIElement **global_ptr_address = state_map_str[i];
            if (global_ptr_address != NULL)
            {
                // Dereference once (*) to overwrite the actual pointer inside G_UIState
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

    UIElement *submit_btn = CreateUIElementInTree(UI_ELEMENT_BUTTON_SUBMIT, btn_default_size, lpanel_edit_entity_tcont, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                                  btn_default_padding, COLOUR_PANEL_LIGHT_2, COLOUR_PANEL_LIGHT_3);

    strncpy(submit_btn->data.button.label.string, "CREATE", MAX_LABEL_CHARS - 1);
    submit_btn->data.button.label.string[MAX_LABEL_CHARS - 1] = '\0';
    submit_btn->data.button.font = FONT_BASIC;
    submit_btn->is_draggable = true;
    submit_btn->data.button.on_click = HandleBtnSubmitClick;
    submit_btn->data.button.data_bind = (void *)"create-entity"; // Tag this button as the create-entity submit action.
}   Need to associate the submit button with the entity creating (enum?), which will read the values from the textboxes and create a new entity in the game world. Currently it doesnt work.