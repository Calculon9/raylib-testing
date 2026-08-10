/**********************************************************************************************

 **********************************************************************************************/
#include <stdio.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "ui/text_region.h"
#include "system/ui_system.h"
#include "raylib.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
void DrawChar(char c, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour);

// Takes in pixel coord point and determines if they are in the target region
bool IsFocused(Vector2d pixel_coords, Vector2d *vertices, int vertex_count)
{
    // Vector2d *vertices = polygon->vertices.coll.items;
    return IsPointInPolygon(pixel_coords, vertices, ZERO_VECTOR_2D, vertex_count);
}

UIElement *CreateTextFieldInTree(Size size, UIElement *parent, Offset parent_offset, Size tbox_size, Vector2d padding, bool label_is_inline, ColourRgba colour_border, ColourRgba colour_fill, Bitmap_Font font)
{
    UIElement *tf = CreateUIElementInTree(UI_ELEMENT_TEXTFIELD, size, parent, parent_offset, padding, colour_border, colour_fill);
    tf->type = UI_ELEMENT_TEXTFIELD;

    UIElement *tl = CreateUIElement(UI_ELEMENT_LABEL, (Size){ZERO_VECTOR_2D, SIZE_PERCENT}, (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, ZERO_VECTOR_2D, colour_border, colour_fill);
    UIElement *tb = CreateUIElement(UI_ELEMENT_TEXTBOX_SAFE_IO, (Size){ZERO_VECTOR_2D, SIZE_PERCENT}, (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, ZERO_VECTOR_2D, colour_border, colour_fill);
    tl->type = UI_ELEMENT_LABEL;
    tb->type = UI_ELEMENT_TEXTBOX_SAFE_IO;
    SetUIElementTextVerticalAlignment(tl, UI_TEXT_VERTICAL_ALIGN_CENTRE);
    SetUIElementTextVerticalAlignment(tb, UI_TEXT_VERTICAL_ALIGN_CENTRE);
    tl->data.label.font = font;
    tb->data.textbox.font = font;

    // Determine layout style
    if (tbox_size.size_mode == SIZE_PERCENT)
    {
        if (label_is_inline) // Inline layout
        {
            // 1. Percentage-based Inline Layout (e.g., 40% Label, 60% TextBox)
            tl->size.dimensions.x = 1.0 - tbox_size.dimensions.x; // 40% of parent width
            tl->size.dimensions.y = 1.0f;                       // 100% of parent height
            tl->resolved_offset.offset = (Vector2d){0, 0};

            tb->size.dimensions.x = tbox_size.dimensions.x; // 60% of parent width
            tb->size.dimensions.y = 1.0f;
            // Offset starts where the label ends (40% mark)
            // Note: Using percentage for offset requires Resolve function to handle it!
            tb->resolved_offset.offset = (Vector2d){1.0 - tbox_size.dimensions.x, 0};
        }
        else // Stacked Layout: Label top (e.g. 30%), TextBox bottom (70%)
        {

            tl->size.dimensions.x = 1.0f;
            tl->size.dimensions.y = 1.0 - tbox_size.dimensions.y;
            tl->resolved_offset.offset = (Vector2d){0, 0};

            tb->size.dimensions.x = 1.0f;
            tb->size.dimensions.y = tbox_size.dimensions.y;
            tb->resolved_offset.offset = (Vector2d){0, 1.0 - tbox_size.dimensions.y};
        }

        tl->authored_offset = tl->resolved_offset;
        tb->authored_offset = tb->resolved_offset;
    }

    AddElementToTree(tl, tf);
    AddElementToTree(tb, tf);

    return tf;
}

UIElement *CreateTextFieldContainerInTree(Size size, UIElement *parent, Offset parent_offset, Vector2d padding, Spacing child_spacing, ColourRgba colour_border, ColourRgba colour_fill)
{
    UIElement *tc = CreateUIElementInTree(UI_ELEMENT_CONTAINER, size, parent, parent_offset, padding, colour_border, colour_fill);
    tc->child_spacing = child_spacing;
    // tc->children = *NewLArray(4, sizeof(UIElement *));

    return tc;
}

ShortString GetText_TextField(TextField *text_box)
{
    ShortString str = {0};

    if (!text_box)
    {
        return str;
    }

    strncpy(str.text, text_box->text_box.text, sizeof(str.text) - 1);
    str.text[sizeof(str.text) - 1] = '\0';

    return str;
}

int GetTextWidth(char *text, char font_spacing, char scale)
{
    if (text == NULL)
        return 0;

    int char_count = strlen(text);
    if (char_count == 0)
        return 0;

    // In case there was no string termination
    // If there isn't, strlen will wonder outside the provided text buffer and count the 1st byte outside it as a char!
    if (text[char_count - 1] != '\0')
    {
        char_count--;
    }
    // 8 pixels for the char + 1 pixel for spacing = 9 total per char
    // Note: We subtract the very last spacing pixel for a perfect fit
    // (8 pixels per char + x pixel spacing) * scale
    int width = char_count * ((8 * scale) + (scale * font_spacing));
    // int width = char_count * ((8 + font_spacing) * scale); (8 * scale) + (scale * font.spacing);
    //  int width = (char_count * (8 * scale));// + ((char_count - 1) * scale);

    return width;
}

Vector2d GetTextCenterPos(const char *text, float fontSize, Vector2d origin)
{
    // 1. Calculate the center of the specific cell (c, r)
    // float centerX = origin.x + (c + 0.5f) * u.x + (r + 0.5f) * v.x;
    // float centerY = origin.y + (c + 0.5f) * u.y + (r + 0.5f) * v.y;

    // // 2. Measure the text dimensions
    // Vector2d textSize = MeasureTextEx(font, text, fontSize, 1.0f);

    // // 3. Subtract half dimensions to get the starting (top-left) point
    // Vector2d startPos;
    // startPos.x = centerX - (textSize.x / 2.0f);
    // startPos.y = centerY - (textSize.y / 2.0f);

    // return startPos;
}

// Custom text drawing function that uses our Bitmap_Font and supports scaling and color. Coordinate origin is the top-left corner of the text in pixels.
float DrawTextCustom(const char *text, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour)
{
    Vector2d char_coords = origin_coords;
    while (*text)
    {
        DrawChar(*text, char_coords, scale, font, colour);

        // Move to the next character slot
        // 8 pixels wide * scale + 1 pixel of "letter spacing"
        char_coords.x += (8 * scale) + (scale * font.spacing); // - (scale * scale) + 1; // Subtracting "scale" removes embedded white-space in each character
        text++;
    }
    return char_coords.x - (scale * font.spacing);
}

void DrawChar(char c, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour)
{
    // Cast to unsigned to handle extended ASCII safely
    unsigned char u_c = (unsigned char)c;
    int bitmap_width = font.bitmap_width > 0 && font.bitmap_width <= 8 ? font.bitmap_width : 8;
    int bitmap_offset = (8 - bitmap_width) / 2;

    for (int row = 0; row < 8; row++)
    {
        unsigned char row_data = (unsigned char)(font.bitmap[u_c][row] << font.bitmap_shift);

        for (int col = 0; col < bitmap_width; col++)
        {
            int cell_col = bitmap_offset + col;
            if (row_data & (0x80 >> cell_col))
            {
                // If the bit is 1, draw a "pixel" scaled up
                DrawRectangle(origin_coords.x + (cell_col * scale), origin_coords.y + (row * scale), scale, scale, (Color){colour.r, colour.g, colour.b, colour.a});
            }
        }
    }
}

