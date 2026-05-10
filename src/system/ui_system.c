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
//
UIElement *G_FocusedElement = NULL;
char tbox_input_buffer[sizeof(G_FocusedElement->data.textbox.text.string)] = "";
char tbox_temp_buffer[sizeof(G_FocusedElement->data.textbox.text.string)] = "";

// ------------------TOTAL SCREEN-------------------------
// Logical->pixel-space conversion properties
// Default font
static int font_scale_l = 3;
static int font_scale_m = 2;
static int font_scale_s = 1;
// Default UI Properties
static Vector2d tbox_tlabel_default_offset = {0.06, 0};
static Vector2d tbox_default_padding = {0.04, 0.04};
static ColourRgba tbox_default_colour_border = COLOUR_PANEL_DARK_1; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
static ColourRgba tbox_default_colour_fill = COLOUR_PANEL_LIGHT_3;  // COLOUR_PANEL_DARK_1;
static Vector2d tfield_default_dims = {3.6, 0.4};
static Vector2d tfield_default_padding = {0.08, 0.08};
static ColourRgba tfield_default_colour_fill = COLOURLESS_RGBA;
static Vector2d tcont_default_padding = {0.15, 0.15};
static Vector2d tcont_default_child_spacing = {0, 0.05};
static ColourRgba tcont_default_colour_fill = COLOUR_PANEL_LIGHT_1;
static ColourRgba tcont_default_colour_border = COLOUR_PANEL_DARK_2; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};

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
Vector2d lpanel_pixel_u = {75, 0};
Vector2d lpanel_pixel_v = {0, 75};
// UI Elements
static UIBox seed_box = {0}; // This is the box that will be used as the parent box for the root element of the panel, and all other elements will calculate their positions and dimensions based on this box, which represents the entire panel area in pixel coordinates
static UIElement *lpanel_root = {0};
static UIElement *lpanel_properties_tcont = {0};
static UIElement *lpanel_stats_tcont = {0};
static Vector2d lpanel_properties_tcont_offset = {0, 5};
static Vector2d lpanel_stats_tcont_offset = {0};
// - default text container props
static Vector2d lpanel_tcont_default_dims = {3, 5};
static Vector2d lpanel_tcont_default_padding = {0.05, 0.05};

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
void InitPanelTextContainers();
// void DrawCircloids();
// void DrawPanelRegion(UIElement *lpanel_element, Camera2d camera);
// void DrawPanelRegion_ObjectProps(CoordSpace2d lpanel_space, Color fill_colour);
void DrawPanelRegion_Stats(CoordSpace2d lpanel_spacelpanel_space, Color fill_colour);
void DrawTextFieldChildren(UIElement *text_field, Vector2d parent_pixel_coords, Bitmap_Font font, int font_scale, Camera2d camera);
// void DrawTextFieldsContainer(UIElement *text_fields_container, Camera2d camera);
void DrawRootUIElement(UIElement *root_element, UIBox seed_box, Camera2d camera);
void DrawUIElement(UIElement *element, UIBox parent_box, Camera2d camera);
void UpdatePanelRegion(int mouse_x, int mouse_y, bool cursor_in_panel);
void UpdateTextBoxInput();
void HandleTextBoxClick(UIElement *clicked);
void DrawElementBox(UIElement *e);
void DrawTextBoxText(UIElement *e);
void ClearUIFocus(void);

