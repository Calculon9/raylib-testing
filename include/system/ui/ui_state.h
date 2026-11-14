/**********************************************************************************************
*
UI STATE MODULE
*
**********************************************************************************************/
#ifndef UI_STATE_H
#define UI_STATE_H

#include "common/common.h"
#include "world/target_resolver.h"

//----------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------
typedef struct UIElement UIElement;
typedef struct Newtonoid2d Newtonoid2d;
typedef struct EntityCreateParams EntityCreateParams;
typedef struct Cell Cell;

typedef enum
{
    WORLD_MANAGER_PROPS_VIEW,
    WORLD_MANAGER_FLAGS_VIEW,
    WORLD_MANAGER_GRID_VIEW,
    WORLD_MANAGER_NEW_VIEW,
    WORLD_MANAGER_CELL_VIEW,
    STATE_MANAGER_PHYSICS_VIEW,
    STATE_MANAGER_ATTRIBUTES_VIEW,
    POPUP_MENU_CREATE_VIEW,
    POPUP_MENU_RECENT_VIEW,
    // Entity Mgmt ancestry view: the state-manager's own read-only ancestry chain and
    // relation display (the fifth view). Appended LAST so no existing enumerator value shifts.
    STATE_MANAGER_ANCESTRY_VIEW,
    // Entity Mgmt components view: the attach/remove component manager (Phase 1). Appended
    // LAST so no existing enumerator value shifts.
    STATE_MANAGER_COMPONENTS_VIEW,
    // Entity Mgmt geometry view: read-only listing of the selected entity's surface vectors
    // (vertex count + per-vertex local vectors). Appended LAST so no existing enumerator value
    // shifts.
    STATE_MANAGER_GEOMETRY_VIEW
} ViewType;

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct
{
    UIElement *focused_element;

    // Deepest enabled UI element under the pointer this frame, or NULL when the pointer is not
    // over the UI. Cleared and rewritten at the start of every ProcessUIInput call, so it is
    // never carried across frames. Read-only for consumers such as the detail strip's hover hints.
    UIElement *hovered_element;

    // ENTITY CREATION UI
    String64 *create_id_str;
    String64 *create_edges_str;
    String64 *create_mass_str;
    // String64 *create_pos_tl_str;
    String64 *create_pos_c_str;
    String64 *create_vel_str;
    String64 *create_accel_str;
    String64 *create_moment_str;

    // Type of the most recently shown view in any panel (written by the view host, read by the geometry-editor log).
    ViewType active_view;

    // SELECTION STATE (previously in WorldState)
    EntityId selected_object_id;
    Newtonoid2d *selected_object;
    EntityCreateParams *entity_create_params;
    Cell *selected_cell;
    int selected_cell_index;
    // Central mixed selection: TargetRef entries (entity/world/cell). Source of truth for
    // multi-selection; the single-valued fields above mirror its primary (last-added) member.
    LArray selection_set;
} UIState;

//----------------------------------------------------------------------------------
// Global Variables Declaration
//----------------------------------------------------------------------------------
extern UIState G_UIState;

#endif
