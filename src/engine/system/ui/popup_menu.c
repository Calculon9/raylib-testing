#include "system/ui/popup_menu.h"

#include <string.h>
#include "entities/entity_prefab.h"
#include "system/view_host_system.h"
#include "system/ui/ui_loader.h"
#include "system/ui/panel_shared.h"   // PanelShared_Find
#include "system/command_system.h"
#include "system/universe_system.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"
#include "world/world_internal.h"

static ViewHostSystem *popup_menu = NULL;
static UIElement *popup_menu_create_cont = NULL;
static UIElement *popup_menu_recent_cont = NULL;
static UIElement *popup_create_entity_submenu_cont = NULL;
static UIElement *popup_create_world_submenu_cont = NULL;
// The ENTITY / WORLD category rows; their role follows whether their submenu is open.
static UIElement *popup_item_entity = NULL;
static UIElement *popup_item_world = NULL;
static bool popup_menu_visible = false;
static Vector2d popup_spawn_position = {0.0f, 0.0f};
// Root spacing passed to ViewHostSystem_Create in InitPopupMenu. Padding, row spacing and sizes come from
// popup.xml (the standard sheet), so no layout literals live here.
static const Layout popup_menu_root_spacing = UI_SPACING_NONE;

// Forward declarations for the loader-hybrid wiring pass (defined below InitPopupMenu
// mirrors state_manager; the id-pass and its helpers are declared here so InitPopupMenu
// can call them regardless of definition order).
static ViewType Popup_ResolveViewType(const char *type_string, bool *resolved,
                                      UILoaderContext *ctx);
static UIBinding Popup_ResolveBinding(const char *binding_string, UILoaderContext *ctx);
static int Popup_ResolveCommand(const char *cmd_string, UILoaderContext *ctx);
static void Popup_RegisterElements(UIElement *root);
static UIElement *Popup_Find(UIElement *root, const char *id);
static void Popup_WireClick(UIElement *root, const char *id,
                            UIEventHandler on_click, void *user_data);

typedef struct
{
    ShapeType shape;
    unsigned int component_flags;
} PopupCreateAction;

static PopupCreateAction popup_triangle_action = {SHAPE_TRIANGLE, 0};
static PopupCreateAction popup_square_action = {SHAPE_SQUARE, 0};
static PopupCreateAction popup_circle_action = {SHAPE_CIRCLE, 0};
static PopupCreateAction popup_rotor_action = {SHAPE_AUTO, CREATION_COMPONENT_ROTOR};
static PopupCreateAction popup_gear_action = {SHAPE_AUTO, CREATION_COMPONENT_GEAR};
static PopupCreateAction popup_portal_action = {SHAPE_AUTO, CREATION_COMPONENT_PORTAL};

typedef struct
{
    UIElement *submenu;
    bool enable_submenu;
} PopupHoverAction;

static PopupHoverAction popup_create_entity_action = {0, true};
static PopupHoverAction popup_create_world_action = {0, true};

// Give the ENTITY / WORLD category rows their role from the open state of their submenu:
// ACTIVE (amber) while the submenu is open, INACTIVE (teal) otherwise. Called after every change
// to a submenu's enabled state (hover, show, hide). Null-guarded for a partly loaded popup.
static void Popup_SyncRowRoles(void)
{
    if (!popup_menu)
    {
        return;
    }
    UIElement_ApplyRoleColours(popup_item_entity, popup_menu->palette,
        (popup_create_entity_submenu_cont && popup_create_entity_submenu_cont->is_enabled) ? UI_ROLE_ACTIVE : UI_ROLE_INACTIVE);
    UIElement_ApplyRoleColours(popup_item_world, popup_menu->palette,
        (popup_create_world_submenu_cont && popup_create_world_submenu_cont->is_enabled) ? UI_ROLE_ACTIVE : UI_ROLE_INACTIVE);
}

static void HandlePopupHover(UIElement *item)
{
    if (!item)
    {
        return;
    }

    PopupHoverAction *action = (PopupHoverAction *)item->interaction_user_data;
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
            // Place the submenu's left edge at the popup sheet's OUTER edge: one row width across (the
            // authored 100 % percent offset) plus the sheet's side padding, so it clears the padding
            // strip instead of overlapping it. The offset is a fraction of the row width, so it is
            // rebuilt from the row's laid-out width (layout units, as is the shared padding constant).
            float row_width = item->local_box.dimensions.x;
            if (row_width > 0.0f)
            {
                const float outer_edge_offset = (row_width + ui_section_padding.x) / row_width;
                action->submenu->authored_offset.offset.x = outer_edge_offset;
                action->submenu->resolved_offset.offset.x = outer_edge_offset; // The SPACING_NONE row runs no flow pass, so keep the resolved copy in step.
            }
            EnableElement(action->submenu);
        }
        else
        {
            DisableElement(action->submenu);
        }
        Popup_SyncRowRoles();
        UpdateUISpace(popup_menu->root, popup_menu->seed_box);
    }
}