void InitUI(void)
{
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
    // 1. Create the Container (The Parent)
    // Note: We use an offset relative to lpanel_root, NOT an absolute origin.
    lpanel_properties_tcont = CreateTextFieldContainerInTree(
        (Size){lpanel_tcont_default_dims, SIZE_FIXED},
        lpanel_root,
        (Offset){lpanel_properties_tcont_offset, OFFSET_FIXED}, // Relative to lpanel_root
        tcont_default_padding,
        tcont_default_child_spacing,
        tcont_default_colour_border,
        tcont_default_colour_fill);

    char *tbox_labels[] = {"OBJECT PROPERTIES", "MASS.", "POS.(X,Y)", "VEL.(X,Y)", "ACCEL.(X,Y)"};

    // Create Title (Label)
    UIElement *title = CreateUIElementInTree(UI_ELEMENT_LABEL,
                                             (Size){tfield_default_dims, SIZE_FIXED},
                                             lpanel_properties_tcont,
                                             (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                             tfield_default_padding,
                                             COLOURLESS_RGBA, COLOURLESS_RGBA);
    strncpy(title->data.label.text.string, tbox_labels[0], MAX_LABEL_CHARS - 1);
    title->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';
    title->data.label.font = FONT_DEFAULT;

    for (int i = 1; i < 5; i++)
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

        // 4. Access Children via the Tree
        UIElement *label_child = tfield->first_child;
        UIElement *input_child = (label_child) ? label_child->next_sibling : NULL;

        if (label_child && input_child)
        {
            // Configure Label
            label_child->padding = tbox_default_padding;
            label_child->colour_border = tbox_default_colour_border;
            label_child->colour_fill = tbox_default_colour_fill;
            label_child->data.label.font = FONT_DEFAULT;

            strncpy(label_child->data.label.text.string, tbox_labels[i], MAX_LABEL_CHARS - 1);
            label_child->data.label.text.string[MAX_LABEL_CHARS - 1] = '\0';

            // Configure TextBox
            input_child->padding = tbox_default_padding;
            input_child->colour_border = tbox_default_colour_border;
            input_child->colour_fill = tbox_default_colour_fill;
            input_child->data.textbox.font = FONT_DEFAULT;
            input_child->type = UI_ELEMENT_TEXTBOX_SAFE; // Will only save over previous text if ENTER is pressed
        }
    }
}

void UpdateUISystem(int mouse_x, int mouse_y)
{
    bool cursor_in_panel = mouse_x >= lpanel_pixel_origin.x && mouse_x <= (lpanel_pixel_origin.x + (lpanel_pixel_u.x * lpanel_resolution.x)) && mouse_y >= lpanel_pixel_origin.y && mouse_y <= (lpanel_pixel_origin.y + (lpanel_pixel_v.y * lpanel_resolution.y));
    UpdatePanelRegion(mouse_x, mouse_y, cursor_in_panel);
}

// Gameplay Screen Stage Update logic
void UpdatePanelRegion(int mouse_x, int mouse_y, bool cursor_in_region)
{
    Vector2d mouse_pixel_coords = {(float)mouse_x, (float)mouse_y};
    // Check if something was clicked on the panel
    // 1. Initial Guard: Only process if the user is actually clicking in the panel
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || !cursor_in_region)
    {
        UpdateTextBoxInput(); // Still run input update for previously focused boxes
        return;
    }

    // 2. Get the target
    UIElement *clicked_element = GetElementAt(lpanel_root, mouse_pixel_coords);
    Vector2d clicked_coords = TransformCoordinates(camera_lpanel.dest_to_source_mtx, mouse_pixel_coords);
    // int cell_index = ((int)click_panel_coords.y * (int)lpanel_resolution.x) + (int)click_panel_coords.x;

    // 3. Handle Empty Clicks (Clicking nothing or the background)
    if (G_FocusedElement && (G_FocusedElement->type == UI_ELEMENT_TEXTBOX || G_FocusedElement->type == UI_ELEMENT_TEXTBOX_SAFE))
    {
        // If we clicked something that is NOT the currently focused box
        if (clicked_element != G_FocusedElement)
        {
            // DISCARD: Restore the old text from the backup buffer
            strncpy(G_FocusedElement->data.textbox.text.string, tbox_temp_buffer, MAX_TEXTBOX_CHARS - 1);

            // Clean up focus
            ClearUIFocus();
            tbox_temp_buffer[0] = '\0'; // Clear the backup
        }
    }
    if (clicked_element == NULL)
    {
        ClearUIFocus();
        return;
    }

    // 4. Handle Interaction Types (The "Positive" Logic)
    bool is_text_type = (clicked_element->type == UI_ELEMENT_TEXTBOX || clicked_element->type == UI_ELEMENT_TEXTFIELD || clicked_element->type == UI_ELEMENT_TEXTBOX_SAFE);
    if (is_text_type)
    {
        HandleTextBoxClick(clicked_element);
    }
    else // If they clicked a non-text element (like a label or a frame), clear focus
    {
        ClearUIFocus();
    }

    printf("CLICKED [%s] (%.0f, %.0f) (%.0f, %.0f)\n", GetElementTypeName(clicked_element->type), clicked_coords.x, clicked_coords.y, clicked_element->cached_box.coords.x, clicked_element->cached_box.coords.y);
    UpdateTextBoxInput();

    return;

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

