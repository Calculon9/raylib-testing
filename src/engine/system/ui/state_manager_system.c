#include "system/ui/state_manager_system.h"
#include <string.h>
#include "system/command_queue.h"
#include "system/panel_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/utility_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"
#include "physics/newtonoid.h"
#include "math/coordinate_space.h"
#include "world/universe.h"
#include "entities/entity_registry.h"
#include "entities/entity_components.h"

// ============================================================================
// Module State
// ============================================================================

static PanelSystem *state_manager_panel = NULL;

static Size view_section_size = UI_SIZE_CONTENT;
static bool state_manager_refresh_dirty = true;

// Component button binding: associates a button control with an EntityComponentType.
typedef struct StateManagerComponentButton
{
    const char *label;
    EntityComponentType type;
    UIElement *button;
} StateManagerComponentButton;

// UIElement and String64 references owned by the State Manager panel.
typedef struct StateManagerUI
{
    // Action codes
    int delete_action;

    // Component toggle buttons
    StateManagerComponentButton comp_buttons[3];

    // Identity readouts
    UIElement *id_tbox;
    UIElement *slot_tbox;
    UIElement *generation_tbox;
    UIElement *world_tbox;

    // Physics readouts
    UIElement *mass_tbox;
    UIElement *restitution_tbox;
    UIElement *friction_tbox;
    UIElement *pos_c_tbox;
    UIElement *vel_tbox;
    UIElement *accel_tbox;
    UIElement *moment_tbox;
    UIElement *angular_velocity_tbox;
    UIElement *angular_acceleration_tbox;

    // Geometry readouts
    UIElement *pos_tl_tbox;
    UIElement *geometry_center_tbox;
    UIElement *rotation_tbox;
    UIElement *basis_u_tbox;
    UIElement *basis_v_tbox;

    // Gameplay readouts & section
    UIElement *gameplay_section;
    UIElement *health_tbox;
    UIElement *max_health_tbox;
    UIElement *damage_tbox;

    // Components readouts & section
    UIElement *components_section;
    UIElement *comp_portal_cooldown_tbox;
    String64 *comp_portal_cooldown_str;
    UIElement *comp_portal_entrant_mask_tbox;
    String64 *comp_portal_entrant_mask_str;
    UIElement *comp_relation_type_tbox;
    String64 *comp_relation_type_str;
    UIElement *comp_relation_target_tbox;
    String64 *comp_relation_target_str;
    UIElement *comp_relation_active_tbox;
    String64 *comp_relation_active_str;

    // World physics readouts
    UIElement *world_restitution_tbox;
    UIElement *world_friction_tbox;

    // Cell readouts
    String64 *cell_id_str;
    String64 *cell_occu_str;
    String64 *cell_value_str;
    String64 *cell_fill_str;
} StateManagerUI;

static StateManagerUI s_sm_ui = {0};

// Flag button binding: associates a button control with its display label and underlying bitflag.
typedef struct StateManagerFlagButton
{
    const char *label;
    uint32_t flag;
    UIElement *button;
} StateManagerFlagButton;

// All flag definitions and their bound controls stay together as module state.
typedef struct StateManagerFlags
{
    uint32_t type_flags;
    StateManagerFlagButton entity_type[5];
    StateManagerFlagButton entity_attribute[7];
    StateManagerFlagButton entity_status[3];
    StateManagerFlagButton collision_mask[5];
    StateManagerFlagButton world[7];
    StateManagerFlagButton cell[4];
} StateManagerFlags;

