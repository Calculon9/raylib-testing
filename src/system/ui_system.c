/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 *   Gameplay Screen Functions Definitions (Init, Update, Draw, Unload)
 *
 *   Copyright (c) 2014-2022 Ramon Santamaria (@raysan5)
 *
 *   This software is provided "as-is", without any express or implied warranty. In no event
 *   will the authors be held liable for any damages arising from the use of this software.
 *
 *   Permission is granted to anyone to use this software for any purpose, including commercial
 *   applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not claim that you
 *     wrote the original software. If you use this software in a product, an acknowledgment
 *     in the product documentation would be appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *     as being the original software.
 *
 *     3. This notice may not be removed or altered from any source distribution.
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
//Font font_default = {0};

// ------------------TOTAL SCREEN-------------------------
// Logical->pixel-space conversion properties
// static float screen_resolution_scalar = 100.0; // used to divide up the pixel resolution to get a local coordinate resolution for the entire screen
// Default font
static int font_scale_l = 3;
static int font_scale_m = 2;
static int font_scale_s = 1;
// Default UI Properties
static Vector2d tbox_tlabel_default_offset = {0.04, 0};
static Vector2d tbox_default_dims = {2.5, 0.4};
static Vector2d tbox_default_padding = {0.03, 0.03};
//static Vector2d tbox_default_padding_inner = {0.02, 0.02};
static ColourRgba tbox_default_colour_border = COLOUR_PANEL_DARK_1; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
//static ColourRgba tbox_default_colour_border_inner = COLOUR_PANEL_MID_1;  // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
static ColourRgba tbox_default_colour_fill = COLOUR_PANEL_LIGHT_1;// COLOUR_PANEL_DARK_1;
//static ColourRgba tbox_default_colour_fill_inner = COLOUR_PANEL_MID_1;
static Vector2d tfield_default_padding = {0.2, 0.2};
static Vector2d tfield_default_spacing = {0, 0.2};
static ColourRgba tfield_default_colour_fill = COLOURLESS_RGBA;
static ColourRgba tcont_default_colour_fill = COLOUR_PANEL_DARK_2;
static ColourRgba tcont_default_colour_border = COLOUR_PANEL_LIGHT_2; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};

//static ColourRgba tfield_default_colour_fill_inner = COLOURLESS_RGBA;
// ----------LEFT PANEL SCREEN----------
//  Visual Properties
static ColourRgba lpanel_text_colour = COLOUR_PANEL_LIGHT_3;
static ColourRgba lpanel_fill_colour = COLOUR_PANEL_DARK_1;
// Coordinate Space Properties
static CoordSpace2d_Grid lpanel_space = {0};
Vector2d lpanel_origin, lpanel_end = {0}; // Dependent on the game world screen area
Vector2d lpanel_pixel_origin, lpanel_pixel_end = {0};
Vector2d lpanel_u = {1, 0};
Vector2d lpanel_v = {0, 1};
Vector2d lpanel_resolution = {0};
// Logical->pixel-space conversion properties
Vector2d lpanel_pixel_u = {75, 0};
Vector2d lpanel_pixel_v = {0, 75};
Camera2d camera_lpanel = {0};
// UI Elements
static UIElement *lpanel_properties_tcont = {0};
static UIElement *lpanel_stats_tcont = {0};
// static TextFieldsContainer *lpanel_properties_tcont = {0};
// static TextFieldsContainer *lpanel_stats_tcont = {0};
static Vector2d lpanel_properties_tcont_origin = {0, 5};
static Vector2d lpanel_stats_tcont_origin = {0};
// - default text container props
static Vector2d lpanel_tcont_default_dims = {3, 5};
static Vector2d lpanel_tcont_default_padding = {0.05, 0.05};
// - other default text container props are same as text box
// - default text box props

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
void InitPanelTextContainers();
// void DrawCircloids();
void DrawPanelRegion(CoordSpace2d_Grid lpanel_space);
void DrawPanelRegion_ObjectProps(CoordSpace2d lpanel_space, Color fill_colour);
void DrawPanelRegion_Stats(CoordSpace2d lpanel_spacelpanel_space, Color fill_colour);
void DrawTextFieldChildren(UIElement *text_field, Vector2d parent_pixel_coords, Bitmap_Font font, int font_scale, Camera2d camera);
void DrawTextFieldsContainer(UIElement *text_fields_container, Camera2d camera);
void UpdatePanelRegion(int mouse_x, int mouse_y, bool cursor_in_panel);
void HandleTextBoxClick(UIElement *clicked);

