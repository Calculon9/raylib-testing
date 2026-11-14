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
// Module Functions Declaration
//----------------------------------------------------------------------------------
// Draw a whole UI tree: `seed_box` is the root's box in UI space and `M_ui_to_pixel` maps it to
// the screen. Every other draw helper is private to ui_renderer.c.
void DrawRootUIElement(UIElement *root_element, UIBox seed_box, Matrix3x3 M_ui_to_pixel);

#endif
