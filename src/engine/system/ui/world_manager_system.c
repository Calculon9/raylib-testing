#include "system/ui/world_manager_system.h"
#include <string.h>
#include <stdio.h>
#include "system/command_system.h"
#include "ui/ui_input.h"                         // HandleBtnSubmitClick (NEW WORLD)
#include "system/ui/ui_loader.h"
#include "system/ui/panel_shared.h"              // Flag families, header block, field lock, selection helpers
#include "system/view_host_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/universe_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"
#include "world/universe.h"
#include "world/target_resolver.h"               // TargetRef (selection payload)
#include "ui/binding.h"                          // BindingValue / BIND_*

// ============================================================================
// Module State
// ============================================================================

static ViewHostSystem *s_wm_panel = NULL;
static ViewSelector *s_wm_selector = NULL;       // The world tab selector (NULL if allocation failed).

// View indices in markup order. 0..3 are the world tabs (option index == view index, which the
// selector relies on); the CELL view is reached only by selection.
enum
{
    WM_VIEW_PROPS = 0,
    WM_VIEW_FLAGS,
    WM_VIEW_GRID,
    WM_VIEW_NEW,
    WM_VIEW_CELL,
};

#define WM_SHOWN_UNKNOWN (-2)                    // No layout applied yet.

static int s_wm_tab = WM_VIEW_PROPS;             // World tab remembered across cell / root selections.
static int s_wm_shown_view = WM_SHOWN_UNKNOWN;   // View currently shown.
static bool s_wm_world_fields_locked = false;    // Cached lock states, so types only change on a flip.
static bool s_wm_cell_fields_locked = false;
static TargetKind s_wm_chip_kind = TARGET_KIND_NONE; // Panel-local WORLDS / CELLS choice (never touches the set).

// Which world / cell the panel is showing this frame, resolved once per frame by
// WorldManager_ResolveTargets and read by every query.
typedef struct WMTargets
{
    TargetKind kind;        // TARGET_KIND_WORLD or TARGET_KIND_CELL (NONE before the first frame)
    int count;              // Members of the shown kind (1 for the context-world fallback)
    TargetRef primary;      // Last member of the shown kind (or the context world)
    bool from_context;      // True when no world / cell is selected and the context world is shown
    bool is_root;           // True when the single shown world is the root world
    int world_count;        // Worlds in the SelectionSet
    int cell_count;         // Cells in the SelectionSet
} WMTargets;

static WMTargets s_wm_targets = {0};             // This frame's targets (read by the binding queries)

// Layout inputs; ApplyLayout only does work when this changes.
typedef struct WMLayoutSignature
{
    TargetKind kind;
    int count;
    int world_count;
    int cell_count;
    int deletable;
    bool is_root;
} WMLayoutSignature;

static WMLayoutSignature s_wm_signature = {0};
static bool s_wm_signature_valid = false;

// Action code and chip payloads for the C-wired buttons.
static int s_wm_action_create_world = BUTTON_ACTION_CREATE_WORLD;
static const TargetKind s_wm_chip_kinds[2] = { TARGET_KIND_WORLD, TARGET_KIND_CELL };

// UIElement references owned by the World Manager.
typedef struct WorldManagerUI
{
    PanelHeader header;             // world_header container + title + sub-line
    UIElement *kind_chips;          // chip row container (worlds + cells selected only)
    UIElement *kind_chip[2];        // WORLDS / CELLS chips (index matches s_wm_chip_kinds)
    UIElement *tab_container;       // the ViewSelector's container (hidden for a CELL kind)
    UIElement *delete_button;       // wm_delete_world (label reflects the shown worlds)
    UIElement *note[3];             // "values: primary only" notes: PROPS, GRID, CELL
    UIElement *world_fields[5];     // gravity, restitution, friction, basis u, basis v (lockable)
    UIElement *cell_fields[2];      // warp u, warp v (lockable)
} WorldManagerUI;

static WorldManagerUI s_wm_ui = {0};

// Flag definitions and their bound controls (wired in WorldManager_RegisterElements).
static FlagButton s_wm_world_flags[7] = {
    {"ACTIVE", WORLD_FLAG_ACTIVE, NULL},
    {"VISIBLE", WORLD_FLAG_VISIBLE, NULL},
    {"SELECTABLE", WORLD_FLAG_SELECTABLE, NULL},
    {"PHYSICS", WORLD_FLAG_PHYSICS_ENABLED, NULL},
    {"SPAWNS", WORLD_FLAG_SPAWNS_ENABLED, NULL},
    {"LOCKED", WORLD_FLAG_LOCKED, NULL},
    {"DRAG", WORLD_FLAG_DRAGGABLE, NULL},
};
static FlagButton s_wm_cell_flags[4] = {
    {"SOLID", CELL_FLAG_SOLID, NULL},
    {"WALKABLE", CELL_FLAG_WALKABLE, NULL},
    {"HAZARD", CELL_FLAG_HAZARDOUS, NULL},
    {"SPAWN", CELL_FLAG_SPAWNABLE, NULL},
};

// Per-family id tables; order MUST match the flag arrays above.
static const char *wm_world_ids[7] = {"wm_world_active", "wm_world_visible", "wm_world_selectable",
                                      "wm_world_physics", "wm_world_spawns", "wm_world_locked",
                                      "wm_world_drag"};