// Draws Left Panel
// void DrawPanelRegion(UIElement *element, Camera2d camera)
// {
//     // Need to convert world coordinates to screen coordinates
//     CoordSpace2d panel_space = element->data.root.coord_space;
//     Basis2d basis = panel_space.basis;

//     // The world position of the coordinate space object is the origin of the coordinate space, so (0,0).
//     // But to make it more flexible for different coordinate space origins, we will add the world position to the start and end points of the lines to get their actual coordinates in world space, and then convert those to screen coordinates using the basis transform matrix
//     Vector2d origin = panel_space.coords_origin;
//     Vector2d end = VectorSum_2d(origin, panel_space.resolution_ixj);
//     Vector2d basis_scale = BasisTransform_2d_Scale(camera.source_basis, camera.destination_basis); // Need to scale dimensions from world units to pixel units using the camera's basis transform

//     // Resolve rendered position and dimensions of panel element
//     UIBox panel_box = ResolveElementBox(element, ZERO_VECTOR_2D, basis_scale);

//     // First: Draw background & border
//     ColourRgba colour_fill = element->colour_fill;
//     ColourRgba colour_border = element->colour_border;
//     DrawRectangle(panel_box.coords.x, panel_box.coords.y, panel_box.dimensions.x, panel_box.dimensions.y, (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a});
//     DrawRectangleLines(panel_box.coords.x, panel_box.coords.y, panel_box.dimensions.x, panel_box.dimensions.y, (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a});

//     // Draw the text boxes in the Properties container
//     DrawTextFieldsContainer(lpanel_properties_tcont, camera_lpanel);

//     // Draw the text boxes in the Stats container
//     // DrawTextFieldsContainer(&lpanel_stats_tcont, camera_lpanel);

//     // Memory display - Consumed memory in bytes out of the total allocated bytes
//     // snprintf(text, sizeof(text), "Memory Consumed (bytes): %zu", GetCurrentMemoryAllocated()); // Format the FPS value into the buffer
//     // DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + 2 * lineSpacing.y}, font.baseSize * 2.0f, 2, (Color)BEIGE);
// }
void ClearUIFocus()
{
    if (G_FocusedElement)
    {
        G_FocusedElement->is_focused = false;
        G_FocusedElement = NULL;
    }
}
void UpdateTextBoxInput()
{
    if (!G_FocusedElement || (G_FocusedElement->type != UI_ELEMENT_TEXTBOX && G_FocusedElement->type != UI_ELEMENT_TEXTBOX_SAFE))
        return;

    char *output_buf = G_FocusedElement->data.textbox.text.string;

    // 1. "Snapshot" for Undo/Cancel
    // If this is the very first time we are typing, save the original state
    if (strlen(tbox_input_buffer) == 0 && strlen(tbox_temp_buffer) == 0)
    {
        strncpy(tbox_temp_buffer, output_buf, MAX_TEXTBOX_CHARS - 1);
        tbox_temp_buffer[MAX_TEXTBOX_CHARS - 1] = '\0';
    }

    // 2. Handle Character Input
    int key = GetCharPressed();
    while (key > 0)
    {
        int current_len = strlen(output_buf);

        if (current_len < MAX_TEXTBOX_CHARS - 1 && (key >= 32 && key <= 125))
        {
            output_buf[current_len] = (char)key;
            output_buf[current_len + 1] = '\0';

            // Mirror to your tracking buffer so we know we've started "editing"
            tbox_input_buffer[0] = ' '; // Just a flag to say "not empty"
        }
        key = GetCharPressed();
    }

    // 3. Handle Backspace
    if (IsKeyPressed(KEY_BACKSPACE))
    {
        int len = strlen(output_buf);
        if (len > 0)
            output_buf[len - 1] = '\0';
    }

    // 4. Handle Escape (Cancel/Undo)
    if (IsKeyPressed(KEY_ESCAPE))
    {
        // Restore from backup
        strncpy(output_buf, tbox_temp_buffer, MAX_TEXTBOX_CHARS - 1);
        // Reset tracking buffers
        tbox_input_buffer[0] = '\0';
        tbox_temp_buffer[0] = '\0';
        G_FocusedElement->is_focused = false;
        G_FocusedElement = NULL;
    }

    // 5. Handle Enter (Commit)
    if (IsKeyPressed(KEY_ENTER))
    {
        tbox_input_buffer[0] = '\0';
        tbox_temp_buffer[0] = '\0';
        G_FocusedElement->is_focused = false;
        G_FocusedElement = NULL;
        // Trigger physics update here! (e.g., UpdateObjectMass())
    }
}

