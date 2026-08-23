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

// Module state.
static PanelSystem *state_manager_panel = NULL;
static int btn_action_delete_entity = BUTTON_ACTION_DELETE_ENTITY;

static Size view_section_size = UI_SIZE_CONTENT;
static bool state_manager_refresh_dirty = true;

// PHYS view readouts.
static UIElement *state_rotation_tbox = NULL;
static UIElement *state_basis_u_tbox = NULL;
static UIElement *state_basis_v_tbox = NULL;
static UIElement *state_geometry_center_tbox = NULL;
static UIElement *state_gameplay_section = NULL;

// Field visibility metadata.
typedef enum
{
    STATE_MANAGER_FIELD_VISIBILITY_ALWAYS,
    STATE_MANAGER_FIELD_VISIBILITY_DAMAGEABLE,
    STATE_MANAGER_FIELD_VISIBILITY_PROJECTILE,
} StateManagerFieldVisibilityRule;

typedef struct
{
    UIElement **textbox_slot;
    StateManagerFieldVisibilityRule visibility_rule;
} StateManagerFieldVisibilitySpec;

static const StateManagerFieldVisibilitySpec state_manager_gameplay_field_specs[] = {
    {&G_UIState.state_health_tbox, STATE_MANAGER_FIELD_VISIBILITY_DAMAGEABLE},
    {&G_UIState.state_max_health_tbox, STATE_MANAGER_FIELD_VISIBILITY_DAMAGEABLE},
    {&G_UIState.state_damage_tbox, STATE_MANAGER_FIELD_VISIBILITY_PROJECTILE},
};

// Which flag namespace a toggle belongs to.
typedef enum
{
    STATE_MANAGER_FLAG_CATEGORY_ENTITY_TYPE,
    STATE_MANAGER_FLAG_CATEGORY_ENTITY_ATTRIBUTE,
    STATE_MANAGER_FLAG_CATEGORY_ENTITY_STATUS,
    STATE_MANAGER_FLAG_CATEGORY_COLLISION_MASK,
    STATE_MANAGER_FLAG_CATEGORY_WORLD,
    STATE_MANAGER_FLAG_CATEGORY_CELL,
} StateManagerFlagCategory;

typedef struct
{
    const char *label;
    uint32_t flag;
    StateManagerFlagCategory category;
} StateManagerFlagSpec;

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
} StateManagerFlagSource;

typedef uint32_t *(*StateManagerFlagTargetFn)(StateManagerFlagSource source);

typedef struct
{
    StateManagerFlagSourceKind source_kind;
    StateManagerFlagTargetFn get_target;
} StateManagerFlagCategorySpec;

typedef struct
{
    StateManagerFlagCategory category;
    size_t first_spec;
    size_t first_button;
    size_t count;
} StateManagerFlagGroup;

// Field visibility helpers.
static bool StateManagerObjectMatchesFieldRule(const Newtonoid2d *object,
                                               StateManagerFieldVisibilityRule rule)
{
    if (!object)
    {
        return false;
    }

    switch (rule)
    {
        case STATE_MANAGER_FIELD_VISIBILITY_DAMAGEABLE:
            return (object->attribute_flags & FLAG_ATTR_DAMAGEABLE) != 0;
        case STATE_MANAGER_FIELD_VISIBILITY_PROJECTILE:
            return (object->entity_flags & FLAG_TYPE_PROJECTILE) != 0;
        case STATE_MANAGER_FIELD_VISIBILITY_ALWAYS:
        default:
            return true;
    }
}

static void StateManagerSetFieldRowEnabled(UIElement *textbox, bool is_enabled)
{
    if (!textbox || !textbox->parent)
    {
        return;
    }

    textbox->parent->is_enabled = is_enabled;
}

