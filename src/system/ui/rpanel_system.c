#include "system/ui/rpanel_system.h"

#include "system/systems.h"
#include "system/ui_system.h"
#include "world/world.h"
#include "world/universe.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"
#include "system/panel_system.h"
#include "system/utility_system.h"

// ============================================================================
// Panel System
// ============================================================================
static PanelSystem *rpanel = NULL;

// ============================================================================
// Action Codes
// ============================================================================
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
// ============================================================================
// UI Element Pointers
// ============================================================================
static View *rpanel_state_view = NULL;
static View *rpanel_create_view = NULL;
static UIElement *rpanel_toggle_cont = NULL;
static UIElement *rpanel_state_view_cont = NULL;
static UIElement *rpanel_create_view_cont = NULL;
static ViewSelector *rpanel_view_selector = NULL;
static int rpanel_basis_world_index = -1;
static Basis2d rpanel_last_world_basis = {0};

static UIElement *rpanel_stats_entities_tbox = NULL;
static UIElement *rpanel_world_index_tbox = NULL;
static UIElement *rpanel_world_universe_pos_tbox = NULL;
static UIElement *rpanel_world_gravity_edit_tbox = NULL;
static UIElement *rpanel_world_resolution_tbox = NULL;
static UIElement *rpanel_world_basis_u_tbox = NULL;
static UIElement *rpanel_world_basis_v_tbox = NULL;
static UIElement *rpanel_world_objects_tbox = NULL;
static UIElement *rpanel_world_next_id_tbox = NULL;
static UIElement *rpanel_create_spawn_tbox = NULL;
static UIElement *rpanel_create_resolution_tbox = NULL;
static UIElement *rpanel_create_basis_u_tbox = NULL;
static UIElement *rpanel_create_basis_v_tbox = NULL;
static UIElement *rpanel_create_gravity_tbox = NULL;
static UIElement *rpanel_create_objects_tbox = NULL;

// ============================================================================
// Root Layout
// ============================================================================
static Offset rpanel_state_view_cont_offset = {{0.0f, 0.0f}, OFFSET_PERCENT};
static Offset rpanel_create_view_cont_offset = {{0.0f, 0.0f}, OFFSET_PERCENT};

// ============================================================================
// State View Layout
// ============================================================================
static Offset rpanel_state_world_cont_offset = {{0.0f, 0.0f}, OFFSET_FIXED};
static Size rpanel_state_world_cont_size = {{1.0f, 0.36f}, SIZE_PERCENT};

// ============================================================================
// Create View Layout
// ============================================================================
static Offset rpanel_create_world_cont_offset = {{0.0f, 0.0f}, OFFSET_PERCENT};
static Size rpanel_create_world_cont_size = {{1.0f, 0.32f}, SIZE_PERCENT};

static void InitRPanelStateView(void);
static void InitRPanelCreateView(void);
static void InitRPanelStateWorldContainer(void);
// static void InitRPanelStateStatsContainer(void);
static void InitRPanelCreateWorldContainer(void);

