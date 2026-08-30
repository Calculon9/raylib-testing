/**********************************************************************************************
*
UI STATE MODULE
*
**********************************************************************************************/
#ifndef UI_STATE_H
#define UI_STATE_H

#include "common/common.h"

//----------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------
typedef struct UIElement UIElement;
typedef struct Newtonoid2d Newtonoid2d;
typedef struct Newtonoid2dParams Newtonoid2dParams;
typedef struct Cell Cell;

typedef enum
{
    LPANEL_STATE_VIEW,
    // The lpanel draw view also enables geometry editing in the game viewport.
    LPANEL_DRAW_VIEW,
    RPANEL_STATE_VIEW,
    RPANEL_WORLD_CREATE_VIEW,
    STATE_MANAGER_PHYSICS_VIEW,
    STATE_MANAGER_ATTRIBUTES_VIEW,
    STATE_MANAGER_WORLD_VIEW,
    STATE_MANAGER_CELL_STATE_VIEW,
    POPUP_MENU_CREATE_VIEW,
    POPUP_MENU_RECENT_VIEW
} ViewType;

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct
{
    UIElement *focused_element;
    // OBJECT PROPERTIES UI
    UIElement *state_id_tbox;
    String64 *state_id_str;
    UIElement *state_mass_tbox;
    String64 *state_mass_str;
    UIElement *state_restitution_tbox;
    String64 *state_restitution_str;
    UIElement *state_friction_tbox;
    String64 *state_friction_str;
    UIElement *state_world_restitution_tbox;
    UIElement *state_world_friction_tbox;
    UIElement *state_pos_tl_tbox;
    String64 *state_pos_tl_str;
    UIElement *state_pos_c_tbox;
    String64 *state_pos_c_str;
    UIElement *state_vel_tbox;
    String64 *state_vel_str;
    UIElement *state_accel_tbox;
    String64 *state_accel_str;
    UIElement *state_moment_tbox;
    String64 *state_moment_str;
    UIElement *state_angular_velocity_tbox;
    String64 *state_angular_velocity_str;
    UIElement *state_angular_acceleration_tbox;
    String64 *state_angular_acceleration_str;
    UIElement *state_health_tbox;
    String64 *state_health_str;
    UIElement *state_max_health_tbox;
    String64 *state_max_health_str;
    UIElement *state_damage_tbox;
    String64 *state_damage_str;
    String64 *state_world_str;

    // STATS UI
    String64 *stats_polygs_str;
    String64 *stats_fps_str;
    String64 *stats_ftime_str;
    String64 *stats_mem_str;

    // CELL STATE UI
    String64 *cell_id_str;
    String64 *cell_occu_str;
    String64 *cell_value_str;
    String64 *cell_fill_str;

    // ENTIY EDITOR UI
    // UIElement *edit_id_tbox;
    // String64 *edit_id_str;
    UIElement *edit_edge_count_tbox;
    String64 *edit_edge_count_str;
    UIElement *edit_vertice_count_tbox;
    String64 *edit_vertice_count_str;
    UIElement *edit_width_tbox;
    String64 *edit_width_str;
    UIElement *edit_height_tbox;
    String64 *edit_height_str;
    UIElement *edit_mass_tbox;
    String64 *edit_mass_str;
    UIElement *edit_restitution_tbox;
    String64 *edit_restitution_str;
    UIElement *edit_friction_tbox;
    String64 *edit_friction_str;
    // UIElement *edit_pos_tl_tbox;
    // String64 *edit_pos_tl_str;
    UIElement *edit_pos_c_tbox;
    String64 *edit_pos_c_str;
    UIElement *edit_vel_tbox;
    String64 *edit_vel_str;
    UIElement *edit_accel_tbox;
    String64 *edit_accel_str;
    UIElement *edit_moment_tbox;
    String64 *edit_moment_str;

    // ENTITY CREATION UI
    String64 *create_id_str;
    String64 *create_edges_str;
    String64 *create_mass_str;
    // String64 *create_pos_tl_str;
    String64 *create_pos_c_str;
    String64 *create_vel_str;
    String64 *create_accel_str;
    String64 *create_moment_str;

    // VIEWS
    LArray *lpanel_views;
    ViewType active_panel_view;

    // SELECTION STATE (previously in WorldState)
    EntityId selected_object_id;
    Newtonoid2d *selected_object;
    Newtonoid2dParams *newtonoid_params;
    Cell *selected_cell;
    int selected_cell_index;
} UIState;

//----------------------------------------------------------------------------------
// Global Variables Declaration
//----------------------------------------------------------------------------------
extern UIState G_UIState;

#endif