static StateManagerFlags s_sm_flags = {
    .type_flags = ENTITY_FLAG_WALL | ENTITY_FLAG_NEWTONOID |
                  ENTITY_FLAG_PROJECTILE | ENTITY_FLAG_EFFECT | ENTITY_FLAG_CAMERA,
    .entity_type = {
        {"WALL", ENTITY_FLAG_WALL, NULL},
        {"NEWTONOID", ENTITY_FLAG_NEWTONOID, NULL},
        {"PROJECTILE", ENTITY_FLAG_PROJECTILE, NULL},
        {"EFFECT", ENTITY_FLAG_EFFECT, NULL},
        {"CAMERA", ENTITY_FLAG_CAMERA, NULL},
    },
    .entity_attribute = {
        {"DAMAGEABLE", ENTITY_ATTR_FLAG_DAMAGEABLE, NULL},
        {"VELOCITY", ENTITY_ATTR_FLAG_VELOCITY_ALIGNED, NULL},
        {"AFFECT OWNER", ENTITY_ATTR_FLAG_AFFECT_OWNER, NULL},
        {"RIGID", ENTITY_ATTR_FLAG_RIGID, NULL},
        {"POSITION LOCKED", ENTITY_ATTR_FLAG_POSITION_LOCKED, NULL},
        {"SENSOR", ENTITY_ATTR_FLAG_SENSOR, NULL},
        {"NO CONTACT RESPONSE", ENTITY_ATTR_FLAG_NO_CONTACT_RESPONSE, NULL},
    },
    .entity_status = {
        {"ALIVE", ENTITY_STATUS_FLAG_ALIVE, NULL},
        {"SLEEPING", ENTITY_STATUS_FLAG_SLEEPING, NULL},
        {"CLOCKED", ENTITY_STATUS_FLAG_CLOCKED, NULL},
    },
    .collision_mask = {
        {"WALL", ENTITY_FLAG_WALL, NULL},
        {"NEWTONOID", ENTITY_FLAG_NEWTONOID, NULL},
        {"PROJECTILE", ENTITY_FLAG_PROJECTILE, NULL},
        {"EFFECT", ENTITY_FLAG_EFFECT, NULL},
        {"CAMERA", ENTITY_FLAG_CAMERA, NULL},
    },
    .world = {
        {"ACTIVE", WORLD_FLAG_ACTIVE, NULL},
        {"VISIBLE", WORLD_FLAG_VISIBLE, NULL},
        {"SELECTABLE", WORLD_FLAG_SELECTABLE, NULL},
        {"PHYSICS", WORLD_FLAG_PHYSICS_ENABLED, NULL},
        {"SPAWNS", WORLD_FLAG_SPAWNS_ENABLED, NULL},
        {"LOCKED", WORLD_FLAG_LOCKED, NULL},
        {"DRAG", WORLD_FLAG_DRAGGABLE, NULL},
    },
    .cell = {
        {"SOLID", CELL_FLAG_SOLID, NULL},
        {"WALKABLE", CELL_FLAG_WALKABLE, NULL},
        {"HAZARD", CELL_FLAG_HAZARDOUS, NULL},
        {"SPAWN", CELL_FLAG_SPAWNABLE, NULL},
    },
};

// ============================================================================
// Refresh Invalidation
// ============================================================================

// Mark the State Manager for a refresh on its next draw.
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

// ============================================================================
// Flag Interaction
// ============================================================================

// Toggle an exclusive entity type flag on the currently selected object.
static void HandleEntityTypeFlagClick(UIElement *button)
{
    Newtonoid2d *object = UIState_GetSelectedObject();
    const StateManagerFlagButton *spec = (const StateManagerFlagButton *)button->data.button.user_data;
    if (!object || !spec)
    {
        return;
    }

    object->entity_flags &= ~s_sm_flags.type_flags;
    object->entity_flags |= spec->flag;
    MarkStateManagerRefreshDirty();
}

// Toggle an entity attribute flag on the currently selected object.
static void HandleEntityAttributeFlagClick(UIElement *button)
{
    Newtonoid2d *object = UIState_GetSelectedObject();
    const StateManagerFlagButton *spec = (const StateManagerFlagButton *)button->data.button.user_data;
    if (!object || !spec)
    {
        return;
    }

    object->attribute_flags ^= spec->flag;
    MarkStateManagerRefreshDirty();
}

// Toggle an entity status flag on the currently selected object.
static void HandleEntityStatusFlagClick(UIElement *button)
{
    Newtonoid2d *object = UIState_GetSelectedObject();
    const StateManagerFlagButton *spec = (const StateManagerFlagButton *)button->data.button.user_data;
    if (!object || !spec)
    {
        return;
    }

    if (spec->flag == ENTITY_STATUS_FLAG_SLEEPING)
    {
        if (object->status_flags & ENTITY_STATUS_FLAG_SLEEPING)
        {
            WakeUp(object);
        }
        else
        {
            ApplySleep(object);
        }
    }
    else
    {
        // The XOR operation equates to a toggling of the flag.
        object->status_flags ^= spec->flag;
    }

    MarkStateManagerRefreshDirty();
}

// Toggle a collision mask flag on the currently selected object.
static void HandleCollisionMaskFlagClick(UIElement *button)
{
    Newtonoid2d *object = UIState_GetSelectedObject();
    const StateManagerFlagButton *spec = (const StateManagerFlagButton *)button->data.button.user_data;
    if (!object || !spec)
    {
        return;
    }

    object->collision_mask ^= spec->flag;
    MarkStateManagerRefreshDirty();
}

// Toggle a world flag on the currently selected world.
static void HandleWorldFlagClick(UIElement *button)
{
    World2d *world = Universe_GetSelectedWorld(&G_Universe);
    const StateManagerFlagButton *spec = (const StateManagerFlagButton *)button->data.button.user_data;
    if (!world || !spec)
    {
        return;
    }

    world->flags ^= spec->flag;
    MarkStateManagerRefreshDirty();
}

