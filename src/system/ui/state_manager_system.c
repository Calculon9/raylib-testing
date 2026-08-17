#include "system/ui/state_manager_system.h"

#include "system/command_queue.h"
#include "system/panel_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"
#include "physics/newtonoid.h"
#include "math/coordinate_space.h"
#include "world/universe.h"

static PanelSystem *state_manager_panel = NULL;
static int btn_action_delete_entity = BUTTON_ACTION_DELETE_ENTITY;

static Offset state_manager_entity_offset = {{0.0f, 0.15f}, OFFSET_PERCENT};
static Size state_manager_view_size = {{1.0f, 0.85f}, SIZE_PERCENT};
static Size state_manager_toggle_size = {{1.0f, 0.15f}, SIZE_PERCENT};
//static Size standard_section_size = {{0.25f, 1.0f}, SIZE_PERCENT};
static Size phys_section_size = {{0.375f, 1.0f}, SIZE_PERCENT};
static Size attr_section_size = {{0.25f, 1.0f}, SIZE_PERCENT};
static ViewSelector *state_manager_view_selector = NULL;
static bool state_manager_refresh_dirty = true;

typedef struct
{
    uint32_t flag;
    bool is_type;
    bool is_collision;
    bool is_entity;
    bool is_world;
    bool is_cell;
} StateManagerFlagBinding;

typedef enum
{
    STATE_MANAGER_FLAG_SOURCE_NONE,
    STATE_MANAGER_FLAG_SOURCE_ENTITY,
    STATE_MANAGER_FLAG_SOURCE_WORLD,
    STATE_MANAGER_FLAG_SOURCE_CELL,
} StateManagerFlagSourceKind;

typedef struct
{
    StateManagerFlagSourceKind kind;
    const Newtonoid2d *entity;
    const World2d *world;
    const Cell *cell;
    bool use_collision_mask;
} StateManagerFlagSource;

typedef struct
{
    const char *label;
    StateManagerFlagBinding *binding;
    UIElement **registry;
    size_t registry_count;
    size_t registry_offset;
} StateManagerButtonSpec;

static StateManagerFlagBinding flag_alive = {FLAG_STATUS_ALIVE, false, false, false, false, false};
static StateManagerFlagBinding flag_rigid = {FLAG_ATTR_RIGID, false, false, false, false, false};
static StateManagerFlagBinding flag_clocked = {FLAG_LIFETIME_CLOCKED, false, false, false, false, false};
static StateManagerFlagBinding flag_wall = {FLAG_TYPE_WALL, true, false, true, false, false};
static StateManagerFlagBinding flag_newtonoid = {FLAG_TYPE_NEWTONOID, true, false, true, false, false};
static StateManagerFlagBinding flag_projectile = {FLAG_TYPE_PROJECTILE, true, false, true, false, false};
static StateManagerFlagBinding flag_effect = {FLAG_TYPE_EFFECT, true, false, true, false, false};
static StateManagerFlagBinding flag_camera = {FLAG_TYPE_CAMERA, true, false, true, false, false};
static StateManagerFlagBinding mask_wall = {FLAG_TYPE_WALL, false, true, false, false, false};
static StateManagerFlagBinding mask_newtonoid = {FLAG_TYPE_NEWTONOID, false, true, false, false, false};
static StateManagerFlagBinding mask_projectile = {FLAG_TYPE_PROJECTILE, false, true, false, false, false};
static StateManagerFlagBinding mask_effect = {FLAG_TYPE_EFFECT, false, true, false, false, false};
static StateManagerFlagBinding mask_camera = {FLAG_TYPE_CAMERA, false, true, false, false, false};
static StateManagerFlagBinding world_active = {WORLD_FLAG_ACTIVE, false, false, false, true, false};
static StateManagerFlagBinding world_visible = {WORLD_FLAG_VISIBLE, false, false, false, true, false};
static StateManagerFlagBinding world_selectable = {WORLD_FLAG_SELECTABLE, false, false, false, true, false};
static StateManagerFlagBinding world_physics = {WORLD_FLAG_PHYSICS_ENABLED, false, false, false, true, false};
static StateManagerFlagBinding world_spawns = {WORLD_FLAG_SPAWNS_ENABLED, false, false, false, true, false};
static StateManagerFlagBinding world_locked = {WORLD_FLAG_LOCKED, false, false, false, true, false};
static StateManagerFlagBinding world_draggable = {WORLD_FLAG_DRAGGABLE, false, false, false, true, false};
static StateManagerFlagBinding cell_solid = {CELL_FLAG_SOLID, false, false, false, false, true};
static StateManagerFlagBinding cell_walkable = {CELL_FLAG_WALKABLE, false, false, false, false, true};
static StateManagerFlagBinding cell_hazardous = {CELL_FLAG_HAZARDOUS, false, false, false, false, true};
static StateManagerFlagBinding cell_spawnable = {CELL_FLAG_SPAWNABLE, false, false, false, false, true};

