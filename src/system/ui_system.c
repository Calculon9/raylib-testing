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
#include "system/ui_system.h"
#include "system/systems.h"
#include "system/utility_system.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
UIState G_UIState = (UIState){0};
// ------------------TOTAL SCREEN-------------------------
// Logical->pixel-space conversion properties

// Default UI Properties
Vector2d tbox_tlabel_default_offset = {0.06, 0};
Vector2d tbox_default_padding = {0.04, 0.04};
Vector2d tlabel_default_padding = {0.04, 0.04};
ColourRgba tbox_default_colour_border = COLOUR_PANEL_DARK_1; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
ColourRgba tbox_default_colour_fill = COLOUR_PANEL_LIGHT_3;  // COLOUR_PANEL_DARK_1;
Vector2d tfield_default_dims = {6, 0.5};
Vector2d tfield_default_padding = {0.08, 0.08};
ColourRgba tfield_default_colour_fill = COLOURLESS_RGBA;
Vector2d tcont_default_dims = {1, 0.45};
Vector2d tcont_default_padding = {0.15, 0.15};
Vector2d tcont_default_child_spacing = {0, 0.05};
ColourRgba tcont_default_colour_fill = COLOUR_PANEL_LIGHT_1;
ColourRgba tcont_default_colour_border = COLOUR_PANEL_DARK_2; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};

//  ----------LEFT PANEL SCREEN----------
//   Visual Properties
static ColourRgba lpanel_text_colour = COLOUR_PANEL_DARK_1;
static ColourRgba lpanel_fill_colour = COLOUR_PANEL_DARK_1;
static Vector2d lpanel_default_padding = {0.2, 0.2};
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
// UI Elements
UIBox seed_box = {0}; // This is the box that will be used as the parent box for the root element of the panel, and all other elements will calculate their positions and dimensions based on this box, which represents the entire panel area in pixel coordinates
UIElement *lpanel_root = {0};
UIElement *lpanel_properties_tcont = {0};
UIElement *lpanel_stats_tcont = {0};
Vector2d lpanel_properties_tcont_offset = {0, 5};
Vector2d lpanel_stats_tcont_offset = {0, 0};
// - default text container props
Vector2d lpanel_tcont_default_dims = {100, 40};
Vector2d lpanel_tcont_default_padding = {0.05, 0.05};

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
void InitPanelTextContainers();
void InitStatsContainer(void);
void InitObjectPropsContainer(void);
void DrawRootUIElement(UIElement *root_element, UIBox seed_box, Camera2d camera);
void UpdatePanelRegion(int mouse_x, int mouse_y, bool cursor_in_panel);
void UpdateGlobalUIState();

void InitUI(void)
{
    // G_UIState.lpanel_properties_id_str;       // = NULL;
    // G_UIState.lpanel_properties_pos_str;      // = NULL;
    // G_UIState.lpanel_properties_mass_str;     // = NULL;
    // G_UIState.lpanel_properties_vel_str;      // = NULL;
    // G_UIState.lpanel_properties_accel_str;    // = NULL;
    // G_UIState.lpanel_properties_moment_str;   // = NULL;
    // G_UIState.lpanel_stats_polygs_str.string; //[0] = '\0';
    // G_UIState.lpanel_stats_fps_str.string;    //[0] = '\0';
    // G_UIState.lpanel_stats_mem_str.string;    //[0] = '\0';
    InitPanelSpace();
    InitPanelTextContainers();
}
void InitPanelSpace(void)
{
    // 0. Define the properties of the panel space and camera based on the overall screen properties defined in Step 0, and then use those properties to initialise the panel's camera, coordinate space and UI elements in the subsequent steps

    // 1. INIT CAMERA using using the resolutions, sceen basis, origins etc. from Step 0
    Basis2d lpanel_basis = (Basis2d){lpanel_u, lpanel_v};
    Basis2d lpanel_pixel_basis = (Basis2d){lpanel_pixel_u, lpanel_pixel_v};
    camera_lpanel = CreateCamera2d(lpanel_pixel_basis, lpanel_basis, lpanel_pixel_origin, lpanel_origin, 1, 0);

    // 2. INIT a LOCAL COORD SPACE for the SIDE PANEL using the resolutions, origins etc. from Step 0
    // This will be the Root UI Element for the panel, and all other UI elements within the panel will be children of this Root element and will use this coordinate space for their positioning and dimensions
    lpanel_space = NewCoordSpace2d(lpanel_origin, lpanel_resolution, lpanel_basis);
    lpanel_root = CreateUIElement(UI_ELEMENT_ROOT, (Size){lpanel_space.resolution_ixj, SIZE_FIXED}, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, lpanel_default_padding, COLOURLESS_RGBA, lpanel_fill_colour);
    lpanel_root->data.root.coord_space = lpanel_space;

    Vector2d basis_scale = BasisTransform_2d_Scale(camera_lpanel.source_basis, camera_lpanel.destination_basis);
    seed_box.coords = (Vector2d){lpanel_pixel_origin.x * basis_scale.x, lpanel_pixel_origin.y * basis_scale.y};
    seed_box.dimensions = (Vector2d){lpanel_resolution.x * basis_scale.x, lpanel_resolution.y * basis_scale.y}; //

    // Init input buffers
}

