/**********************************************************************************************
*
FONT MODULE
*
**********************************************************************************************/
#ifndef CFONT_H
#define CFONT_H
#include "common/common.h"
#include "colour/colour.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define FONT_BASIC (Bitmap_Font){.bitmap = font8x8_bitmap_basic, .spacing = -2, .scale = 2, .colour = BLACK_RGBA}
#define FONT_BASIC_S (Bitmap_Font){.bitmap = font8x8_bitmap_basic, .spacing = -1, .scale = 1, .colour = BLACK_RGBA}

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct
{
    unsigned char (*bitmap)[8]; // Pointer to the bitmap data for the font
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
extern uint8_t font8x8_bitmap_basic[128][8];
extern uint8_t bitmap_square[8];
extern uint8_t bitmap_circle[8];
extern uint8_t bitmap_triangle[8];
extern uint8_t bitmap_cross[8];
//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

#endif