static const char *wm_cell_ids[4]  = {"wm_cell_solid", "wm_cell_walkable", "wm_cell_hazard",
                                      "wm_cell_spawn"};

// ============================================================================
// Target resolution
// ============================================================================

// Work out what the panel shows. Worlds and cells in the SelectionSet win; with neither, the context
// world is shown (so an entity selection shows its own world and an empty selection shows ROOT).
// When both worlds and cells are selected, the chips choose the kind. That choice is panel-local
// (s_wm_chip_kind) and never reorders the set, because the context world follows the set's primary.
// Defaults to the kind of the first world / cell member, and resets whenever only one kind remains.
// Never uses the context world while the set holds a world, as it could belong to another kind.
static void WorldManager_ResolveTargets(WMTargets *out)
{
    memset(out, 0, sizeof(*out));
    out->world_count = PanelShared_CountKind(TARGET_KIND_WORLD);
    out->cell_count = PanelShared_CountKind(TARGET_KIND_CELL);

    if (out->world_count > 0 && out->cell_count > 0)
    {
        if (s_wm_chip_kind != TARGET_KIND_WORLD && s_wm_chip_kind != TARGET_KIND_CELL)
        {
            // Unset: take the kind of the first world / cell member in selection order.
            int count = SelectionSet_Count();
            for (int i = 0; i < count; i++)
            {
                TargetRef ref;
                if (SelectionSet_Get(i, &ref) && (ref.kind == TARGET_KIND_WORLD || ref.kind == TARGET_KIND_CELL))
                {
                    s_wm_chip_kind = ref.kind;
                    break;
                }
            }
        }
        out->kind = s_wm_chip_kind;
    }
    else
    {
        s_wm_chip_kind = TARGET_KIND_NONE; // Only one kind present: recompute the default next time.
        out->kind = (out->cell_count > 0) ? TARGET_KIND_CELL : TARGET_KIND_WORLD;
    }

    if (out->kind == TARGET_KIND_WORLD && out->world_count == 0)
    {
        // Nothing world-like selected: show the context world as a single target.
        out->from_context = true;
        out->count = 1;
        out->primary = (TargetRef){ .kind = TARGET_KIND_WORLD,
                                    .as.world_index = UIState_GetContextWorldIndex() };
    }
    else
    {
        out->count = (out->kind == TARGET_KIND_WORLD) ? out->world_count : out->cell_count;
        PanelShared_PrimaryOfKind(out->kind, &out->primary);
    }

    out->is_root = out->kind == TARGET_KIND_WORLD && out->count == 1 &&
                   out->primary.as.world_index == UNIVERSE_ROOT_WORLD_INDEX;
}

// The primary shown world, or NULL when the panel shows cells or the ref is stale.
static World2d *WorldManager_PrimaryWorld(const WMTargets *targets)
{
    if (targets->kind != TARGET_KIND_WORLD)
    {
        return NULL;
    }
    return Universe_GetWorld(&G_Universe, targets->primary.as.world_index);
}

// Collect the nested world indices (>= 0) the DELETE WORLD button would delete: the selected worlds,
// or the context world when it is shown by fallback. Root is never deletable. Returns the count.
static int WorldManager_CollectDeletableWorlds(const WMTargets *targets, int *indices, int capacity)
{
    int total = 0;
    if (targets->kind != TARGET_KIND_WORLD)
    {
        return 0;
    }
    if (targets->from_context)
    {
        if (targets->primary.as.world_index >= 0 && capacity > 0)
        {
            indices[total++] = targets->primary.as.world_index;
        }
        return total;
    }

    int count = SelectionSet_Count();
    for (int i = 0; i < count && total < capacity; i++)
    {
        TargetRef ref;
        if (SelectionSet_Get(i, &ref) && ref.kind == TARGET_KIND_WORLD && ref.as.world_index >= 0)
        {
            indices[total++] = ref.as.world_index;
        }
    }
    return total;
}

// ============================================================================
// Binding queries and writes
// ============================================================================

// Field keys carried by the binding queries / writes below.
typedef enum WMField
{
    WM_FIELD_HEADER_TITLE,
    WM_FIELD_HEADER_SUB,
    WM_FIELD_ORIGIN,
    WM_FIELD_RES,
    WM_FIELD_OBJECTS,
    WM_FIELD_GRAVITY,
    WM_FIELD_RESTITUTION,
    WM_FIELD_FRICTION,
    WM_FIELD_BASIS_U,
    WM_FIELD_BASIS_V,
    WM_FIELD_CELL_INDEX,
    WM_FIELD_CELL_OCCU,
    WM_FIELD_CELL_VALUE,
    WM_FIELD_CELL_FILL,
    WM_FIELD_CELL_TILE,
    WM_FIELD_CELL_WARP_U,
    WM_FIELD_CELL_WARP_V,
} WMField;

// Panel-owned backing text for the string results (the returned pointers must outlive the call).
static char s_wm_header_title_text[48];
static char s_wm_header_sub_text[64];
static char s_wm_res_text[24];

// A neutral "no value" result; BIND_STRING (never BIND_NONE) so stale text is cleared.
static BindingValue WorldManager_Text(const char *text)
{
    return (BindingValue){ .type = BIND_STRING, .as.s = text };
}

