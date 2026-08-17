#include "system/ui/popup_menu.h"

#include "system/panel_system.h"
#include "system/command_queue.h"
#include "system/universe_system.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"
#include "world/universe.h"

static PanelSystem *popup_menu = NULL;
static UIElement *popup_menu_create_cont = NULL;
static UIElement *popup_menu_recent_cont = NULL;
static UIElement *popup_create_entity_submenu_cont = NULL;
static UIElement *popup_create_world_submenu_cont = NULL;
static View *popup_create_view = NULL;
static View popup_create_view_storage = {0};
static View *popup_recent_view = NULL;
static View popup_recent_view_storage = {0};
static bool popup_menu_visible = false;
static Vector2d popup_spawn_position = ZERO_VECTOR_2D;
static float max_w = 3.75f;
static float max_h = 5.75f;
static const Size popup_menu_size = {{6.9f, 5.75f}, SIZE_FIXED};
static const Offset popup_menu_offset = {{0.0f, 0.0f}, OFFSET_FIXED};
static const Vector2d popup_menu_padding = {0.15f, 0.15f};
static const Spacing popup_menu_root_spacing = {{0.0f, 0.0f}, NONE, SPACING_NONE};
static const Spacing popup_menu_child_spacing = {{0.0f, 0.025f}, SIZE_FIXED, SPACING_STACKED};
static const Size popup_menu_item_size = {{3.75f, 0.5f}, SIZE_FIXED};
static const Size popup_view_selector_size = {{3.75f, 0.5f}, SIZE_FIXED};
static const Size popup_view_selector_button_size = {{1.875f, 0.5f}, SIZE_FIXED};
static const Size popup_menu_view_size = {{3.75f, 5.0f}, SIZE_FIXED};
static const Size popup_create_entity_submenu_size = {{0.0f, 0.0f}, SIZE_CONTENT};
static const Vector2d popup_view_selector_offset = {0.0f, 0.0f};
static const Vector2d popup_menu_view_offset = {0.0f, 0.5f};
static const Vector2d popup_create_entity_submenu_offset = {3.75f, 0.5f};
static const Vector2d popup_create_world_submenu_offset = {3.75f, 0.5f};
static UIElement *popup_view_selector_cont = NULL;
static ViewSelector *popup_view_selector = NULL;

static void InitCreateEntitySubmenu(void);
static void InitCreateWorldSubmenu(void);

typedef struct
{
    ShapeType shape;
} PopupShapeAction;

static PopupShapeAction popup_triangle_action = {SHAPE_TRIANGLE};
static PopupShapeAction popup_square_action = {SHAPE_SQUARE};
static PopupShapeAction popup_circle_action = {SHAPE_CIRCLE};

typedef struct
{
    UIElement *submenu;
    bool enable_submenu;
} PopupHoverAction;

static PopupHoverAction popup_create_entity_action = {0, true};
static PopupHoverAction popup_create_world_action = {0, true};

static void HandlePopupHover(UIElement *item)
{
    if (!item)
    {
        return;
    }

    PopupHoverAction *action = (PopupHoverAction *)item->data.hover_item.user_data;
    if (action && action->submenu)
    {
        if (popup_create_entity_submenu_cont &&
            popup_create_entity_submenu_cont != action->submenu)
        {
            DisableElement(popup_create_entity_submenu_cont);
        }
        if (popup_create_world_submenu_cont &&
            popup_create_world_submenu_cont != action->submenu)
        {
            DisableElement(popup_create_world_submenu_cont);
        }
        if (action->enable_submenu)
        {
            EnableElement(action->submenu);
        }
        else
        {
            DisableElement(action->submenu);
        }
        UpdateUISpace(popup_menu->root, popup_menu->seed_box);
    }
}

