/**********************************************************************************************
*
TEXT REGION MODULE
*
**********************************************************************************************/
#ifndef TEXT_REGION_H
#define TEXT_REGION_H
#include "common/common.h"
#include "ui/cfont.h"
#include "ui/ui.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
float DrawTextCustom(const char *text, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour);
float DrawTextCustomClipped(const char *text, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour, UIClipRect clip);
void DrawCharClipped(char c, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour, UIClipRect clip);

#endif