// Header query: the title / sub-line for the shown world or cell. Stale refs print "-" rather than
// being dereferenced.
static BindingValue WorldManager_QueryHeader(const WMTargets *targets, bool want_sub)
{
    char *out = want_sub ? s_wm_header_sub_text : s_wm_header_title_text;
    size_t out_bytes = want_sub ? sizeof(s_wm_header_sub_text) : sizeof(s_wm_header_title_text);
    out[0] = '\0';

    if (targets->kind == TARGET_KIND_NONE)
    {
        snprintf(out, out_bytes, "-");
    }
    else if (targets->count > 1)
    {
        snprintf(out, out_bytes, want_sub ? "applies to all shown" : "EDITING %d SELECTED", targets->count);
    }
    else if (targets->kind == TARGET_KIND_WORLD)
    {
        int index = targets->primary.as.world_index;
        World2d *world = WorldManager_PrimaryWorld(targets);
        bool root = (index == UNIVERSE_ROOT_WORLD_INDEX);
        if (!want_sub)
        {
            if (root)
            {
                snprintf(out, out_bytes, "ROOT WORLD");
            }
            else
            {
                snprintf(out, out_bytes, "WORLD #%d", index);
            }
        }
        else if (!world)
        {
            snprintf(out, out_bytes, "-");
        }
        else if (root)
        {
            snprintf(out, out_bytes, "objects %d", (int)world->objects.count);
        }
        else
        {
            snprintf(out, out_bytes, "%d/%d  res %dx%d  objects %d", index + 1, GetWorldCount(),
                     (int)world->grid_space.space.columns, (int)world->grid_space.space.rows,
                     (int)world->objects.count);
        }
    }
    else
    {
        TargetRef ref = targets->primary;
        if (!want_sub)
        {
            snprintf(out, out_bytes, "CELL (%d,%d)", ref.as.cell.lattice_x, ref.as.cell.lattice_y);
        }
        else
        {
            Cell *cell = UIState_ResolveCellRef(ref);
            if (cell)
            {
                snprintf(out, out_bytes, "world %d  tile %d", ref.as.cell.world_index, (int)cell->tile_type);
            }
            else
            {
                snprintf(out, out_bytes, "world %d  tile -", ref.as.cell.world_index);
            }
        }
    }
    return WorldManager_Text(out);
}

// Query for a world field of the primary shown world. The root world has no origin, resolution,
// gravity, material or basis, so those read "N/A" (matching the old panel); its object count is real.
static BindingValue WorldManager_QueryWorldField(const WMTargets *targets, WMField field)
{
    World2d *world = WorldManager_PrimaryWorld(targets);
    if (!world)
    {
        return WorldManager_Text("-");
    }
    if (field == WM_FIELD_OBJECTS)
    {
        return (BindingValue){ .type = BIND_INT, .as.i = (int)world->objects.count };
    }
    if (targets->primary.as.world_index < 0)
    {
        return WorldManager_Text("N/A");
    }

    switch (field)
    {
    case WM_FIELD_ORIGIN:
        return (BindingValue){ .type = BIND_VECTOR2D, .as.v = world->uni_coords_center };
    case WM_FIELD_RES:
        snprintf(s_wm_res_text, sizeof(s_wm_res_text), "%dx%d",
                 (int)world->grid_space.space.columns, (int)world->grid_space.space.rows);
        return WorldManager_Text(s_wm_res_text);
    case WM_FIELD_GRAVITY:
        return (BindingValue){ .type = BIND_FLOAT, .as.f = world->gravity };
    case WM_FIELD_RESTITUTION:
        return (BindingValue){ .type = BIND_FLOAT, .as.f = world->grid_space.object.restitution };
    case WM_FIELD_FRICTION:
        return (BindingValue){ .type = BIND_FLOAT, .as.f = world->grid_space.object.friction };
    case WM_FIELD_BASIS_U:
        return (BindingValue){ .type = BIND_VECTOR2D, .as.v = world->grid_space.space.frame.basis.u };
    case WM_FIELD_BASIS_V:
        return (BindingValue){ .type = BIND_VECTOR2D, .as.v = world->grid_space.space.frame.basis.v };
    default:
        return WorldManager_Text("-");
    }
}

// Query for a cell field of the primary shown cell. A stale cell prints "-", except the warp fields,
// which show the identity component so an inert value is displayed rather than stale data.
static BindingValue WorldManager_QueryCellField(const WMTargets *targets, WMField field)
{
    Cell *cell = (targets->kind == TARGET_KIND_CELL) ? UIState_ResolveCellRef(targets->primary) : NULL;
    if (field == WM_FIELD_CELL_WARP_U || field == WM_FIELD_CELL_WARP_V)
    {
        bool is_u = (field == WM_FIELD_CELL_WARP_U);
        Vector2d v = is_u ? (Vector2d){1.0f, 0.0f} : (Vector2d){0.0f, 1.0f};
        if (cell)
        {
            v = is_u ? cell->warp_basis.u : cell->warp_basis.v;
        }
        return (BindingValue){ .type = BIND_VECTOR2D, .as.v = v };
    }
    if (!cell)
    {
        return WorldManager_Text("-");
    }

    switch (field)
    {
    case WM_FIELD_CELL_INDEX:
        return (BindingValue){ .type = BIND_INT, .as.i = UIState_GetCellIndexForRef(targets->primary) };
    case WM_FIELD_CELL_OCCU:
        return (BindingValue){ .type = BIND_INT, .as.i = UIState_GetCellOccupancyForRef(targets->primary) };
    case WM_FIELD_CELL_VALUE:
        return (BindingValue){ .type = BIND_FLOAT, .as.f = cell->value };
    case WM_FIELD_CELL_FILL:
        // Fill has no backing data yet; it reads zero as the old Cell view did.
        return (BindingValue){ .type = BIND_FLOAT, .as.f = 0.0f };
    case WM_FIELD_CELL_TILE:
        return (BindingValue){ .type = BIND_INT, .as.i = cell->tile_type };
    default:
        return WorldManager_Text("-");
    }
}