static void HandlePopupShapeClick(UIElement *button)
{
    if (!button || !button->is_enabled ||
        !G_UIState.newtonoid_params)
    {
        return;
    }

    PopupShapeAction *action =
        (PopupShapeAction *)button->data.button.user_data;
    if (!action)
    {
        return;
    }

    switch (action->shape)
    {
    case SHAPE_TRIANGLE:
        G_UIState.newtonoid_params->vertice_count = 3;
        break;
    case SHAPE_SQUARE:
        G_UIState.newtonoid_params->vertice_count = 4;
        break;
    case SHAPE_CIRCLE:
        G_UIState.newtonoid_params->vertice_count = MAX_SHAPE_VERTICES;
        break;
    default:
        return;
    }

    G_UIState.newtonoid_params->shape_type = action->shape;
    G_UIState.newtonoid_params->anchor_position = popup_spawn_position;
    EnqueueCreateEntity(G_UIState.newtonoid_params);
    HidePopupMenu();
}

static void InitCreateView(void)
{
    // Create View's container & register the View
    popup_menu_create_cont = CreateUIContainer(
        popup_menu->root, popup_menu_view_size,
        (Offset){popup_menu_view_offset, OFFSET_FIXED},
        popup_menu_padding, popup_menu->palette,
        UI_PALETTE_SURFACE_CONTAINER, popup_menu_child_spacing,
        true, true);

    // Customise the View
    popup_create_view = &popup_create_view_storage;
    PanelSystem_AddView(popup_menu, popup_create_view, popup_menu_create_cont, POPUP_MENU_CREATE_VIEW);

    popup_create_entity_action.submenu = popup_create_entity_submenu_cont;
    popup_create_world_action.submenu = popup_create_world_submenu_cont;
    UIElement *create_entity_item = CreateUIHoverItemDefault(
        popup_menu_create_cont, "ENTITY", popup_menu_item_size,
        ZERO_VECTOR_2D, popup_menu->palette, HandlePopupHover,
        &popup_create_entity_action);
    UIElement *create_world_item = CreateUIHoverItemDefault(
        popup_menu_create_cont, "WORLD", popup_menu_item_size,
        ZERO_VECTOR_2D, popup_menu->palette, HandlePopupHover,
        &popup_create_world_action);

    if (create_entity_item)
    {
        create_entity_item->data.hover_item.hover_fill =
            popup_menu->palette->container_fill;
    }
    if (create_world_item)
    {
        create_world_item->data.hover_item.hover_fill =
            popup_menu->palette->container_fill;
    }
}

static void InitCreateEntitySubmenu(void)
{
    popup_create_entity_submenu_cont = CreateUIContainer(
        popup_menu->root, popup_create_entity_submenu_size,
        (Offset){popup_create_entity_submenu_offset, OFFSET_FIXED},
        popup_menu_padding, popup_menu->palette,
        UI_PALETTE_SURFACE_CONTAINER, popup_menu_child_spacing,
        true, false);

    CreateUILabelTitleDefault(popup_create_entity_submenu_cont,
                              "Shapes", popup_menu_item_size,
                              ZERO_VECTOR_2D, popup_menu->palette);
    CreateUIButtonDefault(popup_create_entity_submenu_cont, UI_ELEMENT_BUTTON_SIMPLE, "triangle",
                          popup_menu_item_size, ZERO_VECTOR_2D,
                          popup_menu->palette, HandlePopupShapeClick,
                          &popup_triangle_action, NULL);
    CreateUIButtonDefault(popup_create_entity_submenu_cont, UI_ELEMENT_BUTTON_SIMPLE, "square",
                          popup_menu_item_size, ZERO_VECTOR_2D,
                          popup_menu->palette, HandlePopupShapeClick,
                          &popup_square_action, NULL);
    CreateUIButtonDefault(popup_create_entity_submenu_cont, UI_ELEMENT_BUTTON_SIMPLE, "circle",
                          popup_menu_item_size, ZERO_VECTOR_2D,
                          popup_menu->palette, HandlePopupShapeClick,
                          &popup_circle_action, NULL);
}