// FIRST: Initialisation of Gameplay Screen

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
    // lpanel_space = NewCoordSpace2d(lpanel_origin, lpanel_resolution, lpanel_basis);
    lpanel_space = NewCoordSpace2d_Grid(lpanel_origin, lpanel_resolution, lpanel_basis, lpanel_fill_colour, lpanel_fill_colour); 
}

void InitPanelTextContainers()
{
    // PROPERTIES Text Container and its Text Boxes for the panel

    lpanel_properties_tcont = CreateTextFieldContainer(NULL, lpanel_tcont_default_dims.x, lpanel_tcont_default_dims.y, ZERO_VECTOR_2D, lpanel_properties_tcont_origin, lpanel_tcont_default_padding, tfield_default_spacing, tcont_default_colour_border, tcont_default_colour_fill);
    // lpanel_properties_tcont = CreateTextFieldContainer(lpanel_tcont_default_dims.x, lpanel_tcont_default_dims.y, lpanel_properties_tcont_origin, lpanel_tcont_default_padding_inner, lpanel_tcont_default_padding_outer, lpanel_tbox_default_colour_border_outer, lpanel_tbox_default_colour_fill_outer, lpanel_tbox_default_colour_border_inner, lpanel_tbox_default_colour_fill_inner);
    Vector2d text_field_spacing = (Vector2d){0.05, 0.05};

    // This will be the initial origin for the first text field in the properties container, and the rest will be positioned relative to this one using the text_field_spacing property of the container
    // Just set to container origin coords {0,0} for now and will update in the Draw loop based on the actual position and dimensions of the container, so that it works even if we change the container's properties later
    // char *tbox_labels[] = {"OBJECT PROPERTIES"};
    char *tbox_labels[] = {"OBJECT PROPERTIES", "MASS.", "POS.(X,Y)", "VEL.(X,Y)", "ACCEL.(X,Y)"};
    Vector2d tf_initial_origin = (Vector2d){lpanel_properties_tcont_origin.x, lpanel_properties_tcont_origin.y};
    for (int i = 0; i < 5; i++)
    {
        // Create TextField - they will be stacked so the offset between each TextField is the spacing + height
        Vector2d offset_y = (Vector2d){0, tbox_default_dims.y};
        Vector2d tf_offset = VectorSum_2d(VectorScale_2d(tfield_default_spacing, i + 1), VectorScale_2d(offset_y, i));
        UIElement *tfield = CreateTextFieldUnderParent(lpanel_properties_tcont, tbox_default_dims.x, tbox_default_dims.y, tf_initial_origin, tf_offset, tfield_default_padding, tbox_tlabel_default_offset, tbox_default_padding, COLOURLESS_RGBA, COLOURLESS_RGBA, MAX_LABEL_CHARS, MAX_TEXTBOX_CHARS);
        tfield->parent = lpanel_properties_tcont;

        // Customise the TextField's TextBox and calculate the TextBox's origin based on the TextField's origin and the TextBox's padding, so that the text box is positioned correctly within the field
        UIElement **children = (UIElement **)tfield->children.items;
        children[0]->padding = tbox_default_padding;
        children[0]->colour_border = tbox_default_colour_border;
        children[0]->colour_fill = tbox_default_colour_fill;
        children[1]->padding = tbox_default_padding;
        children[1]->colour_border = tbox_default_colour_border;
        children[1]->colour_fill = tbox_default_colour_fill;

        strncpy(children[0]->data.textfield.label_data.text.string, tbox_labels[i], MAX_LABEL_CHARS - 1);
        children[0]->data.textfield.label_data.text.string[MAX_LABEL_CHARS - 1] = '\0'; // Ensure null termination!

        // No need to add the TextField as a child of the container since it's already added as a child within the CreateTextFieldUnderParent function.
        // LArray_Push(&lpanel_properties_tcont->children, &tfield);
    }

    // lpanel_stats_tcont = CreateTextFieldContainer(lpanel_tcont_default_dims.x, lpanel_tcont_default_dims.y, lpanel_stats_text_container_origin, lpanel_tcont_default_padding_inner, lpanel_tcont_default_padding_outer, lpanel_tbox_default_colour_border_outer, lpanel_tbox_default_colour_fill_outer, lpanel_tbox_default_colour_border_inner, lpanel_tbox_default_colour_fill_inner);
}

void UpdateUISystem(int mouse_x, int mouse_y)
{
    bool cursor_in_panel = mouse_x >= lpanel_pixel_origin.x && mouse_x <= (lpanel_pixel_origin.x + (lpanel_pixel_u.x * lpanel_resolution.x)) && mouse_y >= lpanel_pixel_origin.y && mouse_y <= (lpanel_pixel_origin.y + (lpanel_pixel_v.y * lpanel_resolution.y));
    UpdatePanelRegion(mouse_x, mouse_y, cursor_in_panel);
}

