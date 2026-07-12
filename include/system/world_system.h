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
#include "system/systems.h"
#include "world/universe.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
// Panel Colour Pallette
#define COLOUR_WORLD_DARK_1 OLIVE_GARDEN_GREEN_D // BROWN_1_RGBA
// #define COLOUR_PANEL_MID_1 BEIGE_RGBA
#define COLOUR_WORLD__XIGHT_1 OLIVE_GARDEN_GREEN_XL // BROWN_1_RGBA_4
#define COLOUR_WORLD__LIGHT_1 OLIVE_GARDEN_GREEN_L // BROWN_1_RGBA_4

#define COLOUR_WORLD__LIGHT_3 OLIVE_GARDEN_CREAM

#define COLOUR_WORLD__DARK_2 OLIVE_GARDEN_TAN_D // DARKBROWN_RGBA
// #define COLOUR_PANEL_MID_2 BROWN_2_RGBA_1
#define COLOUR_WORLD_LIGHT_2 OLIVE_GARDEN_TAN_L // BROWN_2_RGBA_4

#define COLOUR_ERROR RED_ERROR_RGBA
#define COLOUR_WARNING YELLOW_WARNING_RGBA
//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------



// Game-region placement in logical units inside the full screen layout.
extern Vector2d game_region_origin, game_region_end;// = {0};
extern Vector2d game_region_resolution;// = {0};

// Game viewport in pixel-space (destination area for world/universe rendering).
extern Vector2d game_viewport_origin, game_viewport_end;// = {0};
extern Vector2d game_viewport_u;// = {75, 0};
extern Vector2d game_viewport_v;// = {0, 75};
//extern Camera2d camera_world;// = {0};

extern bool world_grid_debug_labels_enabled;

// typedef struct {
//     Texture *texture;
// } UIImageData;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
//void *GetEntityByID(WorldState *context, int entity_id);
//UIElement *CreateTextField(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d label_tbox_offset, Vector2d label_tbox_padding, char max_label_chars, char max_text_box_chars);
void ProcessCommandQueue(void);
#endif