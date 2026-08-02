#include "system/ui/state_manager_system.h"

#include "system/command_queue.h"
#include "system/panel_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"
#include "physics/newtonoid.h"

static PanelSystem *state_manager_panel = NULL;
static int btn_action_delete_entity = BUTTON_ACTION_DELETE_ENTITY;

static Vector2d state_manager_tfield_padding = {0.03f, 0.03f};
static Spacing state_manager_button_spacing = {{0.0f, 0.03f}, NONE, SPACING_STACKED};
static Size state_manager_button_size = {{0.0f, 0.0f}, SIZE_FIXED};
static Offset state_manager_entity_offset = {{0.0f, 0.15f}, OFFSET_PERCENT};
static Size state_manager_view_size = {{1.0f, 0.85f}, SIZE_PERCENT};
static Size state_manager_toggle_size = {{1.0f, 0.15f}, SIZE_PERCENT};
static Size state_manager_toggle_button_size = {{0.0f, 0.0f}, SIZE_FIXED};
static Size state_manager_attribute_section_size = {{0.275f, 1.0f}, SIZE_PERCENT};
static Spacing state_manager_attribute_section_spacing = {{0.0f, 0.03f}, NONE, SPACING_STACKED_WRAP};
static ViewSelector *state_manager_view_selector = NULL;

typedef struct
{
    uint32_t flag;
    bool is_type;
    bool is_collision;
    bool is_entity;
} StateManagerFlagBinding;

typedef struct
{
    const char *label;
    StateManagerFlagBinding *binding;
    UIElement **registry;
    size_t registry_count;
} StateManagerButtonSpec;

static StateManagerFlagBinding flag_alive = {FLAG_STATUS_ALIVE, false, false, false};
static StateManagerFlagBinding flag_rigid = {FLAG_ATTR_RIGID, false, false, false};
static StateManagerFlagBinding flag_clocked = {FLAG_LIFETIME_CLOCKED, false, false, false};
static StateManagerFlagBinding flag_wall = {FLAG_TYPE_WALL, true, false, true};
static StateManagerFlagBinding flag_newtonoid = {FLAG_TYPE_NEWTONOID, true, false, true};
static StateManagerFlagBinding flag_projectile = {FLAG_TYPE_PROJECTILE, true, false, true};
static StateManagerFlagBinding flag_effect = {FLAG_TYPE_EFFECT, true, false, true};
static StateManagerFlagBinding flag_camera = {FLAG_TYPE_CAMERA, true, false, true};
static StateManagerFlagBinding mask_wall = {FLAG_TYPE_WALL, false, true, false};
static StateManagerFlagBinding mask_newtonoid = {FLAG_TYPE_NEWTONOID, false, true, false};
static StateManagerFlagBinding mask_projectile = {FLAG_TYPE_PROJECTILE, false, true, false};
static StateManagerFlagBinding mask_effect = {FLAG_TYPE_EFFECT, false, true, false};
static StateManagerFlagBinding mask_camera = {FLAG_TYPE_CAMERA, false, true, false};

static UIElement *state_manager_flag_buttons[8] = {0};
static UIElement *state_manager_collision_buttons[5] = {0};

static const uint32_t state_manager_type_flags = FLAG_TYPE_WALL | FLAG_TYPE_NEWTONOID |
                                                 FLAG_TYPE_PROJECTILE | FLAG_TYPE_EFFECT | FLAG_TYPE_CAMERA;

static void InitStateManagerCellStateView(void);