// Single query entry point for every World Manager binding (key is a WMField). Reads the targets
// resolved at the top of this frame's draw.
static BindingValue WorldManager_Query(int key)
{
    const WMTargets *targets = &s_wm_targets;
    WMField field = (WMField)key;
    switch (field)
    {
    case WM_FIELD_HEADER_TITLE:
        return WorldManager_QueryHeader(targets, false);
    case WM_FIELD_HEADER_SUB:
        return WorldManager_QueryHeader(targets, true);
    case WM_FIELD_CELL_INDEX:
    case WM_FIELD_CELL_OCCU:
    case WM_FIELD_CELL_VALUE:
    case WM_FIELD_CELL_FILL:
    case WM_FIELD_CELL_TILE:
    case WM_FIELD_CELL_WARP_U:
    case WM_FIELD_CELL_WARP_V:
        return WorldManager_QueryCellField(targets, field);
    default:
        return WorldManager_QueryWorldField(targets, field);
    }
}

// Commit path for the editable fields (gravity, restitution, friction, basis, cell warp): called by
// the binding core when a SAFE_IO textbox commits. Returning false makes the core revert the field,
// so a bad value, a root / stale target or a rejected basis leaves the data untouched. Only a single
// target is editable (multi-select locks the fields), and the targets are re-resolved fresh.
static bool WorldManager_Write(int key, BindingValue value)
{
    WMTargets targets;
    WorldManager_ResolveTargets(&targets);
    if (targets.count != 1)
    {
        return false;
    }

    WMField field = (WMField)key;
    if (field == WM_FIELD_CELL_WARP_U || field == WM_FIELD_CELL_WARP_V)
    {
        Cell *cell = (targets.kind == TARGET_KIND_CELL) ? UIState_ResolveCellRef(targets.primary) : NULL;
        if (!cell || value.type != BIND_VECTOR2D)
        {
            return false;
        }
        if (field == WM_FIELD_CELL_WARP_U)
        {
            cell->warp_basis.u = value.as.v;
        }
        else
        {
            cell->warp_basis.v = value.as.v;
        }
        return true;
    }

    // World fields: nested worlds only (root is read-only).
    int index = targets.primary.as.world_index;
    World2d *world = (targets.kind == TARGET_KIND_WORLD && index >= 0) ? Universe_GetWorld(&G_Universe, index) : NULL;
    if (!world)
    {
        return false;
    }

    switch (field)
    {
    case WM_FIELD_GRAVITY:
    case WM_FIELD_RESTITUTION:
    case WM_FIELD_FRICTION:
        if (value.type != BIND_FLOAT)
        {
            return false;
        }
        if (field == WM_FIELD_GRAVITY)
        {
            world->gravity = value.as.f;
        }
        else if (field == WM_FIELD_RESTITUTION)
        {
            world->grid_space.object.restitution = value.as.f;
        }
        else
        {
            world->grid_space.object.friction = value.as.f;
        }
        return true;

    case WM_FIELD_BASIS_U:
    case WM_FIELD_BASIS_V:
    {
        if (value.type != BIND_VECTOR2D)
        {
            return false;
        }
        // The basis is validated before anything is stored, so a rejected (degenerate) basis leaves
        // the world untouched and the field reverts.
        Basis2d current = world->grid_space.space.frame.basis;
        Vector2d new_u = (field == WM_FIELD_BASIS_U) ? value.as.v : current.u;
        Vector2d new_v = (field == WM_FIELD_BASIS_V) ? value.as.v : current.v;
        return Universe_SetWorldBasis(&G_Universe, index, new_u, new_v);
    }

    default:
        return false;
    }
}

// ============================================================================
// UI Loader Resolvers (Application-Specific)
// ============================================================================

// Resolve a world-manager view-type string to its ViewType code. The application owns the enum
// names so the loader stays domain-free. An unrecognised string reports failure and defaults to
// the first view.
static ViewType WorldManager_ResolveViewType(const char *type_string, bool *resolved,
                                             UILoaderContext *ctx)
{
    (void)ctx;
    static const struct { const char *name; ViewType type; } views[] = {
        {"WORLD_MANAGER_PROPS_VIEW", WORLD_MANAGER_PROPS_VIEW},
        {"WORLD_MANAGER_FLAGS_VIEW", WORLD_MANAGER_FLAGS_VIEW},
        {"WORLD_MANAGER_GRID_VIEW", WORLD_MANAGER_GRID_VIEW},
        {"WORLD_MANAGER_NEW_VIEW", WORLD_MANAGER_NEW_VIEW},
        {"WORLD_MANAGER_CELL_VIEW", WORLD_MANAGER_CELL_VIEW},
    };

    if (resolved)
    {
        *resolved = true;
    }
    for (size_t i = 0; type_string && i < ARRAY_COUNT(views); i++)
    {
        if (!strcmp(type_string, views[i].name))
        {
            return views[i].type;
        }
    }

    // Unrecognised: report failure and default to the first world-manager view.
    if (resolved)
    {
        *resolved = false;
    }
    return WORLD_MANAGER_PROPS_VIEW;
}

