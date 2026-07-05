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



// Coordinate Space Properties
extern Vector2d world_origin, world_end;// = {0};
extern Vector2d world_u;// = {1, 0};
extern Vector2d world_v;// = {0, 1};
extern Vector2d world_resolution;// = {0};

// Logical->pixel-space conversion properties
extern Vector2d universe_viewport_origin, universe_viewport_end;// = {0};
extern Vector2d universe_viewport_u;// = {75, 0};
extern Vector2d universe_viewport_v;// = {0, 75};
extern Camera2d camera_world;// = {0};
extern Vector2d local_to_world_scale;
extern Vector2d world_to_local_scale;

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
int CreateNewWorldDefault(void);
bool SelectWorldByIndex(int index);
int GetWorldCount(void);
int GetSelectedWorldIndex(void);
World2d *GetSelectedWorld(void);
World2d *GetWorldByIndex(int index);
Vector2d *GetNextWorldSpawnOriginPtr(void);
void SetNextWorldSpawnOrigin(Vector2d origin);
Vector2d *GetNextWorldResolutionPtr(void);
Vector2d GetUniverseCameraOffset(void);
float *GetNextWorldGravityPtr(void);
#endif