/**********************************************************************************************
*
UI SYSTEM MODULE
*
**********************************************************************************************/
#ifndef SCREEN_H
#define SCREEN_H
#include "common/common.h"
#include "colour/colour.h"
#include "math/cvectors.h"
#include "ui/cfont.h"
#include "camera/camera.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

#define FONT_DEFAULT FONT_BASIC



//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Coordinate Space Properties
extern const int screenWidth;// = 1920;
extern const int screenHeight;// = 1080;

// Logical->pixel-space conversion properties
// extern Vector2d lpanel_pixel_u;// = {75, 0};
// extern Vector2d lpanel_pixel_v;// = {0, 75};
// extern Vector2d lpanel_origin, lpanel_end; //= {0}; // Dependent on the game world screen area
// extern Camera2d camera_lpanel;// = {0};


//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
//void UpdateUISystem(int mouse_x, int mouse_y);

#endif