static void HandlePopupWorldClick(UIElement *button)
{
    if (!button || !button->is_enabled || !button->data.button.user_data)
    {
        return;
    }

    const CoordinateSpacePreset *preset =
        (const CoordinateSpacePreset *)button->data.button.user_data;
    if (preset)
    {
        CreateNewWorld_Preset(IsCreateWorldAutoSelectEnabled(), *preset);
        HidePopupMenu();
    }
}

static void InitCreateWorldSubmenu(void)
{
    popup_create_world_submenu_cont = CreateUIContainer(
        popup_menu->root, popup_create_entity_submenu_size,
        (Offset){popup_create_world_submenu_offset, OFFSET_FIXED},
        popup_menu_padding, popup_menu->palette,
        UI_PALETTE_SURFACE_CONTAINER, popup_menu_child_spacing,
        true, false);

    CreateUILabelTitleDefault(popup_create_world_submenu_cont,
                              "Small", popup_menu_item_size,
                              ZERO_VECTOR_2D, popup_menu->palette);
    CreateUIButtonDefault(popup_create_world_submenu_cont,
                          UI_ELEMENT_BUTTON_SIMPLE, "regular",
                          popup_menu_item_size, ZERO_VECTOR_2D,
                          popup_menu->palette, HandlePopupWorldClick,
                          (void *)&COORDINATE_SPACE_PRESET_REGULAR, NULL);
    CreateUIButtonDefault(popup_create_world_submenu_cont,
                          UI_ELEMENT_BUTTON_SIMPLE, "sheared-y",
                          popup_menu_item_size, ZERO_VECTOR_2D,
                          popup_menu->palette, HandlePopupWorldClick,
                          (void *)&COORDINATE_SPACE_PRESET_SHEARED_Y, NULL);
    CreateUIButtonDefault(popup_create_world_submenu_cont,
                          UI_ELEMENT_BUTTON_SIMPLE, "sheared-x",
                          popup_menu_item_size, ZERO_VECTOR_2D,
                          popup_menu->palette, HandlePopupWorldClick,
                          (void *)&COORDINATE_SPACE_PRESET_SHEARED_X, NULL);
    CreateUIButtonDefault(popup_create_world_submenu_cont,
                          UI_ELEMENT_BUTTON_SIMPLE, "isometric",
                          popup_menu_item_size, ZERO_VECTOR_2D,
                          popup_menu->palette, HandlePopupWorldClick,
                          (void *)&COORDINATE_SPACE_PRESET_ISOMETRIC, NULL);
    CreateUIButtonDefault(popup_create_world_submenu_cont,
                          UI_ELEMENT_BUTTON_SIMPLE, "perspective",
                          popup_menu_item_size, ZERO_VECTOR_2D,
                          popup_menu->palette, HandlePopupWorldClick,
                          (void *)&COORDINATE_SPACE_PRESET_PERSPECTIVE, NULL);
    CreateUIButtonDefault(popup_create_world_submenu_cont,
                          UI_ELEMENT_BUTTON_SIMPLE, "radial/polar",
                          popup_menu_item_size, ZERO_VECTOR_2D,
                          popup_menu->palette, HandlePopupWorldClick,
                          (void *)&COORDINATE_SPACE_PRESET_RADIAL, NULL);
}

static void InitRecentView(void)
{
    popup_menu_recent_cont = CreateUIContainer(
        popup_menu->root, popup_menu_view_size,
        (Offset){popup_menu_view_offset, OFFSET_FIXED},
        popup_menu_padding, popup_menu->palette,
        UI_PALETTE_SURFACE_CONTAINER, popup_menu_child_spacing,
        true, false);

    popup_recent_view = &popup_recent_view_storage;
    PanelSystem_AddView(popup_menu, popup_recent_view,
                        popup_menu_recent_cont, POPUP_MENU_RECENT_VIEW);

    CreateUILabelDefault(popup_menu_recent_cont, "No recent actions",
                         popup_menu_item_size, ZERO_VECTOR_2D,
                         popup_menu->palette);
}