// Toggle a cell flag on the currently selected cell.
static void HandleCellFlagClick(UIElement *button)
{
    Cell *cell = UIState_GetSelectedCell();
    const StateManagerFlagButton *spec = (const StateManagerFlagButton *)button->data.button.user_data;
    if (!cell || !spec)
    {
        return;
    }

    cell->flags ^= spec->flag;
    MarkStateManagerRefreshDirty();
}

// Toggle attachment of a component on the currently selected object.
static void HandleComponentToggleClick(UIElement *button)
{
    Newtonoid2d *object = UIState_GetSelectedObject();
    if (!object || object->id == INVALID_ENTITY_ID || !button || !button->data.button.user_data)
    {
        return;
    }

    const StateManagerComponentButton *spec = (const StateManagerComponentButton *)button->data.button.user_data;
    EntityComponentType type = spec->type;

    if (EntityRegistry_HasComponent(object->id, type))
    {
        EnqueueRemoveComponent(object->id, type);
    }
    else
    {
        EntityComponent comp = {.type = type};
        switch (type)
        {
        case ENTITY_COMPONENT_PORTAL:
            PortalEntity_Initialise(&comp.data.portal, (PortalDestination){INVALID_ENTITY_ID},
                                    ENTITY_FLAG_NEWTONOID | ENTITY_FLAG_PROJECTILE, 30);
            break;
        case ENTITY_COMPONENT_ROTOR:
            memset(&comp.data.rotor, 0, sizeof(comp.data.rotor));
            break;
        case ENTITY_COMPONENT_GEAR:
            memset(&comp.data.gear, 0, sizeof(comp.data.gear));
            break;
        case ENTITY_COMPONENT_RELATION:
        case ENTITY_COMPONENT_NONE:
        default:
            return;
        }
        EnqueueAttachComponent(object->id, &comp);
    }
}

// Create toggle buttons within a section and bind them to their respective flag definitions.
static void CreateFlagButtons(UIElement *section, StateManagerFlagButton *buttons, size_t count, UIEventHandler click_handler)
{
    for (size_t i = 0; i < count; i++)
    {
        buttons[i].button = CreateUIButtonDefault(
            section, UI_ELEMENT_BUTTON_SIMPLE, buttons[i].label,
            ui_wide_button_size, ui_standard_button_padding,
            state_manager_panel->palette, click_handler, (void *)&buttons[i], NULL);
    }
}

// Update the label and active state for a set of flag buttons based on the current bitmask.
static void UpdateFlagButtons(StateManagerFlagButton *buttons, size_t count, uint32_t current_flags, bool is_valid)
{
    for (size_t i = 0; i < count; i++)
    {
        if (!buttons[i].button)
        {
            continue;
        }

        bool enabled = is_valid && ((current_flags & buttons[i].flag) != 0);
        UpdateString64(buttons[i].button->data.button.label.string,
                       "%s: %s", buttons[i].label, enabled ? "ON" : "OFF");
        buttons[i].button->is_enabled = true;
    }
}

// ============================================================================
// View Construction
// ============================================================================