void DrawUI()
{
    // Draw the root element to kick it off
    DrawRootUIElement(lpanel_root, seed_box, camera_lpanel);
}

// NEED TO CREATE BOX FOR ENTIRE SCREEN AND PASS THAT AS THE PARENT BOX FOR THE ROOT ELEMENT, SO THAT THE ROOT ELEMENT AND ALL OTHER ELEMENTS CAN CALCULATE THEIR POSITIONS AND DIMENSIONS BASED ON THAT, RATHER THAN HAVING THE ROOT ELEMENT CALCULATE ITS POSITION AND DIMENSIONS BASED ON A ZERO VECTOR, WHICH IS NOT FLEXIBLE IF WE WANT TO CHANGE THE PANEL OR ROOT ELEMENT PROPERTIES LATER
void DrawRootUIElement(UIElement *root_element, UIBox seed_box, Camera2d camera)
{
    if (!root_element)
    {
        return;
    }

    // Need to convert world coordinates to screen coordinates
    CoordSpace2d panel_space = root_element->data.root.coord_space;
    Basis2d basis = panel_space.basis;

    Vector2d origin = panel_space.coords_origin;
    Vector2d basis_scale = BasisTransform_2d_Scale(camera.source_basis, camera.destination_basis); // Need to scale dimensions from world units to pixel units using the camera's basis transform

    // Resolve rendered position and dimensions of panel element
    UIBox box = ResolveElementBox(root_element, seed_box, basis_scale);
    root_element->cached_box = box;

    // Draw background & border
    DrawElementBox(root_element);
    frame_counter.total_frames % 800 == 0 ? printf("Drew [%s] pos: (%.1f, %.1f) | Size: (%.1f, %.1f)\n", GetElementTypeName(root_element->type), box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y) : (void)0;

    // Recursively draw children
    UIElement *child = root_element->first_child;
    while (child)
    {
        DrawUIElement(child, box, camera);
        child = child->next_sibling;
    }
}

void DrawUIElement(UIElement *e, UIBox parent_box, Camera2d camera)
{
    if (!e)
    {
        return;
    }

    // Need to convert world coordinates to screen coordinates
    Basis2d basis = camera.destination_basis;

    // The world position of the coordinate space object is the origin of the coordinate space, so (0,0).
    // But to make it more flexible for different coordinate space origins, we will add the world position to the start and end points of the lines to get their actual coordinates in world space, and then convert those to screen coordinates using the basis transform matrix
    Vector2d basis_scale = BasisTransform_2d_Scale(camera.source_basis, camera.destination_basis); // Need to scale dimensions from world units to pixel units using the camera's basis transform

    // Resolve rendered position and dimensions of panel element
    UIBox box = ResolveElementBox(e, parent_box, basis_scale);
    box.coords = (Vector2d){(int)box.coords.x, (int)box.coords.y};
    box.dimensions = (Vector2d){(int)box.dimensions.x, (int)box.dimensions.y};
    e->cached_box = box;

    // Draw background & border
    DrawElementBox(e);
    if (e->type == UI_ELEMENT_LABEL)
    {
        // Draw the Text
        DrawTextCustom(e->data.label.text.string, e->cached_box.coords, 2, FONT_DEFAULT, FONT_DEFAULT.colour);
        // DrawTextCustom(e->data.label.text.string, e->cached_box.coords, 2, FONT_DEFAULT, FONT_DEFAULT.colour);
        // DrawTextCustom(e->data.label.text.string, e->cached_box.coords, 2, e->data.label.font, e->data.label.font.colour);
        //  DrawText(text_box->text, text_x, text_y, font_size, (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a});
    }
    if (e->type == UI_ELEMENT_TEXTBOX || e->type == UI_ELEMENT_TEXTBOX_SAFE)
    {
        DrawTextBoxText(e);
    }

    frame_counter.total_frames % 800 == 0 ? printf("Drew [%s] pos: (%.1f, %.1f) | Size: (%.1f, %.1f)\n", GetElementTypeName(e->type), box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y) : (void)0;

    // Recursively draw children elements
    UIElement *child = e->first_child;
    while (child)
    {
        DrawUIElement(child, box, camera);
        child = child->next_sibling;
    }
}