static void InitRPanelStateWorldContainer(void)
{
    UIElement *world_cont = CreateUIContainer(
        rpanel_state_view_cont, rpanel_state_world_cont_size,
        rpanel_state_world_cont_offset, ui_standard_container_padding,
        rpanel->palette, UI_PALETTE_SURFACE_CONTAINER,
        ui_standard_stack_spacing, true, true);

    CreateUILabelTitleDefault(world_cont, "World Manager", ui_standard_control_size,
                              ui_standard_field_padding, rpanel->palette);

    const UIFieldSpec world_fields[] = {
        {"World", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, &rpanel_world_index_tbox, NULL},
        {"World origin", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, &rpanel_world_universe_pos_tbox, NULL},
        {"Res", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, &rpanel_world_resolution_tbox, NULL},
        {"Objects", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, &rpanel_world_objects_tbox, NULL},
        {"Next id", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, &rpanel_world_next_id_tbox, NULL},
        {"Gravity", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &rpanel_world_gravity_edit_tbox, NULL},
        {"Basis u", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &rpanel_world_basis_u_tbox, NULL},
        {"Basis v", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &rpanel_world_basis_v_tbox, NULL},
    };
    InitUIFields(world_cont, world_fields, ARRAY_COUNT(world_fields), ui_standard_field_padding,
                 rpanel->palette);

    if (rpanel_world_gravity_edit_tbox)
    {
        rpanel_world_gravity_edit_tbox->data.textbox.data_type = FLOAT;
    }

    CreateUIButtonDefault(world_cont, UI_ELEMENT_BUTTON_SUBMIT, "PREV",
                          ui_standard_button_size, ui_standard_button_padding, rpanel->palette,
                          HandleBtnSubmitClick, &btn_action_select_world_prev, NULL);

    CreateUIButtonDefault(world_cont, UI_ELEMENT_BUTTON_SUBMIT, "NEXT",
                          ui_standard_button_size, ui_standard_button_padding, rpanel->palette,
                          HandleBtnSubmitClick, &btn_action_select_world_next, NULL);
}

static void InitRPanelStateView(void)
{
    rpanel_state_view_cont = CreateUIContainer(
        rpanel->root, ui_fill_container_size,
        rpanel_state_view_cont_offset, ZERO_VECTOR_2D,
        rpanel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
        ui_standard_stack_spacing, false, true);

    PanelSystem_AddView(rpanel, rpanel_state_view, rpanel_state_view_cont, RPANEL_STATE_VIEW);

    InitRPanelStateWorldContainer();
    // InitRPanelStateStatsContainer();
}

static void InitRPanelCreateWorldContainer(void)
{
    UIElement *create_world_cont = CreateUIContainer(
        rpanel_create_view_cont, rpanel_create_world_cont_size,
        rpanel_create_world_cont_offset, ui_standard_container_padding,
        rpanel->palette, UI_PALETTE_SURFACE_CONTAINER,
        ui_standard_stack_spacing, true, true);

    CreateUILabelTitleDefault(create_world_cont, "World Create", ui_standard_control_size,
                              ui_standard_field_padding, rpanel->palette);
    const UIFieldSpec create_fields[] = {
        {"Spawn", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &rpanel_create_spawn_tbox, NULL},
        {"Resolution", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &rpanel_create_resolution_tbox, NULL},
        {"Objects", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, INT, &rpanel_create_objects_tbox, NULL},
        {"Gravity", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &rpanel_create_gravity_tbox, NULL},
        {"Basis u", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &rpanel_create_basis_u_tbox, NULL},
        {"Basis v", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &rpanel_create_basis_v_tbox, NULL},
    };
    InitUIFields(create_world_cont, create_fields,
                 ARRAY_COUNT(create_fields), ui_standard_field_padding,
                 rpanel->palette);

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

    int *next_objects = GetNextWorldObjectCountPtr();
    BindTextboxData(rpanel_create_objects_tbox, INT, next_objects);

    CreateUIButtonDefault(create_world_cont, UI_ELEMENT_BUTTON_SUBMIT,
                          "NEW WORLD", ui_standard_button_size, ui_standard_button_padding,
                          rpanel->palette, HandleBtnSubmitClick,
                          &btn_action_create_world, NULL);
}

static void InitRPanelCreateView(void)
{
    rpanel_create_view_cont = CreateUIContainer(
        rpanel->root, ui_fill_container_size,
        rpanel_create_view_cont_offset, ZERO_VECTOR_2D,
        rpanel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
        ui_standard_stack_spacing, false, false);

    PanelSystem_AddView(rpanel, rpanel_create_view, rpanel_create_view_cont, RPANEL_WORLD_CREATE_VIEW);

    InitRPanelCreateWorldContainer();
}

