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
void DrawUIElement(UIElement *e, UIBox parent_box, Camera2d viewport_camera, Matrix3x3 M_ui_to_pixel);
void DrawTextArea(UIElement *e);

void DrawElementBox(UIElement *e)
{
    UIBox box = e->cached_box;
    ColourRgba colour_fill = e->colour_fill;
    ColourRgba colour_border = e->colour_border;
    DrawRectangle((int)box.coords.x, (int)box.coords.y, (int)box.dimensions.x, (int)box.dimensions.y, (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a});
    DrawRectangleLines((int)box.coords.x, (int)box.coords.y, (int)(box.dimensions.x), (int)(box.dimensions.y), (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a});
}

void DrawTextArea(UIElement *e)
{
    if (!e)
        return;

    // Data Setup - Abstract the difference between Label and Textbox
    char *text_ptr = NULL;  //(e->type == UI_ELEMENT_LABEL) ? e->data.label.text.string : e->data.textbox.text.string;
    Bitmap_Font font = {0}; // e->data.textbox.font; // Assuming similar struct layout
    if (IsTextbox(e))
    {
        text_ptr = e->data.textbox.text.string;
        font = e->data.textbox.font;
    }
    else if (IsBtn(e))
    {
        text_ptr = e->data.button.label.string;
        font = e->data.button.font;
    }
    else if (e->type == UI_ELEMENT_LABEL)
    {
        text_ptr = e->data.label.text.string;
        font = e->data.label.font;
    }
    Vector2d available_space = e->cached_box.dimensions;

    // 2. Metrics calculation
    int glyph_advance = (8 * font.scale) + (font.scale * (int)font.spacing);
    int row_height = (8 * font.scale) + (font.scale * (int)fabs(font.spacing));
    if (glyph_advance <= 0 || row_height <= 0)
    {
        return;
    }
    int rows_that_fit = (int)(available_space.y / row_height);
    int char_ptr = 0;
    int current_row = 0;
    float last_row_x_end = e->cached_box.coords.x;
    // if (rows_that_fit <= 0)
    // return;

    // 3. Segmenting and Drawing
    // We'll draw row-by-row to save memory (no need for a massive 2D array)
    while (text_ptr[char_ptr] != '\0' && current_row < rows_that_fit)
    {
        char row_buffer[256] = {0}; // Local buffer for the current line
        int row_char_count = 0;
        float current_row_width = 0;

        // Fill the buffer for this row until it's full or text ends
        while (text_ptr[char_ptr] != '\0' && row_char_count < 255)
        {
            char c = text_ptr[char_ptr];
            int char_width = glyph_advance;

            if (current_row_width + char_width > available_space.x)
                break;

            row_buffer[row_char_count++] = c;
            current_row_width += char_width;
            char_ptr++;
        }
        row_buffer[row_char_count] = '\0';

        // Draw the row
        Vector2d draw_pos = {
            e->cached_box.coords.x,
            e->cached_box.coords.y + (current_row * row_height)};

        // Draw and capture the end X position for the cursor
        last_row_x_end = DrawTextCustom(row_buffer, draw_pos, font.scale, font, font.colour);
        current_row++;
    }

    // 4. Cursor Rendering
    if (e->is_focused && (frame_counter.total_frames % 60 < 30))
    {
        // Place cursor at the end of the last drawn character
        // Note: Subtract 1 from current_row because it was incremented after the last draw
        // Adjust x_coord because the '|' is drawn in the middle of the 8x8 bitmap, and we need the cursor to be closer to the prev char
        float adjusted_x = last_row_x_end - (font.scale * fabs(font.spacing));
        float adjusted_y = char_ptr > 0 ? e->cached_box.coords.y + ((current_row - 1) * row_height) : e->cached_box.coords.y;
        Vector2d cursor_pos = {adjusted_x, adjusted_y};
        DrawTextCustom("|", cursor_pos, font.scale, font, font.colour);
    }
}

void DrawTextBoxText(UIElement *e)
{

    // Draw the Text
    // DrawTextCustom(e->data.textbox.text.string, e->cached_box.coords, 2, FONT_DEFAULT, FONT_DEFAULT.colour);
}

