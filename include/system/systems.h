/**********************************************************************************************
*
SYSTEMS MODULE
*
**********************************************************************************************/
#ifndef SYSTEMS_H
#define SYSTEMS_H
#include "common/common.h"
#include "math/coordinate_space.h"
#include "colour/colour.h"
#include "raylib.h"
#include "system/utility_system.h"
#include "physics/polygonoid.h"
//#include "ui/ui.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// typedef struct String32
// {
//     char string[32];
// } String32;

// typedef struct String64
// {
//     char string[64];
// } String64;

// typedef struct String128
// {
//     char string[128];
// } String128;

// typedef struct String256
// {
//     char string[256];
// } String256;

typedef enum
{
    FLOAT,
    VECTOR2D,
    STRING64,
    STRING128,
    STRING256
} DatatType;

typedef struct UIElement UIElement;

typedef struct
{
    // OBJECT PROPERTIES UI
    UIElement *lpanel_entity_state_id_tbox;
    String64 *lpanel_entity_state_id_str;
    UIElement *lpanel_entity_state_mass_tbox;
    String64 *lpanel_entity_state_mass_str;
    UIElement *lpanel_entity_state_pos_tbox;
    String64 *lpanel_entity_state_pos_str;
    UIElement *lpanel_entity_state_vel_tbox;
    String64 *lpanel_entity_state_vel_str;
    UIElement *lpanel_entity_state_accel_tbox;
    String64 *lpanel_entity_state_accel_str;
    UIElement *lpanel_entity_state_moment_tbox;
    String64 *lpanel_entity_state_moment_str;

    // STATS UI
    String64 *lpanel_stats_polygs_str;
    String64 *lpanel_stats_fps_str;
    String64 *lpanel_stats_mem_str;

    // CELL STATE UI
    String64 *lpanel_cell_state_index_str;
    String64 *lpanel_cell_state_occu_str;
    String64 *lpanel_cell_state_value_str;
    String64 *lpanel_cell_state_fill_str;

} UIState;

typedef struct
{
    CoordSpace2d_Grid *world_coord_space;
    Cell *selected_cell;
    Polygonoid *selected_object;

} WorldState;
//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

extern Font font;
extern FrameCounter frame_counter;
extern int screen_resolution_scalar;
extern size_t memory_allocated;
extern const int screenWidth;  // = 1920;
extern const int screenHeight; // = 1080;
extern UIState G_UIState;
extern WorldState G_WorldState;
//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
// Utility Functions Declaration
//----------------------------------------------------------------------------------
void InitUtilities();
FrameCounter InitFrameCounter();
void UpdateUtilities();
void UpdateFrameCounter(FrameCounter *fc);
size_t GetCurrentMemoryAllocated();

//----------------------------------------------------------------------------------
// UI Functions Declaration
//----------------------------------------------------------------------------------
void InitUI(void);
void InitPanelSpace(void);
void UpdateUISystem(int mouse_x, int mouse_y);
void DrawUI(void);
void ProcessUIInput(int mouse_x, int mouse_y, bool cursor_in_region);

//----------------------------------------------------------------------------------
// World Functions Declaration
//----------------------------------------------------------------------------------
void UpdateWorldSystem(int mouse_x, int mouse_y);
int GetPolygonoidCount(void);
void InitGameWorld(void);
void DrawGameWorld(void);

//----------------------------------------------------------------------------------
// Integration Functions Declaration
//----------------------------------------------------------------------------------
bool PipelineTextToVector(char *input_buffer, Vector2d *target_vector);
bool PipelineTextToFloat(char *input_buffer, float *target_float);
void PipelineVectorToText(Vector2d input_vector, char *target_buffer, size_t target_buffer_bytes);//, NewtonProperty object_property);
void PipelineNumberToText(float input_float, int precision, char *target_buffer, size_t target_buffer_bytes);//, NewtonProperty object_property);
#endif