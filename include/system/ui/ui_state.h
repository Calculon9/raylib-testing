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
typedef struct EntityCreateParams EntityCreateParams;
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

    // STATS UI
    String64 *stats_polygs_str;
    String64 *stats_fps_str;
    String64 *stats_ftime_str;
    String64 *stats_mem_str;

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
    EntityCreateParams *entity_create_params;
    Cell *selected_cell;
    int selected_cell_index;
} UIState;

//----------------------------------------------------------------------------------
// Global Variables Declaration
//----------------------------------------------------------------------------------
extern UIState G_UIState;

#endif