void DrawPanelRegion_Stats(CoordSpace2d panel_space, Color fill_colour)
{
    Vector2 pos = {20, 100};
    Vector2 lineSpacing = {0, 40};

    // Count display
    char text[32];
    // snprintf(text, sizeof(text), "Polygonoids: %d", GetPolygonoidCount());                                                                                 // Format the FPS value into the buffer
    // DrawTextEx(font_default, text, pos, font_default.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a}); // Buffer to hold the text
    //  snprintf(text, sizeof(text), "Circloids: %d", GetCircloidCount()); // Format the FPS value into the buffer
    //  DrawTextEx(font, text, pos, font.baseSize * 2.0f, 2, (Color)BEIGE);

    // FPS display
    snprintf(text, sizeof(text), "FPS: %.1f", frame_counter.fps); // Format the FPS value into the buffer
    DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + lineSpacing.y}, font.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a});

    // Memory display - Total allocated memory in bytes
    snprintf(text, sizeof(text), "Memory (bytes): %zu", GetCurrentMemoryAllocated()); // Format the FPS value into the buffer
    DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + 2 * lineSpacing.y}, font.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a});
}

void HandleTextBoxClick(UIElement *clicked)
{
    if (!clicked)
        return;

    // 1. If we clicked the same element that already has focus, do nothing
    if (G_FocusedElement == clicked)
        return;

    // 2. Clear focus from the previous element (if any)
    if (G_FocusedElement != NULL)
    {
        G_FocusedElement->is_focused = false;
        // Optional: Trigger an "OnBlur" event here if needed
    }

    // Assign focus to the new element
    G_FocusedElement = clicked;
    clicked->is_focused = true;

    // Move the cursor to the end of the text string
    // This allows the user to start typing immediately after what's already there
    if (clicked->type == UI_ELEMENT_TEXTBOX || clicked->type == UI_ELEMENT_TEXTBOX_SAFE)
    {
        int length = strlen(clicked->data.textbox.text.string);
        clicked->data.textbox.cursor_position = length;
    }

    printf("Focus shifted to: %s\n", GetElementTypeName(clicked->type));
}

void DrawElementBox(UIElement *e)
{
    UIBox box = e->cached_box;
    ColourRgba colour_fill = e->colour_fill;
    ColourRgba colour_border = e->colour_border;
    DrawRectangle((int)box.coords.x, (int)box.coords.y, (int)box.dimensions.x, (int)box.dimensions.y, (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a});
    DrawRectangleLines((int)box.coords.x, (int)box.coords.y, (int)(box.dimensions.x), (int)(box.dimensions.y), (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a});
}
void DrawTextBoxText(UIElement *e)
{
    // Draw the Text
    DrawTextCustom(e->data.textbox.text.string, e->cached_box.coords, 2, FONT_DEFAULT, FONT_DEFAULT.colour);
    if (e->is_focused)
    {
        int text_len = GetTextWidth(e->data.textbox.text.string, (char)e->data.textbox.font.spacing, 2);
        Vector2d cursor_pos = {e->cached_box.coords.x + text_len + 2, e->cached_box.coords.y};
        frame_counter.total_frames % 60 < 30 ? DrawTextCustom(".", cursor_pos, 2, FONT_DEFAULT, FONT_DEFAULT.colour) : (void)0;
    }
}
// void DrawTextFieldChildren(UIElement *text_field, Vector2d parent_pixel_coords, Bitmap_Font font, int font_scale, Camera2d camera)
// {
//     UIElement **children = (UIElement **)text_field->children.items;
//     UIElement *text_label = children[0]; // Assuming the TextLabel is the second child of the TextField
//     UIElement *text_box = children[1];   // Assuming the TextLabel is the second child of the TextField
//     Color color_border = (Color){text_box->colour_border.r, text_box->colour_border.g, text_box->colour_border.b, text_box->colour_border.a};
//     Color color_fill = (Color){text_box->colour_fill.r, text_box->colour_fill.g, text_box->colour_fill.b, text_box->colour_fill.a};