void InitRPanel(void)
{
    rpanel_last_world_basis.u.x = 1.0f;
    rpanel_last_world_basis.u.y = 0.0f;
    rpanel_last_world_basis.v.x = 0.0f;
    rpanel_last_world_basis.v.y = 1.0f;

    const char *labels[] = {"STATE", "CREATE"};
    rpanel = PanelSystem_CreateStandard(&rpanel_viewport, 2, labels, ARRAY_COUNT(labels),
                                        PanelSystem_HandleViewSelected,
                                        &ui_default_palette, ui_standard_stack_spacing);
    if (!rpanel)
    {
        return;
    }

    // Setup view storage
    rpanel_state_view = &rpanel_state_view_storage;
    rpanel_create_view = &rpanel_create_view_storage;

    // Build panel-specific UI
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

    TextboxField create_fields[] = {
        {rpanel_create_spawn_tbox, VECTOR2D, GetNextWorldSpawnOriginPtr(), 0, NULL},
        {rpanel_create_resolution_tbox, VECTOR2D, GetNextWorldResolutionPtr(), 0, NULL},
        {rpanel_create_basis_u_tbox, VECTOR2D, GetNextWorldBasisUPtr(), 0, NULL},
        {rpanel_create_basis_v_tbox, VECTOR2D, GetNextWorldBasisVPtr(), 0, NULL},
        {rpanel_create_gravity_tbox, FLOAT, GetNextWorldGravityPtr(), 2, NULL},
        {rpanel_create_objects_tbox, INT, GetNextWorldObjectCountPtr(), 0, NULL},
    };
    RefreshTextboxFields(create_fields, ARRAY_COUNT(create_fields));

    if (selected_world)
    {
        if (rpanel_basis_world_index != selected_world_idx)
        {
            rpanel_basis_world_index = selected_world_idx;
            rpanel_last_world_basis = selected_world->grid_space.space.frame.basis;
        }

        Basis2d edited_basis = selected_world->grid_space.space.frame.basis;
        if (edited_basis.u.x != rpanel_last_world_basis.u.x ||
            edited_basis.u.y != rpanel_last_world_basis.u.y ||
            edited_basis.v.x != rpanel_last_world_basis.v.x ||
            edited_basis.v.y != rpanel_last_world_basis.v.y)
        {
            if (Universe_SetWorldBasis(&G_Universe, selected_world_idx,
                                       edited_basis.u, edited_basis.v))
            {
                rpanel_last_world_basis = edited_basis;
            }
            else
            {
                selected_world->grid_space.space.frame.basis = rpanel_last_world_basis;
            }
        }

        TextboxField world_fields[] = {
            {rpanel_world_gravity_edit_tbox, FLOAT,
             &selected_world->gravity, 2, "N/A"},
            {rpanel_world_basis_u_tbox, VECTOR2D,
             &selected_world->grid_space.space.frame.basis.u, 0, "N/A"},
            {rpanel_world_basis_v_tbox, VECTOR2D,
             &selected_world->grid_space.space.frame.basis.v, 0, "N/A"},
        };
        RefreshTextboxFields(world_fields, ARRAY_COUNT(world_fields));
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
            WriteTextboxInt(rpanel_world_next_id_tbox, G_Universe.next_entity_id);
        }
    }
    else
    {
        TextboxField world_fields[] = {
            {rpanel_world_gravity_edit_tbox, FLOAT, NULL, 0, "N/A"},
            {rpanel_world_basis_u_tbox, VECTOR2D, NULL, 0, "N/A"},
            {rpanel_world_basis_v_tbox, VECTOR2D, NULL, 0, "N/A"},
        };
        RefreshTextboxFields(world_fields, ARRAY_COUNT(world_fields));
        rpanel_basis_world_index = -1;
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

UIElement *GetRPanelRoot(void)
{
    return rpanel ? rpanel->root : NULL;
}