void DrawUI()
{
    DrawPanelRegion(lpanel_space);
}

// Gameplay Screen Stage Update logic
void UpdatePanelRegion(int mouse_x, int mouse_y, bool cursor_in_region)
{
    // Check if something was clicked on the panel
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }
    if (!cursor_in_region)
    {
        return;
    }

    char log[256] = "";
    int offset = 0;

    // Convert to local panel coordinates
    Vector2d click_pixel_coords = {mouse_x, mouse_y};
    Vector2d click_panel_coords = TransformCoordinates(camera_lpanel.dest_to_source_mtx, click_pixel_coords);

    int cell_index = ((int)click_panel_coords.y * (int)lpanel_resolution.x) + (int)click_panel_coords.x;

    // Check if there are any objects in that cell and print info about those objects if so
    Cell *cells = lpanel_space.coord_space.cells.coll.items;
    Cell cell = cells[cell_index];
    // selectedCell = &cells[cell_index];
    offset += snprintf(log + offset, sizeof(log) - offset, "Region: Panel (%.1f, %.1f) --> Cell %d (%.1f, %.1f) --> ", lpanel_pixel_origin.x, lpanel_pixel_origin.y, cell_index, cell.coords.x, cell.coords.y);

    // Loop through all properties text boxes and apply focus if the clicked_coords are within a text_box, otherwise set focus --> false
    UIElement **tfields = (UIElement **)lpanel_properties_tcont->children.items; //.coll;
    UIElement *t = NULL;
    int tfield_count = lpanel_properties_tcont->children.count;
    Vector2d vertices[4] = {0};
    bool any_focus = false;
    if (lpanel_properties_tcont->children.count > 0)
    {
        for (int i = 0; i < tfield_count; i++)
        {
            // Check if the click is within the text box
            t = (UIElement *)(tfields[i]);
            GetUIElementVertices(t, vertices); // ISSUE HERE
            if (IsFocused(click_panel_coords, vertices, 4))
            {
                // Apply focus on text box
                HandleTextBoxClick(t);
                any_focus = true;
                offset += snprintf(log + offset, sizeof(log) - offset, "Element: TextBox (%.1f, %.1f)", click_panel_coords.x, click_panel_coords.y);
                break;
                // printf("Clicked on object properties text box! Text box properties - Position: (%.1f, %.1f), Dimensions: (%.1f, %.1f)\n", lpanel_properties_tbox.origin.x, lpanel_properties_tbox.origin.y, lpanel_properties_tbox.width, lpanel_properties_tbox.height);
            }
        }
    }
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
    if (!any_focus)
    {
        offset += snprintf(log + offset, sizeof(log) - offset, "Element: Nill");
    }

    printf("CLICKED (%d, %d) { %s }\n", mouse_x, mouse_x, log);

    // finishScreen = 1;
    //  PlaySound(fxCoin);
}

// Draws Left Panel
void DrawPanelRegion(CoordSpace2d_Grid lpanel_space)
{
    // Need to convert world coordinates to screen coordinates
    Basis2d basis = lpanel_space.coord_space.basis;

    // The world position of the coordinate space object is the origin of the coordinate space, so (0,0).
    // But to make it more flexible for different coordinate space origins, we will add the world position to the start and end points of the lines to get their actual coordinates in world space, and then convert those to screen coordinates using the basis transform matrix
    Vector2d origin = lpanel_space.coord_space.coords_origin;
    Vector2d end = VectorSum_2d(origin, lpanel_space.coord_space.resolution_ixj);

    // Transform local space position to pixel space
    Vector2d pixel_origin = TransformCoordinates(camera_lpanel.source_to_dest_mtx, origin);
    Vector2d pixel_end = TransformCoordinates(camera_lpanel.source_to_dest_mtx, end);

    // First: Draw background
    ColourRgba colour_fill = lpanel_space.colour_fill;
    ColourRgba colour_line = lpanel_space.colour_line;
    DrawRectangle(pixel_origin.x, pixel_origin.y, abs(pixel_end.x - pixel_origin.x), abs(pixel_end.y - pixel_origin.y), (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a});

    // DrawPanelRegion_Stats(panel_space, fill_colour);
    //  DrawPanelRegion_ObjectProps(panel_space, fill_colour);

    // Draw the text boxes in the Properties container
    DrawTextFieldsContainer(lpanel_properties_tcont, camera_lpanel);

    // Draw the text boxes in the Stats container
    // DrawTextFieldsContainer(&lpanel_stats_tcont, camera_lpanel);

    // Memory display - Consumed memory in bytes out of the total allocated bytes
    // snprintf(text, sizeof(text), "Memory Consumed (bytes): %zu", GetCurrentMemoryAllocated()); // Format the FPS value into the buffer
    // DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + 2 * lineSpacing.y}, font.baseSize * 2.0f, 2, (Color)BEIGE);
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
    // 1. "Bubble up" to parent
    UIElement *p = clicked->parent;

    if (p == NULL)
        return;
    LArray siblings = p->children; //->text_fields.coll.items;
    int sibling_count = p->children.count;

    // 2. Access siblings via parent's list
    for (int i = 0; i < sibling_count; i++)
    {
        UIElement *sibling = ((UIElement **)siblings.items)[i];
        UIElement *sibling_tbox = sibling->children.count == 2 ? ((UIElement **)sibling->children.items)[0] : NULL; // Assuming the TextBox is the first child of the TextField

        if (sibling == clicked)
        {
            continue; // Skip the one we clicked
        }

        // 3. Do something to the siblings (e.g., deselect them)
        sibling_tbox->is_focused = false;
    }

    clicked->is_focused = true;
}