static void HandlePopupCreateClick(UIElement *button)
{
    if (!button || !button->is_enabled ||
        !G_UIState.entity_create_params)
    {
        return;
    }

    PopupCreateAction *action = (PopupCreateAction *)button->interaction_user_data;
    if (!action)
    {
        return;
    }

    // Handle rotor: set vertex count and seed the historic physics-flag policy so a
    // popup-created rotor stays locked + spinning (lock_position/auto_spin true, spin_rate
    // 0 => historic 2.0f). Set explicitly because C zero-inits omitted fields to false.
    if (action->component_flags & CREATION_COMPONENT_ROTOR)
    {
        G_UIState.entity_create_params->physics.vertice_count = 4;
        G_UIState.entity_create_params->rotor_params.lock_position = true;
        G_UIState.entity_create_params->rotor_params.auto_spin = true;
        G_UIState.entity_create_params->rotor_params.spin_rate = 0.0f;
    }
    // Handle gear: load from prefab and return.
    else if (action->component_flags & CREATION_COMPONENT_GEAR)
    {
        EntityCreateParams gear_params = {0};
        if (!EntityPrefab_LoadFile("prefabs/gear_0.json", &gear_params))
        {
            return;
        }

        gear_params.physics.anchor_position = popup_spawn_position;
        if (!EnqueueCreateEntity(&gear_params))
        {
            return;
        }

        HidePopupMenu();
        return;
    }
    // Handle portal: load from prefab and return.
    else if (action->component_flags & CREATION_COMPONENT_PORTAL)
    {
        EntityCreateParams portal_params = {0};
        if (!EntityPrefab_LoadFile("prefabs/portal_0.json", &portal_params))
        {
            return;
        }

        portal_params.physics.anchor_position = popup_spawn_position;
        if (!EnqueueCreateEntity(&portal_params))
        {
            return;
        }

        HidePopupMenu();
        return;
    }
    // Handle standard entity: set vertex count based on shape.
    else
    {
        switch (action->shape)
        {
        case SHAPE_TRIANGLE:
            G_UIState.entity_create_params->physics.vertice_count = 3;
            break;
        case SHAPE_SQUARE:
            G_UIState.entity_create_params->physics.vertice_count = 4;
            break;
        case SHAPE_CIRCLE:
            G_UIState.entity_create_params->physics.vertice_count = MAX_SHAPE_VERTICES;
            break;
        default:
            return;
        }
    }

    G_UIState.entity_create_params->physics.shape_type =
        (action->component_flags & (CREATION_COMPONENT_ROTOR | CREATION_COMPONENT_GEAR)) ? SHAPE_AUTO : action->shape;
    G_UIState.entity_create_params->component_flags = action->component_flags;
    G_UIState.entity_create_params->physics.anchor_position = popup_spawn_position;
    EnqueueCreateEntity(G_UIState.entity_create_params);
    HidePopupMenu();
}

// Handle a world-preset button click: create a new world from the preset carried by
// the button's interaction_user_data, then hide the popup. (Live handler; unchanged.)
static void HandlePopupWorldClick(UIElement *button)
{
    if (!button || !button->is_enabled || !button->interaction_user_data)
    {
        return;
    }

    const CoordinateSpacePreset *preset =
        (const CoordinateSpacePreset *)button->interaction_user_data;
    if (preset)
    {
        CreateNewWorld_Preset(true, *preset);
        HidePopupMenu();
    }
}

// ============================================================================
// XML loader-hybrid: resolvers + id-registration wiring pass
// (mirrors InitStateManagerSystem / StateManager_RegisterElements)
// ============================================================================

