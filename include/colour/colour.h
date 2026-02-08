/**********************************************************************************************
*
COLOURS MODULE
*
**********************************************************************************************/
#ifndef COLOUR_H
#define COLOUR_H
#include <stddef.h>

// Some Basic Colors
#define LIGHTGRAY_RGBA  { 200, 200, 200, 255 }   // Light Gray
#define GRAY_RGBA       { 130, 130, 130, 255 }   // Gray
#define DARKGRAY_RGBA   { 80, 80, 80, 255 }      // Dark Gray
#define YELLOW_RGBA     { 253, 249, 0, 255 }     // Yellow
#define GOLD_RGBA       { 255, 203, 0, 255 }     // Gold
#define ORANGE_RGBA     { 255, 161, 0, 255 }     // Orange
#define PINK_RGBA       { 255, 109, 194, 255 }   // Pink
#define RED_RGBA        { 230, 41, 55, 255 }     // Red
#define MAROON_RGBA     { 190, 33, 55, 255 }     // Maroon
#define GREEN_RGBA      { 0, 228, 48, 255 }      // Green
#define LIME_RGBA       { 0, 158, 47, 255 }      // Lime
#define DARKGREEN_RGBA  { 0, 117, 44, 255 }      // Dark Green
#define SKYBLUE_RGBA    { 102, 191, 255, 255 }   // Sky Blue
#define BLUE_RGBA       { 0, 121, 241, 255 }     // Blue
#define DARKBLUE_RGBA   { 0, 82, 172, 255 }      // Dark Blue
#define PURPLE_RGBA     { 200, 122, 255, 255 }   // Purple
#define VIOLET_RGBA     { 135, 60, 190, 255 }    // Violet
#define DARKPURPLE_RGBA { 112, 31, 126, 255 }    // Dark Purple
#define BEIGE_RGBA      { 211, 176, 131, 255 }   // Beige
#define BROWN_RGBA      { 127, 106, 79, 255 }    // Brown
#define DARKBROWN_RGBA  { 76, 63, 47, 255 }      // Dark Brown

#define WHITE_RGBA      { 255, 255, 255, 255 }   // White
#define BLACK_RGBA      { 0, 0, 0, 255 }         // Black
#define BLANK_RGBA      { 0, 0, 0, 0 }           // Blank (Transparent)
#define MAGENTA_RGBA    { 255, 0, 255, 255 }     // Magenta

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
//#define NEW_COLOUR_RGBA(count, type) allocate_array(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef union
{
    struct
    {
        unsigned char r, g, b, a;
    };
    unsigned int rgba; // Access the whole thing at once
} ColourRgba;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

//void *allocate_array(size_t element_count, size_t element_bytes);

#endif