static void HandleStateManagerFlagClick(UIElement *button)
{
    if (!button || !G_UIState.selected_object || !button->data.button.user_data)
    {
        return;
    }

    StateManagerFlagBinding *binding = (StateManagerFlagBinding *)button->data.button.user_data;
    Newtonoid2d *object = G_UIState.selected_object;

    if (binding->is_collision)
    {
        if (object->collision_mask & binding->flag)
        {
            object->collision_mask &= ~binding->flag;
        }
        else
        {
            object->collision_mask |= binding->flag;
        }
        UpdateStateManagerSelectedObject();
        return;
    }

    uint32_t *target_flags = binding->is_entity ? &object->entity_flags : &object->status_flags;
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
    UpdateStateManagerSelectedObject();
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

static UIElement *CreateStateManagerBoundButton(UIElement *parent, const char *label,
                                                StateManagerFlagBinding *binding,
                                                const UIPalette *palette,
                                                UIElement **registry,
                                                size_t registry_count)
{
    UIElement *button = CreateUIButtonDefault(parent, UI_ELEMENT_BUTTON_SIMPLE, label,
                                              state_manager_button_size, btn_default_padding,
                                              palette, HandleStateManagerFlagClick,
                                              binding, NULL);
    RegisterStateManagerButton(button, registry, registry_count);
    return button;
}

static void CreateStateManagerSectionButtons(UIElement *section,
                                             const StateManagerButtonSpec *specs,
                                             size_t count)
{
    if (!section || !specs)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        CreateStateManagerBoundButton(section, specs[i].label, specs[i].binding,
                                      state_manager_panel->palette,
                                      specs[i].registry, specs[i].registry_count);
    }
}