// Resolve action strings to command codes (adapter for CommandSystem_ResolveString). Supplied to
// the loader for parity with the other panels; the buttons here are wired in C.
static int WorldManager_ResolveCommand(const char *cmd_string, UILoaderContext *ctx)
{
    (void)ctx;
    return CommandSystem_ResolveString(cmd_string);
}

// Resolve a binding= name to a query source (and a write sink for the editable fields).
static UIBinding WorldManager_ResolveBinding(const char *binding_string, UILoaderContext *ctx)
{
    (void)ctx;
    static const struct
    {
        const char *name;
        WMField key;
        BindingValueType type; // Advisory display type; also the parse type for editable fields.
        bool writable;
    } bindings[] = {
        {"wm.header.title",       WM_FIELD_HEADER_TITLE,   BIND_STRING,   false},
        {"wm.header.sub",         WM_FIELD_HEADER_SUB,     BIND_STRING,   false},
        {"wm.world.origin",       WM_FIELD_ORIGIN,         BIND_VECTOR2D, false},
        {"wm.world.res",          WM_FIELD_RES,            BIND_STRING,   false},
        {"wm.world.objects",      WM_FIELD_OBJECTS,        BIND_INT,      false},
        {"wm.world.gravity",      WM_FIELD_GRAVITY,        BIND_FLOAT,    true},
        {"wm.world.restitution",  WM_FIELD_RESTITUTION,    BIND_FLOAT,    true},
        {"wm.world.friction",     WM_FIELD_FRICTION,       BIND_FLOAT,    true},
        {"wm.world.basis_u",      WM_FIELD_BASIS_U,        BIND_VECTOR2D, true},
        {"wm.world.basis_v",      WM_FIELD_BASIS_V,        BIND_VECTOR2D, true},
        {"wm.cell.index",         WM_FIELD_CELL_INDEX,     BIND_INT,      false},
        {"wm.cell.occu",          WM_FIELD_CELL_OCCU,      BIND_INT,      false},
        {"wm.cell.value",         WM_FIELD_CELL_VALUE,     BIND_FLOAT,    false},
        {"wm.cell.fill",          WM_FIELD_CELL_FILL,      BIND_FLOAT,    false},
        {"wm.cell.tile",          WM_FIELD_CELL_TILE,      BIND_INT,      false},
        {"wm.cell.warp_u",        WM_FIELD_CELL_WARP_U,    BIND_VECTOR2D, true},
        {"wm.cell.warp_v",        WM_FIELD_CELL_WARP_V,    BIND_VECTOR2D, true},
    };

    for (size_t i = 0; binding_string && i < ARRAY_COUNT(bindings); i++)
    {
        if (!strcmp(binding_string, bindings[i].name))
        {
            return (UIBinding){
                .kind = UI_BIND_SRC_QUERY,
                .query = WorldManager_Query,
                .query_key = (int)bindings[i].key,
                .value_type = bindings[i].type,
                .write = bindings[i].writable ? WorldManager_Write : NULL,
                .write_key = (int)bindings[i].key,
            };
        }
    }
    return (UIBinding){ .kind = UI_BIND_SRC_NONE };
}

// ============================================================================
// Click handlers
// ============================================================================

// Click a kind chip: record the panel-local kind. This never touches the SelectionSet (the context
// world follows the set's primary, so reordering it would change the world being shown elsewhere).
static void HandleKindChipClick(UIElement *chip)
{
    if (chip && chip->interaction_user_data)
    {
        s_wm_chip_kind = *(const TargetKind *)chip->interaction_user_data;
    }
}

// DELETE WORLD: enqueue deletes for the shown worlds, highest index first (shared with the DELETE
// key). The selection is left alone; the delete command clears it as the DELETE key's does.
static void HandleDeleteWorldsClick(UIElement *button)
{
    (void)button;
    int indices[UNIVERSE_MAX_WORLDS];
    int count = WorldManager_CollectDeletableWorlds(&s_wm_targets, indices, UNIVERSE_MAX_WORLDS);
    if (count > 0)
    {
        EnqueueDeleteWorldsDescending(indices, count);
    }
}

// ============================================================================
// Element id-registration + wiring (post-load)
// ============================================================================

// Look up an element by id in the loaded tree, logging (and tolerating) a miss. Every use is
// null-guarded, so a missing id degrades to an inert field, not a crash.
static UIElement *WorldManager_Find(UIElement *root, const char *id)
{
    UIElement *element = UILoader_FindById(root, id);
    if (!element)
    {
        LOG_ERROR("WorldManager: id '%s' not found in markup", id);
    }
    return element;
}

// Set the display precision of a bound textbox (the loader attaches bindings with the default
// precision of 0, which would show a float as a whole number).
static void WorldManager_SetPrecision(UIElement *textbox, int precision)
{
    if (textbox && textbox->data.textbox.binding)
    {
        textbox->data.textbox.binding->precision = precision;
    }
}