void DrawTextFieldsContainer(UIElement *text_fields_container, Camera2d camera)
{
    // EVERY ELEMENT WILL HAVE BOTH RAW AND PADDED DIMENSIONS AND ORIGINS/OFFSETS
    // OFFSETS ARE ALWAYS RELATIVE TO THE RAW ORIGINS 
    // 1. Get the primary anchor in pixel space
    Vector2d tcont_local_coords = text_fields_container->origin;
    Vector2d tcont_pixel_coords = TransformCoordinates(camera.source_to_dest_mtx, text_fields_container->origin);

    // Need to scale dimensions from world units to pixel units using the camera's basis transform
    Vector2d basis_scale = BasisTransform_2d_Scale(camera.source_basis, camera.destination_basis);

    float tcont_w = text_fields_container->width * basis_scale.x;  // Assuming width is defined in world units and needs to be scaled to pixel units
    float tcont_h = text_fields_container->height * basis_scale.y; // Assuming height is defined in world units and needs to be scaled to pixel units

    ColourRgba colour_border = text_fields_container->colour_border;
    Color color_border = (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a};
    ColourRgba colour_fill = text_fields_container->colour_fill;
    Color color_fill = (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a};

    // 3. DRAW FILL (Outer Background) -- WORKING
    DrawRectangle(tcont_pixel_coords.x, tcont_pixel_coords.y, tcont_w, tcont_h, color_fill);

    //  7. DRAW TEXT FIELDS
    UIElement **text_fields = (UIElement **)text_fields_container->children.items;
    int tfield_count = text_fields_container->children.count;
    // printf("Container Items Pointer: %p\n", text_fields_container->children.items);
    // printf("First Child Pointer Address: %p\n", ((UIElement **)text_fields_container->children.items)[0]);

    // if (text_fields_container->children.count > 0)
    // {
    //     UIElement *first = ((UIElement **)text_fields_container->children.items)[0];
    //     printf("First Child Type: %d\n", first->type); // Should be UI_ELEMENT_TEXTFIELD
    // }
    for (int i = 0; i < tfield_count; i++)
    {
        UIElement *text_field = text_fields[i];
        Vector2d tfield_pixel_coords = {(text_field->parent_offset.x * basis_scale.x) + tcont_pixel_coords.x, (text_field->parent_offset.y * basis_scale.y) + tcont_pixel_coords.y};
        // text_field->origin = (Vector2d){inner_x + text_field_spacing.x, inner_y + text_field_spacing.y}; // Position each text field below the previous one with some spacing
        float w = text_field->width * basis_scale.x;
        float h = text_field->height * basis_scale.y;

        // DEBUG - draw rectangle around the field to check it's in the right place
        DrawRectangleLines(tfield_pixel_coords.x, tfield_pixel_coords.y, w, h, WHITE);
        if (frame_counter.total_frames % 800 == 0)
        {
            printf("Drew Text Field %d (%.1f, %.1f) : w = %.1f, h = %.1f\n", i, tfield_pixel_coords.x, tfield_pixel_coords.y, w, h);
        }

        // NEED TO ADJUST tfield_pixel_coords
        //Vector2d tfield_pixel_coords_padded = tfield_pixel_coords
        //DrawTextFieldChildren(text_field, tfield_pixel_coords, FONT_DEFAULT, font_scale_m, camera);
    }
}