static UIElement *CreateStateManagerAttributeSection(UIElement *parent, const char *title)
{
    UIElement *section = CreateUIContainer(
        parent, state_manager_attribute_section_size,
        (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, tcont_default_padding,
        state_manager_panel->palette, UI_PALETTE_SURFACE_CONTAINER,
        state_manager_attribute_section_spacing, true, true);
    CreateUILabelTitleDefault(section, title, ui_default_control_size,
                              state_manager_tfield_padding, state_manager_panel->palette);
    return section;
}

static void InitStateManagerContainer(void)
{
    UIElement *entity_container = CreateUIContainer(
        state_manager_panel->root, state_manager_view_size,
        state_manager_entity_offset, tcont_default_padding,
        state_manager_panel->palette, UI_PALETTE_SURFACE_CONTAINER,
        tcont_default_child_spacing, true, true);
    View *physics_view = AllocateBytes(sizeof(View));
    PanelSystem_AddView(state_manager_panel, physics_view, entity_container, STATE_MANAGER_PHYSICS_VIEW);
    entity_container->child_spacing = (Spacing){{0.01f, 0.01f}, PERCENT, SPACING_STACKED_WRAP};

    const UIFieldSpec state_specs[] = {
        {"Id", UI_ELEMENT_TEXTBOX_O, ui_default_control_size, FLOAT, &G_UIState.lpanel_entity_state_id_tbox, NULL},
        {"Mass", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, FLOAT, &G_UIState.lpanel_entity_state_mass_tbox, NULL},
        {"Pos.tl", UI_ELEMENT_TEXTBOX_O, ui_default_control_size, VECTOR2D, &G_UIState.lpanel_entity_state_pos_tl_tbox, NULL},
        {"Pos.c", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, VECTOR2D, &G_UIState.lpanel_entity_state_pos_c_tbox, NULL},
        {"Vel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, VECTOR2D, &G_UIState.lpanel_entity_state_vel_tbox, NULL},
        {"Accel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, VECTOR2D, &G_UIState.lpanel_entity_state_accel_tbox, NULL},
        {"Moment", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_default_control_size, VECTOR2D, &G_UIState.lpanel_entity_state_moment_tbox, NULL},
    };
    InitUIFields(entity_container, state_specs,
                 sizeof(state_specs) / sizeof(state_specs[0]),
                 state_manager_tfield_padding, state_manager_panel->palette);

    UIElement *delete_container = CreateUIContainer(
        entity_container, ui_default_control_size,
        (Offset){{0.0f, 0.0f}, OFFSET_PERCENT}, ZERO_VECTOR_2D,
        state_manager_panel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
        state_manager_button_spacing, false, true);

    CreateUIButtonDefault(delete_container, UI_ELEMENT_BUTTON_SUBMIT,
                          "DELETE", btn_default_size, btn_default_padding,
                          state_manager_panel->palette, HandleBtnSubmitClick,
                          &btn_action_delete_entity, NULL);

    UIElement *attributes_container = CreateUIContainer(
        state_manager_panel->root, state_manager_view_size,
        state_manager_entity_offset, tcont_default_padding,
        state_manager_panel->palette, UI_PALETTE_SURFACE_CONTAINER,
        tcont_default_child_spacing, true, false);
    View *attributes_view = AllocateBytes(sizeof(View));
    PanelSystem_AddView(state_manager_panel, attributes_view, attributes_container, STATE_MANAGER_ATTRIBUTES_VIEW);
    attributes_container->child_spacing = (Spacing){{0.01f, 0.01f}, PERCENT, SPACING_STACKED_WRAP};

    const size_t flag_registry_count = sizeof(state_manager_flag_buttons) / sizeof(state_manager_flag_buttons[0]);
    const size_t collision_registry_count = sizeof(state_manager_collision_buttons) / sizeof(state_manager_collision_buttons[0]);

    UIElement *identity_section = CreateStateManagerAttributeSection(attributes_container, "Entity");
    const StateManagerButtonSpec identity_specs[] = {
        {"WALL", &flag_wall, state_manager_flag_buttons, flag_registry_count},
        {"NEWTONOID", &flag_newtonoid, state_manager_flag_buttons, flag_registry_count},
        {"PROJECTILE", &flag_projectile, state_manager_flag_buttons, flag_registry_count},
        {"EFFECT", &flag_effect, state_manager_flag_buttons, flag_registry_count},
        {"CAMERA", &flag_camera, state_manager_flag_buttons, flag_registry_count},
    };
    CreateStateManagerSectionButtons(identity_section, identity_specs,
                                     sizeof(identity_specs) / sizeof(identity_specs[0]));

    UIElement *behaviour_section = CreateStateManagerAttributeSection(attributes_container, "Behaviour");
    const StateManagerButtonSpec behaviour_specs[] = {
        {"ALIVE", &flag_alive, state_manager_flag_buttons, flag_registry_count},
        {"RIGID", &flag_rigid, state_manager_flag_buttons, flag_registry_count},
        {"CLOCKED", &flag_clocked, state_manager_flag_buttons, flag_registry_count},
    };
    CreateStateManagerSectionButtons(behaviour_section, behaviour_specs,
                                     sizeof(behaviour_specs) / sizeof(behaviour_specs[0]));

    UIElement *collision_section = CreateStateManagerAttributeSection(attributes_container, "Collision");
    const StateManagerButtonSpec collision_specs[] = {
        {"WALL", &mask_wall, state_manager_collision_buttons, collision_registry_count},
        {"NEWTONOID", &mask_newtonoid, state_manager_collision_buttons, collision_registry_count},
        {"PROJECTILE", &mask_projectile, state_manager_collision_buttons, collision_registry_count},
        {"EFFECT", &mask_effect, state_manager_collision_buttons, collision_registry_count},
        {"CAMERA", &mask_camera, state_manager_collision_buttons, collision_registry_count},
    };
    CreateStateManagerSectionButtons(collision_section, collision_specs,
                                     sizeof(collision_specs) / sizeof(collision_specs[0]));
}

static void InitStateManagerCellStateView(void)
{
    UIElement *cell_container = CreateUIContainer(
        state_manager_panel->root, state_manager_view_size,
        state_manager_entity_offset, tcont_default_padding,
        state_manager_panel->palette, UI_PALETTE_SURFACE_CONTAINER,
        tcont_default_child_spacing, true, false);
    View *cell_view = AllocateBytes(sizeof(View));
    PanelSystem_AddView(state_manager_panel, cell_view, cell_container, STATE_MANAGER_CELL_STATE_VIEW);

    const UIFieldSpec cell_specs[] = {
        {"Index", UI_ELEMENT_TEXTBOX_O, ui_default_control_size, FLOAT, NULL, &G_UIState.lpanel_cell_state_id_str},
        {"Occu", UI_ELEMENT_TEXTBOX_O, ui_default_control_size, FLOAT, NULL, &G_UIState.lpanel_cell_state_occu_str},
        {"Value", UI_ELEMENT_TEXTBOX_O, ui_default_control_size, FLOAT, NULL, &G_UIState.lpanel_cell_state_value_str},
        {"Fill", UI_ELEMENT_TEXTBOX_O, ui_default_control_size, FLOAT, NULL, &G_UIState.lpanel_cell_state_fill_str},
    };

    InitUIFields(cell_container, cell_specs,
                 sizeof(cell_specs) / sizeof(cell_specs[0]),
                 state_manager_tfield_padding, state_manager_panel->palette);
}

void InitStateManagerSystem(void)
{
    state_manager_panel = PanelSystem_Create(&entity_panel_viewport, 1.0f, (Vector2d){0.1f, 0.1f}, &ui_default_palette,
                                             (Spacing){{0.0f, 0.0f}, PERCENT, SPACING_NONE});
    if (!state_manager_panel)
        return;

    state_manager_button_size = ui_default_control_size;

    PanelSystem_InitViews(state_manager_panel, 3);
    PanelSystem_InitRoot(state_manager_panel);
    UIElement *toggle_container = CreateUIContainer(
        state_manager_panel->root, state_manager_toggle_size,
        (Offset){{0.0f, 0.0f}, OFFSET_PERCENT}, ZERO_VECTOR_2D,
        state_manager_panel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
        (Spacing){{0.01f, 0.0f}, PERCENT, SPACING_INLINE}, false, true);
        state_manager_toggle_button_size = (Size){{(lpanel_viewport.resolution.x -
                                                                                                     (lpanel_viewport.resolution.x * 0.02f)) / 3.0f,
                                                  btn_default_size.dimensions.y},
                                                SIZE_FIXED};
    InitStateManagerContainer();
    InitStateManagerCellStateView();
    const char *labels[] = {"PHYS", "ATTRI", "CELL"};
    state_manager_view_selector = PanelSystem_CreateViewSelector(
        state_manager_panel, toggle_container, state_manager_toggle_button_size,
        labels, sizeof(labels) / sizeof(labels[0]), NULL);
    PanelSystem_SelectView(state_manager_view_selector, 0);
    UpdateUISpace(state_manager_panel->root, state_manager_panel->seed_box);
}

void DrawStateManagerSystem(void)
{
    if (state_manager_panel)
        PanelSystem_Draw(state_manager_panel);
}

UIElement *GetStateManagerRoot(void)
{
    return state_manager_panel ? state_manager_panel->root : NULL;
}

static void UpdateStateManagerButtonGroup(UIElement **buttons,
                                          const StateManagerFlagBinding *bindings[],
                                          const char *labels[], size_t count,
                                          Newtonoid2d *object, bool use_collision_mask)
{
    for (size_t i = 0; i < count; i++)
    {
        if (!buttons[i])
        {
            continue;
        }
        uint32_t source_flags = object
                                    ? (use_collision_mask ? object->collision_mask
                                                           : (bindings[i]->is_entity ? object->entity_flags
                                                                                      : object->status_flags))
                                    : 0;
        bool enabled = object && (source_flags & bindings[i]->flag) != 0;
        UpdateString64(buttons[i]->data.button.label.string,
                       "%s: %s", labels[i], enabled ? "ON" : "OFF");
        buttons[i]->is_enabled = true;
    }
}

void UpdateStateManagerSelectedObject(void)
{
    Newtonoid2d *object = G_UIState.selected_object;
    const StateManagerFlagBinding *bindings[] = {
        &flag_wall, &flag_newtonoid, &flag_projectile, &flag_effect, &flag_camera,
        &flag_alive, &flag_rigid, &flag_clocked};
    const char *labels[] = {"Wall", "Newtonoid", "Projectile", "Effect", "Camera",
                            "Alive", "Rigid", "Clocked"};
    UpdateStateManagerButtonGroup(state_manager_flag_buttons, bindings, labels,
                                  sizeof(bindings) / sizeof(bindings[0]), object, false);

    const StateManagerFlagBinding *collision_bindings[] = {
        &mask_wall, &mask_newtonoid, &mask_projectile, &mask_effect, &mask_camera};
    const char *collision_labels[] = {"Wall", "Newtonoid", "Projectile", "Effect", "Camera"};
    UpdateStateManagerButtonGroup(state_manager_collision_buttons, collision_bindings,
                                  collision_labels, sizeof(collision_bindings) / sizeof(collision_bindings[0]),
                                  object, true);
}