static UIElement *state_manager_flag_buttons[8] = {0};
static UIElement *state_manager_collision_buttons[5] = {0};
static UIElement *state_manager_world_buttons[7] = {0};
static UIElement *state_manager_cell_buttons[4] = {0};

static const StateManagerButtonSpec state_manager_identity_specs[] = {
    {"WALL", &flag_wall, state_manager_flag_buttons, 8, 0},
    {"NEWTONOID", &flag_newtonoid, state_manager_flag_buttons, 8, 0},
    {"PROJECTILE", &flag_projectile, state_manager_flag_buttons, 8, 0},
    {"EFFECT", &flag_effect, state_manager_flag_buttons, 8, 0},
    {"CAMERA", &flag_camera, state_manager_flag_buttons, 8, 0},
};
static const StateManagerButtonSpec state_manager_behaviour_specs[] = {
    {"ALIVE", &flag_alive, state_manager_flag_buttons, 8, 5},
    {"RIGID", &flag_rigid, state_manager_flag_buttons, 8, 5},
    {"CLOCKED", &flag_clocked, state_manager_flag_buttons, 8, 5},
};
static const StateManagerButtonSpec state_manager_collision_specs[] = {
    {"WALL", &mask_wall, state_manager_collision_buttons, 5, 0},
    {"NEWTONOID", &mask_newtonoid, state_manager_collision_buttons, 5, 0},
    {"PROJECTILE", &mask_projectile, state_manager_collision_buttons, 5, 0},
    {"EFFECT", &mask_effect, state_manager_collision_buttons, 5, 0},
    {"CAMERA", &mask_camera, state_manager_collision_buttons, 5, 0},
};
static const StateManagerButtonSpec state_manager_world_specs[] = {
    {"ACTIVE", &world_active, state_manager_world_buttons, 7, 0},
    {"VISIBLE", &world_visible, state_manager_world_buttons, 7, 0},
    {"SELECTABLE", &world_selectable, state_manager_world_buttons, 7, 0},
    {"PHYSICS", &world_physics, state_manager_world_buttons, 7, 0},
    {"SPAWNS", &world_spawns, state_manager_world_buttons, 7, 0},
    {"LOCKED", &world_locked, state_manager_world_buttons, 7, 0},
    {"DRAG", &world_draggable, state_manager_world_buttons, 7, 0},
};
static const StateManagerButtonSpec state_manager_cell_specs[] = {
    {"SOLID", &cell_solid, state_manager_cell_buttons, 4, 0},
    {"WALKABLE", &cell_walkable, state_manager_cell_buttons, 4, 0},
    {"HAZARD", &cell_hazardous, state_manager_cell_buttons, 4, 0},
    {"SPAWN", &cell_spawnable, state_manager_cell_buttons, 4, 0},
};

// PHYS view geometry readouts.
static UIElement *state_rotation_tbox = NULL;
static UIElement *state_basis_u_tbox = NULL;
static UIElement *state_basis_v_tbox = NULL;
static UIElement *state_geometry_center_tbox = NULL;

typedef enum
{
    STATE_MANAGER_GEOMETRY_NUMBER,
    STATE_MANAGER_GEOMETRY_VECTOR,
} StateManagerGeometryFormat;

void MarkStateManagerRefreshDirty(void)
{
    state_manager_refresh_dirty = true;
}

static void HandleSelectionChanged(EntityId selected_object_id,
                                   const Newtonoid2d *selected_object,
                                   const Cell *selected_cell,
                                   int selected_cell_index,
                                   void *user_data)
{
    (void)selected_object_id;
    (void)selected_object;
    (void)selected_cell;
    (void)selected_cell_index;
    (void)user_data;

    if (!state_manager_panel)
    {
        return;
    }

    // Selection transitions mark the state-manager UI dirty and defer refresh to draw.
    MarkStateManagerRefreshDirty();
}

