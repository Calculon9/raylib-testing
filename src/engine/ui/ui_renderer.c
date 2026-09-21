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
#include "system/draw_primitives.h"
#include "system/ui_system.h"
#include "system/debug_overlay_system.h"
#include "system/systems.h"
#include "system/utility_system.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
//
void DrawUIElement(UIElement *e, UIBox parent_box, Matrix3x3 M_ui_to_pixel, UIClipRect current_clip);
void DrawTextArea(UIElement *e);
void DrawTextAreaClipped(UIElement *e, UIClipRect clip);

// Consume one wrapped text row and optionally copy its characters into a draw buffer.
static int ConsumeTextRow(const char **text_cursor, float available_width,
                          int glyph_advance, int glyph_cell_width,
                          char *row_buffer, size_t row_buffer_capacity,
                          float *out_row_width)
{
    if (!text_cursor || !*text_cursor || available_width <= 0.0f ||
        glyph_advance <= 0 || glyph_cell_width <= 0)
    {
        return 0;
    }

    const char *cursor = *text_cursor;
    int row_char_count = 0;
    int max_row_chars = row_buffer ? (int)row_buffer_capacity - 1 : 255;
    if (max_row_chars <= 0)
    {
        return 0;
    }

    float row_width = 0.0f;
    while (*cursor != '\0' && row_char_count < max_row_chars)
    {
        int char_width = row_char_count == 0 ? glyph_cell_width : glyph_advance;
        if (row_width + (float)char_width > available_width)
        {
            break;
        }

        if (row_buffer)
        {
            row_buffer[row_char_count] = *cursor;
        }
        row_width += (float)char_width;
        row_char_count++;
        cursor++;
    }

    if (row_buffer)
    {
        row_buffer[row_char_count] = '\0';
    }
    if (out_row_width)
    {
        *out_row_width = row_width;
    }

    *text_cursor = cursor;
    return row_char_count;
}

static int CountTextRows(const char *text, float available_width, int glyph_advance, int glyph_cell_width, int max_rows)
{
    const char *text_cursor = text;
    int row_count = 0;

    while (text_cursor && *text_cursor != '\0' && row_count < max_rows)
    {
        if (ConsumeTextRow(&text_cursor, available_width, glyph_advance,
                           glyph_cell_width, NULL, 0, NULL) == 0)
        {
            break;
        }

        row_count++;
    }

    return row_count;
}

void DrawElementBox(UIElement *e)
{
    UIBox box = e->screen_box;
    ColourRgba colour_fill = e->colour_fill;
    ColourRgba colour_border = e->colour_border;
    if (!IsDebugEnabled(DEBUG_UI_BORDERS))
    {
        colour_border.a = 0;
    }
    DrawRectangleRec((Rectangle){box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y},
                     ToRaylibColor(colour_fill));
    DrawRectangleLinesEx((Rectangle){box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y},
                         1.0f, ToRaylibColor(colour_border));
}