// Map the <View type=...> strings to the popup ViewType enum. The application owns the
// enum names so the loader stays domain-free; an unrecognised string reports failure and
// defaults to the CREATE view.
static ViewType Popup_ResolveViewType(const char *type_string, bool *resolved,
                                      UILoaderContext *ctx)
{
    (void)ctx; // Currently unused; provided for future application-specific context.
    if (resolved)
    {
        *resolved = true;
    }
    if (type_string && !strcmp(type_string, "POPUP_MENU_CREATE_VIEW"))
    {
        return POPUP_MENU_CREATE_VIEW;
    }
    if (type_string && !strcmp(type_string, "POPUP_MENU_RECENT_VIEW"))
    {
        return POPUP_MENU_RECENT_VIEW;
    }
    // Unrecognised: report failure and default to the first (CREATE) view.
    if (resolved)
    {
        *resolved = false;
    }
    return POPUP_MENU_CREATE_VIEW;
}

// No declarative bindings in the popup; every name resolves to NONE.
static UIBinding Popup_ResolveBinding(const char *binding_string, UILoaderContext *ctx)
{
    (void)binding_string;
    (void)ctx;
    return (UIBinding){ .kind = UI_BIND_SRC_NONE };
}

// No declarative command buttons in the popup (create/world clicks are wired in C).
static int Popup_ResolveCommand(const char *cmd_string, UILoaderContext *ctx)
{
    (void)cmd_string;
    (void)ctx;
    return 0;
}

// Look up an element by id in the loaded tree, logging (and tolerating) a miss.
// Every consumer null-guards, so a missing id degrades to an inert element.
static UIElement *Popup_Find(UIElement *root, const char *id)
{
    return PanelShared_Find(root, "Popup", id);
}

// Acquire a clickable element by id and attach its base on_click + typed user_data.
static void Popup_WireClick(UIElement *root, const char *id,
                            UIEventHandler on_click, void *user_data)
{
    UIElement *element = Popup_Find(root, id);
    if (element)
    {
        element->on_click = on_click;
        element->interaction_user_data = user_data;
    }
}

// Re-acquire every UIElement* the C handlers/positioning need from the loaded tree,
// re-apply the container customisation the old Init* builders did, and wire the C-owned
// hover/click handlers + typed user_data by id. Any missing id is logged and left NULL;
// ShowPopupMenu/HidePopupMenu/HandlePopupHover null-guard every ref.
static void Popup_RegisterElements(UIElement *root)
{
    // --- Re-acquire the view containers into the existing static refs ---
    popup_menu_create_cont = Popup_Find(root, "create_view");
    popup_menu_recent_cont = Popup_Find(root, "recent_view");

    // --- Re-acquire the two submenu containers into the existing static refs ---
    popup_create_entity_submenu_cont = Popup_Find(root, "popup_entity_submenu");
    popup_create_world_submenu_cont  = Popup_Find(root, "popup_world_submenu");

    // --- Patch the hover actions' submenu pointers to the re-acquired containers ---
    popup_create_entity_action.submenu = popup_create_entity_submenu_cont;
    popup_create_world_action.submenu  = popup_create_world_submenu_cont;

    // --- Re-apply the only container flag the markup cannot express ---
    // The sheet fill, padding and row spacing all come from popup.xml (standard sheet), so only
    // is_draggable (a C-side flag) is set here. recent_view is authored enabled="false"; the host
    // selector owns live enable/disable.
    if (popup_menu_create_cont)
    {
        popup_menu_create_cont->is_draggable = true;
    }
    if (popup_menu_recent_cont)
    {
        popup_menu_recent_cont->is_draggable = true;
    }
    // Floating-sheet edge: the submenus open over the bright world, and the standard sheet has no
    // border, so each gets a 1 px outline in the panel background colour (the PANEL surface fill).
    // This is the only place a popup surface takes a border.
    const ColourRgba floating_edge = UIPalette_GetSurfaceStyle(popup_menu->palette, UI_PALETTE_SURFACE_PANEL).fill;
    UIElement *floating_sheets[2] = {popup_create_entity_submenu_cont, popup_create_world_submenu_cont};
    for (int i = 0; i < 2; i++)
    {
        if (floating_sheets[i])
        {
            floating_sheets[i]->colour_border = floating_edge;
            floating_sheets[i]->border_weight = UI_POPUP_EDGE_WEIGHT_PX;
        }
    }

    // --- Wire the ENTITY/WORLD labels: base on_hover + typed hover action, and restore
    //     the former CreateUIHoverItemDefault presentation (LEFT/CENTRE text alignment,
    //     is_draggable=false) that authoring as <Label> loses. The row colours come from the
    //     INACTIVE / ACTIVE roles (Popup_SyncRowRoles), not a hand-picked surface. ---
    popup_item_entity = Popup_Find(root, "popup_item_entity");
    popup_item_world = Popup_Find(root, "popup_item_world");
    UIElement *category_items[2] = {popup_item_entity, popup_item_world};
    PopupHoverAction *category_actions[2] = {&popup_create_entity_action, &popup_create_world_action};
    for (int i = 0; i < 2; i++)
    {
        UIElement *item = category_items[i];
        if (!item)
        {
            continue;
        }
        item->on_hover = HandlePopupHover;
        item->interaction_user_data = category_actions[i];
        // Border weight of the INACTIVE / ACTIVE control surfaces (they share the input border).
        item->border_weight = UIPalette_GetSurfaceStyle(popup_menu->palette, UI_PALETTE_SURFACE_INACTIVE).border_weight;
        SetUIElementTextHorizontalAlignment(item, UI_TEXT_ALIGN_LEFT);
        SetUIElementTextVerticalAlignment(item, UI_TEXT_VERTICAL_ALIGN_CENTRE);
        item->is_draggable = false;
    }
    Popup_SyncRowRoles();
    // --- Wire the six CREATE buttons: on_click + typed PopupCreateAction* ---
    Popup_WireClick(root, "popup_btn_triangle", HandlePopupCreateClick, &popup_triangle_action);
    Popup_WireClick(root, "popup_btn_square",   HandlePopupCreateClick, &popup_square_action);
    Popup_WireClick(root, "popup_btn_circle",   HandlePopupCreateClick, &popup_circle_action);
    Popup_WireClick(root, "popup_btn_rotor",    HandlePopupCreateClick, &popup_rotor_action);
    Popup_WireClick(root, "popup_btn_gear",     HandlePopupCreateClick, &popup_gear_action);
    Popup_WireClick(root, "popup_btn_portal",   HandlePopupCreateClick, &popup_portal_action);

    // --- Wire the six WORLD buttons: on_click + CoordinateSpacePreset* (same mapping) ---
    Popup_WireClick(root, "popup_btn_regular",     HandlePopupWorldClick, (void *)&COORDINATE_SPACE_PRESET_REGULAR);
    Popup_WireClick(root, "popup_btn_sheared_y",   HandlePopupWorldClick, (void *)&COORDINATE_SPACE_PRESET_SHEARED_Y);
    Popup_WireClick(root, "popup_btn_sheared_x",   HandlePopupWorldClick, (void *)&COORDINATE_SPACE_PRESET_SHEARED_X);
    Popup_WireClick(root, "popup_btn_isometric",   HandlePopupWorldClick, (void *)&COORDINATE_SPACE_PRESET_ISOMETRIC);
    Popup_WireClick(root, "popup_btn_perspective", HandlePopupWorldClick, (void *)&COORDINATE_SPACE_PRESET_PERSPECTIVE);
    Popup_WireClick(root, "popup_btn_radial",      HandlePopupWorldClick, (void *)&COORDINATE_SPACE_PRESET_RADIAL);
}