static int GetStateManagerObjectWorld(const Newtonoid2d *object)
{
    return Universe_FindWorldContainingObject(&G_Universe, object);
}

static const uint32_t state_manager_type_flags = FLAG_TYPE_WALL | FLAG_TYPE_NEWTONOID |
                                                 FLAG_TYPE_PROJECTILE | FLAG_TYPE_EFFECT | FLAG_TYPE_CAMERA;

//static void InitStateManagerCellStateView(void);

static void HandleStateManagerFlagClick(UIElement *button)
{
    if (!button || !button->data.button.user_data)
    {
        return;
    }

    StateManagerFlagBinding *binding = (StateManagerFlagBinding *)button->data.button.user_data;
    Newtonoid2d *object = UIState_GetSelectedObject();

    if (!binding->is_cell && !binding->is_world && !object)
    {
        return;
    }

    if (binding->is_world)
    {
        World2d *world = Universe_GetSelectedWorld(&G_Universe);
        if (!world)
        {
            return;
        }

        if (world->flags & binding->flag)
        {
            world->flags &= ~binding->flag;
        }
        else
        {
            world->flags |= binding->flag;
        }
        MarkStateManagerRefreshDirty();
        return;
    }

    if (binding->is_collision)
    {
        if (!object)
        {
            return;
        }
        if (object->collision_mask & binding->flag)
        {
            object->collision_mask &= ~binding->flag;
        }
        else
        {
            object->collision_mask |= binding->flag;
        }
        MarkStateManagerRefreshDirty();
        return;
    }

    if (binding->is_cell)
    {
        Cell *cell = UIState_GetSelectedCell();
        if (!cell)
        {
            return;
        }

        uint32_t *target_flags = (uint32_t *)&cell->flags;
        if (*target_flags & binding->flag)
        {
            *target_flags &= ~binding->flag;
        }
        else
        {
            *target_flags |= binding->flag;
        }
        MarkStateManagerRefreshDirty();
        return;
    }

    uint32_t *target_flags = NULL;
    if (binding->is_entity)
    {
        target_flags = (uint32_t *)&object->entity_flags;
    }
    else
    {
        target_flags = (uint32_t *)&object->status_flags;
    }
    if (binding->is_type)
    {
        *target_flags &= ~state_manager_type_flags;
        *target_flags |= binding->flag;
    }
    else if (*target_flags & binding->flag)
    {
        *target_flags &= ~binding->flag;
    }
    else
    {
        *target_flags |= binding->flag;
    }
    MarkStateManagerRefreshDirty();
}

static void RegisterStateManagerButton(UIElement *button, UIElement **buttons, size_t count)
{
    if (!button || !buttons)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        if (!buttons[i])
        {
            buttons[i] = button;
            return;
        }
    }
}

static UIElement *CreateStateManagerBoundButton(UIElement *parent, const char *label, StateManagerFlagBinding *binding,
                                                const UIPalette *palette, UIElement **registry, size_t registry_count)
{
    UIElement *button = CreateUIButtonDefault(parent, UI_ELEMENT_BUTTON_SIMPLE, label,
                                              ui_standard_button_size, ui_standard_button_padding,
                                              palette, HandleStateManagerFlagClick, binding, NULL);
    RegisterStateManagerButton(button, registry, registry_count);
    return button;
}

static void CreateStateManagerSectionButtons(UIElement *section,const StateManagerButtonSpec *specs, size_t count)
{
    if (!section || !specs)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        CreateStateManagerBoundButton(section, specs[i].label, specs[i].binding, state_manager_panel->palette,
                                      specs[i].registry, specs[i].registry_count);
    }
}

// Create and register a state-manager view while preserving per-view layout behavior.
static UIElement *CreateStateManagerViewContainer(int view_id, bool is_draggable, bool is_enabled,
                                                  bool use_zero_wrap_spacing)
{
    UIElement *container = CreateUIContainer(
        state_manager_panel->root, state_manager_view_size,
        state_manager_entity_offset, ui_standard_container_padding,
        state_manager_panel->palette, UI_PALETTE_SURFACE_CONTAINER,
        ui_standard_inline_spacing, is_draggable, is_enabled);
    View *view = AllocateBytes(sizeof(View));
    PanelSystem_AddView(state_manager_panel, view, container, view_id);

    if (use_zero_wrap_spacing)
    {
        container->child_spacing = ui_zero_horizontal_wrap_spacing;
    }

    return container;
}