// Flag metadata.
static const StateManagerFlagSpec state_manager_flag_specs[] = {
    {"WALL", FLAG_TYPE_WALL, STATE_MANAGER_FLAG_CATEGORY_ENTITY_TYPE},
    {"NEWTONOID", FLAG_TYPE_NEWTONOID, STATE_MANAGER_FLAG_CATEGORY_ENTITY_TYPE},
    {"PROJECTILE", FLAG_TYPE_PROJECTILE, STATE_MANAGER_FLAG_CATEGORY_ENTITY_TYPE},
    {"EFFECT", FLAG_TYPE_EFFECT, STATE_MANAGER_FLAG_CATEGORY_ENTITY_TYPE},
    {"CAMERA", FLAG_TYPE_CAMERA, STATE_MANAGER_FLAG_CATEGORY_ENTITY_TYPE},

    {"DAMAGEABLE", FLAG_ATTR_DAMAGEABLE, STATE_MANAGER_FLAG_CATEGORY_ENTITY_ATTRIBUTE},
    {"VELOCITY", FLAG_ATTR_VELOCITY_ALIGNED, STATE_MANAGER_FLAG_CATEGORY_ENTITY_ATTRIBUTE},
    {"AFFECT OWNER", FLAG_ATTR_AFFECT_OWNER, STATE_MANAGER_FLAG_CATEGORY_ENTITY_ATTRIBUTE},
    {"RIGID", FLAG_ATTR_RIGID, STATE_MANAGER_FLAG_CATEGORY_ENTITY_ATTRIBUTE},

    {"ALIVE", FLAG_STATUS_ALIVE, STATE_MANAGER_FLAG_CATEGORY_ENTITY_STATUS},
    {"CLOCKED", FLAG_LIFETIME_CLOCKED, STATE_MANAGER_FLAG_CATEGORY_ENTITY_STATUS},

    {"WALL", FLAG_TYPE_WALL, STATE_MANAGER_FLAG_CATEGORY_COLLISION_MASK},
    {"NEWTONOID", FLAG_TYPE_NEWTONOID, STATE_MANAGER_FLAG_CATEGORY_COLLISION_MASK},
    {"PROJECTILE", FLAG_TYPE_PROJECTILE, STATE_MANAGER_FLAG_CATEGORY_COLLISION_MASK},
    {"EFFECT", FLAG_TYPE_EFFECT, STATE_MANAGER_FLAG_CATEGORY_COLLISION_MASK},
    {"CAMERA", FLAG_TYPE_CAMERA, STATE_MANAGER_FLAG_CATEGORY_COLLISION_MASK},

    {"ACTIVE", WORLD_FLAG_ACTIVE, STATE_MANAGER_FLAG_CATEGORY_WORLD},
    {"VISIBLE", WORLD_FLAG_VISIBLE, STATE_MANAGER_FLAG_CATEGORY_WORLD},
    {"SELECTABLE", WORLD_FLAG_SELECTABLE, STATE_MANAGER_FLAG_CATEGORY_WORLD},
    {"PHYSICS", WORLD_FLAG_PHYSICS_ENABLED, STATE_MANAGER_FLAG_CATEGORY_WORLD},
    {"SPAWNS", WORLD_FLAG_SPAWNS_ENABLED, STATE_MANAGER_FLAG_CATEGORY_WORLD},
    {"LOCKED", WORLD_FLAG_LOCKED, STATE_MANAGER_FLAG_CATEGORY_WORLD},
    {"DRAG", WORLD_FLAG_DRAGGABLE, STATE_MANAGER_FLAG_CATEGORY_WORLD},

    {"SOLID", CELL_FLAG_SOLID, STATE_MANAGER_FLAG_CATEGORY_CELL},
    {"WALKABLE", CELL_FLAG_WALKABLE, STATE_MANAGER_FLAG_CATEGORY_CELL},
    {"HAZARD", CELL_FLAG_HAZARDOUS, STATE_MANAGER_FLAG_CATEGORY_CELL},
    {"SPAWN", CELL_FLAG_SPAWNABLE, STATE_MANAGER_FLAG_CATEGORY_CELL},
};