//     // Need to scale dimensions from world units to pixel units using the camera's basis transform
//     Vector2d basis_scale = BasisTransform_2d_Scale(camera.source_basis, camera.destination_basis);

//     // --- DRAW TEXT BOX ---
//     // Raw Properties (before applying padding)
//     // These are the raw dimensions of the text box in pixel space before padding is applied, so the actual area available for text will be smaller once we apply padding
//     Vector2d tbox_pixel_coords = {(text_box->parent_offset.x * basis_scale.x) + parent_pixel_coords.x, (text_box->parent_offset.y * basis_scale.y) + parent_pixel_coords.y};
//     float tbox_w = text_box->width * basis_scale.x;  // Assuming width is defined in world units and needs to be scaled to pixel units
//     float tbox_h = text_box->height * basis_scale.y; // Assuming height is defined in world units and needs to be scaled to pixel units

//     // Text Box - raw
//     DrawRectangleLines(tbox_pixel_coords.x, tbox_pixel_coords.y, tbox_w, tbox_h, color_border);

//     // Inner Properties (after applying padding).
//     Vector2d padding = {text_box->padding.x * basis_scale.x, text_box->padding.y * basis_scale.y}; // Outer padding (margin)
//     float tbox_inner_x = tbox_pixel_coords.x + padding.x;
//     float tbox_inner_y = tbox_pixel_coords.y + padding.y;
//     float tbox_inner_w = tbox_w - (2 * padding.x);
//     float tbox_inner_h = tbox_h - (2 * padding.y);

//     // Text Box - inner
//     DrawRectangleLines(tbox_inner_x, tbox_inner_y, tbox_inner_w, tbox_inner_h, color_border);

//     // 7. DRAW TEXT
//     // 7.1 Draw the label of the text field above the text box (using the same x coordinate but a y coordinate above the box with some spacing)
//     // DrawTextCustom(text_box->text, text_x, text_y, font_scale, font_default, font.colour);
//     // DrawText(text_box->text, text_x, text_y, font_size, (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a});

//     // --- DRAW TEXT LABEL ---
//     // Raw Properties (before applying padding)
//     Vector2d tlabel_pixel_coords = {(text_label->parent_offset.x * basis_scale.x) + parent_pixel_coords.x, (text_label->parent_offset.y * basis_scale.y) + parent_pixel_coords.y};
//     float tlabel_w = text_label->width * basis_scale.x;  //-(2 * padding.x);
//     float tlabel_h = text_label->height * basis_scale.y; //-(2 * padding.y);

//     // Text Label - raw
//     DrawRectangleLines(tlabel_pixel_coords.x, tlabel_pixel_coords.y, tlabel_w, tlabel_h, color_border);

//     // Inner Properties (after applying padding).
//     float tlabel_inner_x = tlabel_pixel_coords.x + padding.x;
//     float tlabel_inner_y = tlabel_pixel_coords.y + padding.y;
//     float tlabel_inner_w = tlabel_w - (2 * padding.x);
//     float tlabel_inner_h = tlabel_h - (2 * padding.y);

//     // Text Label- inner
//     DrawRectangleLines(tlabel_inner_x, tlabel_inner_y, tlabel_inner_w, tlabel_inner_h, color_border);
//     // DrawTextCustom(text_label->text, tlabel_pixel_coords.x, tlabel_pixel_coords.y, font_scale, font_default, font.colour);

