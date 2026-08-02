/**********************************************************************************************
*
FONT MODULE
*
**********************************************************************************************/
#ifndef CFONT_H
#define CFONT_H
#include "common/common.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define FONT_BASIC (Bitmap_Font){.bitmap = font8x8_bitmap_basic_6x7, .bitmap_width = 7, .bitmap_shift = 0, .spacing = -2, .scale = 2, .colour = COLOUR_UI_INK_RGBA}
#define FONT_BASIC_WITH_COLOUR(text_colour) (Bitmap_Font){.bitmap = font8x8_bitmap_basic_6x7, .bitmap_width = 7, .bitmap_shift = 0, .spacing = -2, .scale = 2, .colour = (text_colour)}
#define FONT_MEDIUM (Bitmap_Font){.bitmap = font8x8_bitmap_basic_5x6, .bitmap_width = 5, .bitmap_shift = 2, .spacing = -2, .scale = 2, .colour = COLOUR_UI_INK_RGBA}
#define FONT_MEDIUM_WITH_COLOUR(text_colour) (Bitmap_Font){.bitmap = font8x8_bitmap_basic_5x6, .bitmap_width = 5, .bitmap_shift = 2, .spacing = -2, .scale = 2, .colour = (text_colour)}
#define FONT_SMALL (Bitmap_Font){.bitmap = font8x8_bitmap_basic_5x5, .bitmap_width = 5, .bitmap_shift = 2, .spacing = -3, .scale = 2, .colour = COLOUR_UI_INK_RGBA}
#define FONT_SMALL_WITH_COLOUR(text_colour) (Bitmap_Font){.bitmap = font8x8_bitmap_basic_5x5, .bitmap_width = 5, .bitmap_shift = 2, .spacing = -3, .scale = 2, .colour = (text_colour)}
#define FONT_BASIC_S FONT_SMALL

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct
{
    unsigned char (*bitmap)[8]; // Pointer to the bitmap data for the font
    short bitmap_width;          // Logical width of the glyph inside its 8-pixel cell
    short bitmap_shift;          // Shift from the bitmap's packed bit position to the high-bit convention
    float spacing;              // Additional spacing between characters in pixels. Use -ve numbers to remove embedded spacing in bitmap.
    ColourRgba colour;          // Default color for rendering the font
    short scale;            // Base size for scaling (e.g., 8 for an 8x8 font)
    // short width;                // Width of each character in pixels
    // short height;               // Height of each character in pixels
} Bitmap_Font;



//extern Bitmap_Font default_font;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
/* * 8x8 Fixed-Width Bitfont
 * Each index corresponds to the ASCII value of the character.
 * A '1' bit represents a colored pixel, a '0' is transparent.
 */
extern uint8_t font8x8_bitmap_basic_6x7[128][8];
extern uint8_t font8x8_bitmap_basic_5x6[128][8];
extern uint8_t font8x8_bitmap_basic_5x5[128][8];
extern uint8_t font8x8_bitmap_basic_4x5[128][8];
extern uint8_t bitmap_square[8];
extern uint8_t bitmap_circle[8];
extern uint8_t bitmap_triangle[8];
extern uint8_t bitmap_cross[8];
//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

#endif