// Bind a NEW-view field to one of the next-world parameters (a fixed address, source and sink).
static void WorldManager_BindNewField(UIElement *root, const char *id, DataType type, void *address,
                                      int precision)
{
    UIElement *textbox = WorldManager_Find(root, id);
    if (textbox)
    {
        textbox->data.textbox.data_type = type; // Parse / format agrees with the bound data.
        BindTextboxStable(textbox, type, address, precision);
    }
}

// Re-acquire every UIElement* the layout and the C click handlers need, from the loaded tree, and
// wire the C-owned behaviour (flags, chips, buttons, NEW-view bindings, notes).
static void WorldManager_RegisterElements(UIElement *root)
{
    // --- Header (title / sub-line / tint box) and chips ---
    PanelShared_HeaderBind(&s_wm_ui.header, root, "world_header", "wm_header_title", "wm_header_sub");
    s_wm_ui.kind_chips = WorldManager_Find(root, "kind_chips");
    static const char *chip_ids[2] = {"wm_kind_world", "wm_kind_cell"};
    for (int i = 0; i < 2; i++)
    {
        s_wm_ui.kind_chip[i] = WorldManager_Find(root, chip_ids[i]);
        if (s_wm_ui.kind_chip[i])
        {
            s_wm_ui.kind_chip[i]->on_click = HandleKindChipClick;
            s_wm_ui.kind_chip[i]->interaction_user_data = (void *)&s_wm_chip_kinds[i];
        }
    }
    s_wm_ui.tab_container = WorldManager_Find(root, "world_manager_views");

    // --- "values: primary only" notes (a Textbox value= attribute would be parsed as a number) ---
    static const char *note_ids[3] = {"wm_note_props", "wm_note_grid", "wm_note_cell"};
    for (int i = 0; i < 3; i++)
    {
        s_wm_ui.note[i] = WorldManager_Find(root, note_ids[i]);
        if (s_wm_ui.note[i])
        {
            snprintf(s_wm_ui.note[i]->data.textbox.text.string,
                     sizeof(s_wm_ui.note[i]->data.textbox.text.string), "values: primary only");
        }
    }

    // --- Lockable editable fields. Their bindings (query + write) come from the markup; floats
    //     need a display precision of 2 as the old panel used. ---
    static const char *world_field_ids[5] = {"wm_gravity", "wm_restitution", "wm_friction",
                                             "wm_basis_u", "wm_basis_v"};
    for (int i = 0; i < 5; i++)
    {
        s_wm_ui.world_fields[i] = WorldManager_Find(root, world_field_ids[i]);
    }
    for (int i = 0; i < 3; i++)
    {
        WorldManager_SetPrecision(s_wm_ui.world_fields[i], 2);
    }
    s_wm_ui.cell_fields[0] = WorldManager_Find(root, "wm_cell_warp_u");
    s_wm_ui.cell_fields[1] = WorldManager_Find(root, "wm_cell_warp_v");
    WorldManager_SetPrecision(WorldManager_Find(root, "wm_cell_value"), 1);
    WorldManager_SetPrecision(WorldManager_Find(root, "wm_cell_fill"), 1);

    // --- Flag buttons: acquire + wire handler + user_data + query SOURCE per family ---
    PanelShared_WireFlags(root, s_wm_world_flags, wm_world_ids, ARRAY_COUNT(s_wm_world_flags), FLAG_FAMILY_WORLD);
    PanelShared_WireFlags(root, s_wm_cell_flags, wm_cell_ids, ARRAY_COUNT(s_wm_cell_flags), FLAG_FAMILY_CELL);

    // --- DELETE WORLD and NEW WORLD ---
    s_wm_ui.delete_button = WorldManager_Find(root, "wm_delete_world");
    if (s_wm_ui.delete_button)
    {
        s_wm_ui.delete_button->on_click = HandleDeleteWorldsClick;
    }
    UIElement *new_world = WorldManager_Find(root, "wm_new_world");
    if (new_world)
    {
        new_world->on_click = HandleBtnSubmitClick;
        new_world->interaction_user_data = &s_wm_action_create_world;
    }

    // --- NEW view: bind the six next-world parameters once (fixed addresses) ---
    WorldManager_BindNewField(root, "wm_new_spawn", VECTOR2D, GetNextWorldSpawnOriginPtr(), 0);
    WorldManager_BindNewField(root, "wm_new_res", VECTOR2D, GetNextWorldResolutionPtr(), 0);
    WorldManager_BindNewField(root, "wm_new_objects", INT, GetNextWorldObjectCountPtr(), 0);
    WorldManager_BindNewField(root, "wm_new_gravity", FLOAT, GetNextWorldGravityPtr(), 2);
    WorldManager_BindNewField(root, "wm_new_basis_u", VECTOR2D, GetNextWorldBasisUPtr(), 0);
    WorldManager_BindNewField(root, "wm_new_basis_v", VECTOR2D, GetNextWorldBasisVPtr(), 0);
}

// ============================================================================
// Selection-driven layout
// ============================================================================

// Selector callback. Runs for user tab clicks (and the layout's own SelectView, which restores the
// remembered tab afterwards): records the tab and keeps the shown-view code in step.
static void WorldManager_OnViewSelected(View *view)
{
    ViewHostSystem_HandleViewSelected(view);
    if (s_wm_selector)
    {
        s_wm_tab = (int)s_wm_selector->active_index;
        s_wm_shown_view = s_wm_tab;
    }
}