static UIElement *state_manager_flag_buttons[ARRAY_COUNT(state_manager_flag_specs)] = {0};
static StateManagerFlagGroup state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_CELL + 1] = {0};
// Keep every flag button in one contiguous array so refresh groups use stable offsets.
static size_t state_manager_button_count = 0;

static const uint32_t state_manager_type_flags = FLAG_TYPE_WALL | FLAG_TYPE_NEWTONOID |
                                                 FLAG_TYPE_PROJECTILE | FLAG_TYPE_EFFECT | FLAG_TYPE_CAMERA;

void MarkStateManagerRefreshDirty(void)
{
    state_manager_refresh_dirty = true;
}

// Selection callback.
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

// Flag target accessors.
static uint32_t *GetStateManagerEntityTypeTarget(StateManagerFlagSource source)
{
    return source.entity ? (uint32_t *)&source.entity->entity_flags : NULL;
}

static uint32_t *GetStateManagerEntityAttributeTarget(StateManagerFlagSource source)
{
    return source.entity ? (uint32_t *)&source.entity->attribute_flags : NULL;
}

static uint32_t *GetStateManagerEntityStatusTarget(StateManagerFlagSource source)
{
    return source.entity ? (uint32_t *)&source.entity->status_flags : NULL;
}

static uint32_t *GetStateManagerCollisionMaskTarget(StateManagerFlagSource source)
{
    return source.entity ? (uint32_t *)&source.entity->collision_mask : NULL;
}

static uint32_t *GetStateManagerWorldTarget(StateManagerFlagSource source)
{
    return source.world ? (uint32_t *)&source.world->flags : NULL;
}

static uint32_t *GetStateManagerCellTarget(StateManagerFlagSource source)
{
    return source.cell ? (uint32_t *)&source.cell->flags : NULL;
}

// Flag source routing.
static const StateManagerFlagCategorySpec state_manager_category_specs[] = {
    [STATE_MANAGER_FLAG_CATEGORY_ENTITY_TYPE] = {
        STATE_MANAGER_FLAG_SOURCE_ENTITY, GetStateManagerEntityTypeTarget},
    [STATE_MANAGER_FLAG_CATEGORY_ENTITY_ATTRIBUTE] = {
        STATE_MANAGER_FLAG_SOURCE_ENTITY, GetStateManagerEntityAttributeTarget},
    [STATE_MANAGER_FLAG_CATEGORY_ENTITY_STATUS] = {STATE_MANAGER_FLAG_SOURCE_ENTITY, GetStateManagerEntityStatusTarget},
    [STATE_MANAGER_FLAG_CATEGORY_COLLISION_MASK] = {STATE_MANAGER_FLAG_SOURCE_ENTITY, GetStateManagerCollisionMaskTarget},
    [STATE_MANAGER_FLAG_CATEGORY_WORLD] = {STATE_MANAGER_FLAG_SOURCE_WORLD, GetStateManagerWorldTarget},
    [STATE_MANAGER_FLAG_CATEGORY_CELL] = {STATE_MANAGER_FLAG_SOURCE_CELL, GetStateManagerCellTarget},
};

static const StateManagerFlagCategorySpec *GetStateManagerCategorySpec(StateManagerFlagCategory category)
{
    if (category < STATE_MANAGER_FLAG_CATEGORY_ENTITY_TYPE ||
        category > STATE_MANAGER_FLAG_CATEGORY_CELL)
    {
        return NULL;
    }

    return &state_manager_category_specs[category];
}

static bool StateManagerFlag_IsApplicable(StateManagerFlagCategory category, StateManagerFlagSource source)
{
    const StateManagerFlagCategorySpec *category_spec = GetStateManagerCategorySpec(category);
    return category_spec && source.kind == category_spec->source_kind &&
           category_spec->get_target(source) != NULL;
}