// Draw an element's background and border clipped against an active clip rectangle.
void DrawElementBoxClipped(UIElement *e, UIClipRect clip)
{
    if (!e || !UIClipRect_IntersectsBox(clip, e->screen_box))
    {
        return;
    }

    if (UIClipRect_ContainsBox(clip, e->screen_box))
    {
        DrawElementBox(e);
        return;
    }

    UIBox clipped_box = UIBox_Clip(e->screen_box, clip);
    if (clipped_box.dimensions.x <= 0.0f || clipped_box.dimensions.y <= 0.0f)
    {
        return;
    }

    ColourRgba colour_fill = e->colour_fill;
    ColourRgba colour_border = e->colour_border;
    if (!IsDebugEnabled(DEBUG_UI_BORDERS))
    {
        colour_border.a = 0;
    }

    // Draw the clipped background fill.
    if (colour_fill.a > 0)
    {
        DrawRectangleRec((Rectangle){clipped_box.coords.x, clipped_box.coords.y,
                                     clipped_box.dimensions.x, clipped_box.dimensions.y},
                         ToRaylibColor(colour_fill));
    }

    // Only draw borders along the original element edges if they fall within the clip rect.
    if (colour_border.a > 0)
    {
        Color border_color = ToRaylibColor(colour_border);
        const float epsilon = 0.5f;

        // Draw top border if not clipped away.
        if (fabsf(clipped_box.coords.y - e->screen_box.coords.y) <= epsilon)
        {
            DrawRectangleRec((Rectangle){clipped_box.coords.x, clipped_box.coords.y,
                                         clipped_box.dimensions.x, 1.0f},
                             border_color);
        }

        // Draw bottom border if not clipped away.
        if (fabsf((clipped_box.coords.y + clipped_box.dimensions.y) -
                  (e->screen_box.coords.y + e->screen_box.dimensions.y)) <= epsilon)
        {
            DrawRectangleRec((Rectangle){clipped_box.coords.x,
                                         clipped_box.coords.y + clipped_box.dimensions.y - 1.0f,
                                         clipped_box.dimensions.x, 1.0f},
                             border_color);
        }

        // Draw left border if not clipped away.
        if (fabsf(clipped_box.coords.x - e->screen_box.coords.x) <= epsilon)
        {
            DrawRectangleRec((Rectangle){clipped_box.coords.x, clipped_box.coords.y,
                                         1.0f, clipped_box.dimensions.y},
                             border_color);
        }

        // Draw right border if not clipped away.
        if (fabsf((clipped_box.coords.x + clipped_box.dimensions.x) -
                  (e->screen_box.coords.x + e->screen_box.dimensions.x)) <= epsilon)
        {
            DrawRectangleRec((Rectangle){clipped_box.coords.x + clipped_box.dimensions.x - 1.0f,
                                         clipped_box.coords.y,
                                         1.0f, clipped_box.dimensions.y},
                             border_color);
        }
    }
}

void DrawTextArea(UIElement *e)
{
    DrawTextAreaClipped(e, UI_UNCONSTRAINED_CLIP);
}