static void InitPopupViewSelector(void)
{
    popup_view_selector_cont = CreateUIContainer(
        popup_menu->root, popup_view_selector_size,
        (Offset){popup_view_selector_offset, OFFSET_FIXED},
        ZERO_VECTOR_2D, popup_menu->palette,
        UI_PALETTE_SURFACE_TRANSPARENT,
        (Spacing){{0.0f, 0.0f}, NONE, SPACING_INLINE},
        false, true);
    popup_view_selector_cont->colour_border = popup_menu->palette->container_border;

    const char *labels[] = {"CREATE", "RECENT"};
    popup_view_selector = PanelSystem_CreateHoverViewSelector(
        popup_menu, popup_view_selector_cont, popup_view_selector_button_size,
        labels, sizeof(labels) / sizeof(labels[0]), NULL);
}

void InitPopupMenu(void)
{
    if (popup_menu)
    {
        return;
    }

    popup_menu = PanelSystem_Create(&game_viewport, 1.0f, ZERO_VECTOR_2D,
                                    &ui_default_palette, popup_menu_root_spacing);
    if (!popup_menu)
    {
        return;
    }

    PanelSystem_InitRoot(popup_menu);
    popup_menu->root->colour_fill = COLOURLESS_RGBA;
    popup_menu->root->is_enabled = false;

    // Initialize views array
    PanelSystem_InitViews(popup_menu, 2);

    InitCreateEntitySubmenu();
    InitCreateWorldSubmenu();
    InitPopupViewSelector();
    InitCreateView();
    InitRecentView();
    PanelSystem_SelectView(popup_view_selector, 0);

    UpdateUISpace(popup_menu->root, popup_menu->seed_box);
}

void ShowPopupMenu(Vector2d position)
{
    if (!popup_menu)
    {
        InitPopupMenu();
    }

    if (!popup_menu || !popup_menu_create_cont)
    {
        return;
    }
    // Update the position of the popup menu's root container to the specified position
    popup_menu->root->authored_offset = (Offset){position, OFFSET_FIXED};
    popup_spawn_position = position;
    popup_menu->root->resolved_offset = (Offset){position, OFFSET_FIXED};
    // popup_menu_create_cont->authored_offset = (Offset){position, OFFSET_FIXED};
    // popup_menu_create_cont->resolved_offset = popup_menu_create_cont->authored_offset;
    // popup_menu_recent_cont->authored_offset = (Offset){position, OFFSET_FIXED};
    // popup_menu_recent_cont->resolved_offset = popup_menu_recent_cont->authored_offset;
    // popup_view_selector_cont->authored_offset = (Offset){position, OFFSET_FIXED};
    // popup_view_selector_cont->resolved_offset = popup_view_selector_cont->authored_offset;
    UpdateUISpace(popup_menu->root, popup_menu->seed_box);
    popup_menu_visible = true;
    popup_menu->root->is_enabled = true;
    DisableElement(popup_create_entity_submenu_cont);
    DisableElement(popup_create_world_submenu_cont);
}

void HidePopupMenu(void)
{
    popup_menu_visible = false;
    if (popup_menu && popup_menu->root)
    {
        popup_menu->root->is_enabled = false;
    }
    if (popup_create_entity_submenu_cont)
    {
        DisableElement(popup_create_entity_submenu_cont);
    }
    if (popup_create_world_submenu_cont)
    {
        DisableElement(popup_create_world_submenu_cont);
    }
}

bool IsPopupMenuVisible(void)
{
    return popup_menu_visible;
}

void DrawPopupMenu(void)
{
    if (popup_menu && popup_menu_visible)
    {
        PanelSystem_Draw(popup_menu);
    }
}

Frame2d *GetPopupMenuSpaceFrame(void)
{
    return PanelSystem_GetSpaceFrame(popup_menu);
}

UIElement *GetPopupMenuRoot(void)
{
    return popup_menu ? popup_menu->root : NULL;
}

UIElement *GetPopupMenuContainer(void)
{
    return popup_menu_create_cont;
}
