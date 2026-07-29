#include "system/rpanel_system.h"

#include <stdio.h>
#include "raylib.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "world/world.h"
#include "world/universe.h"
#include "world/universe.h"
#include "system/viewport_system.h"
#include "system/panel_ui_helpers.h"
#include "system/panel_system.h"
#include "system/utility_system.h"
#include "math/coordinate_space.h"
#include "world/world.h"
#include "ui/text_region.h"
#include "ui/ui_renderer.h"

// ============================================================================
// Panel System
// ============================================================================
static PanelSystem *rpanel = NULL;

// ============================================================================
// Action Codes
// ============================================================================
static int btn_action_rpanel_enumerate = 0;
static int btn_action_create_world = BUTTON_ACTION_CREATE_WORLD;
static int btn_action_select_world_prev = BUTTON_ACTION_SELECT_WORLD_PREV;
static int btn_action_select_world_next = BUTTON_ACTION_SELECT_WORLD_NEXT;

// ============================================================================
// View Storage
// ============================================================================
static View rpanel_state_view_storage = {0};
static View rpanel_create_view_storage = {0};

// ============================================================================
// Visual Style Properties
// ============================================================================
static Vector2d rpanel_tfield_padding = {0.03f, 0.03f};

// ============================================================================
// UI Element Pointers
// ============================================================================
static View *rpanel_state_view = NULL;
static View *rpanel_create_view = NULL;
static UIElement *rpanel_toggle_cont = NULL;
static UIElement *rpanel_state_view_cont = NULL;
static UIElement *rpanel_create_view_cont = NULL;
static LArray rpanel_views = {0};

static UIElement *rpanel_stats_entities_tbox = NULL;
static UIElement *rpanel_world_index_tbox = NULL;
static UIElement *rpanel_world_universe_pos_tbox = NULL;
static UIElement *rpanel_world_gravity_edit_tbox = NULL;
static UIElement *rpanel_world_resolution_tbox = NULL;
static UIElement *rpanel_world_objects_tbox = NULL;
static UIElement *rpanel_world_next_id_tbox = NULL;
static UIElement *rpanel_create_world_count_tbox = NULL;
static UIElement *rpanel_create_selected_world_tbox = NULL;
static UIElement *rpanel_create_spawn_tbox = NULL;
static UIElement *rpanel_create_resolution_tbox = NULL;
static UIElement *rpanel_create_basis_u_tbox = NULL;
static UIElement *rpanel_create_basis_v_tbox = NULL;
static UIElement *rpanel_create_gravity_tbox = NULL;
static UIElement *rpanel_create_auto_select_tbox = NULL;

// ============================================================================
// Root Layout
// ============================================================================
static Spacing rpanel_root_child_spacing = {{0.0f, 0.0f}, PERCENT, SPACING_NORMAL};
static Size rpanel_toggle_cont_size = {{1.0f, 0.08f}, SIZE_PERCENT};
static Size rpanel_view_cont_size = {{1.0f, 0.92f}, SIZE_PERCENT};
static Offset rpanel_state_view_cont_offset = {{0.0f, 0.08f}, OFFSET_PERCENT};
static Offset rpanel_create_view_cont_offset = {{0.0f, 0.08f}, OFFSET_PERCENT};

// ============================================================================
// State View Layout
// ============================================================================
static Offset rpanel_state_world_cont_offset = {{0.0f, 0.0f}, OFFSET_FIXED};
static Size rpanel_state_world_cont_size = {{1.0f, 0.5f}, SIZE_PERCENT};
static Offset rpanel_state_stats_cont_offset = {{0.0f, 0.0f}, OFFSET_FIXED};
static Size rpanel_state_stats_cont_size = {{1.0f, 0.32f}, SIZE_PERCENT};
static Size rpanel_state_world_btn_cont_size = {{1.0f, 0.08f}, SIZE_PERCENT};
static Spacing rpanel_world_btn_child_spacing = {{0.0f, 0.03f}, NONE, SPACING_STACKED};