// Draw text within an element's screen bounds, clipped against an active clip rectangle.
void DrawTextAreaClipped(UIElement *e, UIClipRect clip)
{
    if (!e || UIClipRect_IsEmpty(clip))
    {
        return;
    }

    if (!UIClipRect_IntersectsBox(clip, e->screen_box))
    {
        return;
    }

    // Data Setup - Abstract the difference between Label, Button, HoverItem, and Textbox.
    char *text_ptr = NULL;
    Bitmap_Font font = {0};
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
    else if (e->type == UI_ELEMENT_HOVER_ITEM)
    {
        text_ptr = e->data.hover_item.label.string;
        font = e->data.hover_item.font;
    }
    else if (e->type == UI_ELEMENT_LABEL)
    {
        text_ptr = e->data.label.text.string;
        font = e->data.label.font;
    }
    if (!text_ptr || text_ptr[0] == '\0')
    {
        return;
    }
    Vector2d available_space = e->screen_box.dimensions;

    // Metrics calculation
    int glyph_advance = BitmapFont_GetGlyphAdvance(&font);
    int glyph_cell_width = BitmapFont_GetGlyphCellWidth(&font);
    int row_height = BitmapFont_GetRowHeight(&font);
    if (glyph_advance <= 0 || row_height <= 0)
    {
        return;
    }
    int rows_that_fit = (int)(available_space.y / row_height);
    int text_row_count = CountTextRows(text_ptr, available_space.x, glyph_advance, glyph_cell_width, rows_that_fit);
    float vertical_offset = 0.0f;
    float text_height = (float)(text_row_count * row_height);
    if (e->text_vertical_alignment == UI_TEXT_VERTICAL_ALIGN_CENTRE)
    {
        vertical_offset = fmaxf(0.0f, (available_space.y - text_height) / 2.0f);
    }
    else if (e->text_vertical_alignment == UI_TEXT_VERTICAL_ALIGN_BOTTOM)
    {
        vertical_offset = fmaxf(0.0f, available_space.y - text_height);
    }
    const char *text_cursor = text_ptr;
    int current_row = 0;
    float last_row_x_end = e->screen_box.coords.x;
    bool fully_contained = UIClipRect_ContainsBox(clip, e->screen_box);

    // Segmenting and Drawing
    // We'll draw row-by-row to save memory (no need for a massive 2D array)
    while (text_cursor && *text_cursor != '\0' && current_row < rows_that_fit)
    {
        char row_buffer[256] = {0}; // Local buffer for the current line
        float current_row_width = 0.0f;
        int row_char_count = ConsumeTextRow(
            &text_cursor, available_space.x, glyph_advance, glyph_cell_width,
            row_buffer, sizeof(row_buffer), &current_row_width);
        if (row_char_count == 0)
        {
            break;
        }

        // Draw the row
        Vector2d draw_pos = {
            e->screen_box.coords.x,
            e->screen_box.coords.y + vertical_offset + (float)(current_row * row_height)};

        if (e->text_horizontal_alignment == UI_TEXT_ALIGN_CENTRE)
        {
            draw_pos.x += fmaxf(0.0f, (available_space.x - current_row_width) / 2.0f);
        }
        else if (e->text_horizontal_alignment == UI_TEXT_ALIGN_RIGHT)
        {
            draw_pos.x += fmaxf(0.0f, available_space.x - current_row_width);
        }

        if (fully_contained)
        {
            // Fast path: element is entirely inside the clip rectangle
            last_row_x_end = DrawTextCustom(row_buffer, draw_pos, font.scale, font, font.colour);
        }
        else
        {
            float row_bottom = draw_pos.y + (float)row_height;
            float row_right = draw_pos.x + current_row_width;

            // Check if this row overlaps the clipping rectangle
            if (row_bottom > clip.min.y && draw_pos.y < clip.max.y &&
                row_right > clip.min.x && draw_pos.x < clip.max.x)
            {
                if (draw_pos.x >= clip.min.x && row_right <= clip.max.x &&
                    draw_pos.y >= clip.min.y && row_bottom <= clip.max.y)
                {
                    last_row_x_end = DrawTextCustom(row_buffer, draw_pos, font.scale, font, font.colour);
                }
                else
                {
                    last_row_x_end = DrawTextCustomClipped(row_buffer, draw_pos, font.scale, font, font.colour, clip);
                }
            }
            else
            {
                last_row_x_end = draw_pos.x + current_row_width;
            }
        }

        current_row++;
    }

    const char *remaining_text = text_cursor;
    if (remaining_text[0] != '\0')
    {
        if (frame_counter.total_frames % 300 == 0)
        {
            LOG_WARN("Text was truncated for %s. Rendered rows: %d, Rows that fit: %d. Remaining text: \"%s\"",
                     GetElementTypeName(e->type), current_row, rows_that_fit, remaining_text);
        }
    }

    // Cursor Rendering
    if (e->is_focused && (frame_counter.total_frames % 60 < 30) && IsTextbox(e))
    {
        // Place cursor at the end of the last drawn character
        // Note: Subtract 1 from current_row because it was incremented after the last draw
        float adjusted_x = last_row_x_end;
        float adjusted_y = text_cursor != text_ptr
                               ? e->screen_box.coords.y + vertical_offset + (float)((current_row - 1) * row_height)
                               : e->screen_box.coords.y + vertical_offset;
        Vector2d cursor_pos = {adjusted_x, adjusted_y};

        if (fully_contained)
        {
            DrawTextCustom("|", cursor_pos, font.scale, font, font.colour);
        }
        else if (cursor_pos.x >= clip.min.x && cursor_pos.x < clip.max.x &&
                 cursor_pos.y + (float)row_height > clip.min.y && cursor_pos.y < clip.max.y)
        {
            DrawTextCustomClipped("|", cursor_pos, font.scale, font, font.colour, clip);
        }
    }
}

void DrawTextBoxText(UIElement *e)
{

    // Draw the Text
    // DrawTextCustom(e->data.textbox.text.string, e->cached_box.coords, 2, FONT_DEFAULT, FONT_DEFAULT.colour);
}