void DrawTextFieldChildren(UIElement *text_field, Vector2d parent_pixel_coords, Bitmap_Font font, int font_scale, Camera2d camera)
{
    UIElement **children = (UIElement **)text_field->children.items;
    UIElement *text_label = children[0]; // Assuming the TextLabel is the second child of the TextField
    UIElement *text_box = children[1]; // Assuming the TextLabel is the second child of the TextField
    Color color_border = (Color){text_box->colour_border.r, text_box->colour_border.g, text_box->colour_border.b, text_box->colour_border.a};
    Color color_fill = (Color){text_box->colour_fill.r, text_box->colour_fill.g, text_box->colour_fill.b, text_box->colour_fill.a};

    // Need to scale dimensions from world units to pixel units using the camera's basis transform
    Vector2d basis_scale = BasisTransform_2d_Scale(camera.source_basis, camera.destination_basis);

    // --- DRAW TEXT BOX ---
    // Raw Properties (before applying padding)
    // These are the raw dimensions of the text box in pixel space before padding is applied, so the actual area available for text will be smaller once we apply padding
    Vector2d tbox_pixel_coords = {(text_box->parent_offset.x * basis_scale.x) + parent_pixel_coords.x, (text_box->parent_offset.y * basis_scale.y) + parent_pixel_coords.y};
    float tbox_w = text_box->width * basis_scale.x;  // Assuming width is defined in world units and needs to be scaled to pixel units
    float tbox_h = text_box->height * basis_scale.y; // Assuming height is defined in world units and needs to be scaled to pixel units

    // Text Box - raw
    DrawRectangleLines(tbox_pixel_coords.x, tbox_pixel_coords.y, tbox_w, tbox_h, color_border);

    // Inner Properties (after applying padding).
    Vector2d padding = {text_box->padding.x * basis_scale.x, text_box->padding.y * basis_scale.y}; // Outer padding (margin)
    float tbox_inner_x = tbox_pixel_coords.x + padding.x;
    float tbox_inner_y = tbox_pixel_coords.y + padding.y;
    float tbox_inner_w = tbox_w - (2 * padding.x);
    float tbox_inner_h = tbox_h - (2 * padding.y);

    // Text Box - inner
    DrawRectangleLines(tbox_inner_x, tbox_inner_y, tbox_inner_w, tbox_inner_h, color_border);

    // 7. DRAW TEXT
    // 7.1 Draw the label of the text field above the text box (using the same x coordinate but a y coordinate above the box with some spacing)
    // DrawTextCustom(text_box->text, text_x, text_y, font_scale, font_default, font.colour);
    // DrawText(text_box->text, text_x, text_y, font_size, (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a});

    // --- DRAW TEXT LABEL ---
    // Raw Properties (before applying padding)
    Vector2d tlabel_pixel_coords = {(text_label->parent_offset.x * basis_scale.x) + parent_pixel_coords.x, (text_label->parent_offset.y * basis_scale.y) + parent_pixel_coords.y};
    float tlabel_w = text_label->width * basis_scale.x;  //-(2 * padding.x);
    float tlabel_h = text_label->height * basis_scale.y; //-(2 * padding.y);

    // Text Label - raw
    DrawRectangleLines(tlabel_pixel_coords.x, tlabel_pixel_coords.y, tlabel_w, tlabel_h, color_border);

    // Inner Properties (after applying padding).
    float tlabel_inner_x = tlabel_pixel_coords.x + padding.x;
    float tlabel_inner_y = tlabel_pixel_coords.y + padding.y;
    float tlabel_inner_w = tlabel_w - (2 * padding.x);
    float tlabel_inner_h = tlabel_h - (2 * padding.y);

    // Text Label- inner
    DrawRectangleLines(tlabel_inner_x, tlabel_inner_y, tlabel_inner_w, tlabel_inner_h, color_border);
    // DrawTextCustom(text_label->text, tlabel_pixel_coords.x, tlabel_pixel_coords.y, font_scale, font_default, font.colour);

    if (frame_counter.total_frames % 800 == 0)
    {
        printf("Drew Text Box at (%.1f, %.1f) : w = %.1f, h = %.1f\n", tbox_pixel_coords.x, tbox_pixel_coords.y, tbox_w, tbox_h);
        printf("Drew Text Label at (%.1f, %.1f) : w = %.1f, h = %.1f\n", tlabel_pixel_coords.x, tlabel_pixel_coords.y, tlabel_w, tlabel_h);
    }
}