//     if (frame_counter.total_frames % 800 == 0)
//     {
//         printf("Drew Text Box at (%.1f, %.1f) : w = %.1f, h = %.1f\n", tbox_pixel_coords.x, tbox_pixel_coords.y, tbox_w, tbox_h);
//         printf("Drew Text Label at (%.1f, %.1f) : w = %.1f, h = %.1f\n", tlabel_pixel_coords.x, tlabel_pixel_coords.y, tlabel_w, tlabel_h);
//     }
// }

// void DrawTextFieldsContainer(UIElement *text_fields_container, Camera2d camera)
// {
//     // EVERY ELEMENT WILL HAVE BOTH RAW AND PADDED DIMENSIONS AND ORIGINS/OFFSETS
//     // OFFSETS ARE ALWAYS RELATIVE TO THE RAW ORIGINS
//     // 1. Get the primary anchor in pixel space
//     Vector2d tcont_local_coords = text_fields_container->origin;
//     Vector2d tcont_pixel_coords = TransformCoordinates(camera.source_to_dest_mtx, text_fields_container->origin);
//     Vector2d basis_scale = BasisTransform_2d_Scale(camera.source_basis, camera.destination_basis); // Need to scale dimensions from world units to pixel units using the camera's basis transform
//     Vector2d tcont_resolved_coords = ResolveElementPosition(text_fields_container, tcont_pixel_coords, basis_scale);
//     float tcont_w = text_fields_container->width * basis_scale.x;
//     float tcont_h = text_fields_container->height * basis_scale.y;

//     ColourRgba colour_border = text_fields_container->colour_border;
//     Color color_border = (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a};
//     ColourRgba colour_fill = text_fields_container->colour_fill;
//     Color color_fill = (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a};

//     // 3. DRAW FILL (Outer Background) -- WORKING
//     DrawRectangle(tcont_pixel_coords.x, tcont_pixel_coords.y, tcont_w, tcont_h, color_fill);

//     //  7. DRAW TEXT FIELDS
//     UIElement **text_fields = (UIElement **)text_fields_container->children.items;
//     int tfield_count = text_fields_container->children.count;
//     float tfield_pixel_padding_x = text_fields_container->padding.x * basis_scale.x; // Apply padding to the container's pixel coordinates to get the starting pixel coordinates for the text fields within the container
//     float tfield_pixel_padding_y = text_fields_container->padding.y * basis_scale.y; // Apply padding to the container's pixel coordinates to get the starting pixel coordinates for the text fields within the container

//     for (int i = 0; i < tfield_count; i++)
//     {
//         UIElement *text_field = text_fields[i];
//         Vector2d tfield_pixel_coords = {(text_field->parent_offset.x * basis_scale.x) + tcont_pixel_coords.x, (text_field->parent_offset.y * basis_scale.y) + tcont_pixel_coords.y};
//         // text_field->origin = (Vector2d){inner_x + text_field_spacing.x, inner_y + text_field_spacing.y}; // Position each text field below the previous one with some spacing
//         float w = text_field->width * basis_scale.x;
//         float h = text_field->height * basis_scale.y;

//         // DEBUG - draw rectangle around the field to check it's in the right place
//         DrawRectangleLines(tfield_pixel_coords.x, tfield_pixel_coords.y, w, h, WHITE);
//         if (frame_counter.total_frames % 800 == 0)
//         {
//             printf("Drew Text Field %d (%.1f, %.1f) : w = %.1f, h = %.1f\n", i, tfield_pixel_coords.x, tfield_pixel_coords.y, w, h);
//         }

//         // NEED TO ADJUST tfield_pixel_coords
//         Vector2d tfield_pixel_coords_padded = VectorSum_2d(tfield_pixel_coords, text_fields_container->padding);
//         DrawTextFieldChildren(text_field, tfield_pixel_coords_padded, FONT_DEFAULT, font_scale_m, camera);
//         // DrawTextFieldChildren(text_field, tfield_pixel_coords, FONT_DEFAULT, font_scale_m, camera);
//     }
// }