// ============================================================================
// Create View Layout
// ============================================================================
static Offset rpanel_create_world_cont_offset = {{0.02f, 0.02f}, OFFSET_PERCENT};
static Size rpanel_create_world_cont_size = {{1.0f, 0.6f}, SIZE_PERCENT};

static void InitRPanelToggleButtons(void);
static void InitRPanelStateView(void);
static void InitRPanelCreateView(void);
static void InitRPanelStateWorldContainer(void);
static void InitRPanelStateStatsContainer(void);
static void InitRPanelCreateWorldContainer(void);

static void InitRPanelToggleButtons(void)
{
    rpanel_toggle_cont = CreatePanelContainer(
        rpanel->root, rpanel_toggle_cont_size,
        (Offset){{0.0f, 0.0f}, OFFSET_FIXED}, ZERO_VECTOR_2D,
        COLOURLESS_RGBA, COLOURLESS_RGBA,
        btn_cont_default_child_spacing, false, true);

    CreatePanelButtonDefault(rpanel_toggle_cont, UI_ELEMENT_BUTTON_ENUMERATE,
                             "STATE -- CREATE", btn_default_size,
                             btn_default_padding, HandleBtnEnumerateClick,
                             &btn_action_rpanel_enumerate, &rpanel->views);
}

static void InitRPanelStateWorldContainer(void)
{
    UIElement *world_cont = CreatePanelContainer(
        rpanel_state_view_cont, rpanel_state_world_cont_size,
        rpanel_state_world_cont_offset, tcont_default_padding,
        tcont_default_colour_border, tcont_default_colour_fill,
        tcont_default_child_spacing, true, true);

    CreatePanelTitleLabelDefault(world_cont, "WORLD MANAGER", tfield_default_size, rpanel_tfield_padding);

    UIElement *world_btn_cont = CreatePanelContainer(
        world_cont, rpanel_state_world_btn_cont_size,
        (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, ZERO_VECTOR_2D,
        COLOURLESS_RGBA, COLOURLESS_RGBA,
        rpanel_world_btn_child_spacing, false, true);

    CreatePanelButtonDefault(world_btn_cont, UI_ELEMENT_BUTTON_SUBMIT, "SELECT PREV", btn_default_size,
                             btn_default_padding, HandleBtnSubmitClick, &btn_action_select_world_prev, NULL);
    CreatePanelButtonDefault(world_btn_cont, UI_ELEMENT_BUTTON_SUBMIT, "SELECT NEXT", btn_default_size,
                             btn_default_padding, HandleBtnSubmitClick, &btn_action_select_world_next, NULL);

    const PanelFieldSpec world_fields[] = {
        {"WORLD", UI_ELEMENT_TEXTBOX_O, tfield_default_size, FLOAT, &rpanel_world_index_tbox, NULL},
        {"UNIVERSE", UI_ELEMENT_TEXTBOX_O, tfield_default_size, FLOAT, &rpanel_world_universe_pos_tbox, NULL},
        {"GRAVITY", UI_ELEMENT_TEXTBOX_SAFE_IO, tfield_default_size, FLOAT, &rpanel_world_gravity_edit_tbox, NULL},
        {"RES", UI_ELEMENT_TEXTBOX_O, tfield_default_size, FLOAT, &rpanel_world_resolution_tbox, NULL},
        {"OBJECTS", UI_ELEMENT_TEXTBOX_O, tfield_default_size, FLOAT, &rpanel_world_objects_tbox, NULL},
        {"NEXT ID", UI_ELEMENT_TEXTBOX_O, tfield_default_size, FLOAT, &rpanel_world_next_id_tbox, NULL},
    };
    InitPanelFields(world_cont, world_fields,
                    sizeof(world_fields) / sizeof(world_fields[0]), rpanel_tfield_padding,
                    WHITE_RGBA, COLOURLESS_RGBA);

    if (rpanel_world_gravity_edit_tbox)
    {
        rpanel_world_gravity_edit_tbox->data.textbox.data_type = FLOAT;
    }
}

static void InitRPanelStateStatsContainer(void)
{
    UIElement *stats_cont = CreatePanelContainer(
        rpanel_state_view_cont, rpanel_state_stats_cont_size,
        rpanel_state_stats_cont_offset, tcont_default_padding,
        tcont_default_colour_border, tcont_default_colour_fill,
        tcont_default_child_spacing, true, true);

    CreatePanelTitleLabelDefault(stats_cont, "UTILITY PANEL", tfield_default_size, rpanel_tfield_padding);
    const PanelFieldSpec stat_fields[] = {
        {"ENTITIES", UI_ELEMENT_TEXTBOX_O, tfield_default_size, FLOAT, &rpanel_stats_entities_tbox, NULL},
    };
    InitPanelFields(stats_cont, stat_fields,
                    sizeof(stat_fields) / sizeof(stat_fields[0]), rpanel_tfield_padding,
                    WHITE_RGBA, COLOURLESS_RGBA);
}

static void InitRPanelStateView(void)
{
    rpanel_state_view_cont = CreatePanelContainer(
        rpanel->root, rpanel_view_cont_size,
        rpanel_state_view_cont_offset, ZERO_VECTOR_2D,
        COLOURLESS_RGBA, COLOURLESS_RGBA,
        cont_default_child_spacing, false, true);

    rpanel_state_view->container = rpanel_state_view_cont;
    rpanel_state_view->type = RPANEL_STATE_VIEW;
    LArray_Push(&rpanel->views, &rpanel_state_view);

    InitRPanelStateWorldContainer();
    InitRPanelStateStatsContainer();
}

static void InitRPanelCreateWorldContainer(void)
{
    UIElement *create_world_cont = CreatePanelContainer(
        rpanel_create_view_cont, rpanel_create_world_cont_size,
        rpanel_create_world_cont_offset, tcont_default_padding,
        tcont_default_colour_border, tcont_default_colour_fill,
        tcont_default_child_spacing, true, true);

    CreatePanelTitleLabelDefault(create_world_cont, "WORLD CREATION", tfield_default_size, rpanel_tfield_padding);
    const PanelFieldSpec create_fields[] = {
        {"WORLDS", UI_ELEMENT_TEXTBOX_O, tfield_default_size, FLOAT, &rpanel_create_world_count_tbox, NULL},
        {"SELECTED", UI_ELEMENT_TEXTBOX_O, tfield_default_size, FLOAT, &rpanel_create_selected_world_tbox, NULL},
        {"SPAWN", UI_ELEMENT_TEXTBOX_SAFE_IO, tfield_default_size, VECTOR2D, &rpanel_create_spawn_tbox, NULL},
        {"RESOLUTION", UI_ELEMENT_TEXTBOX_SAFE_IO, tfield_default_size, VECTOR2D, &rpanel_create_resolution_tbox, NULL},
        {"BASIS U", UI_ELEMENT_TEXTBOX_SAFE_IO, tfield_default_size, VECTOR2D, &rpanel_create_basis_u_tbox, NULL},
        {"BASIS V", UI_ELEMENT_TEXTBOX_SAFE_IO, tfield_default_size, VECTOR2D, &rpanel_create_basis_v_tbox, NULL},
        {"GRAVITY", UI_ELEMENT_TEXTBOX_SAFE_IO, tfield_default_size, FLOAT, &rpanel_create_gravity_tbox, NULL},
        {"AUTO SELECT", UI_ELEMENT_TEXTBOX_SAFE_IO, tfield_default_size, INT, &rpanel_create_auto_select_tbox, NULL},
    };
    InitPanelFields(create_world_cont, create_fields,
                    sizeof(create_fields) / sizeof(create_fields[0]), rpanel_tfield_padding,
                    WHITE_RGBA, COLOURLESS_RGBA);

    Vector2d *spawn_origin = GetNextWorldSpawnOriginPtr();
    BindTextboxData(rpanel_create_spawn_tbox, VECTOR2D, spawn_origin);

    Vector2d *next_res = GetNextWorldResolutionPtr();
    BindTextboxData(rpanel_create_resolution_tbox, VECTOR2D, next_res);

    Vector2d *basis_u = GetNextWorldBasisUPtr();
    BindTextboxData(rpanel_create_basis_u_tbox, VECTOR2D, basis_u);

    Vector2d *basis_v = GetNextWorldBasisVPtr();
    BindTextboxData(rpanel_create_basis_v_tbox, VECTOR2D, basis_v);

    float *next_grav = GetNextWorldGravityPtr();
    BindTextboxData(rpanel_create_gravity_tbox, FLOAT, next_grav);

    int *auto_select = GetCreateWorldAutoSelectPtr();
    BindTextboxData(rpanel_create_auto_select_tbox, INT, auto_select);

    CreatePanelButtonDefault(create_world_cont, UI_ELEMENT_BUTTON_SUBMIT, "NEW WORLD", btn_default_size,
                             btn_default_padding, HandleBtnSubmitClick, &btn_action_create_world, NULL);
}

static void InitRPanelCreateView(void)
{
    rpanel_create_view_cont = CreatePanelContainer(
        rpanel->root, rpanel_view_cont_size,
        rpanel_create_view_cont_offset, ZERO_VECTOR_2D,
        COLOURLESS_RGBA, COLOURLESS_RGBA,
        tcont_default_child_spacing, false, false);

    rpanel_create_view->container = rpanel_create_view_cont;
    rpanel_create_view->type = RPANEL_WORLD_CREATE_VIEW;
    LArray_Push(&rpanel->views, &rpanel_create_view);

    InitRPanelCreateWorldContainer();
}

void InitRPanel(void)
{
    // Create panel system
    rpanel = PanelSystem_Create(&rpanel_viewport, 1.0f, (Vector2d){0.1f, 0.1f},
                                COLOUR_PANEL_DARK_1, rpanel_root_child_spacing);
    if (!rpanel)
    {
        return;
    }
    
    // Initialize views array
    PanelSystem_InitViews(rpanel, 2);
    
    // Initialize root UI structure
    PanelSystem_InitRoot(rpanel);
    
    // Setup view storage
    rpanel_state_view = &rpanel_state_view_storage;
    rpanel_create_view = &rpanel_create_view_storage;
    btn_action_rpanel_enumerate = 0;
    
    // Build panel-specific UI
    InitRPanelToggleButtons();
    InitRPanelStateView();
    InitRPanelCreateView();
    
    // Initial layout update
    UpdateUISpace(rpanel->root, rpanel->seed_box);
}


void DrawRPanel(void)
{
    if (!rpanel || !rpanel->root)
    {
        return;
    }

    // Keep UI layout in sync with interactive/manual offset changes.
    UpdateUISpace(rpanel->root, rpanel->seed_box);

    if (rpanel_stats_entities_tbox)
    {
        WriteTextboxInt(rpanel_stats_entities_tbox, GetNewtonoidCount());
    }

    World2d *selected_world = GetSelectedWorld();
    int selected_world_idx = GetSelectedWorldIndex();
    int world_count = GetWorldCount();
    bool has_selected_world = selected_world && world_count > 0;

    if (rpanel_world_index_tbox)
    {
        if (has_selected_world)
        {
            UpdateString64(rpanel_world_index_tbox->data.textbox.text.string, "%d/%d", selected_world_idx + 1, world_count);
        }
        else
        {
            WriteTextboxText(rpanel_world_index_tbox, "0/0");
        }
    }

    if (rpanel_world_universe_pos_tbox)
    {
        if (selected_world)
        {
            WriteTextboxVectorPair(rpanel_world_universe_pos_tbox, selected_world->uni_coords_center);
        }
        else
        {
            WriteTextboxText(rpanel_world_universe_pos_tbox, "N/A");
        }
    }

    if (rpanel_create_world_count_tbox)
    {
        WriteTextboxInt(rpanel_create_world_count_tbox, world_count);
    }

    if (rpanel_create_selected_world_tbox)
    {
        if (has_selected_world)
        {
            WriteTextboxInt(rpanel_create_selected_world_tbox, selected_world_idx + 1);
        }
        else
        {
            WriteTextboxText(rpanel_create_selected_world_tbox, "0");
        }
    }

    Vector2d *spawn_origin = GetNextWorldSpawnOriginPtr();
    if (spawn_origin)
    {
        WriteTextboxVectorIfUnfocused(rpanel_create_spawn_tbox, *spawn_origin);
    }

    Vector2d *next_res = GetNextWorldResolutionPtr();
    if (next_res)
    {
        WriteTextboxVectorIfUnfocused(rpanel_create_resolution_tbox, *next_res);
    }

    Vector2d *basis_u = GetNextWorldBasisUPtr();
    if (basis_u)
    {
        WriteTextboxVectorIfUnfocused(rpanel_create_basis_u_tbox, *basis_u);
    }

    Vector2d *basis_v = GetNextWorldBasisVPtr();
    if (basis_v)
    {
        WriteTextboxVectorIfUnfocused(rpanel_create_basis_v_tbox, *basis_v);
    }

    float *next_grav = GetNextWorldGravityPtr();
    if (next_grav)
    {
        WriteTextboxNumberIfUnfocused(rpanel_create_gravity_tbox, *next_grav, 2);
    }

    int *auto_select = GetCreateWorldAutoSelectPtr();
    if (auto_select && rpanel_create_auto_select_tbox && !rpanel_create_auto_select_tbox->is_focused)
    {
        WriteTextboxInt(rpanel_create_auto_select_tbox, *auto_select);
    }

    if (selected_world)
    {
        if (rpanel_world_gravity_edit_tbox)
        {
            rpanel_world_gravity_edit_tbox->data.textbox.data_bind = &selected_world->gravity;
            WriteTextboxNumberIfUnfocused(rpanel_world_gravity_edit_tbox, selected_world->gravity, 2);
        }
        if (rpanel_world_resolution_tbox)
        {
            Vector2d world_size = {(float)selected_world->grid_space.space.columns,
                                   (float)selected_world->grid_space.space.rows};
            UpdateString64(rpanel_world_resolution_tbox->data.textbox.text.string, "%.0fx%.0f", world_size.x, world_size.y);
        }
        if (rpanel_world_objects_tbox)
        {
            WriteTextboxInt(rpanel_world_objects_tbox, selected_world->objects.count);
        }
        if (rpanel_world_next_id_tbox)
        {
            WriteTextboxInt(rpanel_world_next_id_tbox, selected_world->next_object_id);
        }
    }
    else
    {
        if (rpanel_world_gravity_edit_tbox)
        {
            WriteTextboxText(rpanel_world_gravity_edit_tbox, "N/A");
            rpanel_world_gravity_edit_tbox->data.textbox.data_bind = NULL;
        }
        if (rpanel_world_resolution_tbox)
            WriteTextboxText(rpanel_world_resolution_tbox, "N/A");
        if (rpanel_world_objects_tbox)
            WriteTextboxText(rpanel_world_objects_tbox, "0");
        if (rpanel_world_next_id_tbox)
            WriteTextboxText(rpanel_world_next_id_tbox, "0");
    }
    
    PanelSystem_Draw(rpanel);
}

Frame2d *GetRPanelSpaceFrame(void)
{
    return PanelSystem_GetSpaceFrame(rpanel);
}

bool SetRPanelSpaceBasis(Vector2d basis_u, Vector2d basis_v)
{
    return PanelSystem_SetSpaceBasis(rpanel, basis_u, basis_v);
}

void ResetRPanelSpaceBasis(void)
{
    PanelSystem_ResetSpaceBasis(rpanel);
}

UIElement* GetRPanelRoot(void)
{
    return rpanel ? rpanel->root : NULL;
}
