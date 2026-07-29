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
    LPANEL_EDIT_ENTITY_VIEW,
    RPANEL_STATE_VIEW,
    RPANEL_WORLD_CREATE_VIEW,
} ViewType;

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
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

    // SELECTION STATE (previously in WorldState)
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
