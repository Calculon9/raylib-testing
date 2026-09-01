/**********************************************************************************************
*
COLOUR MODULE
*
**********************************************************************************************/
#ifndef COLOUR_H
#define COLOUR_H

#include "common/common.h"

// Semantic roles for colours used by game-world rendering and diagnostics.
typedef struct GamePalette
{
    ColourRgba dark;
    ColourRgba light;
    ColourRgba muted;
    ColourRgba primary;
    ColourRgba secondary;
    ColourRgba axis_x;
    ColourRgba axis_y;
} GamePalette;

extern const GamePalette game_default_palette;

#endif
