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
#include "camera/camera.h"
#include "raylib.h"
#include "system/utility_system.h"
// #include "ui/ui.h"

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
    INT,
    VECTOR2D,
    STRING64,
    STRING128,
    STRING256
} DataType;

typedef enum
{
    EDITING,
    RUNNING,
    PAUSED,
} WorldMode;

typedef enum
{
    LPANEL_STATE_VIEW,
    LPANEL_EDIT_ENTITY_VIEW,
} ViewType;

typedef struct UIElement UIElement;
typedef struct World2d World2d;

typedef struct
{
    UIElement *focused_element;
    // OBJECT PROPERTIES UI
    UIElement *lpanel_entity_state_id_tbox;
    String64 *lpanel_entity_state_id_str;
    UIElement *lpanel_entity_state_mass_tbox;
    String64 *lpanel_entity_state_mass_str;
    UIElement *lpanel_entity_state_pos_tl_tbox;
    String64 *lpanel_entity_state_pos_tl_str;
    UIElement *lpanel_entity_state_pos_c_tbox;
    String64 *lpanel_entity_state_pos_c_str;
    UIElement *lpanel_entity_state_vel_tbox;
    String64 *lpanel_entity_state_vel_str;
    UIElement *lpanel_entity_state_accel_tbox;
    String64 *lpanel_entity_state_accel_str;
    UIElement *lpanel_entity_state_moment_tbox;
    String64 *lpanel_entity_state_moment_str;

    // STATS UI
    String64 *lpanel_stats_polygs_str;
    String64 *lpanel_stats_fps_str;
    String64 *lpanel_stats_ftime_str;
    String64 *lpanel_stats_mem_str;

    // CELL STATE UI
    String64 *lpanel_cell_state_id_str;
    String64 *lpanel_cell_state_occu_str;
    String64 *lpanel_cell_state_value_str;
    String64 *lpanel_cell_state_fill_str;

    // ENTIY EDITOR UI
    // UIElement *lpanel_entity_edit_id_tbox;
    // String64 *lpanel_entity_edit_id_str;
    UIElement *lpanel_entity_edit_edge_count_tbox;
    String64 *lpanel_entity_edit_edge_count_str;
    UIElement *lpanel_entity_edit_vertice_count_tbox;
    String64 *lpanel_entity_edit_vertice_count_str;
    UIElement *lpanel_entity_edit_width_tbox;
    String64 *lpanel_entity_edit_width_str;
    UIElement *lpanel_entity_edit_height_tbox;
    String64 *lpanel_entity_edit_height_str;
    UIElement *lpanel_entity_edit_mass_tbox;
    String64 *lpanel_entity_edit_mass_str;
    // UIElement *lpanel_entity_edit_pos_tl_tbox;
    // String64 *lpanel_entity_edit_pos_tl_str;
    UIElement *lpanel_entity_edit_pos_c_tbox;
    String64 *lpanel_entity_edit_pos_c_str;
    UIElement *lpanel_entity_edit_vel_tbox;
    String64 *lpanel_entity_edit_vel_str;
    UIElement *lpanel_entity_edit_accel_tbox;
    String64 *lpanel_entity_edit_accel_str;
    UIElement *lpanel_entity_edit_moment_tbox;
    String64 *lpanel_entity_edit_moment_str;

    // ENTITY CREATION UI
    String64 *lpanel_edit_entity_id_str;
    String64 *lpanel_edit_entity_edges_str;
    String64 *lpanel_edit_entity_mass_str;
    // String64 *lpanel_edit_entity_pos_tl_str;
    String64 *lpanel_edit_entity_pos_c_str;
    String64 *lpanel_edit_entity_vel_str;
    String64 *lpanel_edit_entity_accel_str;
    String64 *lpanel_edit_entity_moment_str;

    // VIEWS
    LArray *lpanel_views;
    ViewType active_panel_view;
} UIState;

typedef struct
{
    Newtonoid2d *selected_object;
    Newtonoid2dParams *newtonoid_params;
    // CoordSpace2d_Grid *world_coord_space;
    Cell *selected_cell;
    World2d *world;
    FlatMapInt *entity_world_index_registry;
    LArray *collisions;
    WorldMode mode;
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
void *GetEntityByID(WorldState *context, int entity_id);
//----------------------------------------------------------------------------------
// UI Functions Declaration
//----------------------------------------------------------------------------------
void InitUI(void);
void InitPanel(void);
void UpdateUISystem(int mouse_x, int mouse_y);
void DrawUI(void);
void ProcessUIInput(int mouse_x, int mouse_y, bool cursor_in_region);
void HandleBtnSwitchClick(UIElement *target);
void HandleBtnEnumerateClick(UIElement *btn);
void HandleBtnSubmitClick(UIElement *btn);
//----------------------------------------------------------------------------------
// World Functions Declaration
//----------------------------------------------------------------------------------
void UpdateWorldSystem(int mouse_x, int mouse_y);
int GetNewtonoidCount(void);
void InitGameWorld(void);
void DrawGameWorld(void);
void DrawWorldRegion(World2d *world, Camera2d *world_camera);
Newtonoid2d *ResolveEntityParamsToEntity(Newtonoid2dParams *newtonoid_params);
//----------------------------------------------------------------------------------
// Integration Functions Declaration
//----------------------------------------------------------------------------------
bool PipelineTextToVector(char *input_buffer, Vector2d *target_vector);
bool PipelineTextToFloat(char *input_buffer, float *target_float);
bool PipelineTextToInt(char *input_buffer, int *target_int);
void PipelineVectorToText(Vector2d input_vector, char *target_buffer, size_t target_buffer_bytes);            //, NewtonProperty object_property);
void PipelineNumberToText(float input_float, int precision, char *target_buffer, size_t target_buffer_bytes); //, NewtonProperty object_property);
#endif