// Apply everything the targets decide: header tint, kind chips, tab row and shown view, the field
// locks and notes, and the DELETE WORLD label. Called when the layout signature changes.
static void WorldManager_ApplyLayout(const WMTargets *targets, int deletable)
{
    if (!s_wm_panel || !s_wm_selector || s_wm_selector->count <= WM_VIEW_NEW)
    {
        return;
    }

    bool multi = targets->count > 1;
    bool world_kind = (targets->kind == TARGET_KIND_WORLD);

    // Header tint: amber while several targets are edited together.
    PanelShared_HeaderSetTint(&s_wm_ui.header, multi);

    // Kind chips: only when the selection holds both worlds and cells; the shown kind is the active chip.
    bool chips = targets->world_count > 0 && targets->cell_count > 0;
    if (s_wm_ui.kind_chips)
    {
        SetEnabledState(s_wm_ui.kind_chips, chips);
    }
    static const char *chip_names[2] = { "WORLDS", "CELLS" };
    for (int i = 0; i < 2; i++)
    {
        UIElement *chip = s_wm_ui.kind_chip[i];
        if (!chip)
        {
            continue;
        }
        char label[32];
        snprintf(label, sizeof(label), "%s %d", chip_names[i],
                 i == 0 ? targets->world_count : targets->cell_count);
        PanelShared_SetButtonText(chip, label);

        // Chips share the tab / toggle two-state colours: active = amber + dark ink, inactive = dimmed.
        UIPalette_GetToggleColours(&ui_default_palette, s_wm_chip_kinds[i] == targets->kind,
                                   &chip->colour_fill, &chip->data.button.font.colour);
    }

    // Tab row: world kinds only; FLAGS is hidden for the root world (it has no editable flags).
    if (s_wm_ui.tab_container)
    {
        SetEnabledState(s_wm_ui.tab_container, world_kind);
    }
    SetEnabledState(s_wm_selector->buttons[WM_VIEW_FLAGS], !targets->is_root);

    // Shown view: the CELL view for cells, otherwise the remembered tab (PROPS while FLAGS is hidden).
    int desired = WM_VIEW_CELL;
    if (world_kind)
    {
        desired = s_wm_tab;
        if (desired == WM_VIEW_FLAGS && targets->is_root)
        {
            desired = WM_VIEW_PROPS;
        }
    }
    if (desired != s_wm_shown_view)
    {
        if (desired == WM_VIEW_CELL)
        {
            PanelShared_ShowOnlyView(s_wm_panel, desired);
        }
        else
        {
            // SelectView fires the callback, which would overwrite the remembered tab; keep it.
            int remembered_tab = s_wm_tab;
            ViewHostSystem_SelectView(s_wm_selector, (size_t)desired);
            s_wm_tab = remembered_tab;
        }
        s_wm_shown_view = desired;
    }

    // Value fields are read-only in multi-select (and for the root world); the notes say so.
    PanelShared_SetFieldsLocked(s_wm_ui.world_fields, ARRAY_COUNT(s_wm_ui.world_fields),
                                &s_wm_world_fields_locked, multi || targets->is_root);
    PanelShared_SetFieldsLocked(s_wm_ui.cell_fields, ARRAY_COUNT(s_wm_ui.cell_fields),
                                &s_wm_cell_fields_locked, multi);
    for (int i = 0; i < 3; i++)
    {
        if (s_wm_ui.note[i])
        {
            SetEnabledState(s_wm_ui.note[i], multi);
        }
    }

    // DELETE WORLD: says what it deletes; hidden for cells, the root world alone, or nothing deletable.
    if (s_wm_ui.delete_button)
    {
        char label[32];
        if (deletable > 1)
        {
            snprintf(label, sizeof(label), "DELETE %d WORLDS", deletable);
        }
        else
        {
            snprintf(label, sizeof(label), "DELETE WORLD");
        }
        PanelShared_SetButtonText(s_wm_ui.delete_button, label);
        SetEnabledState(s_wm_ui.delete_button, deletable > 0);
    }
}

// ============================================================================
// Module Lifecycle
// ============================================================================