static uint32_t *StateManagerFlag_GetTarget(StateManagerFlagCategory category, StateManagerFlagSource source)
{
    const StateManagerFlagCategorySpec *category_spec = GetStateManagerCategorySpec(category);
    return category_spec ? category_spec->get_target(source) : NULL;
}

// Flag controls.
static void HandleStateManagerFlagClick(UIElement *button)
{
    if (!button || !button->data.button.user_data)
    {
        return;
    }

    const StateManagerFlagSpec *spec = (const StateManagerFlagSpec *)button->data.button.user_data;
    Newtonoid2d *object = UIState_GetSelectedObject();
    World2d *world = Universe_GetSelectedWorld(&G_Universe);
    Cell *cell = UIState_GetSelectedCell();

    StateManagerFlagSource source = {0};
    if (object)
    {
        source.kind = STATE_MANAGER_FLAG_SOURCE_ENTITY;
        source.entity = object;
    }
    else if (world && spec->category == STATE_MANAGER_FLAG_CATEGORY_WORLD)
    {
        source.kind = STATE_MANAGER_FLAG_SOURCE_WORLD;
        source.world = world;
    }
    else if (cell && spec->category == STATE_MANAGER_FLAG_CATEGORY_CELL)
    {
        source.kind = STATE_MANAGER_FLAG_SOURCE_CELL;
        source.cell = cell;
    }

    if (!StateManagerFlag_IsApplicable(spec->category, source))
    {
        return;
    }

    uint32_t *target = StateManagerFlag_GetTarget(spec->category, source);
    if (!target)
    {
        return;
    }

    // Entity-type toggles are exclusive within the type namespace.
    if (spec->category == STATE_MANAGER_FLAG_CATEGORY_ENTITY_TYPE)
    {
        *target &= ~state_manager_type_flags;
        *target |= spec->flag;
    }
    else if (*target & spec->flag)
    {
        *target &= ~spec->flag;
    }
    else
    {
        *target |= spec->flag;
    }

    MarkStateManagerRefreshDirty();
}

static UIElement *CreateStateManagerBoundButton(UIElement *parent, const StateManagerFlagSpec *spec,
                                                const UIPalette *palette)
{
    return CreateUIButtonDefault(parent, UI_ELEMENT_BUTTON_SIMPLE, spec->label,
                                 ui_standard_button_size, ui_standard_button_padding,
                                 palette, HandleStateManagerFlagClick, (void *)spec, NULL);
}

static void InitStateManagerFlagGroups(void)
{
    state_manager_button_count = 0;
    for (size_t i = 0; i < ARRAY_COUNT(state_manager_flag_buttons); i++)
    {
        state_manager_flag_buttons[i] = NULL;
    }

    for (size_t i = 0; i < ARRAY_COUNT(state_manager_flag_groups); i++)
    {
        state_manager_flag_groups[i] = (StateManagerFlagGroup){
            .category = (StateManagerFlagCategory)i,
            .first_spec = ARRAY_COUNT(state_manager_flag_specs),
            .first_button = 0,
            .count = 0,
        };
    }

    for (size_t i = 0; i < ARRAY_COUNT(state_manager_flag_specs); i++)
    {
        StateManagerFlagGroup *group = &state_manager_flag_groups[state_manager_flag_specs[i].category];
        if (group->count == 0)
        {
            group->first_spec = i;
        }
        group->count++;
    }
}

static void CreateStateManagerSectionButtons(UIElement *section, StateManagerFlagGroup *group,
                                             size_t *button_index)
{
    if (!section || !group || !button_index)
    {
        return;
    }

    group->first_button = *button_index;
    const StateManagerFlagSpec *specs = state_manager_flag_specs + group->first_spec;
    for (size_t i = 0; i < group->count; i++)
    {
        if (*button_index >= ARRAY_COUNT(state_manager_flag_buttons))
        {
            return;
        }

        state_manager_flag_buttons[*button_index] = CreateStateManagerBoundButton(
            section, &specs[i], state_manager_panel->palette);
        (*button_index)++;
    }
}

