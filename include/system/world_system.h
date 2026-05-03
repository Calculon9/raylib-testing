/**********************************************************************************************
*
WORLD MODULE
*
**********************************************************************************************/
#ifndef WORLD_SYSTEM_H
#define WORLD_SYSTEM_H
#include "common/common.h"
#include "math/cvectors.h"
#include "camera/camera.h"


//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
// Coordinate Space Properties
extern Vector2d world_origin, world_end;// = {0};

extern Vector2d world_u;// = {1, 0};
extern Vector2d world_v;// = {0, 1};
extern Vector2d world_resolution;// = {0};

// Logical->pixel-space conversion properties
extern Vector2d world_pixel_origin, world_pixel_end;// = {0};
extern Vector2d world_pixel_u;// = {75, 0};
extern Vector2d world_pixel_v;// = {0, 75};
extern Camera2d camera_world;// = {0};
//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------




// typedef struct {
//     Texture *texture;
// } UIImageData;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

//UIElement *CreateTextField(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d label_tbox_offset, Vector2d label_tbox_padding, char max_label_chars, char max_text_box_chars);
#endif