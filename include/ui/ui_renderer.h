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