// View construction.
// Create and register a state-manager view with its requested child layout.
static View *CreateStateManagerView(int view_id, bool is_draggable, bool is_enabled,
                                    Spacing child_spacing)
{
    View *view = PanelSystem_CreateView(state_manager_panel, view_id);
    UIElement *container = view ? view->container : NULL;
    if (!view)
    {
        return NULL;
    }

    container->child_spacing = child_spacing;
    container->is_draggable = is_draggable;
    container->is_enabled = is_enabled;

    return view;
}

// Build the four StateManager views and their sections.
static void InitPhysStateView(void)
{
    View *physics_view = CreateStateManagerView(
        STATE_MANAGER_PHYSICS_VIEW, true, true, ui_zero_x_inline_wrap_spacing);
    if (!physics_view)
    {
        return;
    }
    UIElement *view_cont = physics_view->container;

    UIElement *identity_section = CreateViewSection_StackWrap(view_cont, "Identity", view_section_size,
                                                               state_manager_panel->palette);
    const UIFieldSpec identity_specs[] = {
        {"Id", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, INT, &G_UIState.state_id_tbox, NULL},
        {"World", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, INT, NULL, &G_UIState.state_world_str},
    };
    InitUIFields(identity_section, identity_specs,
                 ARRAY_COUNT(identity_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    UIElement *physics_section = CreateViewSection_StackWrap(view_cont, "Physics", view_section_size,
                                                              state_manager_panel->palette);
    const UIFieldSpec physics_specs[] = {
        {"Mass", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.state_mass_tbox, NULL},
        {"Anchor", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.state_pos_c_tbox, NULL},
        {"Vel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.state_vel_tbox, NULL},
        {"Accel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.state_accel_tbox, NULL},
        {"Moment", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.state_moment_tbox, NULL},
        {"AngVel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.state_angular_velocity_tbox, NULL},
        {"AngAccel", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, &G_UIState.state_angular_acceleration_tbox, NULL},
    };
    InitUIFields(physics_section, physics_specs,
                 ARRAY_COUNT(physics_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    UIElement *geometry_section = CreateViewSection_StackWrap(view_cont, "Geometry", view_section_size,
                                                               state_manager_panel->palette);
    const UIFieldSpec geometry_specs[] = {
        {"Bounds.min", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, VECTOR2D, &G_UIState.state_pos_tl_tbox, NULL},
        {"Geo.center", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, VECTOR2D, &state_geometry_center_tbox, NULL},
        {"Rot", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &state_rotation_tbox, NULL},
        {"Basis u", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &state_basis_u_tbox, NULL},
        {"Basis v", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &state_basis_v_tbox, NULL},
    };
    InitUIFields(geometry_section, geometry_specs,
                 ARRAY_COUNT(geometry_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    UIElement *gameplay_section = CreateViewSection_StackWrap(view_cont, "Gameplay", view_section_size,
                                                               state_manager_panel->palette);
    state_gameplay_section = gameplay_section;
    const UIFieldSpec gameplay_specs[] = {
        {"Health", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.state_health_tbox, NULL},
        {"MaxHealth", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.state_max_health_tbox, NULL},
        {"Damage", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.state_damage_tbox, NULL},
    };
    InitUIFields(gameplay_section, gameplay_specs,
                 ARRAY_COUNT(gameplay_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    CreateUIButtonDefault(view_cont, UI_ELEMENT_BUTTON_SUBMIT,
                          "DELETE", ui_standard_button_size, ui_standard_button_padding,
                          state_manager_panel->palette, HandleBtnSubmitClick,
                          &btn_action_delete_entity, NULL);
}

static void InitAttributeStateView(void)
{
    View *attributes_view = CreateStateManagerView(
        STATE_MANAGER_ATTRIBUTES_VIEW, true, false, ui_zero_x_inline_wrap_spacing);
    if (!attributes_view)
    {
        return;
    }
    UIElement *view_cont = attributes_view->container;

    // Customise the View
    UIElement *identity_section = CreateViewSection_StackWrap(view_cont, "Entity", view_section_size,
                                                               state_manager_panel->palette);
    UIElement *attribute_section = CreateViewSection_StackWrap(view_cont, "Attributes", view_section_size,
                                                                state_manager_panel->palette);
    UIElement *status_section = CreateViewSection_StackWrap(view_cont, "Status", view_section_size,
                                                             state_manager_panel->palette);
    UIElement *collision_section = CreateViewSection_StackWrap(view_cont, "Collision", view_section_size,
                                                               state_manager_panel->palette);

    CreateStateManagerSectionButtons(identity_section,
                                     &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_ENTITY_TYPE],
                                     &state_manager_button_count);
    CreateStateManagerSectionButtons(attribute_section,
                                     &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_ENTITY_ATTRIBUTE],
                                     &state_manager_button_count);
    CreateStateManagerSectionButtons(status_section,
                                     &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_ENTITY_STATUS],
                                     &state_manager_button_count);
    CreateStateManagerSectionButtons(collision_section,
                                     &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_COLLISION_MASK],
                                     &state_manager_button_count);
}

static void InitWorldStateView(void)
{
    View *world_view = CreateStateManagerView(
        STATE_MANAGER_WORLD_VIEW, true, false, ui_zero_x_inline_wrap_spacing);
    if (!world_view)
    {
        return;
    }

    UIElement *world_section = CreateViewSection_StackWrap(world_view->container, "World", view_section_size,
                                                            state_manager_panel->palette);

    CreateStateManagerSectionButtons(world_section, &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_WORLD],
                                     &state_manager_button_count);
}

static void InitCellStateView(void)
{
    View *cell_view = CreateStateManagerView(
        STATE_MANAGER_CELL_STATE_VIEW, true, false, ui_zero_x_inline_wrap_spacing);
    if (!cell_view)
    {
        return;
    }
    UIElement *cell_container = cell_view->container;

    const UIFieldSpec cell_specs[] = {
        {"Index", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.cell_id_str},
        {"Occu", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.cell_occu_str},
        {"Value", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.cell_value_str},
        {"Fill", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &G_UIState.cell_fill_str},
    };

    UIElement *cell_phys_section = CreateViewSection_StackWrap(cell_container, "Cell", view_section_size,
                                                               state_manager_panel->palette);
    InitUIFields(cell_phys_section, cell_specs,
                 ARRAY_COUNT(cell_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    UIElement *cell_flags_section = CreateViewSection_StackWrap(cell_container, "Flags", view_section_size,
                                                                state_manager_panel->palette);

    CreateStateManagerSectionButtons(cell_flags_section,
                                     &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_CELL],
                                     &state_manager_button_count);
}

// Resolve the selected object's world for the identity readout.
static int GetStateManagerObjectWorld(const Newtonoid2d *object)
{
    if (!object)
    {
        return -1;
    }

    // Look up by stable ID (O(worlds)) instead of scanning every entity by pointer.
    int world_index = -1;
    Universe_GetEntityByID(&G_Universe, object->id, &world_index);
    return world_index;
}

// Field visibility refresh.
static void UpdateStateManagerGameplayFieldVisibility(const Newtonoid2d *object)
{
    bool any_field_visible = false;
    for (size_t i = 0; i < ARRAY_COUNT(state_manager_gameplay_field_specs); i++)
    {
        UIElement *textbox = state_manager_gameplay_field_specs[i].textbox_slot
                                 ? *state_manager_gameplay_field_specs[i].textbox_slot
                                 : NULL;
        bool visible = StateManagerObjectMatchesFieldRule(
            object, state_manager_gameplay_field_specs[i].visibility_rule);
        StateManagerSetFieldRowEnabled(textbox, visible);
        any_field_visible |= visible;
    }

    if (state_gameplay_section)
    {
        if (any_field_visible)
        {
            EnableElement(state_gameplay_section);
        }
        else
        {
            DisableElement(state_gameplay_section);
        }
    }
}

// Read the current value for a flag button from its selected source.
static uint32_t GetStateManagerSourceFlags(const StateManagerFlagSpec *spec, StateManagerFlagSource source)
{
    uint32_t *target = StateManagerFlag_GetTarget(spec->category, source);
    return target ? *target : 0;
}

// Refresh the labels and active state for one flag group.
static void UpdateStateManagerButtonGroup(const StateManagerFlagGroup *group,
                                          StateManagerFlagSource source)
{
    const StateManagerFlagSpec *specs = state_manager_flag_specs + group->first_spec;
    UIElement **buttons = state_manager_flag_buttons + group->first_button;
    for (size_t i = 0; i < group->count; i++)
    {
        UIElement *button = buttons[i];
        if (!button)
        {
            continue;
        }
        uint32_t source_flags = GetStateManagerSourceFlags(&specs[i], source);
        bool enabled = source.kind != STATE_MANAGER_FLAG_SOURCE_NONE &&
                       (source_flags & specs[i].flag) != 0;
        UpdateString64(button->data.button.label.string,
                       "%s: %s", specs[i].label, enabled ? "ON" : "OFF");
        button->is_enabled = true;
    }
}

// Push the selected object, world, cell, and capability state into the UI.
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
        TextboxField geometry_fields[] = {
            {geometry_textboxes[0], FLOAT, &object->rotation, 3, "N/A"},
            {geometry_textboxes[1], VECTOR2D, &object->local_axis_x, 0, "N/A"},
            {geometry_textboxes[2], VECTOR2D, &object->local_axis_y, 0, "N/A"},
            {geometry_textboxes[3], VECTOR2D, &object->local_geometry_center, 0, "N/A"},
        };
        RefreshTextboxFields(geometry_fields, ARRAY_COUNT(geometry_fields));
    }
    else
    {
        TextboxField geometry_fields[] = {
            {geometry_textboxes[0], FLOAT, NULL, 0, "N/A"},
            {geometry_textboxes[1], VECTOR2D, NULL, 0, "N/A"},
            {geometry_textboxes[2], VECTOR2D, NULL, 0, "N/A"},
            {geometry_textboxes[3], VECTOR2D, NULL, 0, "N/A"},
        };
        RefreshTextboxFields(geometry_fields, ARRAY_COUNT(geometry_fields));
    }

    UpdateStateManagerGameplayFieldVisibility(object);

    StateManagerFlagSource entity_source = {
        .kind = object ? STATE_MANAGER_FLAG_SOURCE_ENTITY : STATE_MANAGER_FLAG_SOURCE_NONE,
        .entity = object,
        .world = NULL,
        .cell = NULL,
    };
    UpdateStateManagerButtonGroup(
        &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_ENTITY_TYPE], entity_source);
    UpdateStateManagerButtonGroup(
        &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_ENTITY_ATTRIBUTE], entity_source);
    UpdateStateManagerButtonGroup(
        &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_ENTITY_STATUS], entity_source);
    UpdateStateManagerButtonGroup(
        &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_COLLISION_MASK], entity_source);

    StateManagerFlagSource world_source = {
        .kind = world ? STATE_MANAGER_FLAG_SOURCE_WORLD : STATE_MANAGER_FLAG_SOURCE_NONE,
        .entity = NULL,
        .world = world,
        .cell = NULL,
    };
    UpdateStateManagerButtonGroup(
        &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_WORLD], world_source);

    StateManagerFlagSource cell_source = {
        .kind = UIState_GetSelectedCell() ? STATE_MANAGER_FLAG_SOURCE_CELL : STATE_MANAGER_FLAG_SOURCE_NONE,
        .entity = NULL,
        .world = NULL,
        .cell = UIState_GetSelectedCell(),
    };
    UpdateStateManagerButtonGroup(
        &state_manager_flag_groups[STATE_MANAGER_FLAG_CATEGORY_CELL], cell_source);
}