// NEED TO CREATE BOX FOR ENTIRE SCREEN AND PASS THAT AS THE PARENT BOX FOR THE ROOT ELEMENT, SO THAT THE ROOT ELEMENT AND ALL OTHER ELEMENTS CAN CALCULATE THEIR POSITIONS AND DIMENSIONS BASED ON THAT, RATHER THAN HAVING THE ROOT ELEMENT CALCULATE ITS POSITION AND DIMENSIONS BASED ON A ZERO VECTOR, WHICH IS NOT FLEXIBLE IF WE WANT TO CHANGE THE PANEL OR ROOT ELEMENT PROPERTIES LATER
void DrawRootUIElement(UIElement *root_element, UIBox seed_box, Camera2d space_camera, Matrix3x3 M_ui_to_pixel)
{
    if (!root_element)
    {
        return;
    }

    // Need to convert world coordinates --> viewport --> screen coordinates
    Space2d panel_space = root_element->data.root.space;
    Basis2d basis = panel_space.frame.basis;

    Vector2d origin = panel_space.frame.origin_in_parent;
    Vector2d basis_scale = Camera_GetBasisScale(&space_camera); THIS IS NOT CORRECT, ITS SKIPPING THE SPACE->VIEWPORT TRANSFORMATION. SHOULD BE 0.5*50. NOT (50/0.5). DONT EVEN USE BASIS SCALING. // Need to scale dimensions from world units to pixel units using the camera's basis transform

    // Resolve rendered position and dimensions of panel element
    UIBox box = ResolveElementBox(root_element, seed_box, basis_scale);
    Vector2d pixel_coords = MatrixMultiply_3x3_Vector2d(M_ui_to_pixel, box.coords);
    box.coords = (Vector2d){(int)pixel_coords.x, (int)pixel_coords.y};
    box.dimensions = (Vector2d){(int)box.dimensions.x * basis_scale.x, (int)box.dimensions.y * basis_scale.y};
    root_element->cached_box = box;

    DistributeChildrenRecursiveResolved(root_element, box, basis_scale);

    // Draw background & border
    DrawElementBox(root_element);
    // frame_counter.total_frames % 800 == 0 ? printf("DREW [%s] Pos: (%.1f, %.1f) | Size: (%.1f, %.1f)\n", GetElementTypeName(root_element->type), box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y) : (void)0;

    // Recursively draw children
    UIElement *child = root_element->first_child;
    while (child)
    {
        DrawUIElement(child, box, space_camera, M_ui_to_pixel);
        child = child->next_sibling;
    }
}

void DrawUIElement(UIElement *e, UIBox parent_box, Camera2d viewport_camera, Matrix3x3 M_ui_to_pixel)
{
    if (!e || !e->is_enabled)
    {
        return;
    }

    // The world position of the coordinate space object is the origin of the coordinate space, so (0,0).
    // But to make it more flexible for different coordinate space origins, we will add the world position to the start and end points of the lines to get their actual coordinates in world space, and then convert those to screen coordinates using the basis transform matrix
    Vector2d basis_scale = Camera_GetBasisScale(&viewport_camera); // Need to scale dimensions from world units to pixel units using the camera's basis transform

    // Resolve rendered position and dimensions of panel element
    UIBox box = ResolveElementBox(e, parent_box, basis_scale);
    Vector2d pixel_coords = MatrixMultiply_3x3_Vector2d(M_ui_to_pixel, box.coords);
    box.coords = (Vector2d){(int)pixel_coords.x, (int)pixel_coords.y};
    box.dimensions = (Vector2d){(int)box.dimensions.x * basis_scale.x, (int)box.dimensions.y * basis_scale.y};
    e->cached_box = box;

    // DRAW IT ... IF it's within the ranged coordinates of its parent
    Vector2d parent_verts_arr[4];
    Vector2d verts_arr[4];
    LArray parent_verts = {sizeof(Vector2d), parent_verts_arr, 4, 4, 0};
    LArray verts = {sizeof(Vector2d), verts_arr, 4, 4, 0};
    for (size_t i = 0; i < 4; i++)
    {
        parent_verts_arr[i] = (Vector2d){
            parent_box.coords.x + (i == 1 || i == 2 ? parent_box.dimensions.x : 0),
            parent_box.coords.y + (i >= 2 ? parent_box.dimensions.y : 0)};
        verts_arr[i] = (Vector2d){
            box.coords.x + (i == 1 || i == 2 ? box.dimensions.x : 0),
            box.coords.y + (i >= 2 ? box.dimensions.y : 0)};
    }

    bool is_within_parent = ShapeFitsWithinShape(&verts, &parent_verts, ZERO_VECTOR_2D, ZERO_VECTOR_2D);

    // Don't draw it or any of its children
    if (!is_within_parent)
    {
        frame_counter.total_frames % 1800 == 0 ? printf("NOT DRAWING [%s] | NOT FULLY BOUNDED BY PARENT [%s]\n", GetElementTypeName(e->type), GetElementTypeName(e->parent->type)) : (void)0;
        return;
    }

    // Draw background & border
    DrawElementBox(e);
    if (IsTextbox(e) || IsBtn(e) || e->type == UI_ELEMENT_LABEL)
    {
        // Draw the Text
        DrawTextArea(e);
        // DrawTextCustom(e->data.label.text.string, e->cached_box.coords, 2, FONT_DEFAULT, FONT_DEFAULT.colour);
        //  DrawTextCustom(e->data.label.text.string, e->cached_box.coords, 2, FONT_DEFAULT, FONT_DEFAULT.colour);
        //  DrawTextCustom(e->data.label.text.string, e->cached_box.coords, 2, e->data.label.font, e->data.label.font.colour);
        //   DrawText(text_box->text, text_x, text_y, font_size, (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a});
    }

    // frame_counter.total_frames % 1600 == 0 ? printf("DREW [%s] PIXEL(%.1f, %.1f) | SIZE(%.1f, %.1f)\n", GetElementTypeName(e->type), box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y) : (void)0;

    // Recursively draw children elements
    UIElement *child = e->first_child;
    while (child)
    {
        DrawUIElement(child, box, viewport_camera, M_ui_to_pixel);
        child = child->next_sibling;
    }
}

// Draws Left Panel
// void DrawPanelRegion(UIElement *element, Camera2d camera)
// {
//     // Need to convert world coordinates to screen coordinates
//     Space2d panel_space = element->data.root.space;
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

// void DrawPanelRegion_Stats(Space2d panel_space, Color fill_colour)
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