static void InitPhysStateView(void)
{
    UIElement *entity_container = CreateStateManagerViewContainer(
        STATE_MANAGER_PHYSICS_VIEW, true, true, true);

    // Customise the View
    UIElement *newtonian_section = CreateViewSection_Wrap(entity_container, "Newtonian", phys_section_size,
                                                     state_manager_panel->palette);
    const UIFieldSpec state_specs[] = {
        {"Id", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, &G_UIState.state_id_tbox, NULL},
        {"Mass", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.state_mass_tbox, NULL},
        {"Bounds.min", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, VECTOR2D, &G_UIState.state_pos_tl_tbox, NULL},
        {"Anchor", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.state_pos_c_tbox, NULL},
        {"Vel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.state_vel_tbox, NULL},
        {"Accel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.state_accel_tbox, NULL},
        {"Moment", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.state_moment_tbox, NULL},
        {"World", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, INT, NULL, &G_UIState.state_world_str},
    };
    InitUIFields(newtonian_section, state_specs,
                 sizeof(state_specs) / sizeof(state_specs[0]),
                 ui_standard_field_padding, state_manager_panel->palette);

    UIElement *geometry_section = CreateViewSection_Wrap(entity_container, "Geometry", phys_section_size,
                                                   state_manager_panel->palette);
    // Show the live rotation and basis used by the object's local frame.
    const UIFieldSpec geometry_specs[] = {
        {"Geo.center", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, VECTOR2D, &state_geometry_center_tbox, NULL},
        {"Rot", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &state_rotation_tbox, NULL},
        {"Basis u", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &state_basis_u_tbox, NULL},
        {"Basis v", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &state_basis_v_tbox, NULL},
    };
    InitUIFields(geometry_section, geometry_specs,
                 sizeof(geometry_specs) / sizeof(geometry_specs[0]),
                 ui_standard_field_padding, state_manager_panel->palette);

    CreateUIButtonDefault(entity_container, UI_ELEMENT_BUTTON_SUBMIT,
                          "DELETE", ui_standard_button_size, ui_standard_button_padding,
                          state_manager_panel->palette, HandleBtnSubmitClick,
                          &btn_action_delete_entity, NULL);
}

static void InitAttributeStateView(void)
{
    UIElement *attributes_container = CreateStateManagerViewContainer(
        STATE_MANAGER_ATTRIBUTES_VIEW, true, false, true);

    // Customise the View
    UIElement *identity_section = CreateViewSection_Wrap(attributes_container, "Entity", attr_section_size,
                                                    state_manager_panel->palette);
    CreateStateManagerSectionButtons(identity_section, state_manager_identity_specs,
                                     sizeof(state_manager_identity_specs) / sizeof(state_manager_identity_specs[0]));

    UIElement *behaviour_section = CreateViewSection_Wrap(attributes_container, "Behaviour", attr_section_size,
                                                     state_manager_panel->palette);
    CreateStateManagerSectionButtons(behaviour_section, state_manager_behaviour_specs,
                                     sizeof(state_manager_behaviour_specs) / sizeof(state_manager_behaviour_specs[0]));

    UIElement *collision_section = CreateViewSection_Wrap(attributes_container, "Collision", attr_section_size,
                                                     state_manager_panel->palette);
    CreateStateManagerSectionButtons(collision_section, state_manager_collision_specs,
                                     sizeof(state_manager_collision_specs) / sizeof(state_manager_collision_specs[0]));
}

static void InitWorldStateView(void)
{
    UIElement *world_container = CreateStateManagerViewContainer(
        STATE_MANAGER_WORLD_VIEW, true, false, true);

    UIElement *world_section = CreateViewSection_Wrap(world_container, "World", attr_section_size,
                                                 state_manager_panel->palette);
    CreateStateManagerSectionButtons(world_section, state_manager_world_specs,
                                     sizeof(state_manager_world_specs) / sizeof(state_manager_world_specs[0]));
}

static void InitCellStateView(void)
{
    UIElement *cell_container = CreateStateManagerViewContainer(
        STATE_MANAGER_CELL_STATE_VIEW, true, false, false);

    const UIFieldSpec cell_specs[] = {
        {"Index", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.cell_id_str},
        {"Occu", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.cell_occu_str},
        {"Value", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.cell_value_str},
        {"Fill", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.cell_fill_str},
    };

    UIElement *cell_phys_section = CreateViewSection_Wrap(cell_container, "Cell", attr_section_size,
                                                     state_manager_panel->palette);
    InitUIFields(cell_phys_section, cell_specs,
                 sizeof(cell_specs) / sizeof(cell_specs[0]),
                 ui_standard_field_padding, state_manager_panel->palette);

    UIElement *cell_flags_section = CreateViewSection_Wrap(cell_container, "Flags", attr_section_size,
                                                      state_manager_panel->palette);
    CreateStateManagerSectionButtons(cell_flags_section, state_manager_cell_specs,
                                     sizeof(state_manager_cell_specs) / sizeof(state_manager_cell_specs[0]));
}

void InitStateManagerSystem(void)
{
    state_manager_panel = PanelSystem_Create(&entity_panel_viewport, 1.0f, (Vector2d){0.1f, 0.1f}, &ui_default_palette,
                                             (Spacing){{0.0f, 0.0f}, PERCENT, SPACING_NONE});
    if (!state_manager_panel)
        return;

    PanelSystem_InitViews(state_manager_panel, 4);
    PanelSystem_InitRoot(state_manager_panel);
    UIElement *toggle_container = CreateUIContainer(
        state_manager_panel->root, state_manager_toggle_size,
        (Offset){{0.0f, 0.0f}, OFFSET_PERCENT}, ZERO_VECTOR_2D,
        state_manager_panel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
        ui_standard_inline_spacing, false, true);
    
    InitPhysStateView();
    InitAttributeStateView();
    InitWorldStateView();
    InitCellStateView();
    const char *labels[] = {"PHYS", "ATTRI", "WORLD", "CELL"};
    state_manager_view_selector = PanelSystem_CreateViewSelector(
        state_manager_panel, toggle_container, ui_small_horizontal_button_size,
        labels, sizeof(labels) / sizeof(labels[0]), NULL);
    PanelSystem_SelectView(state_manager_view_selector, 0);

    // Subscribe once so selection transitions can proactively refresh this panel.
    UIState_SetSelectionChangedCallback(HandleSelectionChanged, NULL);

    // First frame should always render initialised state-manager values.
    MarkStateManagerRefreshDirty();

    UpdateUISpace(state_manager_panel->root, state_manager_panel->seed_box);
}

void DrawStateManagerSystem(void)
{
    if (state_manager_panel)
    {
        if (state_manager_refresh_dirty)
        {
            UpdateStateManagerSelectedObject();
            state_manager_refresh_dirty = false;
        }
        PanelSystem_Draw(state_manager_panel);
    }
}

UIElement *GetStateManagerRoot(void)
{
    return state_manager_panel ? state_manager_panel->root : NULL;
}

static uint32_t GetStateManagerSourceFlags(const StateManagerFlagBinding *binding, StateManagerFlagSource source)
{
    switch (source.kind)
    {
        case STATE_MANAGER_FLAG_SOURCE_ENTITY:
            if (!source.entity)
            {
                return 0;
            }
            return source.use_collision_mask ? source.entity->collision_mask
                                             : (binding->is_entity ? source.entity->entity_flags
                                                                  : source.entity->status_flags);
        case STATE_MANAGER_FLAG_SOURCE_WORLD:
            if (!source.world || !binding->is_world)
            {
                return 0;
            }
            return source.world->flags;
        case STATE_MANAGER_FLAG_SOURCE_CELL:
            if (!source.cell || !binding->is_cell)
            {
                return 0;
            }
            return source.cell->flags;
        case STATE_MANAGER_FLAG_SOURCE_NONE:
        default:
            return 0;
    }
}

static void UpdateStateManagerButtonGroup(const StateManagerButtonSpec *specs, size_t count,
                                          StateManagerFlagSource source)
{
    for (size_t i = 0; i < count; i++)
    {
        UIElement *button = specs[i].registry[specs[i].registry_offset + i];
        if (!button)
        {
            continue;
        }
        uint32_t source_flags = GetStateManagerSourceFlags(specs[i].binding, source);
        bool enabled = source.kind != STATE_MANAGER_FLAG_SOURCE_NONE &&
                       (source_flags & specs[i].binding->flag) != 0;
        UpdateString64(button->data.button.label.string,
                       "%s: %s", specs[i].label, enabled ? "ON" : "OFF");
        button->is_enabled = true;
    }
}

void UpdateStateManagerSelectedObject(void)
{
    Newtonoid2d *object = UIState_GetSelectedObject();
    World2d *world = Universe_GetSelectedWorld(&G_Universe);
    UIElement *geometry_textboxes[] = {
        state_rotation_tbox, state_basis_u_tbox, state_basis_v_tbox, state_geometry_center_tbox};

    if (G_UIState.state_world_str)
    {
        int world_index = GetStateManagerObjectWorld(object);
        if (world_index >= 0)
        {
            UpdateString64(G_UIState.state_world_str->string, "%d", world_index);
        }
        else
        {
            G_UIState.state_world_str->string[0] = '\0';
        }
    }

    if (object)
    {
        // Pipe the object's current frame values into the Geometry readouts.
        SyncNewtonoidRotation(object);
        const DataType geometry_types[] = {FLOAT, VECTOR2D, VECTOR2D, VECTOR2D};
        const StateManagerGeometryFormat geometry_formats[] = {
            STATE_MANAGER_GEOMETRY_NUMBER, STATE_MANAGER_GEOMETRY_VECTOR,
            STATE_MANAGER_GEOMETRY_VECTOR, STATE_MANAGER_GEOMETRY_VECTOR};
        const float *number_values[] = {&object->rotation, NULL, NULL, NULL};
        const Vector2d *vector_values[] = {
            NULL, &object->local_axis_x, &object->local_axis_y, &object->local_geometry_center};

        for (size_t i = 0; i < sizeof(geometry_textboxes) / sizeof(geometry_textboxes[0]); i++)
        {
            if (!geometry_textboxes[i])
            {
                continue;
            }

            BindTextboxData(geometry_textboxes[i], geometry_types[i],
                            (void *)(geometry_formats[i] == STATE_MANAGER_GEOMETRY_NUMBER
                                         ? (const void *)number_values[i]
                                         : (const void *)vector_values[i]));
            if (geometry_formats[i] == STATE_MANAGER_GEOMETRY_NUMBER)
            {
                WriteTextboxNumberIfUnfocused(geometry_textboxes[i], *number_values[i], 3);
            }
            else
            {
                WriteTextboxVectorIfUnfocused(geometry_textboxes[i], *vector_values[i]);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < sizeof(geometry_textboxes) / sizeof(geometry_textboxes[0]); i++)
        {
            if (geometry_textboxes[i])
            {
                WriteTextboxText(geometry_textboxes[i], "N/A");
                geometry_textboxes[i]->data.textbox.data_bind = NULL;
            }
        }
    }

    StateManagerFlagSource entity_source = {
        .kind = STATE_MANAGER_FLAG_SOURCE_ENTITY,
        .entity = object,
        .world = NULL,
        .cell = NULL,
        .use_collision_mask = false,
    };
    UpdateStateManagerButtonGroup(state_manager_identity_specs,
                                  sizeof(state_manager_identity_specs) / sizeof(state_manager_identity_specs[0]),
                                  entity_source);
    UpdateStateManagerButtonGroup(state_manager_behaviour_specs,
                                  sizeof(state_manager_behaviour_specs) / sizeof(state_manager_behaviour_specs[0]),
                                  entity_source);

    entity_source.use_collision_mask = true;
    UpdateStateManagerButtonGroup(state_manager_collision_specs,
                                  sizeof(state_manager_collision_specs) / sizeof(state_manager_collision_specs[0]),
                                  entity_source);

    StateManagerFlagSource world_source = {
        .kind = STATE_MANAGER_FLAG_SOURCE_WORLD,
        .entity = NULL,
        .world = world,
        .cell = NULL,
        .use_collision_mask = false,
    };
    UpdateStateManagerButtonGroup(state_manager_world_specs,
                                  sizeof(state_manager_world_specs) / sizeof(state_manager_world_specs[0]),
                                  world_source);

    StateManagerFlagSource cell_source = {
        .kind = STATE_MANAGER_FLAG_SOURCE_CELL,
        .entity = NULL,
        .world = NULL,
        .cell = UIState_GetSelectedCell(),
        .use_collision_mask = false,
    };
    UpdateStateManagerButtonGroup(state_manager_cell_specs,
                                  sizeof(state_manager_cell_specs) / sizeof(state_manager_cell_specs[0]),
                                  cell_source);
}