// Public lifecycle.
void InitStateManagerSystem(void)
{
    const char *labels[] = {"PHYS", "ATTRI", "WORLD", "CELL"};
    state_manager_panel = PanelSystem_CreateStandard(&entity_panel_viewport, 4,
                                                     labels, ARRAY_COUNT(labels),
                                                     NULL, &ui_default_palette,
                                                     ui_standard_stack_spacing);
    if (!state_manager_panel)
        return;

    // Cap each section at the StateManager panel height so stacked fields wrap into columns.
    view_section_size = UI_SIZE_CONTENT_MAX(0.0f, (float)state_manager_panel->space.rows);

    InitStateManagerFlagGroups();
    InitPhysStateView();
    InitAttributeStateView();
    InitWorldStateView();
    InitCellStateView();

    // Select the initial view after all state-manager views have been registered.
    if (state_manager_panel->selectors.count > 0)
    {
        ViewSelector *view_selector = *((ViewSelector **)LArray_Get(&state_manager_panel->selectors, 0));
        PanelSystem_SelectView(view_selector, 0);
    }

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

PanelSystem *GetStateManagerPanelSystem(void)
{
    return state_manager_panel;
}

// Destroy the state-manager panel and clear its cached UI references.
void DestroyStateManagerSystem(void)
{
    UIState_SetSelectionChangedCallback(NULL, NULL);

    PanelSystem *panel = state_manager_panel;
    state_manager_panel = NULL;
    PanelSystem_Destroy(panel);

    state_rotation_tbox = NULL;
    state_basis_u_tbox = NULL;
    state_basis_v_tbox = NULL;
    state_geometry_center_tbox = NULL;
    state_gameplay_section = NULL;
    state_manager_button_count = 0;
    state_manager_refresh_dirty = true;
    for (size_t i = 0; i < ARRAY_COUNT(state_manager_flag_buttons); i++)
    {
        state_manager_flag_buttons[i] = NULL;
    }

    G_UIState.state_id_tbox = NULL;
    G_UIState.state_mass_tbox = NULL;
    G_UIState.state_pos_tl_tbox = NULL;
    G_UIState.state_pos_c_tbox = NULL;
    G_UIState.state_vel_tbox = NULL;
    G_UIState.state_accel_tbox = NULL;
    G_UIState.state_moment_tbox = NULL;
    G_UIState.state_angular_velocity_tbox = NULL;
    G_UIState.state_angular_acceleration_tbox = NULL;
    G_UIState.state_health_tbox = NULL;
    G_UIState.state_max_health_tbox = NULL;
    G_UIState.state_damage_tbox = NULL;
    G_UIState.state_id_str = NULL;
    G_UIState.state_mass_str = NULL;
    G_UIState.state_pos_tl_str = NULL;
    G_UIState.state_pos_c_str = NULL;
    G_UIState.state_vel_str = NULL;
    G_UIState.state_accel_str = NULL;
    G_UIState.state_moment_str = NULL;
    G_UIState.state_angular_velocity_str = NULL;
    G_UIState.state_angular_acceleration_str = NULL;
    G_UIState.state_health_str = NULL;
    G_UIState.state_max_health_str = NULL;
    G_UIState.state_damage_str = NULL;
    G_UIState.state_world_str = NULL;
    G_UIState.cell_id_str = NULL;
    G_UIState.cell_occu_str = NULL;
    G_UIState.cell_value_str = NULL;
    G_UIState.cell_fill_str = NULL;
}