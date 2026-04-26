/**********************************************************************************************

 **********************************************************************************************/
#include <stdio.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "ui/ui.h"
#include "raylib.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
void DrawChar(char c, float x, float y, int scale, Bitmap_Font font, ColourRgba colour);

// Takes in pixel coord point and determines if they are in the target region
bool IsFocused(Vector2d pixel_coords, Vector2d *vertices, int vertex_count)
{
    // Vector2d *vertices = polygon->vertices.coll.items;
    return IsPointInPolygon(pixel_coords, vertices, vertex_count);
}

TextField CreateTextField(float width, float height, Vector2d origin_coords, Vector2d padding_inner, Vector2d padding_outer, ColourRgba colour_border_outer, ColourRgba colour_fill_outer, ColourRgba colour_border_inner, ColourRgba colour_fill_inner)
{
    TextField t = {0};
    t.origin = origin_coords;
    t.colour_fill_outer = colour_fill_outer;
    t.colour_fill_inner = colour_fill_inner;
    t.colour_border_outer = colour_border_outer;
    t.colour_border_inner = colour_border_inner;
    t.width = width;
    t.height = height;
    t.padding_inner = padding_inner;
    t.padding_outer = padding_outer;
    t.is_focused = false;
    t.is_read_only = false;
    t.cursor_pos = 0;
    TextFieldsContainer *parent; // The "Backlink" for bubbling up changes to the text field container (e.g., for re-rendering when text changes)

    // Allocate memory for the text buffer
    // t.text = calloc(64, sizeof(char));

    return t;
}

TextFieldsContainer CreateTextFieldContainer(float width, float height, Vector2d origin_coords, Vector2d padding_inner, Vector2d padding_outer, ColourRgba colour_border_outer, ColourRgba colour_fill_outer, ColourRgba colour_border_inner, ColourRgba colour_fill_inner)
{
    TextFieldsContainer t = {0};
    t.origin = origin_coords;
    t.colour_fill_outer = colour_fill_outer;
    t.colour_fill_inner = colour_fill_inner;
    t.colour_border_outer = colour_border_outer;
    t.colour_border_inner = colour_border_inner;
    t.width = width;
    t.height = height;
    t.padding_inner = padding_inner;
    t.padding_outer = padding_outer;

    return t;
}

ShortString GetText_TextField(TextField *text_box)
{
    // ShortString str[64] = {0};
    // strncpy(str, text_box->text, sizeof(str) - 1); // Copy text with safety check to prevent overflow

    // return str;
}

int MeasureTextWidth(const char *text, char font_spacing, char scale)
{
    if (text == NULL)
        return 0;

    int char_count = strlen(text);
    if (char_count == 0)
        return 0;

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

float DrawTextCustom(const char *text, float origin_x, float origin_y, char scale, Bitmap_Font font, ColourRgba colour)
{
    float current_x = origin_x;
    while (*text)
    {
        DrawChar(*text, current_x, origin_y, scale, font, colour);

        // Move to the next character slot
        // 8 pixels wide * scale + 1 pixel of "letter spacing"
        current_x += (8 * scale) + (scale * font.spacing); // - (scale * scale) + 1; // Subtracting "scale" removes embedded white-space in each character
        text++;
    }
    return current_x;
}

void DrawChar(char c, float origin_x, float origin_y, int scale, Bitmap_Font font, ColourRgba colour)
{
    // Cast to unsigned to handle extended ASCII safely
    unsigned char u_c = (unsigned char)c;

    for (int row = 0; row < 8; row++)
    {
        unsigned char row_data = font.bitmap[u_c][row];

        for (int col = 0; col < 8; col++)
        {
            // We use a bitmask (0x80 is 10000000) and shift it right to check each bit in the byte.
            // The >> is the Bitwise Right Shift. It literally pushes the bits to the right by the number of places specified by col.
            if (row_data & (0x80 >> col))
            {
                // If the bit is 1, draw a "pixel" scaled up
                DrawRectangle(origin_x + (col * scale), origin_y + (row * scale), scale, scale, (Color){colour.r, colour.g, colour.b, colour.a});
            }
        }
    }
}

Vector2d *GetTextFieldVertices(TextField text_box)
{
    Vector2d *vertices = calloc(4, sizeof(Vector2d));

    vertices[0] = text_box.origin;
    vertices[1] = (Vector2d){text_box.origin.x + text_box.width, text_box.origin.y};
    vertices[2] = (Vector2d){text_box.origin.x + text_box.width, text_box.origin.y + text_box.height};
    vertices[3] = (Vector2d){text_box.origin.x, text_box.origin.y + text_box.height};

    return vertices;
}