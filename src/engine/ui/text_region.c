/**********************************************************************************************

 **********************************************************************************************/
#include "math/cvectors.h"
#include "common/common.h"
#include "system/draw_primitives.h"
#include "ui/text_region.h"
#include "raylib.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
void DrawChar(char c, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour);

// Custom text drawing function that uses our Bitmap_Font and supports scaling and color. Coordinate origin is the top-left corner of the text in pixels.
float DrawTextCustom(const char *text, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour)
{
    Vector2d char_coords = origin_coords;
    while (*text)
    {
        DrawChar(*text, char_coords, scale, font, colour);

        // Move to the next character slot using the font's glyph advance.
        char_coords.x += BitmapFont_GetGlyphAdvance(&font);
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
                DrawRectangle(origin_coords.x + (cell_col * scale), origin_coords.y + (row * scale), scale, scale, ToRaylibColor(colour));
            }
        }
    }
}

// Draw a single bitmap character with pixel-level clipping against a UIClipRect.
void DrawCharClipped(char c, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour, UIClipRect clip)
{
    // Cast to unsigned to handle extended ASCII safely.
    unsigned char u_c = (unsigned char)c;
    int bitmap_width = font.bitmap_width > 0 && font.bitmap_width <= 8 ? font.bitmap_width : 8;
    int bitmap_offset = (8 - bitmap_width) / 2;
    Color ray_colour = ToRaylibColor(colour);

    for (int row = 0; row < 8; row++)
    {
        float pixel_y = origin_coords.y + (float)(row * scale);
        // Skip entire row of scaled pixels if vertically outside clip.
        if (pixel_y + (float)scale <= clip.min.y || pixel_y >= clip.max.y)
        {
            continue;
        }

        unsigned char row_data = (unsigned char)(font.bitmap[u_c][row] << font.bitmap_shift);

        for (int col = 0; col < bitmap_width; col++)
        {
            int cell_col = bitmap_offset + col;
            if (row_data & (0x80 >> cell_col))
            {
                float pixel_x = origin_coords.x + (float)(cell_col * scale);
                // Skip pixel if horizontally outside clip.
                if (pixel_x + (float)scale <= clip.min.x || pixel_x >= clip.max.x)
                {
                    continue;
                }

                // If pixel is fully enclosed by the clip rectangle, draw directly.
                if (pixel_x >= clip.min.x && pixel_x + (float)scale <= clip.max.x &&
                    pixel_y >= clip.min.y && pixel_y + (float)scale <= clip.max.y)
                {
                    DrawRectangle((int)pixel_x, (int)pixel_y, scale, scale, ray_colour);
                }
                else
                {
                    // Pixel partially straddles the boundary: clamp to clip rectangle.
                    float cx1 = fmaxf(pixel_x, clip.min.x);
                    float cy1 = fmaxf(pixel_y, clip.min.y);
                    float cx2 = fminf(pixel_x + (float)scale, clip.max.x);
                    float cy2 = fminf(pixel_y + (float)scale, clip.max.y);
                    if (cx2 > cx1 && cy2 > cy1)
                    {
                        DrawRectangleRec((Rectangle){cx1, cy1, cx2 - cx1, cy2 - cy1}, ray_colour);
                    }
                }
            }
        }
    }
}

// Custom text drawing function supporting pixel-level clipping against a UIClipRect.
float DrawTextCustomClipped(const char *text, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour, UIClipRect clip)
{
    Vector2d char_coords = origin_coords;
    int glyph_width = BitmapFont_GetGlyphCellWidth(&font);
    int row_height = BitmapFont_GetRowHeight(&font);
    int advance = BitmapFont_GetGlyphAdvance(&font);

    while (*text)
    {
        float char_right = char_coords.x + (float)glyph_width;
        float char_bottom = char_coords.y + (float)row_height;

        // Draw only if the glyph bounding box overlaps the clipping rectangle.
        if (char_right > clip.min.x && char_coords.x < clip.max.x &&
            char_bottom > clip.min.y && char_coords.y < clip.max.y)
        {
            // If the character is fully inside the clip rectangle, use fast unclipped drawing.
            if (char_coords.x >= clip.min.x && char_right <= clip.max.x &&
                char_coords.y >= clip.min.y && char_bottom <= clip.max.y)
            {
                DrawChar(*text, char_coords, scale, font, colour);
            }
            else
            {
                DrawCharClipped(*text, char_coords, scale, font, colour, clip);
            }
        }

        // Advance character position regardless of clipping so layout spacing remains consistent.
        char_coords.x += (float)advance;
        text++;
    }
    return char_coords.x - (scale * font.spacing);
}