void InitWorldManagerSystem(void)
{
    // 1) Load the authoritative markup with the world-manager resolvers (mirrors InitStateManagerSystem).
    UIElement *root = UILoader_LoadFromFileWithResolvers(
        UI_COMPONENTS_DIR "/world_manager.xml",
        &ui_default_palette,
        WorldManager_ResolveBinding,
        WorldManager_ResolveCommand,
        WorldManager_ResolveViewType,
        NULL);
    if (!root)
    {
        // XML is the single authoritative source; a load failure is an asset/build error.
        LOG_ERROR("InitWorldManagerSystem: failed to load world_manager.xml; panel will not render");
        return;
    }

    // 2) Extract the Views (each carries its ViewType from type= via WorldManager_ResolveViewType).
    size_t view_count = 0;
    View **xml_views = UILoader_ExtractViews(root, &view_count);

    // 3) Build the host against the right-panel viewport, then build the real root and re-parent the
    //    XML container under it. UpdateUISpace (step 7) lays the panel out over the valid viewport.
    s_wm_panel = ViewHostSystem_Create(&rpanel_viewport, 1.0f, (Vector2d){0.1f, 0.1f},
                                       &ui_default_palette, ui_standard_stack_spacing);
    if (!s_wm_panel)
    {
        DisposeUIElement(root);
        if (xml_views)
        {
            Deallocate((void **)&xml_views, sizeof(View *) * view_count);
        }
        return;
    }

    ViewHostSystem_InitRoot(s_wm_panel);      // Real UI_ELEMENT_ROOT + space + seed_box.
    AddElementToTree(root, s_wm_panel->root); // Re-parent; NEVER panel->root = root.

    // 4) Register the XML Views with the host.
    ViewHostSystem_InitViews(s_wm_panel, view_count);
    for (size_t i = 0; i < view_count; i++)
    {
        LArray_Push(&s_wm_panel->views, &xml_views[i]);
    }
    if (xml_views)
    {
        // Free the array (not the Views, which the host now owns).
        Deallocate((void **)&xml_views, sizeof(View *) * view_count);
    }

    // 5) Build the selector from the XML <Option> metadata.
    s_wm_selector = UILoader_BuildSelectorFromMarkup(s_wm_panel, WorldManager_OnViewSelected);
    s_wm_tab = WM_VIEW_PROPS;
    s_wm_shown_view = WM_SHOWN_UNKNOWN;

    // 6) Honour initial-view="props_view" (fall back to the first view).
    if (s_wm_selector)
    {
        int initial = UILoader_ResolveViewIndexById(s_wm_panel, UILoader_GetInitialViewId());
        if (initial < 0)
        {
            initial = 0;
        }
        ViewHostSystem_SelectView(s_wm_selector, (size_t)initial);
    }
    else
    {
        // Selector allocation failed: establish single-view visibility manually.
        for (int i = 0; i < s_wm_panel->views.count; i++)
        {
            View *view = *((View **)LArray_Get(&s_wm_panel->views, i));
            if (!view || !view->container)
            {
                continue;
            }
            if (i == 0)
            {
                EnableElement(view->container);
            }
            else
            {
                DisableElement(view->container);
            }
        }
    }

    // 7) ID-REGISTRATION + WIRING PASS, then the first layout over the valid viewport space.
    WorldManager_RegisterElements(root);
    UpdateUISpace(s_wm_panel->root, s_wm_panel->seed_box);
}

void DrawWorldManagerSystem(void)
{
    if (!s_wm_panel || !s_wm_panel->root)
    {
        return;
    }

    // Resolve what the panel shows once, so every binding query this frame agrees.
    WorldManager_ResolveTargets(&s_wm_targets);

    // Re-apply the layout only when one of its inputs changes.
    int deletable_indices[UNIVERSE_MAX_WORLDS];
    WMLayoutSignature signature;
    memset(&signature, 0, sizeof(signature)); // Zero the padding too, so the memcmp below is exact.
    signature.kind = s_wm_targets.kind;
    signature.count = s_wm_targets.count;
    signature.world_count = s_wm_targets.world_count;
    signature.cell_count = s_wm_targets.cell_count;
    signature.deletable = WorldManager_CollectDeletableWorlds(&s_wm_targets, deletable_indices, UNIVERSE_MAX_WORLDS);
    signature.is_root = s_wm_targets.is_root;
    if (!s_wm_signature_valid || memcmp(&signature, &s_wm_signature, sizeof(signature)) != 0)
    {
        s_wm_signature = signature;
        s_wm_signature_valid = true;
        WorldManager_ApplyLayout(&s_wm_targets, signature.deletable);
    }

    ViewHostSystem_Draw(s_wm_panel);
}

Frame2d *GetWorldManagerSpaceFrame(void)
{
    return ViewHostSystem_GetSpaceFrame(s_wm_panel);
}

bool SetWorldManagerSpaceBasis(Vector2d basis_u, Vector2d basis_v)
{
    return ViewHostSystem_SetSpaceBasis(s_wm_panel, basis_u, basis_v);
}

void ResetWorldManagerSpaceBasis(void)
{
    ViewHostSystem_ResetSpaceBasis(s_wm_panel);
}

UIElement *GetWorldManagerRoot(void)
{
    return s_wm_panel ? s_wm_panel->root : NULL;
}

struct ViewHostSystem *GetWorldManagerViewHost(void)
{
    return s_wm_panel;
}

// Reset all button element references across a flag array.
static void ResetFlagButtons(FlagButton *buttons, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        buttons[i].button = NULL;
    }
}

// Destroy the World Manager and clear its cached UI references and panel-local state.
void DestroyWorldManagerSystem(void)
{
    ViewHostSystem *panel = s_wm_panel;
    s_wm_panel = NULL;
    ViewHostSystem_Destroy(panel);

    ResetFlagButtons(s_wm_world_flags, ARRAY_COUNT(s_wm_world_flags));
    ResetFlagButtons(s_wm_cell_flags, ARRAY_COUNT(s_wm_cell_flags));
    memset(&s_wm_ui, 0, sizeof(s_wm_ui));
    memset(&s_wm_targets, 0, sizeof(s_wm_targets));
    s_wm_selector = NULL; // Freed by ViewHostSystem_Destroy with the panel.
    s_wm_tab = WM_VIEW_PROPS;
    s_wm_shown_view = WM_SHOWN_UNKNOWN;
    s_wm_world_fields_locked = false;
    s_wm_cell_fields_locked = false;
    s_wm_chip_kind = TARGET_KIND_NONE;
    s_wm_signature_valid = false;
}