// Initialise the popup creation menu from popup.xml via the generic UI loader
// (mirrors InitStateManagerSystem), then re-acquire pointers and wire C-owned
// behaviour in Popup_RegisterElements.
void InitPopupMenu(void)
{
    if (popup_menu)
    {
        return;
    }

    // 1) Load the authoritative markup with the popup resolvers (mirrors state_manager).
    UIElement *root = UILoader_LoadFromFileWithResolvers(
        UI_COMPONENTS_DIR "/popup.xml",
        UIPalette_GetActive(),
        Popup_ResolveBinding,
        Popup_ResolveCommand,
        Popup_ResolveViewType,
        NULL);
    if (!root)
    {
        // XML is the single authoritative source; a load failure is an asset/build error.
        LOG_ERROR("InitPopupMenu: failed to load popup.xml; popup will not render");
        return;
    }

    // 2) Extract the Views (each carries its ViewType from type= via Popup_ResolveViewType).
    size_t view_count = 0;
    View **xml_views = UILoader_ExtractViews(root, &view_count);

    // 3) Build the host against the game viewport, build the real root, re-parent the XML tree.
    popup_menu = ViewHostSystem_Create(&game_viewport, 1.0f, ZERO_VECTOR_2D,
                                       UIPalette_GetActive(), popup_menu_root_spacing);
    if (!popup_menu)
    {
        DisposeUIElement(root);
        if (xml_views)
        {
            Deallocate((void **)&xml_views, sizeof(View *) * view_count);
        }
        return;
    }

    ViewHostSystem_InitRoot(popup_menu);                 // Real UI_ELEMENT_ROOT + space + seed_box.
    popup_menu->root->colour_fill = COLOURLESS_RGBA;     // Transparent root (unchanged behaviour).
    popup_menu->root->is_enabled  = false;               // Hidden until ShowPopupMenu - visibility
                                                         // is owned by popup_menu->root ONLY. The
                                                         // re-parented <ViewHost> stays enabled
                                                         // (authored without enabled="false") so
                                                         // enabling the root lights the whole subtree.
    AddElementToTree(root, popup_menu->root);            // Re-parent; NEVER popup_menu->root = root.

    // 4) Register the XML Views with the host.
    ViewHostSystem_InitViews(popup_menu, view_count);
    for (size_t i = 0; i < view_count; i++)
    {
        LArray_Push(&popup_menu->views, &xml_views[i]);
    }
    if (xml_views)
    {
        // Free the array (not the Views, which the host now owns).
        Deallocate((void **)&xml_views, sizeof(View *) * view_count);
    }

    // 5) Build the selector from the XML <Option> metadata (click/enumerate).
    ViewSelector *view_selector =
        UILoader_BuildSelectorFromMarkup(popup_menu, ViewHostSystem_HandleViewSelected);

    // 6) Honour initial-view="create_view" (fall back to the first view).
    if (view_selector)
    {
        int initial = UILoader_ResolveViewIndexById(popup_menu, UILoader_GetInitialViewId());
        if (initial < 0)
        {
            initial = 0;
        }
        ViewHostSystem_SelectView(view_selector, (size_t)initial);
    }

    // 7) ID-REGISTRATION + WIRING PASS: re-acquire pointers + wire C handlers/user_data.
    Popup_RegisterElements(root);

    // 8) Final layout update over valid viewport space.
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

    // Convert the world anchor into the popup panel's game-viewport-local coordinates.
    Vector2d popup_pixel_position = TransformCoordinates(
        ResolveWorldToPixelMatrix(&G_Universe.root_world, &G_Universe.camera), position);
    Vector2d popup_viewport_position = TransformCoordinates(
        game_viewport.tunnel.dest_to_source_mtx, popup_pixel_position);
    // The popup panel lays out in UI pixels, so convert from game-viewport units.
    Vector2d popup_local_position = ViewHostSystem_ViewportToLocal(popup_menu, popup_viewport_position);

    // Keep the world position for entity creation, but position the UI in panel space.
    popup_spawn_position = position;
    // Offset the root to the click point. The root box is drawn from seed_box, but its
    // children derive from local_box which starts at this offset, so the whole popup subtree
    // translates to the cursor as one group and the hit-test resolves the ENTITY/WORLD items
    // correctly (verified at runtime: GetElementAt returns the hovered LABEL, over=1 through
    // the full chain). The fixed-size <ViewHost> is sized to its content and sits inside the
    // viewport, so no clamping occurs for in-bounds click points.
    popup_menu->root->authored_offset = (Offset){popup_local_position, OFFSET_FIXED};
    popup_menu->root->resolved_offset = (Offset){popup_local_position, OFFSET_FIXED};
    UpdateUISpace(popup_menu->root, popup_menu->seed_box);
    popup_menu_visible = true;
    popup_menu->root->is_enabled = true;
    DisableElement(popup_create_entity_submenu_cont);
    DisableElement(popup_create_world_submenu_cont);
    Popup_SyncRowRoles(); // Both submenus closed: both rows INACTIVE.
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
    Popup_SyncRowRoles(); // Both submenus closed: both rows INACTIVE.
}

bool IsPopupMenuVisible(void)
{
    return popup_menu_visible;
}

void DrawPopupMenu(void)
{
    if (popup_menu && popup_menu_visible)
    {
        ViewHostSystem_Draw(popup_menu);
    }
}

UIElement *GetPopupMenuRoot(void)
{
    return popup_menu ? popup_menu->root : NULL;
}

ViewHostSystem *GetPopupMenuViewHost(void)
{
    return popup_menu;
}

// Destroy the popup panel and clear its cached UI references.
void DestroyPopupMenu(void)
{
    HidePopupMenu();

    ViewHostSystem *panel = popup_menu;
    popup_menu = NULL;
    ViewHostSystem_Destroy(panel);

    popup_menu_create_cont = NULL;
    popup_menu_recent_cont = NULL;
    popup_create_entity_submenu_cont = NULL;
    popup_create_world_submenu_cont = NULL;
    popup_item_entity = NULL;
    popup_item_world = NULL;
    popup_create_entity_action.submenu = NULL;
    popup_create_world_action.submenu = NULL;
    popup_menu_visible = false;
    popup_spawn_position = ZERO_VECTOR_2D;
}