// Create and register a state-manager view with its requested child layout.
static View *CreateStateManagerView(int view_id, bool is_draggable, bool is_enabled, Spacing child_spacing)
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
        STATE_MANAGER_PHYSICS_VIEW, true, true, ui_zero_inline_spacing);
    if (!physics_view)
    {
        return;
    }
    View_SetScrollableX(physics_view, true);

    UIElement *view_cont = physics_view->container;

    UIElement *identity_section = CreateViewSection_StackWrap(view_cont, "Identity", view_section_size,
                                                              state_manager_panel->palette);
    const UIFieldSpec identity_specs[] = {
        {"Id:", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, INT, &s_sm_ui.id_tbox, NULL},
        {"Slot:", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, INT, &s_sm_ui.slot_tbox, NULL},
        {"Generation:", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, INT, &s_sm_ui.generation_tbox, NULL},
        {"World", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, INT, &s_sm_ui.world_tbox, NULL},
    };
    InitUIFields(identity_section, identity_specs, ARRAY_COUNT(identity_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    UIElement *physics_section = CreateViewSection_StackWrap(view_cont, "Physics", view_section_size,
                                                             state_manager_panel->palette);
    const UIFieldSpec physics_specs[] = {
        {"Mass", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &s_sm_ui.mass_tbox, NULL},
        {"Restitution", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &s_sm_ui.restitution_tbox, NULL},
        {"Friction", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &s_sm_ui.friction_tbox, NULL},
        {"Anchor", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &s_sm_ui.pos_c_tbox, NULL},
        {"Vel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &s_sm_ui.vel_tbox, NULL},
        {"Accel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &s_sm_ui.accel_tbox, NULL},
        {"Moment", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &s_sm_ui.moment_tbox, NULL},
        {"AngVel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &s_sm_ui.angular_velocity_tbox, NULL},
        {"AngAccel", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, &s_sm_ui.angular_acceleration_tbox, NULL},
    };
    InitUIFields(physics_section, physics_specs,
                 ARRAY_COUNT(physics_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    UIElement *geometry_section = CreateViewSection_StackWrap(view_cont, "Geometry", view_section_size,
                                                              state_manager_panel->palette);
    const UIFieldSpec geometry_specs[] = {
        {"Bounds.min", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, VECTOR2D, &s_sm_ui.pos_tl_tbox, NULL},
        {"Geo.center", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, VECTOR2D, &s_sm_ui.geometry_center_tbox, NULL},
        {"Rot", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &s_sm_ui.rotation_tbox, NULL},
        {"Basis u", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &s_sm_ui.basis_u_tbox, NULL},
        {"Basis v", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &s_sm_ui.basis_v_tbox, NULL},
    };
    InitUIFields(geometry_section, geometry_specs,
                 ARRAY_COUNT(geometry_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    UIElement *gameplay_section = CreateViewSection_StackWrap(view_cont, "Gameplay", view_section_size,
                                                              state_manager_panel->palette);
    s_sm_ui.gameplay_section = gameplay_section;
    const UIFieldSpec gameplay_specs[] = {
        {"Health", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &s_sm_ui.health_tbox, NULL},
        {"MaxHealth", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &s_sm_ui.max_health_tbox, NULL},
        {"Damage", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &s_sm_ui.damage_tbox, NULL},
    };
    InitUIFields(gameplay_section, gameplay_specs,
                 ARRAY_COUNT(gameplay_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    // Components section: shows attached components and their properties.
    UIElement *components_section = CreateViewSection_StackWrap(view_cont, "Components", view_section_size,
                                                                state_manager_panel->palette);
    s_sm_ui.components_section = components_section;

    // Component toggle buttons: PORTAL, ROTOR, GEAR
    s_sm_ui.comp_buttons[0] = (StateManagerComponentButton){"PORTAL", ENTITY_COMPONENT_PORTAL, NULL};
    s_sm_ui.comp_buttons[1] = (StateManagerComponentButton){"ROTOR", ENTITY_COMPONENT_ROTOR, NULL};
    s_sm_ui.comp_buttons[2] = (StateManagerComponentButton){"GEAR", ENTITY_COMPONENT_GEAR, NULL};

    for (size_t i = 0; i < ARRAY_COUNT(s_sm_ui.comp_buttons); i++)
    {
        s_sm_ui.comp_buttons[i].button = CreateUIButtonDefault(
            components_section, UI_ELEMENT_BUTTON_SIMPLE, s_sm_ui.comp_buttons[i].label,
            ui_wide_button_size, ui_standard_button_padding,
            state_manager_panel->palette, HandleComponentToggleClick,
            (void *)&s_sm_ui.comp_buttons[i], NULL);
    }

    const UIFieldSpec components_specs[] = {
        {"Portal Cooldown", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, INT, &s_sm_ui.comp_portal_cooldown_tbox, &s_sm_ui.comp_portal_cooldown_str},
        {"Portal Mask", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, INT, &s_sm_ui.comp_portal_entrant_mask_tbox, &s_sm_ui.comp_portal_entrant_mask_str},
        {"Relation Type", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, INT, &s_sm_ui.comp_relation_type_tbox, &s_sm_ui.comp_relation_type_str},
        {"Relation Target", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, INT, &s_sm_ui.comp_relation_target_tbox, &s_sm_ui.comp_relation_target_str},
        {"Relation Active", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, INT, &s_sm_ui.comp_relation_active_tbox, &s_sm_ui.comp_relation_active_str},
    };
    InitUIFields(components_section, components_specs,
                 ARRAY_COUNT(components_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    s_sm_ui.delete_action = BUTTON_ACTION_DELETE_ENTITY;
    CreateUIButtonDefault(view_cont, UI_ELEMENT_BUTTON_SUBMIT,
                          "DELETE", ui_standard_button_size, ui_standard_button_padding,
                          state_manager_panel->palette, HandleBtnSubmitClick,
                          &s_sm_ui.delete_action, NULL);
}

static void InitAttributeStateView(void)
{
    View *attributes_view = CreateStateManagerView(
        STATE_MANAGER_ATTRIBUTES_VIEW, true, true, ui_zero_inline_spacing);
    if (!attributes_view)
    {
        return;
    }
    View_SetScrollableX(attributes_view, true);
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

    CreateFlagButtons(identity_section, s_sm_flags.entity_type,
                      ARRAY_COUNT(s_sm_flags.entity_type), HandleEntityTypeFlagClick);
    CreateFlagButtons(attribute_section, s_sm_flags.entity_attribute,
                      ARRAY_COUNT(s_sm_flags.entity_attribute), HandleEntityAttributeFlagClick);
    CreateFlagButtons(status_section, s_sm_flags.entity_status,
                      ARRAY_COUNT(s_sm_flags.entity_status), HandleEntityStatusFlagClick);
    CreateFlagButtons(collision_section, s_sm_flags.collision_mask,
                      ARRAY_COUNT(s_sm_flags.collision_mask), HandleCollisionMaskFlagClick);
}

static void InitWorldStateView(void)
{
    View *world_view = CreateStateManagerView(
        STATE_MANAGER_WORLD_VIEW, true, false, ui_zero_x_inline_wrap_spacing);
    if (!world_view)
    {
        return;
    }
    View_SetScrollableX(world_view, true);

    UIElement *world_section = CreateViewSection_StackWrap(world_view->container, "World", view_section_size,
                                                           state_manager_panel->palette);

    UIElement *world_physics_section = CreateViewSection_StackWrap(
        world_view->container, "Physics", view_section_size, state_manager_panel->palette);
    const UIFieldSpec world_physics_specs[] = {
        {"Restitution", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT,
         &s_sm_ui.world_restitution_tbox, NULL},
        {"Friction", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT,
         &s_sm_ui.world_friction_tbox, NULL},
    };
    InitUIFields(world_physics_section, world_physics_specs,
                 ARRAY_COUNT(world_physics_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    CreateFlagButtons(world_section, s_sm_flags.world,
                      ARRAY_COUNT(s_sm_flags.world), HandleWorldFlagClick);
}

static void InitCellStateView(void)
{
    View *cell_view = CreateStateManagerView(
        STATE_MANAGER_CELL_STATE_VIEW, true, false, ui_zero_inline_spacing);
    if (!cell_view)
    {
        return;
    }
    View_SetScrollableX(cell_view, true);
    UIElement *cell_container = cell_view->container;

    const UIFieldSpec cell_specs[] = {
        {"Index", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &s_sm_ui.cell_id_str},
        {"Occu", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &s_sm_ui.cell_occu_str},
        {"Value", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &s_sm_ui.cell_value_str},
        {"Fill", UI_ELEMENT_TEXTBOX_O, ui_standard_control_size, FLOAT, NULL, &s_sm_ui.cell_fill_str},
    };

    UIElement *cell_phys_section = CreateViewSection_StackWrap(cell_container, "Cell", view_section_size,
                                                               state_manager_panel->palette);
    InitUIFields(cell_phys_section, cell_specs,
                 ARRAY_COUNT(cell_specs),
                 ui_standard_field_padding, state_manager_panel->palette);

    UIElement *cell_flags_section = CreateViewSection_StackWrap(cell_container, "Flags", view_section_size,
                                                                state_manager_panel->palette);

    CreateFlagButtons(cell_flags_section, s_sm_flags.cell,
                      ARRAY_COUNT(s_sm_flags.cell), HandleCellFlagClick);
}

// ============================================================================
// View Refresh
// ============================================================================

// Refresh gameplay section visibility and row states.
static void RefreshGameplaySection(const Newtonoid2d *object)
{
    bool is_damageable = object && object->id != INVALID_ENTITY_ID &&
                         (object->attribute_flags & ENTITY_ATTR_FLAG_DAMAGEABLE) != 0;
    bool is_projectile = object && object->id != INVALID_ENTITY_ID &&
                         (object->entity_flags & ENTITY_FLAG_PROJECTILE) != 0;

    if (s_sm_ui.gameplay_section)
    {
        if (is_damageable || is_projectile)
        {
            EnableElement(s_sm_ui.gameplay_section);
            // Toggle Text Field visibility based on a conditional
            SetParentEnabledState(s_sm_ui.health_tbox,is_damageable);
            SetParentEnabledState(s_sm_ui.max_health_tbox, is_damageable);
            SetParentEnabledState(s_sm_ui.damage_tbox, is_projectile);
        }
        else
        {
            DisableElement(s_sm_ui.gameplay_section);
        }
    }
}

// Update component section visibility based on attached components and populate their values.
static void RefreshComponentsSection(const Newtonoid2d *object)
{
    bool is_valid = (object != NULL && object->id != INVALID_ENTITY_ID);

    if (s_sm_ui.components_section)
    {
        if (is_valid)
        {
            EnableElement(s_sm_ui.components_section);
        }
        else
        {
            DisableElement(s_sm_ui.components_section);
            ClearString64(s_sm_ui.comp_portal_cooldown_str);
            ClearString64(s_sm_ui.comp_portal_entrant_mask_str);
            ClearString64(s_sm_ui.comp_relation_type_str);
            ClearString64(s_sm_ui.comp_relation_target_str);
            ClearString64(s_sm_ui.comp_relation_active_str);
            return; // No components to populate.
        }
    }

    EntityDescription desc = EntityRegistry_Describe(object);

    // Update component toggle button labels: "PORTAL: ON/OFF", "ROTOR: ON/OFF", "GEAR: ON/OFF"
    for (size_t i = 0; i < ARRAY_COUNT(s_sm_ui.comp_buttons); i++)
    {
        if (!s_sm_ui.comp_buttons[i].button)
        {
            continue;
        }

        bool attached = is_valid && (desc.components[s_sm_ui.comp_buttons[i].type] != NULL);
        UpdateString64(s_sm_ui.comp_buttons[i].button->data.button.label.string,
                       "%s: %s", s_sm_ui.comp_buttons[i].label, attached ? "ON" : "OFF");
        s_sm_ui.comp_buttons[i].button->is_enabled = is_valid;
    }

    // Toggle Portal property field row visibility
    bool has_portal = (desc.components[ENTITY_COMPONENT_PORTAL] != NULL);
    SetParentEnabledState(s_sm_ui.comp_portal_cooldown_tbox, has_portal);
    SetParentEnabledState(s_sm_ui.comp_portal_entrant_mask_tbox, has_portal);

    // Populate portal component fields if attached.
    PortalEntity *portal = (PortalEntity *)desc.components[ENTITY_COMPONENT_PORTAL];
    if (portal)
    {
        if (s_sm_ui.comp_portal_cooldown_str)
        {
            UpdateString64(s_sm_ui.comp_portal_cooldown_str->string, "%d", portal->cooldown_frames);
        }
        if (s_sm_ui.comp_portal_entrant_mask_str)
        {
            UpdateString64(s_sm_ui.comp_portal_entrant_mask_str->string, "%d", portal->entrant_mask);
        }
    }
    else
    {
        ClearString64(s_sm_ui.comp_portal_cooldown_str);
        ClearString64(s_sm_ui.comp_portal_entrant_mask_str);
    }

    // Toggle Relation property field row visibility
    bool has_relation = (desc.components[ENTITY_COMPONENT_RELATION] != NULL);
    SetParentEnabledState(s_sm_ui.comp_relation_type_tbox, has_relation);
    SetParentEnabledState(s_sm_ui.comp_relation_target_tbox, has_relation);
    SetParentEnabledState(s_sm_ui.comp_relation_active_tbox, has_relation);

    // Populate relation component fields if attached.
    RelationComponent *relation = (RelationComponent *)desc.components[ENTITY_COMPONENT_RELATION];
    if (relation)
    {
        if (s_sm_ui.comp_relation_type_str)
        {
            UpdateString64(s_sm_ui.comp_relation_type_str->string, "%s", RelationType_ToString(relation->type));
        }
        if (s_sm_ui.comp_relation_target_str)
        {
            UpdateString64(s_sm_ui.comp_relation_target_str->string, "%d", relation->target_entity);
        }
        if (s_sm_ui.comp_relation_active_str)
        {
            UpdateString64(s_sm_ui.comp_relation_active_str->string, "%s", relation->is_active ? "ON" : "OFF");
        }
    }
    else
    {
        ClearString64(s_sm_ui.comp_relation_type_str);
        ClearString64(s_sm_ui.comp_relation_target_str);
        ClearString64(s_sm_ui.comp_relation_active_str);
    }
}

// Refresh the PHYS view: identity, physics, geometry, gameplay, and component readouts.
static void RefreshPhysView(Newtonoid2d *object)
{
    // Derive identity values for the selected object.
    int slot = object ? (int)EntityId_GetSlotIndex(object->id) : -1;
    int generation = object ? (int)EntityId_GetGeneration(object->id) : -1;
    int world_index = -1;
    if (object)
    {
        Universe_GetEntityByID(&G_Universe, object->id, &world_index);
    }
    void *world_ptr = (object && world_index >= 0) ? &world_index : NULL;

    // Refresh identity, physics, and gameplay numerical fields.
    TextboxField state_fields[] = {
        {s_sm_ui.id_tbox, INT, object ? (void *)&object->id : NULL, 0, NULL},
        {s_sm_ui.slot_tbox, INT, object ? (void *)&slot : NULL, 0, NULL},
        {s_sm_ui.generation_tbox, INT, object ? (void *)&generation : NULL, 0, NULL},
        {s_sm_ui.world_tbox, INT, world_ptr, 0, NULL},

        {s_sm_ui.mass_tbox, FLOAT, object ? (void *)&object->mass : NULL, 2, NULL},
        {s_sm_ui.restitution_tbox, FLOAT, object ? (void *)&object->restitution : NULL, 2, NULL},
        {s_sm_ui.friction_tbox, FLOAT, object ? (void *)&object->friction : NULL, 2, NULL},
        {s_sm_ui.pos_tl_tbox, VECTOR2D, object ? (void *)&object->bounds_origin : NULL, 0, NULL},
        {s_sm_ui.pos_c_tbox, VECTOR2D, object ? (void *)&object->anchor_position : NULL, 0, NULL},
        {s_sm_ui.vel_tbox, VECTOR2D, object ? (void *)&object->velocity : NULL, 0, NULL},
        {s_sm_ui.accel_tbox, VECTOR2D, object ? (void *)&object->acceleration : NULL, 0, NULL},
        {s_sm_ui.moment_tbox, VECTOR2D, object ? (void *)&object->momentum : NULL, 0, NULL},
        {s_sm_ui.angular_velocity_tbox, FLOAT, object ? (void *)&object->angular_velocity : NULL, 2, NULL},
        {s_sm_ui.angular_acceleration_tbox, FLOAT, object ? (void *)&object->angular_acceleration : NULL, 2, NULL},

        {s_sm_ui.health_tbox, FLOAT, object ? (void *)&object->health : NULL, 2, NULL},
        {s_sm_ui.max_health_tbox, FLOAT, object ? (void *)&object->max_health : NULL, 2, NULL},
        {s_sm_ui.damage_tbox, FLOAT, object ? (void *)&object->damage : NULL, 2, NULL},
    };
    RefreshTextboxFields(state_fields, ARRAY_COUNT(state_fields));

    // Refresh geometry fields, synchronising object rotation when valid.
    if (object)
    {
        SyncNewtonoidRotation(object);
        TextboxField geometry_fields[] = {
            {s_sm_ui.rotation_tbox, FLOAT, (void *)&object->rotation, 3, "N/A"},
            {s_sm_ui.basis_u_tbox, VECTOR2D, (void *)&object->local_axis_x, 0, "N/A"},
            {s_sm_ui.basis_v_tbox, VECTOR2D, (void *)&object->local_axis_y, 0, "N/A"},
            {s_sm_ui.geometry_center_tbox, VECTOR2D, (void *)&object->local_geometry_center, 0, "N/A"},
        };
        RefreshTextboxFields(geometry_fields, ARRAY_COUNT(geometry_fields));
    }
    else
    {
        TextboxField geometry_fields[] = {
            {s_sm_ui.rotation_tbox, FLOAT, NULL, 0, "N/A"},
            {s_sm_ui.basis_u_tbox, VECTOR2D, NULL, 0, "N/A"},
            {s_sm_ui.basis_v_tbox, VECTOR2D, NULL, 0, "N/A"},
            {s_sm_ui.geometry_center_tbox, VECTOR2D, NULL, 0, "N/A"},
        };
        RefreshTextboxFields(geometry_fields, ARRAY_COUNT(geometry_fields));
    }

    // Refresh contextual gameplay and component section visibility.
    RefreshGameplaySection(object);
    RefreshComponentsSection(object);
}

// Refresh the ATTRI view: entity type, attribute, status, and collision mask flag buttons.
static void RefreshAttributeView(const Newtonoid2d *object)
{
    bool is_valid = (object != NULL);
    uint32_t type_flags = object ? object->entity_flags : 0;
    uint32_t attr_flags = object ? object->attribute_flags : 0;
    uint32_t status_flags = object ? object->status_flags : 0;
    uint32_t mask_flags = object ? object->collision_mask : 0;

    UpdateFlagButtons(s_sm_flags.entity_type, ARRAY_COUNT(s_sm_flags.entity_type),
                      type_flags, is_valid);
    UpdateFlagButtons(s_sm_flags.entity_attribute, ARRAY_COUNT(s_sm_flags.entity_attribute),
                      attr_flags, is_valid);
    UpdateFlagButtons(s_sm_flags.entity_status, ARRAY_COUNT(s_sm_flags.entity_status),
                      status_flags, is_valid);
    UpdateFlagButtons(s_sm_flags.collision_mask, ARRAY_COUNT(s_sm_flags.collision_mask),
                      mask_flags, is_valid);
}

// Refresh the WORLD view: world physics material properties and world status flags.
static void RefreshWorldView(const World2d *world)
{
    // World material values belong to the boundary Newtonoid used by container collision response.
    TextboxField world_physics_fields[] = {
        {s_sm_ui.world_restitution_tbox, FLOAT,
         world ? (void *)&world->grid_space.object.restitution : NULL, 2, "N/A"},
        {s_sm_ui.world_friction_tbox, FLOAT,
         world ? (void *)&world->grid_space.object.friction : NULL, 2, "N/A"},
    };
    RefreshTextboxFields(world_physics_fields, ARRAY_COUNT(world_physics_fields));

    UpdateFlagButtons(s_sm_flags.world, ARRAY_COUNT(s_sm_flags.world),
                      world ? world->flags : 0, world != NULL);
}

// Refresh the CELL view: grid cell state string readouts and cell behaviour flags.
static void RefreshCellView(const Cell *selected_cell)
{
    if (selected_cell)
    {
        int index = UIState_GetSelectedCellIndex();
        int occupancy = selected_cell->occupancy;
        float value = selected_cell->value;
        float fill = 0.0f;

        if (s_sm_ui.cell_id_str)
        {
            UpdateString64(s_sm_ui.cell_id_str->string, "%d", index);
        }
        if (s_sm_ui.cell_occu_str)
        {
            UpdateString64(s_sm_ui.cell_occu_str->string, "%d", occupancy);
        }
        if (s_sm_ui.cell_value_str)
        {
            UpdateString64(s_sm_ui.cell_value_str->string, "%0.1f", value);
        }
        if (s_sm_ui.cell_fill_str)
        {
            UpdateString64(s_sm_ui.cell_fill_str->string, "%0.1f", fill);
        }
    }
    else
    {
        ClearString64(s_sm_ui.cell_id_str);
        ClearString64(s_sm_ui.cell_occu_str);
        ClearString64(s_sm_ui.cell_value_str);
        ClearString64(s_sm_ui.cell_fill_str);
    }

    UpdateFlagButtons(s_sm_flags.cell, ARRAY_COUNT(s_sm_flags.cell),
                      selected_cell ? selected_cell->flags : 0, selected_cell != NULL);
}

// Push the selected object, world, cell, and capability state into the UI views.
void UpdateStateManagerSelectedObject(void)
{
    Newtonoid2d *object = UIState_GetSelectedObject();
    World2d *world = Universe_GetSelectedWorld(&G_Universe);
    Cell *cell = UIState_GetSelectedCell();

    RefreshPhysView(object);
    RefreshAttributeView(object);
    RefreshWorldView(world);
    RefreshCellView(cell);
}

// ============================================================================
// Module Lifecycle
// ============================================================================

void InitStateManagerSystem(void)
{
    const char *labels[] = {"PHYS", "ATTRI", "WORLD", "CELL"};
    state_manager_panel = PanelSystem_CreateStandard(&entity_panel_viewport, 4,
                                                     labels, ARRAY_COUNT(labels),
                                                     NULL, &ui_default_palette,
                                                     ui_standard_stack_spacing);
    if (!state_manager_panel)
        return;

    // Match section measurement to the view height left after the selector and container padding.
    float state_manager_view_height = (float)state_manager_panel->space.rows -
                                      (2.0f * state_manager_panel->default_padding.y) -
                                      ui_standard_selector_button_size.dimensions.y -
                                      state_manager_panel->root_child_spacing.spacing.y -
                                      (2.0f * ui_standard_container_padding.y);
    view_section_size = UI_SIZE_CONTENT_MAX(0.0f, fmaxf(0.0f, state_manager_view_height));

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
        Newtonoid2d *object = UIState_GetSelectedObject();
        // Dynamic entities update continuously; otherwise update when state is marked dirty.
        if (object || state_manager_refresh_dirty)
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

// Reset all button element references across a flag array.
static void ResetFlagButtons(StateManagerFlagButton *buttons, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        buttons[i].button = NULL;
    }
}

// Destroy the state-manager panel and clear its cached UI references.
void DestroyStateManagerSystem(void)
{
    UIState_SetSelectionChangedCallback(NULL, NULL);

    PanelSystem *panel = state_manager_panel;
    state_manager_panel = NULL;
    PanelSystem_Destroy(panel);

    state_manager_refresh_dirty = true;
    ResetFlagButtons(s_sm_flags.entity_type, ARRAY_COUNT(s_sm_flags.entity_type));
    ResetFlagButtons(s_sm_flags.entity_attribute, ARRAY_COUNT(s_sm_flags.entity_attribute));
    ResetFlagButtons(s_sm_flags.entity_status, ARRAY_COUNT(s_sm_flags.entity_status));
    ResetFlagButtons(s_sm_flags.collision_mask, ARRAY_COUNT(s_sm_flags.collision_mask));
    ResetFlagButtons(s_sm_flags.world, ARRAY_COUNT(s_sm_flags.world));
    ResetFlagButtons(s_sm_flags.cell, ARRAY_COUNT(s_sm_flags.cell));
    for (size_t i = 0; i < ARRAY_COUNT(s_sm_ui.comp_buttons); i++)
    {
        s_sm_ui.comp_buttons[i].button = NULL;
    }

    memset(&s_sm_ui, 0, sizeof(s_sm_ui));
}