void InitPanelTextContainers(void)
{
    InitObjectPropsContainer();
    InitStatsContainer();
    // 1. Create the Container (The Parent)
    // Note: We use an offset relative to lpanel_root, NOT an absolute origin.
}

void InitObjectPropsContainer(void)
{
    // 1. Create the Container (The Parent)
    // Note: We use an offset relative to lpanel_root, NOT an absolute origin.
    lpanel_properties_tcont = CreateTextFieldContainerInTree(
        (Size){tcont_default_dims, SIZE_PERCENT},
        lpanel_root,
        (Offset){lpanel_properties_tcont_offset, OFFSET_FIXED}, // Relative to lpanel_root
        tcont_default_padding,
        tcont_default_child_spacing,
        tcont_default_colour_border,
        tcont_default_colour_fill);
    lpanel_properties_tcont->is_draggable = true;

    char *tbox_labels[] = {"OBJECT PROPERTIES", "ID.", "MASS.", "POS.XY", "VEL.XY", "ACCEL.XY", "MOMEN.XY"};
    // String64 **state_map_str[] = {NULL, &G_UIState.lpanel_properties_id_str, &G_UIState.lpanel_properties_mass_str, &G_UIState.lpanel_properties_pos_str, &G_UIState.lpanel_properties_vel_str, &G_UIState.lpanel_properties_accel_str, &G_UIState.lpanel_properties_moment_str};
    UIElement **state_map_tbox[] = {NULL, &G_UIState.lpanel_properties_id_tbox, &G_UIState.lpanel_properties_mass_tbox, &G_UIState.lpanel_properties_pos_tbox, &G_UIState.lpanel_properties_vel_tbox, &G_UIState.lpanel_properties_accel_tbox, &G_UIState.lpanel_properties_moment_tbox};

    // Create Title (Label)
    UIElement *title = CreateUIElementInTree(UI_ELEMENT_LABEL,
                                             (Size){tfield_default_dims, SIZE_FIXED},
                                             lpanel_properties_tcont,
                                             (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                             tfield_default_padding,
                                             COLOURLESS_RGBA, COLOURLESS_RGBA);
    strncpy(title->data.label.text.string, tbox_labels[0], MAX_LABEL_CHARS - 1);
    title->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';
    title->data.label.font = FONT_BASIC;
    title->is_draggable = true;

    for (int i = 1; i < 7; i++)
    {
        // 2. Calculate the local offset for this TextField within the container
        // Formula: (Spacing + Height) * index
        float y_pos = lpanel_properties_tcont->child_spacing.y > 0 ? i * (lpanel_properties_tcont->child_spacing.y + tfield_default_dims.y) : i * lpanel_properties_tcont->child_spacing.y;
        float x_pos = lpanel_properties_tcont->child_spacing.x > 0 ? i * (lpanel_properties_tcont->child_spacing.x + tfield_default_dims.x) : i * lpanel_properties_tcont->child_spacing.x;

        Vector2d tfield_offset = (Vector2d){x_pos, y_pos};

        // 3. Create the TextField
        UIElement *tfield = CreateTextFieldInTree(
            (Size){tfield_default_dims, SIZE_FIXED},
            lpanel_properties_tcont,
            (Offset){tfield_offset, OFFSET_FIXED},
            tfield_default_padding,
            tbox_tlabel_default_offset,
            COLOURLESS_RGBA, COLOURLESS_RGBA);
        tfield->is_draggable = true;

        // 4. Access Children via the Tree
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
            printf("[DEBUG CONTEXT] Address: %p | Raw Value: %d\n", (void*)&i, i);
            if (i == 1){
                input_child->type = UI_ELEMENT_TEXTBOX_O;
                input_child->data.textbox.data_type = FLOAT;
            }
            if (i > 2){
                input_child->data.textbox.data_type = VECTOR2D;
            }
            else
            {
                input_child->data.textbox.data_type = FLOAT;
            }

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

void InitStatsContainer(void)
{
    // 1. Create the Container (The Parent)
    // Note: We use an offset relative to lpanel_root, NOT an absolute origin.
    lpanel_stats_tcont = CreateTextFieldContainerInTree(
        (Size){tcont_default_dims, SIZE_PERCENT},
        lpanel_root,
        (Offset){lpanel_stats_tcont_offset, OFFSET_FIXED}, // Relative to lpanel_root
        tcont_default_padding,
        tcont_default_child_spacing,
        tcont_default_colour_border,
        tcont_default_colour_fill);
    lpanel_stats_tcont->is_draggable = true;

    char *tbox_labels[] = {"STATISTICS", "POLYOIDS.", "FPS.", "MEM."};
    String64 **state_map_str[] = {NULL, &G_UIState.lpanel_stats_polygs_str, &G_UIState.lpanel_stats_fps_str, &G_UIState.lpanel_stats_mem_str};
    // UIElement **state_map_tbox[] = {NULL, &G_UIState.lpanel_properties_id_tbox, &G_UIState.lpanel_properties_mass_tbox, &G_UIState.lpanel_properties_pos_tbox, &G_UIState.lpanel_properties_vel_tbox, &G_UIState.lpanel_properties_accel_tbox, &G_UIState.lpanel_properties_moment_tbox};

    // Create Title (Label)
    UIElement *title = CreateUIElementInTree(UI_ELEMENT_LABEL,
                                             (Size){tfield_default_dims, SIZE_FIXED},
                                             lpanel_stats_tcont,
                                             (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                             tfield_default_padding,
                                             COLOURLESS_RGBA, COLOURLESS_RGBA);
    strncpy(title->data.label.text.string, tbox_labels[0], MAX_LABEL_CHARS - 1);
    title->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';
    title->data.label.font = FONT_BASIC;
    title->is_draggable = true;

    for (int i = 1; i < 4; i++)
    {
        // 2. Calculate the local offset for this TextField within the container
        // Formula: (Spacing + Height) * index
        float y_pos = lpanel_stats_tcont->child_spacing.y > 0 ? i * (lpanel_stats_tcont->child_spacing.y + tfield_default_dims.y) : i * lpanel_stats_tcont->child_spacing.y;
        float x_pos = lpanel_stats_tcont->child_spacing.x > 0 ? i * (lpanel_stats_tcont->child_spacing.x + tfield_default_dims.x) : i * lpanel_stats_tcont->child_spacing.x;

        Vector2d tfield_offset = (Vector2d){x_pos, y_pos};

        // 3. Create the TextField
        UIElement *tfield = CreateTextFieldInTree(
            (Size){tfield_default_dims, SIZE_FIXED},
            lpanel_stats_tcont,
            (Offset){tfield_offset, OFFSET_FIXED},
            tfield_default_padding,
            tbox_tlabel_default_offset,
            COLOURLESS_RGBA, COLOURLESS_RGBA);
        tfield->is_draggable = true;

        // 4. Access Children via the Tree
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

void UpdateUISystem(int mouse_x, int mouse_y)
{
    bool cursor_in_ui = mouse_x >= lpanel_pixel_origin.x && mouse_x <= (lpanel_pixel_origin.x + (lpanel_pixel_u.x * lpanel_resolution.x)) && mouse_y >= lpanel_pixel_origin.y && mouse_y <= (lpanel_pixel_origin.y + (lpanel_pixel_v.y * lpanel_resolution.y));

    // Send useful data to the Dispatcher for it triage and process/update affected elements
    ProcessUIInput(mouse_x, mouse_y, cursor_in_ui);

    UpdateGlobalUIState();
}

void DrawUI()
{
    // Draw the root element to kick it off
    DrawRootUIElement(lpanel_root, seed_box, camera_lpanel);
}

void UpdateGlobalUIState()
{
    // UPDATE STATISTICS
    float fps = frame_counter.fps;
    float bytes = GetCurrentMemoryAllocated();
    float mbytes = bytes / (1024.0f * 1024.0f); // Force floating point division
    int polygs = GetPolygonoidCount();

    if (frame_counter.total_frames % 30 == 0)
    {
        snprintf(G_UIState.lpanel_stats_fps_str->string, sizeof(String64), "%.1f", fps);
        snprintf(G_UIState.lpanel_stats_mem_str->string, sizeof(String64), "%.2f", mbytes);
        snprintf(G_UIState.lpanel_stats_polygs_str->string, sizeof(String64), "%d", polygs);
    }

    if (frame_counter.total_frames % 500 == 0)
    {
        printf("[Telemetry Update] FPS: %s | MEM: %s | POLY: %s\n",
               G_UIState.lpanel_stats_fps_str->string,
               G_UIState.lpanel_stats_mem_str->string,
               G_UIState.lpanel_stats_polygs_str->string);
    }

    // UPDATE SELECTED OBJECT PROPERTIES
    Polygonoid *obj = G_WorldState.selected_object;
    if (obj)
    {
        int id = obj->id;
        float mass = obj->newtonian_properties.mass;
        Vector2d pos = obj->newtonian_properties.coords_origin;
        Vector2d vel = obj->newtonian_properties.velocity;
        Vector2d acc = obj->newtonian_properties.acceleration;
        Vector2d mom = obj->newtonian_properties.momentum;

        // Bind selected_object data to the Object Properties TextBoxes
        G_UIState.lpanel_properties_id_tbox->data.textbox.data_bind = &obj->id;
        G_UIState.lpanel_properties_mass_tbox->data.textbox.data_bind = &obj->newtonian_properties.mass;
        G_UIState.lpanel_properties_pos_tbox->data.textbox.data_bind = &obj->newtonian_properties.coords_origin;
        G_UIState.lpanel_properties_vel_tbox->data.textbox.data_bind = &obj->newtonian_properties.velocity;
        G_UIState.lpanel_properties_accel_tbox->data.textbox.data_bind = &obj->newtonian_properties.acceleration;
        G_UIState.lpanel_properties_moment_tbox->data.textbox.data_bind = &obj->newtonian_properties.momentum;

        size_t str_64 = sizeof(String64);

        // PIPELINE data to text only when the element is NOT focused
        // so that editing of the text by the user doesn't keep getting overwritten with the value stored in the object
        if (!G_UIState.lpanel_properties_id_tbox->is_focused)
        {
            PipelineFloatToText(id, G_UIState.lpanel_properties_id_tbox->data.textbox.text.string, str_64);
        }
        if (!G_UIState.lpanel_properties_mass_tbox->is_focused)
        {
            PipelineFloatToText(mass, G_UIState.lpanel_properties_mass_tbox->data.textbox.text.string, str_64);
        }
        if (!G_UIState.lpanel_properties_pos_tbox->is_focused)
        {
            PipelineVectorToText(pos, G_UIState.lpanel_properties_pos_tbox->data.textbox.text.string, str_64);
        }
        if (!G_UIState.lpanel_properties_vel_tbox->is_focused)
        {
            PipelineVectorToText(vel, G_UIState.lpanel_properties_vel_tbox->data.textbox.text.string, str_64);
        }
        if (!G_UIState.lpanel_properties_accel_tbox->is_focused)
        {
            PipelineVectorToText(acc, G_UIState.lpanel_properties_accel_tbox->data.textbox.text.string, str_64);
        }
        if (!G_UIState.lpanel_properties_moment_tbox->is_focused)
        {
            PipelineVectorToText(mom, G_UIState.lpanel_properties_moment_tbox->data.textbox.text.string, str_64);
        }
    }
    else // Reset their output buffers AND unbind
    {
        G_UIState.lpanel_properties_id_tbox->data.textbox.text.string[0] = '\0';
        G_UIState.lpanel_properties_mass_tbox->data.textbox.text.string[0] = '\0';
        G_UIState.lpanel_properties_pos_tbox->data.textbox.text.string[0] = '\0';
        G_UIState.lpanel_properties_vel_tbox->data.textbox.text.string[0] = '\0';
        G_UIState.lpanel_properties_accel_tbox->data.textbox.text.string[0] = '\0';
        G_UIState.lpanel_properties_moment_tbox->data.textbox.text.string[0] = '\0';
        G_UIState.lpanel_properties_id_tbox->data.textbox.data_bind = NULL;
        G_UIState.lpanel_properties_mass_tbox->data.textbox.data_bind = NULL;
        G_UIState.lpanel_properties_pos_tbox->data.textbox.data_bind = NULL;
        G_UIState.lpanel_properties_vel_tbox->data.textbox.data_bind = NULL;
        G_UIState.lpanel_properties_accel_tbox->data.textbox.data_bind = NULL;
        G_UIState.lpanel_properties_moment_tbox->data.textbox.data_bind = NULL;
    }
}

// Gameplay Screen Stage Update logic
void UpdatePanelRegion(int mouse_x, int mouse_y, bool cursor_in_region)
{
    // Vector2d mouse_pixel_coords = {(float)mouse_x, (float)mouse_y};
    // // Check if something was clicked on the panel
    // // 1. Initial Guard: Only process if the user is actually clicking in the panel
    // if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || !cursor_in_region)
    // {
    //     UpdateTextBoxInput(); // Still run input update for previously focused boxes
    //     return;
    // }

    // // 2. Get the target
    // UIElement *clicked_element = GetElementAt(lpanel_root, mouse_pixel_coords);
    // Vector2d clicked_coords = TransformCoordinates(camera_lpanel.dest_to_source_mtx, mouse_pixel_coords);
    // // int cell_index = ((int)click_panel_coords.y * (int)lpanel_resolution.x) + (int)click_panel_coords.x;

    // // 3. Handle Empty Clicks (Clicking nothing or the background)
    // if (G_FocusedElement && (G_FocusedElement->type == UI_ELEMENT_TEXTBOX || G_FocusedElement->type == UI_ELEMENT_TEXTBOX_SAFE))
    // {
    //     // If we clicked something that is NOT the currently focused box
    //     if (clicked_element != G_FocusedElement)
    //     {
    //         // DISCARD: Restore the old text from the backup buffer
    //         strncpy(G_FocusedElement->data.textbox.text.string, tbox_temp_buffer, MAX_TEXTBOX_CHARS - 1);

    //         // Clean up focus
    //         ClearUIFocus();
    //         tbox_temp_buffer[0] = '\0'; // Clear the backup
    //     }
    // }
    // if (clicked_element == NULL)
    // {
    //     ClearUIFocus();
    //     return;
    // }

    // // 4. Handle Interaction Types (The "Positive" Logic)
    // bool is_text_type = (clicked_element->type == UI_ELEMENT_TEXTBOX || clicked_element->type == UI_ELEMENT_TEXTFIELD || clicked_element->type == UI_ELEMENT_TEXTBOX_SAFE);
    // if (is_text_type)
    // {
    //     HandleTextBoxClick(clicked_element);
    // }
    // else // If they clicked a non-text element (like a label or a frame), clear focus
    // {
    //     ClearUIFocus();
    // }

    // printf("CLICKED [%s] (%.0f, %.0f) (%.0f, %.0f)\n", GetElementTypeName(clicked_element->type), clicked_coords.x, clicked_coords.y, clicked_element->cached_box.coords.x, clicked_element->cached_box.coords.y);
    // UpdateTextBoxInput();

    // return;

    // Check if there are any objects in that cell and print info about those objects if so
    // Cell *cells = lpanel_root->data.root.coord_space.cells.coll.items;
    // Cell cell = cells[cell_index];
    // selectedCell = &cells[cell_index];
    // offset += snprintf(log + offset, sizeof(log) - offset, "Region: Panel (%.1f, %.1f) --> Cell %d (%.1f, %.1f) --> ", lpanel_pixel_origin.x, lpanel_pixel_origin.y, cell_index, cell.coords.x, cell.coords.y);

    // Loop through all properties text boxes and apply focus if the clicked_coords are within a text_box, otherwise set focus --> false
    // UIElement **tfields = (UIElement **)lpanel_properties_tcont->children.items; //.coll;
    // UIElement *e = NULL;
    // int tfield_count = lpanel_properties_tcont->children.count;
    // Vector2d vertices[4] = {0};
    // bool any_focus = false;
    // if (lpanel_properties_tcont->children.count > 0)
    // {
    //     for (int i = 0; i < tfield_count; i++)
    //     {
    //         // Check if the click is within the text box
    //         e = (UIElement *)(tfields[i]);
    //         //GetUIElementVertices(t, vertices); // ISSUE HERE
    //         //if (IsFocused(click_panel_coords, vertices, 4))
    //         if(IsMouseOverElement(e, (Vector2d){mouse_x, mouse_y}))
    //         {
    //             // Apply focus on text box
    //             HandleTextBoxClick(e);
    //             any_focus = true;
    //             offset += snprintf(log + offset, sizeof(log) - offset, "Element: TextBox (%.1f, %.1f)", click_panel_coords.x, click_panel_coords.y);
    //             break;
    //             // printf("Clicked on object properties text box! Text box properties - Position: (%.1f, %.1f), Dimensions: (%.1f, %.1f)\n", lpanel_properties_tbox.origin.x, lpanel_properties_tbox.origin.y, lpanel_properties_tbox.width, lpanel_properties_tbox.height);
    //         }
    //     }
    // }
    // If nothing in Properties was clicked, loop through all stats text boxes and apply focus if the clicked_coords are within a text_box, otherwise set focus --> false
    // if (!any_focus && lpanel_stats_tcont->text_fields.coll.count > 0)
    // {
    //     tbox_coll = (Collection)lpanel_stats_tcont->text_fields.coll;

    //     t = &((TextField *)(tbox_coll.items))[0];
    //     vertices = GetTextFieldVertices(*t);
    //     bool any_focus = false;
    //     for (int i = 0; i < tbox_coll.count; i++)
    //     {
    //         // Check if the click is within the text box
    //         t = &((TextField *)(tbox_coll.items))[i];
    //         vertices = GetTextFieldVertices(*t);
    //         if (IsFocused(click_panel_coords, vertices, 4))
    //         {
    //             // Apply focus on text box
    //             HandleTextFieldClick(t);
    //             // t->is_focused = true;
    //             any_focus = true;
    //             offset += snprintf(log + offset, sizeof(log) - offset, "Element: TextBox (%.1f, %.1f)", click_panel_coords.x, click_panel_coords.y);
    //             break;
    //             // printf("Clicked on object properties text box! Text box properties - Position: (%.1f, %.1f), Dimensions: (%.1f, %.1f)\n", lpanel_properties_tbox.origin.x, lpanel_properties_tbox.origin.y, lpanel_properties_tbox.width, lpanel_properties_tbox.height);
    //         }
    //     }
    // }
    // if (!any_focus)
    // {
    //     offset += snprintf(log + offset, sizeof(log) - offset, "Element: Nill");
    // }

    // printf("CLICKED (%d, %d) { %s }\n", mouse_x, mouse_y, log);

    // finishScreen = 1;
    //  PlaySound(fxCoin);
}

// void DrawUIElement(UIElement *e, UIBox parent_box, Camera2d camera)
// {
//     if (!e)
//     {
//         return;
//     }

//     // Need to convert world coordinates to screen coordinates
//     Basis2d basis = camera.destination_basis;

//     // The world position of the coordinate space object is the origin of the coordinate space, so (0,0).
//     // But to make it more flexible for different coordinate space origins, we will add the world position to the start and end points of the lines to get their actual coordinates in world space, and then convert those to screen coordinates using the basis transform matrix
//     Vector2d basis_scale = BasisTransform_2d_Scale(camera.source_basis, camera.destination_basis); // Need to scale dimensions from world units to pixel units using the camera's basis transform

//     // Resolve rendered position and dimensions of panel element
//     UIBox box = ResolveElementBox(e, parent_box, basis_scale);
//     box.coords = (Vector2d){(int)box.coords.x, (int)box.coords.y};
//     box.dimensions = (Vector2d){(int)box.dimensions.x, (int)box.dimensions.y};
//     e->cached_box = box;

//     // Draw background & border
//     DrawElementBox(e);
//     if (e->type == UI_ELEMENT_LABEL)
//     {
//         // Draw the Text
//         DrawTextCustom(e->data.label.text.string, e->cached_box.coords, 2, FONT_DEFAULT, FONT_DEFAULT.colour);
//         // DrawTextCustom(e->data.label.text.string, e->cached_box.coords, 2, FONT_DEFAULT, FONT_DEFAULT.colour);
//         // DrawTextCustom(e->data.label.text.string, e->cached_box.coords, 2, e->data.label.font, e->data.label.font.colour);
//         //  DrawText(text_box->text, text_x, text_y, font_size, (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a});
//     }
//     if (e->type == UI_ELEMENT_TEXTBOX || e->type == UI_ELEMENT_TEXTBOX_SAFE)
//     {
//         DrawTextBoxText(e);
//     }

//     frame_counter.total_frames % 800 == 0 ? printf("Drew [%s] pos: (%.1f, %.1f) | Size: (%.1f, %.1f)\n", GetElementTypeName(e->type), box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y) : (void)0;

//     // Recursively draw children elements
//     UIElement *child = e->first_child;
//     while (child)
//     {
//         DrawUIElement(child, box, camera);
//         child = child->next_sibling;
//     }
// }

// void DrawPanelRegion_Stats(CoordSpace2d panel_space, Color fill_colour)
// {
//     Vector2 pos = {20, 100};
//     Vector2 lineSpacing = {0, 40};

//     // Count display
//     char text[32];
//     // snprintf(text, sizeof(text), "Polygonoids: %d", GetPolygonoidCount());                                                                                 // Format the FPS value into the buffer
//     // DrawTextEx(font_default, text, pos, font_default.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a}); // Buffer to hold the text
//     //  snprintf(text, sizeof(text), "Circloids: %d", GetCircloidCount()); // Format the FPS value into the buffer
//     //  DrawTextEx(font, text, pos, font.baseSize * 2.0f, 2, (Color)BEIGE);

//     // FPS display
//     snprintf(text, sizeof(text), "FPS: %.1f", frame_counter.fps); // Format the FPS value into the buffer
//     DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + lineSpacing.y}, font.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a});

//     // Memory display - Total allocated memory in bytes
//     snprintf(text, sizeof(text), "Memory (bytes): %zu", GetCurrentMemoryAllocated()); // Format the FPS value into the buffer
//     DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + 2 * lineSpacing.y}, font.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a});
// }

// void HandleTextBoxClick(UIElement *clicked)
// {
//     if (!clicked)
//         return;

//     // 1. If we clicked the same element that already has focus, do nothing
//     if (G_FocusedElement == clicked)
//         return;

//     // 2. Clear focus from the previous element (if any)
//     if (G_FocusedElement != NULL)
//     {
//         G_FocusedElement->is_focused = false;
//         // Optional: Trigger an "OnBlur" event here if needed
//     }

//     // Assign focus to the new element
//     G_FocusedElement = clicked;
//     clicked->is_focused = true;

//     // Move the cursor to the end of the text string
//     // This allows the user to start typing immediately after what's already there
//     if (clicked->type == UI_ELEMENT_TEXTBOX || clicked->type == UI_ELEMENT_TEXTBOX_SAFE)
//     {
//         int length = strlen(clicked->data.textbox.text.string);
//         clicked->data.textbox.cursor_position = length;
//     }

//     printf("Focus shifted to: %s\n", GetElementTypeName(clicked->type));
// }

// void DrawElementBox(UIElement *e)
// {
//     UIBox box = e->cached_box;
//     ColourRgba colour_fill = e->colour_fill;
//     ColourRgba colour_border = e->colour_border;
//     DrawRectangle((int)box.coords.x, (int)box.coords.y, (int)box.dimensions.x, (int)box.dimensions.y, (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a});
//     DrawRectangleLines((int)box.coords.x, (int)box.coords.y, (int)(box.dimensions.x), (int)(box.dimensions.y), (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a});
// }
// void DrawTextBoxText(UIElement *e)
// {
//     // Draw the Text
//     DrawTextCustom(e->data.textbox.text.string, e->cached_box.coords, 2, FONT_DEFAULT, FONT_DEFAULT.colour);
//     if (e->is_focused)
//     {
//         int text_len = GetTextWidth(e->data.textbox.text.string, (char)e->data.textbox.font.spacing, 2);
//         Vector2d cursor_pos = {e->cached_box.coords.x + text_len + 2, e->cached_box.coords.y};
//         frame_counter.total_frames % 60 < 30 ? DrawTextCustom(".", cursor_pos, 2, FONT_DEFAULT, FONT_DEFAULT.colour) : (void)0;
//     }
// }