// NEED TO CREATE BOX FOR ENTIRE SCREEN AND PASS THAT AS THE PARENT BOX FOR THE ROOT ELEMENT, SO THAT THE ROOT ELEMENT AND ALL OTHER ELEMENTS CAN CALCULATE THEIR POSITIONS AND DIMENSIONS BASED ON THAT, RATHER THAN HAVING THE ROOT ELEMENT CALCULATE ITS POSITION AND DIMENSIONS BASED ON A ZERO VECTOR, WHICH IS NOT FLEXIBLE IF WE WANT TO CHANGE THE PANEL OR ROOT ELEMENT PROPERTIES LATER
void DrawRootUIElement(UIElement *root_element, UIBox seed_box, Matrix3x3 M_ui_to_pixel)
{
    if (!root_element)
    {
        return;
    }

    Matrix2x2 basis_matrix = {
        {M_ui_to_pixel.col1.x, M_ui_to_pixel.col1.y},
        {M_ui_to_pixel.col2.x, M_ui_to_pixel.col2.y}};
    Vector2d basis_tfrm = VectorSum_2d(basis_matrix.col1, basis_matrix.col2);

    // Resolve rendered position and dimensions of panel element
    root_element->screen_box.coords = TransformCoordinates(M_ui_to_pixel, seed_box.coords);
    root_element->screen_box.dimensions = (Vector2d){seed_box.dimensions.x * basis_tfrm.x, seed_box.dimensions.y * basis_tfrm.y};

    // Draw background & border
    DrawElementBox(root_element);

    // Recursively draw children using an unconstrained root clip
    ForEachChild(root_element, child)
    {
        DrawUIElement(child, root_element->screen_box, M_ui_to_pixel, UI_UNCONSTRAINED_CLIP);
    }
}

void DrawUIElement(UIElement *e, UIBox parent_box, Matrix3x3 M_ui_to_pixel, UIClipRect current_clip)
{
    if (!e || !e->is_enabled)
    {
        return;
    }

    Matrix2x2 basis_matrix = {
        {M_ui_to_pixel.col1.x, M_ui_to_pixel.col1.y},
        {M_ui_to_pixel.col2.x, M_ui_to_pixel.col2.y}};
    Vector2d basis_tfrm = VectorSum_2d(basis_matrix.col1, basis_matrix.col2);

    // Calculate local offset scaled to pixel space
    Vector2d scaled_local_offset = {
        e->local_box.coords.x * basis_tfrm.x,
        e->local_box.coords.y * basis_tfrm.y};

    // Add to parent's screen coordinates
    e->screen_box.coords = TransformCoordinates(M_ui_to_pixel, e->local_box.coords);
    e->screen_box.dimensions = (Vector2d){e->local_box.dimensions.x * basis_tfrm.x, e->local_box.dimensions.y * basis_tfrm.y};

    if (IsBtn(e))
    {
        // LOG_INFO("I'm a textbox, check my dimensions.");
    }

    // Prune elements completely outside the current clipping rectangle if they cannot scroll children into view.
    if (!(e->is_scrollable_x || e->is_scrollable_y) &&
        !UIClipRect_IntersectsBox(current_clip, e->screen_box))
    {
        return;
    }

    // Draw background & border with clipping against the current clip rectangle
    DrawElementBoxClipped(e, current_clip);

    if (IsTextbox(e) || IsBtn(e) || e->type == UI_ELEMENT_LABEL ||
        e->type == UI_ELEMENT_HOVER_ITEM)
    {
        // Draw text only if element intersects the visible clip
        if (UIClipRect_IntersectsBox(current_clip, e->screen_box))
        {
            DrawTextArea(e);
        }
    }

    // Calculate clip rectangle for children; scrollable containers constrain all descendants.
    UIClipRect child_clip = current_clip;
    if (e->is_scrollable_x || e->is_scrollable_y)
    {
        child_clip = UIClipRect_Intersect(current_clip, UIClipRect_FromBox(e->screen_box));
    }

    if (!UIClipRect_IsEmpty(child_clip))
    {
        // Recursively draw children elements within the propagated clip boundary
        ForEachChild(e, child)
        {
            DrawUIElement(child, e->screen_box, M_ui_to_pixel, child_clip);
        }
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
//     Vector2d origin = panel_space.bounds_origin;
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
