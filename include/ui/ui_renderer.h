/**********************************************************************************************
*
UI SYSTEM MODULE
*
**********************************************************************************************/
#ifndef UI_RENDERER_H
#define UI_RENDERER_H
#include "common/common.h"
#include "math/cvectors.h"
#include "ui/cfont.h"
#include "camera/camera.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

#define FONT_DEFAULT FONT_BASIC

// Panel Colour Pallette 
#define COLOUR_PANEL_DARK_1  OLIVE_GARDEN_GREEN_D//BROWN_1_RGBA
//#define COLOUR_PANEL_MID_1 BEIGE_RGBA
#define COLOUR_PANEL_LIGHT_1 OLIVE_GARDEN_GREEN_L//BROWN_1_RGBA_4

#define COLOUR_PANEL_LIGHT_3 OLIVE_GARDEN_CREAM

#define COLOUR_PANEL_DARK_2 OLIVE_GARDEN_TAN_D//DARKBROWN_RGBA
//#define COLOUR_PANEL_MID_2 BROWN_2_RGBA_1
#define COLOUR_PANEL_LIGHT_2 OLIVE_GARDEN_TAN_L//BROWN_2_RGBA_4

#define COLOUR_ERROR RED_ERROR_RGBA
#define COLOUR_WARNING YELLOW_WARNING_RGBA


//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------


//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
void DrawElementBox(UIElement *e);
void DrawTextArea(UIElement *e);
void DrawRootUIElement(UIElement *root_element, UIBox seed_box, Matrix3x3 M_ui_to_pixel);
//void UpdateUISystem(int mouse_x, int mouse_y);

#endif
