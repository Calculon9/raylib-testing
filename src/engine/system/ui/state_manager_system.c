#include "system/ui/state_manager_system.h"
#include <string.h>
#include "system/command_system.h"
#include "ui/ui_input.h"                         // ResetUIInputState (drop focus when fields lock)
#include "ui/cfont.h"                            // FONT_LARGE_WITH_COLOUR / FONT_MEDIUM_WITH_COLOUR
#include "system/ui/ui_loader.h"
#include "system/view_host_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/utility_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"
#include "physics/newtonoid.h"
#include "math/geometry.h"                       // MAX_SHAPE_VERTICES (GEOMETRY-view row-pool size)
#include "math/coordinate_space.h"
#include "world/universe.h"
#include "entities/entity_registry.h"
#include "entities/entity_components.h"
#include "entities/health.h"
#include "system/entities/ancestry.h"          // Ancestry_WalkUp / Ancestry_GetKind
#include "system/entities/relation_system.h"   // RelationSystem_GetRelation
#include "entities/entity_id.h"                 // EntityId_GetSlotIndex / INVALID_ENTITY_ID
#include "world/target_resolver.h"              // TargetRef (primary selection payload)
#include "ui/binding.h"                          // BindingValue / BIND_STRING
#include <stdio.h>                               // snprintf for the read-only label text

// ============================================================================
// Module State
// ============================================================================

static ViewHostSystem *state_manager_panel = NULL;

static bool state_manager_refresh_dirty = true;

// Selection-driven layout state (see "Selection-driven layout" below).
static ViewSelector *s_sm_selector = NULL;   // The entity tab selector (NULL if allocation failed).
static int s_sm_entity_tab = 0;              // Entity tab remembered across world / cell selections.
static int s_sm_shown_view = -1;             // View currently shown (-1 until first layout).
static bool s_sm_fields_locked = false;      // Cached lock state, so types only change on a flip.
static ColourRgba s_sm_header_default_fill;  // Header box fill captured at load (restored when untinted).

// Chip click payloads: one static TargetKind per chip (index == kind - 1).
static const TargetKind s_sm_chip_kinds[3] = { TARGET_KIND_ENTITY, TARGET_KIND_WORLD, TARGET_KIND_CELL };

static void HandleKindChipClick(UIElement *chip); // Defined with the layout code; wired at registration.

// Component button binding: associates a button control with an EntityComponentType.
typedef struct StateManagerComponentButton
{
    const char *label;
    EntityComponentType type;
    UIElement *button;
} StateManagerComponentButton;

// Attached-component row slot for the COMP view (Phase 1). A fixed positional slot: the
// name Textbox is relabelled and the type is assigned per refresh to whichever attached
// component the slot currently represents, so the one shared Remove handler reads the live
// type from here. name_str caches &name_label->data.textbox.text at register time (the same
// imperative-relabel pattern the CELL readouts use) so the refresh writes the label without
// re-finding the element.
typedef struct StateManagerComponentRow
{
    UIElement *name_label;       // readonly Textbox showing the component name
    String64 *name_str;          // cached &name_label->data.textbox.text for imperative relabel
    UIElement *remove_button;    // simple Button wired to HandleComponentRemoveClick
    EntityComponentType type;    // component this slot currently represents (assigned per refresh)
} StateManagerComponentRow;

// Fixed GEOMETRY-view vertex-row slot count. One row per potential surface vector, so a selection
// with more than this many vertices is capped (rows beyond the pool are not shown); matches the
// MAX_SHAPE_VERTICES vertex budget so no in-range vertex is dropped.
#define STATE_MANAGER_GEOMETRY_ROW_COUNT MAX_SHAPE_VERTICES

// UIElement and String64 references owned by the State Manager panel.
typedef struct StateManagerUI
{
    // Action codes
    int delete_action;

    // COMP view (Phase 1): Add-component buttons (PORTAL/ROTOR/GEAR/HEALTH; RELATION/WORLD
    // excluded) and the fixed attached-row slots (name + Remove). The refresh shows/hides and
    // relabels these to mirror the selected entity's attached set.
    StateManagerComponentButton add_buttons[4];
    StateManagerComponentRow attached_rows[5];

    // Header (selection-driven title / sub-line, tint, kind chips) and view-switching state.
    UIElement *header_box;          // entity_header container; its fill is tinted in multi-select
    UIElement *header_title;        // FONT_LARGE title textbox
    UIElement *header_sub;          // FONT_MEDIUM sub-line textbox
    UIElement *kind_chips;          // chip row container (multi-select only)
    UIElement *kind_chip[3];        // ENTITY / WORLD / CELL chips (index == TargetKind - 1)
    UIElement *values_note;         // "values: primary only" note, shown in multi-select
    UIElement *delete_button;       // sm_delete (label reflects the selection)
    UIElement *tab_container;       // the ViewSelector's container (hidden for world / cell)
    UIElement *world_view_container;
    UIElement *cell_view_container;

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
    UIElement *comp_portal_target_tbox;   // Portal Dest. field (editable; binding-owned, no String64)
    UIElement *comp_portal_entrant_roles_tbox;
    String64 *comp_portal_entrant_roles_str;
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
    String64 *cell_tile_type_str; // read-only tile_type readout (mirrors the other cell readouts)
    UIElement *cell_warp_u_tbox; // editable Warp U (warp_basis.u) — dynamic query/write bound
    UIElement *cell_warp_v_tbox; // editable Warp V (warp_basis.v) — dynamic query/write bound

    // GEOMETRY view (v1): vertex-count readout + fixed readonly vertex-row pool.
    UIElement *geo_vertex_count_tbox;
    UIElement *geo_vertex_tbox[STATE_MANAGER_GEOMETRY_ROW_COUNT];
} StateManagerUI;

static StateManagerUI s_sm_ui = {0};

// Which bitmask on the current selection a flag button targets (object fields, world or cell).
typedef enum FlagFamily
{
    FLAG_FAMILY_ROLE,           // Exclusive entity role
    FLAG_FAMILY_CAPABILITY,
    FLAG_FAMILY_CONSTRAINT,
    FLAG_FAMILY_STATUS,
    FLAG_FAMILY_COLLISION_MASK,
    FLAG_FAMILY_WORLD,
    FLAG_FAMILY_CELL,
} FlagFamily;

// Flag button binding: associates a button control with its display label and underlying bitflag.
// `family` is assigned when the button is wired (StateManager_WireFlags).
typedef struct StateManagerFlagButton
{
    const char *label;
    uint32_t flag;
    UIElement *button;
    FlagFamily family;
} StateManagerFlagButton;

// All flag definitions and their bound controls stay together as module state.
typedef struct StateManagerFlags
{
    uint32_t role_flags;
    StateManagerFlagButton entity_role[5];
    StateManagerFlagButton entity_capability[4];
    StateManagerFlagButton entity_constraint[2];
    StateManagerFlagButton entity_status[3];
    StateManagerFlagButton collision_role_mask[5];
    StateManagerFlagButton world[7];
    StateManagerFlagButton cell[4];
} StateManagerFlags;

static StateManagerFlags s_sm_flags = {
    .role_flags = ENTITY_ROLE_WALL | ENTITY_ROLE_NEWTONOID |
                  ENTITY_ROLE_PROJECTILE | ENTITY_ROLE_EFFECT | ENTITY_ROLE_CAMERA,
    .entity_role = {
        {"WALL", ENTITY_ROLE_WALL, NULL},
        {"NEWTONOID", ENTITY_ROLE_NEWTONOID, NULL},
        {"PROJECTILE", ENTITY_ROLE_PROJECTILE, NULL},
        {"EFFECT", ENTITY_ROLE_EFFECT, NULL},
        {"CAMERA", ENTITY_ROLE_CAMERA, NULL},
    },
    .entity_capability = {
        {"DAMAGEABLE", ENTITY_CAPABILITY_DAMAGEABLE, NULL},
        {"VELOCITY", ENTITY_CAPABILITY_VELOCITY_ALIGNED, NULL},
        {"AFFECT OWNER", ENTITY_CAPABILITY_AFFECT_OWNER, NULL},
        {"SENSOR", ENTITY_CAPABILITY_SENSOR, NULL},
    },
    .entity_constraint = {
        {"POSITION LOCKED", ENTITY_CONSTRAINT_POSITION_LOCKED, NULL},
        {"NO CONTACT RESPONSE", ENTITY_CONSTRAINT_NO_CONTACT_RESPONSE, NULL},
    },
    .entity_status = {
        {"ALIVE", ENTITY_STATUS_FLAG_ALIVE, NULL},
        {"SLEEPING", ENTITY_STATUS_FLAG_SLEEPING, NULL},
        {"CLOCKED", ENTITY_STATUS_FLAG_CLOCKED, NULL},
    },
    .collision_role_mask = {
        {"WALL", ENTITY_ROLE_WALL, NULL},
        {"NEWTONOID", ENTITY_ROLE_NEWTONOID, NULL},
        {"PROJECTILE", ENTITY_ROLE_PROJECTILE, NULL},
        {"EFFECT", ENTITY_ROLE_EFFECT, NULL},
        {"CAMERA", ENTITY_ROLE_CAMERA, NULL},
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
// Ancestry View State (module-owned, read-only)
// ============================================================================
// Fixed number of pre-declared ancestry line slots. Six generations comfortably covers
// selected entity -> parent entities -> owning world -> universe root world for the current
// depth model; any line beyond the actual chain length renders blank. Ported from the lpanel
// ancestry machinery so the Entity Mgmt panel owns its own copy (the lpanel is retired later).
#define STATE_MANAGER_ANCESTRY_LINE_COUNT 6

// Panel-owned backing buffers for the ancestry view's read-only BIND_STRING labels. The binding
// core copies value.as.s into each textbox's own text buffer during the per-frame refresh, but
// the returned pointer must stay valid for the duration of that call, so storage is module-owned
// (never a stack local). One buffer per line slot, plus the header and relation summaries.
static char state_manager_ancestry_line_text[STATE_MANAGER_ANCESTRY_LINE_COUNT][96]; // One per ancestry line.
static char state_manager_ancestry_selected_text[96];                               // Header naming the rooted node.
static char state_manager_ancestry_relation_text[128];                              // Direct-relation summary.

/**
 * Resolve the EntityId the ancestry walk should root on, from the current PRIMARY selection.
 * Selection is read fresh every call (never cached) so the view always reflects the live set.
 * An entity selection roots on itself; a world selection roots on that world's own EntityId
 * (its chain is itself -> universe root world); a cell / empty selection has no frame-bearing
 * node, so INVALID_ENTITY_ID is returned (callers render the neutral "No selection" state).
 * This is the single source of truth the ancestry shims share, so selection resolution is not
 * duplicated across them.
 */
static EntityId StateManager_AncestryRootId(void)
{
    // Nothing selected: no root node.
    if (SelectionSet_Count() <= 0)
    {
        return INVALID_ENTITY_ID;
    }

    TargetRef ref = SelectionSet_Primary();
    switch (ref.kind)
    {
        case TARGET_KIND_ENTITY:
            // A plain entity roots the walk on itself.
            return ref.as.entity;

        case TARGET_KIND_WORLD:
        {
            // Resolve the world index to the world's own EntityId (its body in the registry).
            World2d *world = Universe_GetWorld(&G_Universe, ref.as.world_index);
            return world ? world->grid_space.object.id : INVALID_ENTITY_ID;
        }

        default:
            // Cell / none: ancestry is only meaningful for a frame-bearing node.
            return INVALID_ENTITY_ID;
    }
}

/**
 * Query shim for one ancestry line slot (line_key = 0..STATE_MANAGER_ANCESTRY_LINE_COUNT-1).
 * Rooted fresh each frame via StateManager_AncestryRootId, it walks up to the root world into a
 * small stack buffer and formats the requested generation as "<KIND> #<slot>". Slots beyond the
 * actual chain length (or when nothing is selected) render blank: the buffer is set to an empty
 * string and returned as BIND_STRING "" rather than BIND_NONE, because BIND_NONE leaves the
 * textbox's last-good text untouched (stale) instead of clearing it.
 */
static BindingValue StateManager_QueryAncestryLine(int line_key)
{
    // Clamp defensively; the resolver already bounds the key, but keep the array access safe.
    if (line_key < 0 || line_key >= STATE_MANAGER_ANCESTRY_LINE_COUNT)
    {
        return (BindingValue){ .type = BIND_STRING, .as.s = "" };
    }

    char *buf = state_manager_ancestry_line_text[line_key];
    buf[0] = '\0'; // Default to blank so an unused slot always clears.

    EntityId root = StateManager_AncestryRootId();
    if (root == INVALID_ENTITY_ID)
    {
        // Nothing frame-bearing is selected: this slot stays blank.
        return (BindingValue){ .type = BIND_STRING, .as.s = buf };
    }

    // Walk the chain fresh every frame. max_generations is a HOP count, so slots-1 hops yields up
    // to STATE_MANAGER_ANCESTRY_LINE_COUNT ids (start first), matching the slot capacity.
    EntityId chain[STATE_MANAGER_ANCESTRY_LINE_COUNT];
    int count = Ancestry_WalkUp(root, STATE_MANAGER_ANCESTRY_LINE_COUNT - 1, chain,
                                STATE_MANAGER_ANCESTRY_LINE_COUNT);

    if (line_key >= count)
    {
        // Beyond the chain length: leave the slot blank.
        return (BindingValue){ .type = BIND_STRING, .as.s = buf };
    }

    // Classify and format this generation's node. A node should not be invalid mid-chain, but
    // label it honestly if the registry no longer resolves it.
    AncestryNodeKind kind = Ancestry_GetKind(chain[line_key]);
    const char *label = (kind == ANCESTRY_KIND_WORLD)  ? "WORLD"
                      : (kind == ANCESTRY_KIND_ENTITY) ? "ENTITY"
                                                       : "INVALID";
    snprintf(buf, sizeof(state_manager_ancestry_line_text[line_key]), "%s #%u",
             label, EntityId_GetSlotIndex(chain[line_key]));

    return (BindingValue){ .type = BIND_STRING, .as.s = buf };
}

/**
 * Query shim for the ancestry header line. Names the rooted (primary-selected) node, or shows
 * the neutral "No selection" state when nothing frame-bearing is selected. Resolved fresh each
 * frame; returns BIND_STRING pointing at the panel-owned header buffer.
 */
static BindingValue StateManager_QueryAncestrySelected(int key)
{
    (void)key; // Single header source; key is unused.

    EntityId root = StateManager_AncestryRootId();
    if (root == INVALID_ENTITY_ID)
    {
        // Neutral empty state.
        snprintf(state_manager_ancestry_selected_text, sizeof(state_manager_ancestry_selected_text),
                 "No selection");
        return (BindingValue){ .type = BIND_STRING, .as.s = state_manager_ancestry_selected_text };
    }

    AncestryNodeKind kind = Ancestry_GetKind(root);
    const char *label = (kind == ANCESTRY_KIND_WORLD)  ? "WORLD"
                      : (kind == ANCESTRY_KIND_ENTITY) ? "ENTITY"
                                                       : "INVALID";
    snprintf(state_manager_ancestry_selected_text, sizeof(state_manager_ancestry_selected_text),
             "Selected: %s #%u", label, EntityId_GetSlotIndex(root));

    return (BindingValue){ .type = BIND_STRING, .as.s = state_manager_ancestry_selected_text };
}

/**
 * Query shim for the direct-relation summary line. Reads the rooted node's relation fresh each
 * frame (never caching the RelationComponent* across frames): "Relation: -" when nothing is
 * selected, "Relation: none" when the node has no relation, otherwise
 * "Relation: <type> -> #<target slot>" with an " (inactive)" suffix when the relation is dormant.
 * Returns BIND_STRING pointing at the panel-owned relation buffer.
 */
static BindingValue StateManager_QueryAncestryRelation(int key)
{
    (void)key; // Single relation source; key is unused.

    EntityId root = StateManager_AncestryRootId();
    if (root == INVALID_ENTITY_ID)
    {
        // Neutral empty state (nothing selected).
        snprintf(state_manager_ancestry_relation_text, sizeof(state_manager_ancestry_relation_text),
                 "Relation: -");
        return (BindingValue){ .type = BIND_STRING, .as.s = state_manager_ancestry_relation_text };
    }

    RelationComponent *rel = RelationSystem_GetRelation(root);
    if (!rel)
    {
        // Node exists but carries no relation.
        snprintf(state_manager_ancestry_relation_text, sizeof(state_manager_ancestry_relation_text),
                 "Relation: none");
        return (BindingValue){ .type = BIND_STRING, .as.s = state_manager_ancestry_relation_text };
    }

    // Describe the relation by type and target slot; flag an inactive relation for honesty.
    snprintf(state_manager_ancestry_relation_text, sizeof(state_manager_ancestry_relation_text),
             "Relation: %s -> #%u%s", RelationType_ToString(rel->type),
             EntityId_GetSlotIndex(rel->target_entity), rel->is_active ? "" : " (inactive)");

    return (BindingValue){ .type = BIND_STRING, .as.s = state_manager_ancestry_relation_text };
}

// ============================================================================
// UI Loader Resolvers (Application-Specific)
// ============================================================================

// Resolve a state-manager view-type string to its ViewType code. Mirrors
// LPanel_ResolveViewType: the application owns the enum names so the loader stays
// domain-free. An Unrecognised string reports failure and defaults to the first view.
static ViewType StateManager_ResolveViewType(const char *type_string, bool *resolved,
                                             UILoaderContext *ctx)
{
    (void)ctx; // Currently unused; provided for future application-specific context.

#define SM_VIEW_ENTRY(name) {#name, name}
    static const struct { const char *name; ViewType type; } views[] = {
        SM_VIEW_ENTRY(STATE_MANAGER_PHYSICS_VIEW),
        SM_VIEW_ENTRY(STATE_MANAGER_ATTRIBUTES_VIEW),
        SM_VIEW_ENTRY(STATE_MANAGER_WORLD_VIEW),
        SM_VIEW_ENTRY(STATE_MANAGER_CELL_STATE_VIEW),
        SM_VIEW_ENTRY(STATE_MANAGER_ANCESTRY_VIEW),
        SM_VIEW_ENTRY(STATE_MANAGER_COMPONENTS_VIEW),
        SM_VIEW_ENTRY(STATE_MANAGER_GEOMETRY_VIEW),
    };
#undef SM_VIEW_ENTRY

    for (size_t i = 0; i < ARRAY_COUNT(views); i++)
    {
        if (type_string && !strcmp(type_string, views[i].name))
        {
            if (resolved)
            {
                *resolved = true;
            }
            return views[i].type;
        }
    }

    // Unrecognised: report failure and default to the first state-manager view.
    if (resolved)
    {
        *resolved = false;
    }
    return STATE_MANAGER_PHYSICS_VIEW;
}

// Resolve action strings to command codes (adapter for CommandSystem_ResolveString).
// Supplied to the loader for parity with lpanel and to future-proof a declarative command
// button; no Stage-1 button uses it (DELETE is wired in C - see StateManager_RegisterElements).
static int StateManager_ResolveCommand(const char *cmd_string, UILoaderContext *ctx)
{
    (void)ctx; // Currently unused; provided for future application-specific context.
    return CommandSystem_ResolveString(cmd_string);
}

// Defined below with the other bulk-edit queries; forward-declared for the binding resolver.
static BindingValue StateManager_QueryHeader(int key);

// Resolve data bindings for the state-manager fields.
// STAGE 1: field values stay driven by the per-frame C refresh (RefreshPhysView etc.), so no
// field value is bound here yet. Returns UI_BIND_SRC_NONE for every name. Stage 2 fills this
// in (query + optional write) for entity.* / world.* / cell.* names.
static UIBinding StateManager_ResolveBinding(const char *binding_string, UILoaderContext *ctx)
{
    (void)ctx;

    if (!binding_string)
    {
        return (UIBinding){ .kind = UI_BIND_SRC_NONE };
    }

    // Ancestry view read-only labels: each maps to a BIND_STRING query source over the ported
    // ancestry shims (mirroring the lpanel ancestry branch). A header, a relation summary, and
    // six line slots keyed 0..5 by the trailing digit.
    if (!strcmp(binding_string, "ancestry.selected"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = StateManager_QueryAncestrySelected,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }
    if (!strcmp(binding_string, "ancestry.relation"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = StateManager_QueryAncestryRelation,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }
    if (!strncmp(binding_string, "ancestry.line", 13))
    {
        // Parse the trailing digit into a line slot index, keeping one shim for all six lines
        // (keyed by query_key). Only a single in-range digit resolves; anything else is unknown.
        const char *digits = binding_string + 13;
        if (digits[0] >= '0' && digits[0] <= '9' && digits[1] == '\0')
        {
            int line_key = digits[0] - '0';
            if (line_key >= 0 && line_key < STATE_MANAGER_ANCESTRY_LINE_COUNT)
            {
                return (UIBinding){
                    .kind = UI_BIND_SRC_QUERY,
                    .query = StateManager_QueryAncestryLine,
                    .query_key = line_key,
                    .value_type = BIND_STRING,
                };
            }
        }
        // Malformed ancestry.lineN name -> unresolved (loader warns).
        return (UIBinding){ .kind = UI_BIND_SRC_NONE };
    }

    // Entity Manager header: read-only title / sub-line chosen by the current selection.
    if (!strcmp(binding_string, "sm.header.title") || !strcmp(binding_string, "sm.header.sub"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = StateManager_QueryHeader,
            .query_key = !strcmp(binding_string, "sm.header.sub") ? 1 : 0, // HEADER_KEY_SUB / _TITLE
            .value_type = BIND_STRING,
        };
    }

    // STAGE 1: all other field values stay driven by the per-frame C refresh (RefreshPhysView
    // etc.), so no field value is bound here yet.
    return (UIBinding){ .kind = UI_BIND_SRC_NONE };
}

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
// Dynamic damage binding (selection policy lives here, not in the generic core)
// ============================================================================

// DYNAMIC bindings below share two selection helpers; the clear-when-none policy lives here, so
// the generic binding core never learns what "selected" means.

// The selected object, or NULL when nothing valid is selected.
static Newtonoid2d *StateManager_SelectedObject(void)
{
    Newtonoid2d *object = UIState_GetSelectedObject();
    return (object && object->id != INVALID_ENTITY_ID) ? object : NULL;
}

// The selected object's attached portal, or NULL when there is no selection or no portal. The
// portal can go stale (selection changes, portal detaches), so it is re-resolved every call
// rather than address-bound.
static PortalComponent *StateManager_SelectedPortal(void)
{
    Newtonoid2d *object = StateManager_SelectedObject();
    return object ? EntityRegistry_GetPortal(object->id) : NULL;
}

// DYNAMIC read: the current selection's damage, or BIND_NONE when nothing is selected. The query
// keys only on selection, NOT on is_projectile - visibility stays in RefreshGameplaySection.
static BindingValue StateManager_QueryDamage(int key)
{
    (void)key;
    Newtonoid2d *object = StateManager_SelectedObject();
    if (!object)
    {
        return (BindingValue){ .type = BIND_NONE };
    }
    return (BindingValue){ .type = BIND_FLOAT, .as.f = object->damage };
}

// DYNAMIC write: store a committed FLOAT into the current selection's damage. Reject (false,
// caller reverts) when nothing is selected or the value is the wrong type - writing to a
// stale/absent object is a bug, not a silent no-op.
static bool StateManager_WriteDamage(int key, BindingValue value)
{
    (void)key;
    Newtonoid2d *object = StateManager_SelectedObject();
    if (!object || value.type != BIND_FLOAT)
    {
        return false;
    }
    object->damage = value.as.f;
    return true;
}

// Portal field keys for the shared portal query/write pair below.
enum
{
    PORTAL_FIELD_COOLDOWN = 0,    // Configured cooldown_frames (NOT the live cooldown_frames_remaining counter).
    PORTAL_FIELD_DESTINATION = 1, // portal_destination_id
};

// DYNAMIC read for one INT field of the selected object's portal. Returns BIND_NONE when there
// is no selection or no portal.
static BindingValue StateManager_QueryPortalField(int key)
{
    PortalComponent *portal = StateManager_SelectedPortal();
    if (!portal)
    {
        return (BindingValue){ .type = BIND_NONE };
    }
    int field = (key == PORTAL_FIELD_DESTINATION) ? (int)portal->portal_destination_id
                                                  : portal->cooldown_frames;
    return (BindingValue){ .type = BIND_INT, .as.i = field };
}

// DYNAMIC write: store a committed INT into one field of the selected object's portal. Rejects
// (false, caller reverts) when there is no selection/portal, the value is the wrong type, or the
// value is negative (both fields must stay >= 0, per PortalComponent_IsValid). Only the configured
// cooldown_frames is written; the runtime cooldown_frames_remaining counter is owned by the portal
// system and left untouched.
static bool StateManager_WritePortalField(int key, BindingValue value)
{
    PortalComponent *portal = StateManager_SelectedPortal();
    if (!portal || value.type != BIND_INT || value.as.i < 0)
    {
        return false;
    }
    if (key == PORTAL_FIELD_DESTINATION)
    {
        portal->portal_destination_id = value.as.i;
    }
    else
    {
        portal->cooldown_frames = value.as.i;
    }
    return true;
}

// ============================================================================
// Flag-button families (shared read/write/query)
// ============================================================================
//
// Each FlagFamily names one bitmask on the current selection (object, world or cell). The generic
// refresh walk composes "<label>: ON/OFF" through StateManager_QueryFlag, and HandleFlagClick
// toggles through the same accessors, so adding a family means one enum value and one case in
// each accessor. Unselected families report "not set" (clear-when-none).

// Resolve an ENTITY ref to its live entity, or NULL for a wrong-kind or stale ref. Re-resolved on
// every call; pointers are never cached across frames.
static Newtonoid2d *StateManager_ResolveEntityRef(TargetRef ref)
{
    if (ref.kind != TARGET_KIND_ENTITY)
    {
        return NULL;
    }
    return Universe_GetEntityByID(&G_Universe, ref.as.entity, NULL);
}

// Read the bitmask a family targets on one target. Returns false when the ref is the wrong kind
// for the family (entity families need ENTITY, WORLD needs WORLD, CELL needs CELL) or no longer
// resolves, which is how a mixed selection skips inapplicable members. Guard is `!= NULL` only
// (no id check), so a non-NULL object with an invalid id still displays its real flags.
static bool StateManager_ReadFlags(TargetRef ref, FlagFamily family, uint32_t *out)
{
    switch (family)
    {
    case FLAG_FAMILY_ROLE:
    case FLAG_FAMILY_CAPABILITY:
    case FLAG_FAMILY_CONSTRAINT:
    case FLAG_FAMILY_STATUS:
    case FLAG_FAMILY_COLLISION_MASK:
    {
        Newtonoid2d *o = StateManager_ResolveEntityRef(ref);
        if (!o) return false;
        switch (family)
        {
        case FLAG_FAMILY_ROLE:           *out = o->roles; return true;
        case FLAG_FAMILY_CAPABILITY:     *out = o->capabilities; return true;
        case FLAG_FAMILY_CONSTRAINT:     *out = o->constraints; return true;
        case FLAG_FAMILY_STATUS:         *out = o->status_flags; return true;
        default:                         *out = o->collision_role_mask; return true;
        }
    }
    case FLAG_FAMILY_WORLD:
    {
        World2d *w = (ref.kind == TARGET_KIND_WORLD) ? Universe_GetWorld(&G_Universe, ref.as.world_index) : NULL;
        if (!w) return false;
        *out = w->flags;
        return true;
    }
    case FLAG_FAMILY_CELL:
    {
        Cell *c = UIState_ResolveCellRef(ref);
        if (!c) return false;
        *out = c->flags;
        return true;
    }
    }
    return false;
}

// Store a bitmask into the field a family targets on one target. Returns false when the ref is
// the wrong kind or stale (same rules as StateManager_ReadFlags).
static bool StateManager_WriteFlags(TargetRef ref, FlagFamily family, uint32_t value)
{
    switch (family)
    {
    case FLAG_FAMILY_ROLE:
    case FLAG_FAMILY_CAPABILITY:
    case FLAG_FAMILY_CONSTRAINT:
    case FLAG_FAMILY_STATUS:
    case FLAG_FAMILY_COLLISION_MASK:
    {
        Newtonoid2d *o = StateManager_ResolveEntityRef(ref);
        if (!o) return false;
        switch (family)
        {
        case FLAG_FAMILY_ROLE:           o->roles = value; return true;
        case FLAG_FAMILY_CAPABILITY:     o->capabilities = value; return true;
        case FLAG_FAMILY_CONSTRAINT:     o->constraints = value; return true;
        case FLAG_FAMILY_STATUS:         o->status_flags = value; return true;
        default:                         o->collision_role_mask = value; return true;
        }
    }
    case FLAG_FAMILY_WORLD:
    {
        World2d *w = (ref.kind == TARGET_KIND_WORLD) ? Universe_GetWorld(&G_Universe, ref.as.world_index) : NULL;
        if (!w) return false;
        w->flags = value;
        return true;
    }
    case FLAG_FAMILY_CELL:
    {
        Cell *c = UIState_ResolveCellRef(ref);
        if (!c) return false;
        c->flags = value;
        return true;
    }
    }
    return false;
}

// Tally of a bulk walk: how many targets the action applies to, and how many of those are ON.
typedef struct FlagTally
{
    int applicable;
    int on;
} FlagTally;

// Collapse a tally to the toggle state: 0 off (or nothing applicable), 1 all on, otherwise a
// packed "<on>/<applicable>" count (BIND_TOGGLE_MIXED_PACK) when the applicable targets disagree.
// Shared by flag and component queries so the display and the click decision always agree.
static int StateManager_TriState(int applicable, int on)
{
    if (applicable == 0 || on == 0)
    {
        return 0;
    }
    return (on == applicable) ? 1 : BIND_TOGGLE_MIXED_PACK(on, applicable);
}

// Set `bit` of `family` on one target to `value`. Holds the ROLE exclusivity and SLEEPING
// (sleep/wake keeps the body's physics state consistent) special cases in one place.
static void StateManager_ApplyFlagToTarget(TargetRef ref, FlagFamily family, uint32_t bit, bool value)
{
    uint32_t flags = 0;
    if (!StateManager_ReadFlags(ref, family, &flags))
    {
        return;
    }

    // Skip no-ops (avoids resetting the sleep timer of an already-sleeping entity). ROLE always
    // rewrites so the whole role group is normalised to exactly this role.
    bool current = (flags & bit) != 0;
    if (current == value && family != FLAG_FAMILY_ROLE)
    {
        return;
    }

    if (family == FLAG_FAMILY_ROLE)
    {
        // Exclusive: clear the whole role group, then set this one.
        StateManager_WriteFlags(ref, family, value ? ((flags & ~s_sm_flags.role_flags) | bit) : (flags & ~bit));
    }
    else if (family == FLAG_FAMILY_STATUS && bit == ENTITY_STATUS_FLAG_SLEEPING)
    {
        // A status read succeeded, so the entity resolves.
        Newtonoid2d *object = StateManager_ResolveEntityRef(ref);
        if (value)
        {
            ApplySleep(object);
        }
        else
        {
            WakeUp(object);
        }
    }
    else
    {
        StateManager_WriteFlags(ref, family, value ? (flags | bit) : (flags & ~bit));
    }
}

// Tally (and optionally apply to) one target. Inapplicable targets are not counted.
static void StateManager_VisitFlagTarget(FlagTally *tally, TargetRef ref, FlagFamily family,
                                         uint32_t bit, bool apply, bool value)
{
    uint32_t flags = 0;
    if (!StateManager_ReadFlags(ref, family, &flags))
    {
        return;
    }
    tally->applicable++;
    if (flags & bit)
    {
        tally->on++;
    }
    if (apply)
    {
        StateManager_ApplyFlagToTarget(ref, family, bit, value);
    }
}

// Visit every applicable target of a family in the SelectionSet, tallying the bit state; when
// `apply` is true also writes `value` to each. The only place that enumerates and filters
// targets for flags. Re-resolves every ref (stale ones are skipped) and does not allocate.
// World flags fall back to the navigation cursor world when the set holds no world, preserving
// single-select behaviour (the cursor can differ from the set).
static FlagTally StateManager_WalkFlag(FlagFamily family, uint32_t bit, bool apply, bool value)
{
    FlagTally tally = {0};
    bool saw_world = false;

    int count = SelectionSet_Count();
    for (int i = 0; i < count; i++)
    {
        TargetRef ref;
        if (!SelectionSet_Get(i, &ref))
        {
            continue;
        }
        saw_world = saw_world || (ref.kind == TARGET_KIND_WORLD);
        StateManager_VisitFlagTarget(&tally, ref, family, bit, apply, value);
    }

    if (family == FLAG_FAMILY_WORLD && !saw_world)
    {
        int cursor = Universe_GetSelectedIndex(&G_Universe);
        if (cursor >= 0)
        {
            TargetRef ref = { .kind = TARGET_KIND_WORLD, .as.world_index = cursor };
            StateManager_VisitFlagTarget(&tally, ref, family, bit, apply, value);
        }
    }
    return tally;
}

// Pack a family and flag bit into the int key a flag button's query binding carries. Flag bits are
// all below 1 << 16, so the family lives in the upper half.
#define STATE_MANAGER_FLAG_KEY(family, flag) (((int)(family) << 16) | (int)(flag))

// Is-set query shared by every flag family; returns BIND_INT 0 off, 1 on, BIND_TOGGLE_MIXED when
// the selected targets disagree. Key from STATE_MANAGER_FLAG_KEY.
static BindingValue StateManager_QueryFlag(int key)
{
    FlagTally t = StateManager_WalkFlag((FlagFamily)(key >> 16), (uint32_t)(key & 0xFFFF), false, false);
    return (BindingValue){ .type = BIND_INT, .as.i = StateManager_TriState(t.applicable, t.on) };
}
// DYNAMIC read for one axis of the selected cell's warp basis (key 0 = U, 1 = V). Returns the
// identity component ((1,0) for U, (0,1) for V) when no cell is selected, so an inert/neutral value
// is displayed rather than stale data. The selection can go stale, so it is re-resolved every call.
static BindingValue StateManager_QueryCellWarp(int key)
{
    Cell *c = UIState_GetSelectedCell();
    Vector2d v = (key == 0) ? (Vector2d){1.0f, 0.0f} : (Vector2d){0.0f, 1.0f};
    if (c)
    {
        v = (key == 0) ? c->warp_basis.u : c->warp_basis.v;
    }
    return (BindingValue){ .type = BIND_VECTOR2D, .as.v = v };
}

// DYNAMIC write: store a committed VECTOR2D into the selected cell's warp basis U (key 0) or V
// (key 1). Rejects (false, caller reverts) when there is no selection or the value is the wrong type.
static bool StateManager_WriteCellWarp(int key, BindingValue value)
{
    Cell *c = UIState_GetSelectedCell();
    if (!c || value.type != BIND_VECTOR2D)
    {
        return false;
    }
    if (key == 0)
    {
        c->warp_basis.u = value.as.v;
    }
    else
    {
        c->warp_basis.v = value.as.v;
    }
    return true;
}

// Resolve an ENTITY ref to a live entity with a valid id (the stricter guard the component
// handlers use), or NULL.
static Newtonoid2d *StateManager_ResolveLiveEntity(TargetRef ref)
{
    Newtonoid2d *o = StateManager_ResolveEntityRef(ref);
    return (o && o->id != INVALID_ENTITY_ID) ? o : NULL;
}

// Count the selected live entities (applicable) and those carrying `type` (on).
static FlagTally StateManager_TallyComponent(EntityComponentType type)
{
    FlagTally tally = {0};
    int count = SelectionSet_Count();
    for (int i = 0; i < count; i++)
    {
        TargetRef ref;
        if (!SelectionSet_Get(i, &ref))
        {
            continue;
        }
        Newtonoid2d *o = StateManager_ResolveLiveEntity(ref);
        if (!o)
        {
            continue;
        }
        tally.applicable++;
        if (EntityRegistry_HasComponent(o->id, type))
        {
            tally.on++;
        }
    }
    return tally;
}

// Backing buffers for the header title / sub-line (module-owned so the returned pointers outlive
// the call). Widest title "EDITING 12 SELECTED"; the sub-line carries slot/gen/world numbers.
static char state_manager_header_title_text[48];
static char state_manager_header_sub_text[64];

// Header query keys: which line of the always-shown header a shim call produces.
enum
{
    HEADER_KEY_TITLE = 0,
    HEADER_KEY_SUB = 1,
};

// Query for the Entity Manager header (key HEADER_KEY_TITLE or HEADER_KEY_SUB). The selection
// decides the text: nothing, several (bulk edit), or the primary entity / world / cell. Always
// returns BIND_STRING (an empty string, never BIND_NONE) so stale text is cleared. Stale or
// unresolvable refs print "-" rather than being dereferenced.
static BindingValue StateManager_QueryHeader(int key)
{
    char *out = (key == HEADER_KEY_SUB) ? state_manager_header_sub_text : state_manager_header_title_text;
    size_t out_bytes = (key == HEADER_KEY_SUB) ? sizeof(state_manager_header_sub_text)
                                               : sizeof(state_manager_header_title_text);
    bool want_sub = (key == HEADER_KEY_SUB);
    out[0] = '\0';

    int count = SelectionSet_Count();
    TargetRef ref = SelectionSet_Primary();

    if (count <= 0 || ref.kind == TARGET_KIND_NONE)
    {
        snprintf(out, out_bytes, "%s", want_sub ? "click to select" : "NOTHING SELECTED");
    }
    else if (count > 1)
    {
        if (want_sub)
        {
            snprintf(out, out_bytes, "applies to all shown");
        }
        else
        {
            snprintf(out, out_bytes, "EDITING %d SELECTED", count);
        }
    }
    else if (ref.kind == TARGET_KIND_ENTITY)
    {
        if (!want_sub)
        {
            snprintf(out, out_bytes, "ENTITY #%d", (int)ref.as.entity);
        }
        else
        {
            // World index comes from the universe lookup, as RefreshPhysView does.
            int world_index = -1;
            Newtonoid2d *o = Universe_GetEntityByID(&G_Universe, ref.as.entity, &world_index);
            if (o)
            {
                snprintf(out, out_bytes, "slot %u  gen %u  world %d",
                         (unsigned)EntityId_GetSlotIndex(ref.as.entity),
                         (unsigned)EntityId_GetGeneration(ref.as.entity), world_index);
            }
            else
            {
                snprintf(out, out_bytes, "-");
            }
        }
    }
    else if (ref.kind == TARGET_KIND_WORLD)
    {
        if (!want_sub)
        {
            if (ref.as.world_index == UNIVERSE_ROOT_WORLD_INDEX)
            {
                snprintf(out, out_bytes, "ROOT WORLD");
            }
            else
            {
                snprintf(out, out_bytes, "WORLD #%d", ref.as.world_index);
            }
        }
        else
        {
            World2d *w = Universe_GetWorld(&G_Universe, ref.as.world_index);
            if (w)
            {
                snprintf(out, out_bytes, "flags 0x%04X", (unsigned)w->flags);
            }
            else
            {
                snprintf(out, out_bytes, "-");
            }
        }
    }
    else // TARGET_KIND_CELL
    {
        if (!want_sub)
        {
            snprintf(out, out_bytes, "CELL (%d,%d)", ref.as.cell.lattice_x, ref.as.cell.lattice_y);
        }
        else
        {
            Cell *c = UIState_ResolveCellRef(ref);
            if (c)
            {
                snprintf(out, out_bytes, "world %d  tile %d", ref.as.cell.world_index, (int)c->tile_type);
            }
            else
            {
                snprintf(out, out_bytes, "world %d  tile -", ref.as.cell.world_index);
            }
        }
    }

    return (BindingValue){ .type = BIND_STRING, .as.s = out };
}

// ============================================================================
// Flag Interaction
// ============================================================================

// Click a flag across the whole selection. If the flag is ON for every applicable target it is
// cleared on all; otherwise it is set on all (never a per-object XOR). ROLE is exclusive, so a
// click always makes every applicable entity exactly that role.
static void HandleFlagClick(UIElement *button)
{
    const StateManagerFlagButton *spec = (const StateManagerFlagButton *)button->interaction_user_data;
    if (!spec)
    {
        return;
    }

    FlagTally tally = StateManager_WalkFlag(spec->family, spec->flag, false, false);
    if (tally.applicable == 0)
    {
        return;
    }

    bool value = (spec->family == FLAG_FAMILY_ROLE) ? true : (tally.on != tally.applicable);
    StateManager_WalkFlag(spec->family, spec->flag, true, value);

    MarkStateManagerRefreshDirty();
}
// Build a default-initialised component payload for an addable type. Returns false if the
// type cannot be attached with pure defaults (RELATION/WORLD/NONE). This is the single source
// of truth for the "attach with defaults" path: both the PHYS-view component toggle and the
// COMP-view Add buttons call it, so their defaults can never drift apart. ROTOR/GEAR keep the
// current zero-initialised default to preserve existing behaviour.
static bool StateManager_BuildDefaultComponent(EntityComponentType type, EntityComponent *out)
{
    if (!out)
    {
        return false;
    }

    EntityComponent comp = {.type = type};
    switch (type)
    {
    case ENTITY_COMPONENT_PORTAL:
    {
        PortalComponentParams portal_params = {
            .entrant_roles = ENTITY_ROLE_NEWTONOID | ENTITY_ROLE_PROJECTILE,
            .cooldown_frames = 30};
        PortalComponent_Initialise(&comp.data.portal, &portal_params);
        break;
    }
    case ENTITY_COMPONENT_ROTOR:
        memset(&comp.data.rotor, 0, sizeof(comp.data.rotor));
        break;
    case ENTITY_COMPONENT_GEAR:
        memset(&comp.data.gear, 0, sizeof(comp.data.gear));
        break;
    case ENTITY_COMPONENT_HEALTH:
    {
        HealthComponentParams health_params = {.max_health = 3.0f};
        if (!HealthComponent_Initialise(&comp.data.health, &health_params))
        {
            return false;
        }
        break;
    }
    case ENTITY_COMPONENT_RELATION:
    case ENTITY_COMPONENT_NONE:
    default:
        // RELATION/WORLD/NONE are not attachable with pure defaults: refuse so Add can never
        // fabricate them.
        return false;
    }

    *out = comp;
    return true;
}

// Return the UK-English all-caps display label for an attached component type. RELATION is a
// literal here because no button spec carries its label (RELATION is never addable): it can
// only appear in the Attached list when already present on the entity.
static const char *ComponentTypeName(EntityComponentType type)
{
    switch (type)
    {
    case ENTITY_COMPONENT_PORTAL:
        return "PORTAL";
    case ENTITY_COMPONENT_ROTOR:
        return "ROTOR";
    case ENTITY_COMPONENT_GEAR:
        return "GEAR";
    case ENTITY_COMPONENT_RELATION:
        return "RELATION";
    case ENTITY_COMPONENT_HEALTH:
        return "HEALTH";
    default:
        return "?"; // defensive: NONE/WORLD never reach here
    }
}

// Make `type` present (true) or absent (false) on every selected entity that differs. Only
// enqueues for entities that need a change (the pre-check is the dedupe; the registry also
// rejects duplicates). Enqueue only appends to the command queue, so nothing mutates mid-walk.
static void StateManager_ApplyComponentToSelection(EntityComponentType type, bool present)
{
    // Build the defaults once; the queue copies the component by value so reuse is safe.
    EntityComponent comp = {0};
    if (present && !StateManager_BuildDefaultComponent(type, &comp))
    {
        return;
    }

    int count = SelectionSet_Count();
    for (int i = 0; i < count; i++)
    {
        TargetRef ref;
        if (!SelectionSet_Get(i, &ref))
        {
            continue;
        }
        Newtonoid2d *o = StateManager_ResolveLiveEntity(ref);
        if (!o || EntityRegistry_HasComponent(o->id, type) == present)
        {
            continue;
        }
        if (present)
        {
            EnqueueAttachComponent(o->id, &comp);
        }
        else
        {
            EnqueueRemoveComponent(o->id, type);
        }
    }
}

// Attach the addable component the clicked Add button represents, with defaults, to every
// selected entity lacking it (deferred via the existing command queue). One shared handler
// serves all four Add buttons: the type is read from the button's interaction_user_data spec.
static void HandleComponentAddClick(UIElement *button)
{
    if (!button || !button->interaction_user_data)
    {
        return;
    }

    const StateManagerComponentButton *spec = (const StateManagerComponentButton *)button->interaction_user_data;
    StateManager_ApplyComponentToSelection(spec->type, true);
}

// Remove the component the clicked row represents from every selected entity that has it,
// immediately (no confirmation), via the existing command queue. The row slot is positional, so
// the type is read from the slot spec assigned during the last refresh.
static void HandleComponentRemoveClick(UIElement *button)
{
    if (!button || !button->interaction_user_data)
    {
        return;
    }

    const StateManagerComponentRow *row = (const StateManagerComponentRow *)button->interaction_user_data;
    if (row->type == ENTITY_COMPONENT_NONE)
    {
        return;
    }
    StateManager_ApplyComponentToSelection(row->type, false);
}
// ============================================================================
// Element id-registration + wiring (post-load)
// ============================================================================

// Per-family id tables for the flag buttons. Order MUST match the s_sm_flags.<family>
// index order so each acquired button lines up with its StateManagerFlagButton entry.
static const char *sm_role_ids[5]  = {"sm_role_wall", "sm_role_newtonoid", "sm_role_projectile",
                                      "sm_role_effect", "sm_role_camera"};
static const char *sm_cap_ids[4]   = {"sm_cap_damageable", "sm_cap_velocity",
                                      "sm_cap_affect_owner", "sm_cap_sensor"};
static const char *sm_con_ids[2]   = {"sm_con_position_locked", "sm_con_no_contact_response"};
static const char *sm_status_ids[3] = {"sm_status_alive", "sm_status_sleeping", "sm_status_clocked"};
static const char *sm_coll_ids[5]  = {"sm_coll_wall", "sm_coll_newtonoid", "sm_coll_projectile",
                                      "sm_coll_effect", "sm_coll_camera"};
static const char *sm_world_ids[7] = {"sm_world_active", "sm_world_visible", "sm_world_selectable",
                                      "sm_world_physics", "sm_world_spawns", "sm_world_locked",
                                      "sm_world_drag"};
static const char *sm_cell_ids[4]  = {"sm_cell_solid", "sm_cell_walkable", "sm_cell_hazard",
                                      "sm_cell_spawn"};

// Look up an element by id in the loaded tree, logging (and tolerating) a miss. The Refresh*
// helpers null-guard every pointer, so a missing id degrades to an inert field, not a crash.
static UIElement *StateManager_Find(UIElement *root, const char *id)
{
    UIElement *element = UILoader_FindById(root, id);
    if (!element)
    {
        LOG_ERROR("StateManager: id '%s' not found in markup", id);
    }
    return element;
}

// Wire a family of flag buttons acquired from the loaded tree: store the element into each
// buttons[i].button, attach the C click handler + user_data, and re-attach the query SOURCE
// Binding (keyed on the flag bit) so the generic refresh walk composes "<label>: ON/OFF".
// Reproduces CreateFlagButtons' wiring verbatim, now on id-acquired buttons.
static void StateManager_WireFlags(UIElement *root, StateManagerFlagButton *buttons,
                                   const char **ids, size_t count,
                                   FlagFamily family)
{
    for (size_t i = 0; i < count; i++)
    {
        buttons[i].family = family;
        buttons[i].button = StateManager_Find(root, ids[i]);
        if (buttons[i].button)
        {
            buttons[i].button->on_click  = HandleFlagClick;
            buttons[i].button->interaction_user_data = (void *)&buttons[i];

            // Query SOURCE restores the "<label>: ON/OFF" display (sink stays NONE; the handler
            // owns the write path).
            Binding b = {
                .source = { .kind = BIND_SRC_QUERY, .value_type = BIND_INT,
                            .query = StateManager_QueryFlag,
                            .query_key = STATE_MANAGER_FLAG_KEY(family, buttons[i].flag) },
            };
            buttons[i].button->data.button.binding = Binding_Create(b);
        }
    }
}

// Re-acquire every UIElement* the per-frame C refresh and the C click handlers need, from the
// loaded tree, and wire the C-owned clicks (flags / component toggles / DELETE). Any id that
// fails to resolve is logged and left NULL; the Refresh* helpers null-guard every pointer.
static void StateManager_RegisterElements(UIElement *root)
{
    // --- Header (title / sub-line / tint box), kind chips, tab row, per-kind views, note ---
    s_sm_ui.header_box   = StateManager_Find(root, "entity_header");
    s_sm_ui.header_title = StateManager_Find(root, "sm_header_title");
    s_sm_ui.header_sub   = StateManager_Find(root, "sm_header_sub");
    if (s_sm_ui.header_box)
    {
        s_sm_header_default_fill = s_sm_ui.header_box->colour_fill; // restored when untinted
    }
    s_sm_ui.values_note = StateManager_Find(root, "sm_values_note");
    if (s_sm_ui.values_note)
    {
        // Static caption (a Textbox value= attribute would be parsed as a number, so set it here).
        snprintf(s_sm_ui.values_note->data.textbox.text.string,
                 sizeof(s_sm_ui.values_note->data.textbox.text.string), "values: primary only");
    }
    if (s_sm_ui.header_title)
    {
        // Titles use FONT_LARGE; the tint pass re-applies it with the right ink colour.
        s_sm_ui.header_title->data.textbox.font = FONT_LARGE_WITH_COLOUR(ui_default_palette.text);
    }
    s_sm_ui.kind_chips = StateManager_Find(root, "kind_chips");
    static const char *chip_ids[3] = {"sm_kind_entity", "sm_kind_world", "sm_kind_cell"};
    for (int i = 0; i < 3; i++)
    {
        s_sm_ui.kind_chip[i] = StateManager_Find(root, chip_ids[i]);
        if (s_sm_ui.kind_chip[i])
        {
            s_sm_ui.kind_chip[i]->on_click = HandleKindChipClick;
            s_sm_ui.kind_chip[i]->interaction_user_data = (void *)&s_sm_chip_kinds[i];
        }
    }
    s_sm_ui.tab_container = StateManager_Find(root, "state_manager_views");

    // --- Physics / geometry / gameplay textboxes (ALL re-acquired in Stage 1) ---
    s_sm_ui.mass_tbox                 = StateManager_Find(root, "sm_mass");
    s_sm_ui.restitution_tbox          = StateManager_Find(root, "sm_restitution");
    s_sm_ui.friction_tbox             = StateManager_Find(root, "sm_friction");
    s_sm_ui.pos_c_tbox                = StateManager_Find(root, "sm_anchor");
    s_sm_ui.vel_tbox                  = StateManager_Find(root, "sm_vel");
    s_sm_ui.accel_tbox                = StateManager_Find(root, "sm_accel");
    s_sm_ui.moment_tbox               = StateManager_Find(root, "sm_moment");
    s_sm_ui.angular_velocity_tbox     = StateManager_Find(root, "sm_angvel");
    s_sm_ui.angular_acceleration_tbox = StateManager_Find(root, "sm_angaccel");

    s_sm_ui.pos_tl_tbox          = StateManager_Find(root, "sm_bounds_min");
    s_sm_ui.geometry_center_tbox = StateManager_Find(root, "sm_geo_center");
    s_sm_ui.rotation_tbox        = StateManager_Find(root, "sm_rotation");
    s_sm_ui.basis_u_tbox         = StateManager_Find(root, "sm_basis_u");
    s_sm_ui.basis_v_tbox         = StateManager_Find(root, "sm_basis_v");

    s_sm_ui.health_tbox     = StateManager_Find(root, "sm_health");
    s_sm_ui.max_health_tbox = StateManager_Find(root, "sm_max_health");

    // Damage is NOT refresh-driven: its only value source is a C dynamic binding (query + write).
    // Re-issue the EXISTING C binding here (carved out of the moved InitPhysStateView body) so
    // the field keeps its value source and commit path - reproducing current C behaviour.
    s_sm_ui.damage_tbox = StateManager_Find(root, "sm_damage");
    if (s_sm_ui.damage_tbox)
    {
        BindTextboxDynamic(s_sm_ui.damage_tbox, BIND_FLOAT, 2,
                           StateManager_QueryDamage, StateManager_WriteDamage, 0);
    }

    // --- Sections whose visibility the refresh toggles ---
    s_sm_ui.gameplay_section   = StateManager_Find(root, "gameplay");
    s_sm_ui.components_section = StateManager_Find(root, "components");

    // --- Component property textboxes + their String64 display buffers ---
    // The String64 is the inline data.textbox.text buffer the field draws; the former
    // UIFieldSpec out-param captured exactly that address.
    // Portal cooldown is EDITABLE (io="safe" in the markup): bind it dynamically with a
    // query SOURCE + write SINK, exactly like the damage field, so commits write back to the
    // selected object's portal->cooldown_frames. The target is reached via the selection ->
    // attached portal (can go stale), so a dynamic query/write pair is used rather than an
    // address binding. comp_portal_cooldown_str is left NULL so the per-frame refresh does NOT
    // blit the display buffer (which would fight the binding); the binding owns the value.
    s_sm_ui.comp_portal_cooldown_tbox = StateManager_Find(root, "sm_portal_cooldown");
    if (s_sm_ui.comp_portal_cooldown_tbox)
    {
        BindTextboxDynamic(s_sm_ui.comp_portal_cooldown_tbox, BIND_INT, 0,
                           StateManager_QueryPortalField, StateManager_WritePortalField, PORTAL_FIELD_COOLDOWN);
    }
    s_sm_ui.comp_portal_cooldown_str = NULL;

    // Portal Dest. is EDITABLE (io="safe"): same dynamic query/write pattern as cooldown. The
    // query reads portal->portal_destination_id and the write commits the entered entity id to
    // the selected object's portal. Binding-owned display, so no String64 and no refresh blit.
    s_sm_ui.comp_portal_target_tbox = StateManager_Find(root, "sm_portal_target_id");
    if (s_sm_ui.comp_portal_target_tbox)
    {
        BindTextboxDynamic(s_sm_ui.comp_portal_target_tbox, BIND_INT, 0,
                           StateManager_QueryPortalField, StateManager_WritePortalField, PORTAL_FIELD_DESTINATION);
    }
    s_sm_ui.comp_portal_entrant_roles_tbox = StateManager_Find(root, "sm_portal_roles");
    if (s_sm_ui.comp_portal_entrant_roles_tbox)
    {
        s_sm_ui.comp_portal_entrant_roles_str = &s_sm_ui.comp_portal_entrant_roles_tbox->data.textbox.text;
    }
    s_sm_ui.comp_relation_type_tbox = StateManager_Find(root, "sm_relation_type");
    if (s_sm_ui.comp_relation_type_tbox)
    {
        s_sm_ui.comp_relation_type_str = &s_sm_ui.comp_relation_type_tbox->data.textbox.text;
    }
    s_sm_ui.comp_relation_target_tbox = StateManager_Find(root, "sm_relation_target");
    if (s_sm_ui.comp_relation_target_tbox)
    {
        s_sm_ui.comp_relation_target_str = &s_sm_ui.comp_relation_target_tbox->data.textbox.text;
    }
    s_sm_ui.comp_relation_active_tbox = StateManager_Find(root, "sm_relation_active");
    if (s_sm_ui.comp_relation_active_tbox)
    {
        s_sm_ui.comp_relation_active_str = &s_sm_ui.comp_relation_active_tbox->data.textbox.text;
    }

    // --- WORLD physics textboxes ---
    s_sm_ui.world_restitution_tbox = StateManager_Find(root, "sm_world_restitution");
    s_sm_ui.world_friction_tbox    = StateManager_Find(root, "sm_world_friction");

    // --- CELL string readouts (acquire the owning textbox, then its inline display String64) ---
    UIElement *cell_index_tbox = StateManager_Find(root, "sm_cell_index");
    if (cell_index_tbox)
    {
        s_sm_ui.cell_id_str = &cell_index_tbox->data.textbox.text;
    }
    UIElement *cell_occu_tbox = StateManager_Find(root, "sm_cell_occu");
    if (cell_occu_tbox)
    {
        s_sm_ui.cell_occu_str = &cell_occu_tbox->data.textbox.text;
    }
    UIElement *cell_value_tbox = StateManager_Find(root, "sm_cell_value");
    if (cell_value_tbox)
    {
        s_sm_ui.cell_value_str = &cell_value_tbox->data.textbox.text;
    }
    UIElement *cell_fill_tbox = StateManager_Find(root, "sm_cell_fill");
    if (cell_fill_tbox)
    {
        s_sm_ui.cell_fill_str = &cell_fill_tbox->data.textbox.text;
    }
    UIElement *cell_tile_type_tbox = StateManager_Find(root, "sm_cell_tile_type");
    if (cell_tile_type_tbox)
    {
        s_sm_ui.cell_tile_type_str = &cell_tile_type_tbox->data.textbox.text;
    }

    // --- Editable cell warp basis (DYNAMIC query/write, like the portal fields; NOT a readonly
    // readout, so there is no String64 and no RefreshCellView entry — the generic per-frame
    // binding walk refreshes these). Precision 2 matches the sibling vector2 editable fields. ---
    s_sm_ui.cell_warp_u_tbox = StateManager_Find(root, "sm_cell_warp_u");
    if (s_sm_ui.cell_warp_u_tbox)
    {
        BindTextboxDynamic(s_sm_ui.cell_warp_u_tbox, BIND_VECTOR2D, 2,
                           StateManager_QueryCellWarp, StateManager_WriteCellWarp, 0);
    }
    s_sm_ui.cell_warp_v_tbox = StateManager_Find(root, "sm_cell_warp_v");
    if (s_sm_ui.cell_warp_v_tbox)
    {
        BindTextboxDynamic(s_sm_ui.cell_warp_v_tbox, BIND_VECTOR2D, 2,
                           StateManager_QueryCellWarp, StateManager_WriteCellWarp, 1);
    }

    // --- COMP view: Add-component buttons (PORTAL/ROTOR/GEAR/HEALTH only; RELATION/WORLD
    //     excluded). Each carries its EntityComponentType in interaction_user_data so the one
    //     shared HandleComponentAddClick handler serves every button. No query-source Binding:
    //     the label is the static type name and visibility is driven by the refresh. ---
    s_sm_ui.add_buttons[0] = (StateManagerComponentButton){"PORTAL", ENTITY_COMPONENT_PORTAL, StateManager_Find(root, "comp_add_portal")};
    s_sm_ui.add_buttons[1] = (StateManagerComponentButton){"ROTOR",  ENTITY_COMPONENT_ROTOR,  StateManager_Find(root, "comp_add_rotor")};
    s_sm_ui.add_buttons[2] = (StateManagerComponentButton){"GEAR",   ENTITY_COMPONENT_GEAR,   StateManager_Find(root, "comp_add_gear")};
    s_sm_ui.add_buttons[3] = (StateManagerComponentButton){"HEALTH", ENTITY_COMPONENT_HEALTH, StateManager_Find(root, "comp_add_health")};
    for (size_t i = 0; i < ARRAY_COUNT(s_sm_ui.add_buttons); i++)
    {
        if (s_sm_ui.add_buttons[i].button)
        {
            s_sm_ui.add_buttons[i].button->on_click = HandleComponentAddClick;
            s_sm_ui.add_buttons[i].button->interaction_user_data = (void *)&s_sm_ui.add_buttons[i];
        }
    }

    // --- COMP view: attached-row slots (name Textbox + Remove button). Each slot is positional;
    //     its EntityComponentType is assigned per refresh. The Remove button carries the slot in
    //     interaction_user_data so the shared HandleComponentRemoveClick reads the live type. The
    //     name_str caches the Textbox display buffer for imperative relabel (CELL-readout pattern). ---
    static const char *comp_row_name_ids[5]   = {"comp_row0_name", "comp_row1_name", "comp_row2_name",
                                                  "comp_row3_name", "comp_row4_name"};
    static const char *comp_row_remove_ids[5] = {"comp_row0_remove", "comp_row1_remove", "comp_row2_remove",
                                                  "comp_row3_remove", "comp_row4_remove"};
    for (size_t i = 0; i < ARRAY_COUNT(s_sm_ui.attached_rows); i++)
    {
        s_sm_ui.attached_rows[i].type = ENTITY_COMPONENT_NONE;

        s_sm_ui.attached_rows[i].name_label = StateManager_Find(root, comp_row_name_ids[i]);
        if (s_sm_ui.attached_rows[i].name_label)
        {
            s_sm_ui.attached_rows[i].name_str = &s_sm_ui.attached_rows[i].name_label->data.textbox.text;
        }

        s_sm_ui.attached_rows[i].remove_button = StateManager_Find(root, comp_row_remove_ids[i]);
        if (s_sm_ui.attached_rows[i].remove_button)
        {
            s_sm_ui.attached_rows[i].remove_button->on_click = HandleComponentRemoveClick;
            s_sm_ui.attached_rows[i].remove_button->interaction_user_data = (void *)&s_sm_ui.attached_rows[i];
        }
    }

    // --- Flag buttons: acquire + wire handler + user_data + query SOURCE per family ---
    StateManager_WireFlags(root, s_sm_flags.entity_role,         sm_role_ids,   5, FLAG_FAMILY_ROLE);
    StateManager_WireFlags(root, s_sm_flags.entity_capability,   sm_cap_ids,    4, FLAG_FAMILY_CAPABILITY);
    StateManager_WireFlags(root, s_sm_flags.entity_constraint,   sm_con_ids,    2, FLAG_FAMILY_CONSTRAINT);
    StateManager_WireFlags(root, s_sm_flags.entity_status,       sm_status_ids, 3, FLAG_FAMILY_STATUS);
    StateManager_WireFlags(root, s_sm_flags.collision_role_mask, sm_coll_ids,   5, FLAG_FAMILY_COLLISION_MASK);
    StateManager_WireFlags(root, s_sm_flags.world,               sm_world_ids,  7, FLAG_FAMILY_WORLD);
    StateManager_WireFlags(root, s_sm_flags.cell,                sm_cell_ids,   4, FLAG_FAMILY_CELL);

    // --- DELETE: C-wired (deferred, world-guarded delete); no action= in the XML ---
    s_sm_ui.delete_action = BUTTON_ACTION_DELETE_ENTITY;
    UIElement *del = StateManager_Find(root, "sm_delete");
    s_sm_ui.delete_button = del;
    if (del)
    {
        del->on_click  = HandleBtnSubmitClick;
        del->interaction_user_data = &s_sm_ui.delete_action;
    }

    // --- GEOMETRY view: vertex-count readout + fixed vertex-row pool. The row ids are a dense
    //     "geo_vertexN" sequence, so format them in a loop with a small stack buffer rather than a
    //     64-entry id table. Every pointer is null-guarded by RefreshGeometryView. ---
    s_sm_ui.geo_vertex_count_tbox = StateManager_Find(root, "geo_vertex_count");
    for (int i = 0; i < STATE_MANAGER_GEOMETRY_ROW_COUNT; i++)
    {
        char id[24];
        snprintf(id, sizeof(id), "geo_vertex%d", i);
        s_sm_ui.geo_vertex_tbox[i] = StateManager_Find(root, id);
    }
}

// ============================================================================
// View Refresh
// ============================================================================
// (The views come from state_manager.xml; the pointers/handlers are re-acquired in
// StateManager_RegisterElements.)

// Refresh gameplay section visibility and row states.
static void RefreshGameplaySection(const Newtonoid2d *object)
{
    bool is_damageable = object && object->id != INVALID_ENTITY_ID &&
                         EntityRegistry_GetHealth(object->id) != NULL;
    bool is_projectile = object && object->id != INVALID_ENTITY_ID &&
                         (object->roles & ENTITY_ROLE_PROJECTILE) != 0;

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
            ClearString64(s_sm_ui.comp_portal_entrant_roles_str);
            ClearString64(s_sm_ui.comp_relation_type_str);
            ClearString64(s_sm_ui.comp_relation_target_str);
            ClearString64(s_sm_ui.comp_relation_active_str);
            return; // No components to populate.
        }
    }

    EntityDescription desc = EntityRegistry_Describe(object);

    // Toggle Portal property field row visibility
    bool has_portal = (desc.components[ENTITY_COMPONENT_PORTAL] != NULL);
    SetParentEnabledState(s_sm_ui.comp_portal_cooldown_tbox, has_portal);
    SetParentEnabledState(s_sm_ui.comp_portal_target_tbox, has_portal);
    SetParentEnabledState(s_sm_ui.comp_portal_entrant_roles_tbox, has_portal);

    // Populate portal component fields if attached.
    PortalComponent *portal = (PortalComponent *)desc.components[ENTITY_COMPONENT_PORTAL];
    if (portal)
    {
        if (s_sm_ui.comp_portal_cooldown_str)
        {
            UpdateString64(s_sm_ui.comp_portal_cooldown_str->string, "%d", portal->cooldown_frames);
        }
        if (s_sm_ui.comp_portal_entrant_roles_str)
        {
            UpdateString64(s_sm_ui.comp_portal_entrant_roles_str->string, "%d", portal->entrant_roles);
        }
    }
    else
    {
        ClearString64(s_sm_ui.comp_portal_cooldown_str);
        ClearString64(s_sm_ui.comp_portal_entrant_roles_str);
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

// Refresh the PHYS/SHAPE/COMP value fields: physics, transform, geometry readouts, gameplay and
// component data. (Header identity is bound separately.)
static void RefreshPhysView(Newtonoid2d *object)
{
    // (Identity now lives in the header, resolved by StateManager_QueryHeader.)
    HealthComponent *health = object ? EntityRegistry_GetHealth(object->id) : NULL;

    // Refresh physics and gameplay numerical fields.
    TextboxField state_fields[] = {
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

        {s_sm_ui.health_tbox, FLOAT, health ? (void *)&health->current_health : NULL, 2, NULL},
        {s_sm_ui.max_health_tbox, FLOAT, health ? (void *)&health->max_health : NULL, 2, NULL},
        // {s_sm_ui.damage_tbox, FLOAT, object ? (void *)&object->damage : NULL, 2, NULL}, // MIGRATED to BindTextboxDynamic
    };
    RefreshTextboxFields(state_fields, ARRAY_COUNT(state_fields));

    // Refresh geometry fields, synchronising object rotation when valid. With no object every
    // field gets a NULL source and shows "N/A". The rotation field keeps precision 0 in that case,
    // as before.
    if (object)
    {
        SyncNewtonoidRotation(object);
    }
    TextboxField geometry_fields[] = {
        {s_sm_ui.rotation_tbox, FLOAT, object ? (void *)&object->rotation : NULL, object ? 3 : 0, "N/A"},
        {s_sm_ui.basis_u_tbox, VECTOR2D, object ? (void *)&object->local_axis_x : NULL, 0, "N/A"},
        {s_sm_ui.basis_v_tbox, VECTOR2D, object ? (void *)&object->local_axis_y : NULL, 0, "N/A"},
        {s_sm_ui.geometry_center_tbox, VECTOR2D, object ? (void *)&object->local_geometry_center : NULL, 0, "N/A"},
    };
    RefreshTextboxFields(geometry_fields, ARRAY_COUNT(geometry_fields));

    // Refresh contextual gameplay and component section visibility.
    RefreshGameplaySection(object);
    RefreshComponentsSection(object);
}

// (The ATTRI view needs no refresh function: its flag-button ON/OFF labels are driven entirely by
// the generic refresh walk via each button's query-source Binding.)

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

    // World flag ON/OFF labels are driven by the generic refresh walk via each button's
    // query-source Binding.
}

// Refresh the CELL view: grid cell state string readouts and cell behaviour flags.
static void RefreshCellView(const Cell *selected_cell, int selected_cell_occupancy)
{
    if (selected_cell)
    {
        int index = UIState_GetSelectedCellIndex();
        int occupancy = selected_cell_occupancy;
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
        // Read-only tile_type readout: show the selected cell's authored tile type directly.
        if (s_sm_ui.cell_tile_type_str)
        {
            UpdateString64(s_sm_ui.cell_tile_type_str->string, "%d", selected_cell->tile_type);
        }
    }
    else
    {
        ClearString64(s_sm_ui.cell_id_str);
        ClearString64(s_sm_ui.cell_occu_str);
        ClearString64(s_sm_ui.cell_value_str);
        ClearString64(s_sm_ui.cell_fill_str);
        ClearString64(s_sm_ui.cell_tile_type_str);
    }

    // Cell flag ON/OFF labels are driven by the generic refresh walk via each button's
    // query-source Binding.
}

// Refresh the COMP view (Phase 1: attach/remove only): rebuild the fixed Attached-row slots and
// the Add-button visibility to mirror the selected entity's attached component set.
//  * Attached section: one row per attached component in {PORTAL, ROTOR, GEAR, RELATION, HEALTH}
//    (WORLD is a world-node tag and is never iterated). Each used slot is relabelled imperatively
//    (binding-less, like the CELL readouts) and its type assigned so the shared Remove handler
//    detaches the right component; unused slots are blanked and disabled.
//  * Add section: one fixed button per addable type (PORTAL/ROTOR/GEAR/HEALTH); each is enabled
//    only when the selection is valid and does NOT already have that component.
// When no plain entity is selected (object == NULL, which also covers a world/cell primary since
// UIState_GetSelectedObject() returns NULL for those), every slot is disabled: nothing actionable.
static void RefreshComponentsView(const Newtonoid2d *object)
{
    bool is_valid = (object != NULL && object->id != INVALID_ENTITY_ID);

    // Attached section: iterate the ordered attachable type list (WORLD excluded).
    static const EntityComponentType attached_types[] = {
        ENTITY_COMPONENT_PORTAL, ENTITY_COMPONENT_ROTOR, ENTITY_COMPONENT_GEAR,
        ENTITY_COMPONENT_RELATION, ENTITY_COMPONENT_HEALTH};

    size_t row = 0;
    if (is_valid)
    {
        EntityDescription desc = EntityRegistry_Describe(object);
        bool multi = SelectionSet_Count() > 1;
        for (size_t t = 0; t < ARRAY_COUNT(attached_types) && row < ARRAY_COUNT(s_sm_ui.attached_rows); t++)
        {
            EntityComponentType type = attached_types[t];

            // Single target: the object's own set. Multi-select: the union over the selected
            // entities (a row exists when any member has it) with an "<on>/<applicable>" count.
            FlagTally tally = { .applicable = 1, .on = desc.components[type] ? 1 : 0 };
            if (multi)
            {
                tally = StateManager_TallyComponent(type);
            }
            if (tally.on == 0)
            {
                continue; // not attached anywhere: no row for this type
            }

            // Fill this slot: assign the live type (read by the Remove handler), relabel the name,
            // and show both the name label and the Remove button.
            s_sm_ui.attached_rows[row].type = type;
            if (s_sm_ui.attached_rows[row].name_str)
            {
                if (tally.applicable > 1)
                {
                    UpdateString64(s_sm_ui.attached_rows[row].name_str->string, "%s %d/%d",
                                   ComponentTypeName(type), tally.on, tally.applicable);
                }
                else
                {
                    UpdateString64(s_sm_ui.attached_rows[row].name_str->string, "%s", ComponentTypeName(type));
                }
            }
            // Guard each slot pointer: EnableElement/DisableElement dereference without a NULL
            // check, so a row id that failed to resolve in the id pass (e.g. a renamed markup id)
            // would otherwise crash here. Mirrors the Add-button guard below.
            if (s_sm_ui.attached_rows[row].name_label)
            {
                EnableElement(s_sm_ui.attached_rows[row].name_label);
            }
            if (s_sm_ui.attached_rows[row].remove_button)
            {
                EnableElement(s_sm_ui.attached_rows[row].remove_button);
            }
            row++;
        }
    }

    // Disable the remaining (unused) row slots: clear the stale label and hide name + Remove.
    for (size_t i = row; i < ARRAY_COUNT(s_sm_ui.attached_rows); i++)
    {
        s_sm_ui.attached_rows[i].type = ENTITY_COMPONENT_NONE;
        // Guard each slot pointer before clearing/hiding: DisableElement dereferences without a
        // NULL check, so an unresolved row id must not reach it (mirrors the fill path above).
        if (s_sm_ui.attached_rows[i].name_label)
        {
            ClearString64(&s_sm_ui.attached_rows[i].name_label->data.textbox.text);
            DisableElement(s_sm_ui.attached_rows[i].name_label);
        }
        if (s_sm_ui.attached_rows[i].remove_button)
        {
            DisableElement(s_sm_ui.attached_rows[i].remove_button);
        }
    }

    // Add section: show a button only for an addable type the entity does not already have.
    for (size_t i = 0; i < ARRAY_COUNT(s_sm_ui.add_buttons); i++)
    {
        if (!s_sm_ui.add_buttons[i].button)
        {
            continue;
        }

        // Offer Add while any selected entity still lacks the component (single target: not attached).
        bool offer = false;
        if (is_valid)
        {
            if (SelectionSet_Count() > 1)
            {
                FlagTally tally = StateManager_TallyComponent(s_sm_ui.add_buttons[i].type);
                offer = tally.on < tally.applicable;
            }
            else
            {
                offer = !EntityRegistry_HasComponent(object->id, s_sm_ui.add_buttons[i].type);
            }
        }
        if (offer)
        {
            EnableElement(s_sm_ui.add_buttons[i].button);
        }
        else
        {
            DisableElement(s_sm_ui.add_buttons[i].button);
        }
    }
}

// Refresh the GEOMETRY view (v1): the selected object's surface-vector count and its per-vertex
// LOCAL vectors. Reads surface.surface_vectors directly (the same LArray the in-world drag editor
// edits). Each used slot shows one readonly vector2 row; slots beyond the vertex count (or when
// nothing frame-bearing is selected) are hidden. Vertices past the fixed pool
// (> STATE_MANAGER_GEOMETRY_ROW_COUNT) cannot occur for an in-budget shape, but the loop caps at
// the pool size defensively. No per-frame allocation.
static void RefreshGeometryView(const Newtonoid2d *object)
{
    bool is_valid = (object != NULL && object->id != INVALID_ENTITY_ID);

    // Vertex-count readout: a NULL value pointer makes the field render its "N/A" placeholder.
    int count = is_valid ? (int)object->surface.surface_vectors.count : 0;
    TextboxField count_field[] = {
        {s_sm_ui.geo_vertex_count_tbox, INT, is_valid ? (void *)&count : NULL, 0, "N/A"},
    };
    RefreshTextboxFields(count_field, ARRAY_COUNT(count_field));

    // Clamp the number of shown rows to the fixed pool.
    int shown = 0;
    if (is_valid)
    {
        shown = count;
        if (shown > STATE_MANAGER_GEOMETRY_ROW_COUNT)
        {
            shown = STATE_MANAGER_GEOMETRY_ROW_COUNT;
        }
    }

    // Local vertices as stored (relative to the anchor); the same Vector2d buffer the drag editor
    // reads and writes via surface.surface_vectors.items.
    const Vector2d *verts = is_valid
        ? (const Vector2d *)object->surface.surface_vectors.items
        : NULL;

    for (int i = 0; i < STATE_MANAGER_GEOMETRY_ROW_COUNT; i++)
    {
        UIElement *row = s_sm_ui.geo_vertex_tbox[i];
        if (!row)
        {
            continue;
        }

        if (i < shown && verts)
        {
            // Local vector as stored; matches how the drag editor stores/edits them and the
            // "local coordinates" requirement.
            TextboxField f[] = {
                {row, VECTOR2D, (void *)&verts[i], 0, NULL},
            };
            RefreshTextboxFields(f, ARRAY_COUNT(f));
            SetParentEnabledState(row, true);
        }
        else
        {
            SetParentEnabledState(row, false);
        }
    }
}

// ============================================================================
// Selection-driven layout (header tint, kind chips, per-kind views, field lock)
// ============================================================================
// The primary selection's kind decides which views exist: ENTITY gets the five-tab row, WORLD and
// CELL get one tab-less view. Chips (multi-select only) promote the last member of a kind to
// primary, so "active kind" is always "kind of primary" and views, field readouts and the legacy
// UIState_GetSelected* mirrors agree by construction.

// View indices in markup order. 0..4 are the entity tabs (option index == view index, which the
// selector relies on); WORLD and CELL are reached only by selection.
enum
{
    SM_VIEW_PHYS = 0,
    SM_VIEW_FLAGS,
    SM_VIEW_COMP,
    SM_VIEW_SHAPE,
    SM_VIEW_TREE,
    SM_VIEW_WORLD,
    SM_VIEW_CELL,
};

// Selector callback. Runs for user tab clicks (and the layout's own SelectView, which restores the
// remembered tab afterwards): records the tab and keeps the active-view code in step.
static void StateManager_OnViewSelected(View *view)
{
    ViewHostSystem_HandleViewSelected(view);
    if (s_sm_selector)
    {
        s_sm_entity_tab = (int)s_sm_selector->active_index;
        s_sm_shown_view = s_sm_entity_tab;
    }
}

// Click a kind chip: make the last member of that kind the primary (remove then re-add moves it to
// the end of the set), then re-derive the single-valued primary fields and refresh the panel.
static void HandleKindChipClick(UIElement *chip)
{
    if (!chip || !chip->interaction_user_data)
    {
        return;
    }

    TargetKind kind = *(const TargetKind *)chip->interaction_user_data;
    if (SelectionSet_Primary().kind == kind)
    {
        return; // Already the edited kind.
    }

    TargetRef last = { .kind = TARGET_KIND_NONE };
    int count = SelectionSet_Count();
    for (int i = 0; i < count; i++)
    {
        TargetRef ref;
        if (SelectionSet_Get(i, &ref) && ref.kind == kind)
        {
            last = ref;
        }
    }
    if (last.kind == TARGET_KIND_NONE)
    {
        return;
    }

    SelectionSet_Remove(last);
    SelectionSet_Add(last);
    UIState_SyncPrimaryFieldsFromSet();
    MarkStateManagerRefreshDirty();
}

// Show exactly one of the panel's views by index (used for the WORLD / CELL views, which have no
// tab). Mirrors the host's own single-view activation.
static void StateManager_ShowOnlyView(int view_index)
{
    for (int i = 0; i < state_manager_panel->views.count; i++)
    {
        View *view = *((View **)LArray_Get(&state_manager_panel->views, i));
        if (!view || !view->container)
        {
            continue;
        }
        SetEnabledState(view->container, i == view_index);
        if (i == view_index)
        {
            ViewHostSystem_HandleViewSelected(view);
        }
    }
}

// Tint (or restore) the header box with existing palette colours: amber button_fill when
// several targets are selected, otherwise the default section fill. Only the fill swaps; the
// ink is the dark text colour in both states (dark reads on both amber and the section fill)
// and the coral section border is kept.
static void StateManager_SetHeaderTint(bool tinted)
{
    if (!s_sm_ui.header_box)
    {
        return;
    }

    ColourRgba ink = ui_default_palette.text;
    s_sm_ui.header_box->colour_fill = tinted ? ui_default_palette.button_fill : s_sm_header_default_fill;
    if (s_sm_ui.header_title)
    {
        s_sm_ui.header_title->data.textbox.font = FONT_LARGE_WITH_COLOUR(ink);
    }
    if (s_sm_ui.header_sub)
    {
        s_sm_ui.header_sub->data.textbox.font = FONT_MEDIUM_WITH_COLOUR(ink);
    }
}

// Lock (read-only) or unlock the single-valued editable fields. Value edits are not bulk-editable
// yet, so in multi-select they show the primary's values and ignore input. IsEditableTextbox is
// the single choke point for input, and it keys on the element type, so swapping SAFE_IO <-> O
// is enough. Only acts when the state flips.
static void StateManager_SetFieldsLocked(bool locked)
{
    if (locked == s_sm_fields_locked)
    {
        return;
    }
    s_sm_fields_locked = locked;

    UIElement *fields[] = {
        s_sm_ui.mass_tbox, s_sm_ui.restitution_tbox, s_sm_ui.friction_tbox, s_sm_ui.pos_c_tbox,
        s_sm_ui.vel_tbox, s_sm_ui.accel_tbox, s_sm_ui.moment_tbox, s_sm_ui.angular_velocity_tbox,
        s_sm_ui.rotation_tbox, s_sm_ui.basis_u_tbox, s_sm_ui.basis_v_tbox,
        s_sm_ui.health_tbox, s_sm_ui.max_health_tbox, s_sm_ui.damage_tbox,
        s_sm_ui.comp_portal_cooldown_tbox, s_sm_ui.comp_portal_target_tbox,
        s_sm_ui.world_restitution_tbox, s_sm_ui.world_friction_tbox,
        s_sm_ui.cell_warp_u_tbox, s_sm_ui.cell_warp_v_tbox,
    };
    for (size_t i = 0; i < ARRAY_COUNT(fields); i++)
    {
        if (fields[i])
        {
            fields[i]->type = locked ? UI_ELEMENT_TEXTBOX_O : UI_ELEMENT_TEXTBOX_SAFE_IO;
        }
    }

    // Drop focus and any pending edit so a half-typed value cannot commit into a locked field.
    if (locked)
    {
        ResetUIInputState();
    }
}

// Set a button's authored and drawn text together (the chips and DELETE carry no binding).
static void StateManager_SetButtonText(UIElement *button, const char *text)
{
    if (!button)
    {
        return;
    }
    snprintf(button->data.button.text.string, sizeof(button->data.button.text.string), "%s", text);
    snprintf(button->data.button.display_text.string, sizeof(button->data.button.display_text.string), "%s", text);
}

// Apply everything the selection decides: header tint, kind chips, tab row and shown view, the
// multi-select field lock and note, and the DELETE label. Called at the top of every refresh.
static void StateManager_ApplyKindLayout(void)
{
    if (!state_manager_panel || !s_sm_selector || s_sm_selector->count <= SM_VIEW_TREE)
    {
        return;
    }

    int count = SelectionSet_Count();
    TargetRef primary = SelectionSet_Primary();
    TargetKind kind = (count > 0) ? primary.kind : TARGET_KIND_NONE;
    bool multi = count > 1;

    // Tally the members per kind (one pass, no allocation).
    int kind_count[4] = {0};
    for (int i = 0; i < count; i++)
    {
        TargetRef ref;
        if (SelectionSet_Get(i, &ref) && ref.kind >= TARGET_KIND_ENTITY && ref.kind <= TARGET_KIND_CELL)
        {
            kind_count[ref.kind]++;
        }
    }

    // Header tint.
    StateManager_SetHeaderTint(multi);

    // Kind chips: the row is shown only in multi-select; a chip only when its kind is present.
    if (s_sm_ui.kind_chips)
    {
        SetEnabledState(s_sm_ui.kind_chips, multi);
    }
    static const char *chip_names[3] = { "ENTITIES", "WORLDS", "CELLS" };
    for (int i = 0; i < 3; i++)
    {
        UIElement *chip = s_sm_ui.kind_chip[i];
        if (!chip)
        {
            continue;
        }
        char label[32];
        snprintf(label, sizeof(label), "%s %d", chip_names[i], kind_count[s_sm_chip_kinds[i]]);
        StateManager_SetButtonText(chip, label);
        SetEnabledState(chip, kind_count[s_sm_chip_kinds[i]] > 0);

        // Chips share the tab/toggle two-state colours: active = amber + dark ink,
        // inactive = toggle_off_fill + light ink.
        bool active = (s_sm_chip_kinds[i] == kind);
        UIPalette_GetToggleColours(&ui_default_palette, active,
                                   &chip->colour_fill, &chip->data.button.font.colour);
    }

    // Tab row: only entity selections (including nothing selected) have tabs. In multi-select the
    // SHAPE and TREE tabs are hidden (single-entity views).
    bool single_view_kind = (kind == TARGET_KIND_WORLD || kind == TARGET_KIND_CELL);
    if (s_sm_ui.tab_container)
    {
        SetEnabledState(s_sm_ui.tab_container, !single_view_kind);
    }
    SetEnabledState(s_sm_selector->buttons[SM_VIEW_SHAPE], !multi);
    SetEnabledState(s_sm_selector->buttons[SM_VIEW_TREE], !multi);

    // Shown view.
    int desired = s_sm_entity_tab;
    if (kind == TARGET_KIND_WORLD)
    {
        desired = SM_VIEW_WORLD;
    }
    else if (kind == TARGET_KIND_CELL)
    {
        desired = SM_VIEW_CELL;
    }
    else if (multi && desired > SM_VIEW_COMP)
    {
        desired = SM_VIEW_PHYS;
    }
    if (desired != s_sm_shown_view)
    {
        if (desired <= SM_VIEW_TREE)
        {
            // SelectView fires the callback, which would overwrite the remembered tab; keep it.
            int remembered_tab = s_sm_entity_tab;
            ViewHostSystem_SelectView(s_sm_selector, (size_t)desired);
            s_sm_entity_tab = remembered_tab;
        }
        else
        {
            StateManager_ShowOnlyView(desired);
        }
        s_sm_shown_view = desired;
    }

    // Value fields are read-only in multi-select; the PHYS note says so.
    StateManager_SetFieldsLocked(multi);
    if (s_sm_ui.values_note)
    {
        SetEnabledState(s_sm_ui.values_note, multi);
    }

    // DELETE acts on the primary entity only (HandleBtnSubmitClick), so the multi-select label says
    // so; a non-entity primary has nothing to delete here, so the button is hidden.
    if (s_sm_ui.delete_button)
    {
        if (multi && kind == TARGET_KIND_ENTITY)
        {
            char label[32];
            snprintf(label, sizeof(label), "DELETE PRIMARY #%d", (int)primary.as.entity);
            StateManager_SetButtonText(s_sm_ui.delete_button, label);
        }
        else
        {
            StateManager_SetButtonText(s_sm_ui.delete_button, "DELETE");
        }
        SetEnabledState(s_sm_ui.delete_button, !(multi && kind != TARGET_KIND_ENTITY));
    }
}

// Push the selected object, world, cell, and capability state into the UI views.
void UpdateStateManagerSelectedObject(void)
{
    StateManager_ApplyKindLayout();

    Newtonoid2d *object = UIState_GetSelectedObject();
    World2d *world = Universe_GetSelectedWorld(&G_Universe);
    Cell *cell = UIState_GetSelectedCell();

    RefreshPhysView(object);
    RefreshWorldView(world);
    RefreshCellView(cell, UIState_GetSelectedCellOccupancy());
    RefreshComponentsView(object);
    RefreshGeometryView(object);
}

// ============================================================================
// Module Lifecycle
// ============================================================================

void InitStateManagerSystem(void)
{
    // 1) Load the authoritative markup with the state-manager resolvers (mirrors InitLPanel).
    UIElement *root = UILoader_LoadFromFileWithResolvers(
        UI_COMPONENTS_DIR "/state_manager.xml",
        &ui_default_palette,
        StateManager_ResolveBinding,
        StateManager_ResolveCommand,
        StateManager_ResolveViewType,
        NULL);
    if (!root)
    {
        // XML is the single authoritative source; a load failure is an asset/build error.
        LOG_ERROR("InitStateManagerSystem: failed to load state_manager.xml; panel will not render");
        return;
    }

    // 2) Extract the Views (each carries its ViewType from type= via StateManager_ResolveViewType).
    size_t view_count = 0;
    View **xml_views = UILoader_ExtractViews(root, &view_count);

    // 3) Build the host against the former left-panel viewport, then build the real root and
    //    re-parent the XML container under it. The panel is presented to the user as 'Entity Mgmt'
    //    and now occupies the former left-panel slot (&lpanel_viewport); its C identifiers are
    //    unchanged. Scale 1.0f / padding {0.1,0.1} are adopted from InitLPanel; UpdateUISpace
    //    (step 10) re-lays-out the panel over the valid viewport space afterwards.
    state_manager_panel = ViewHostSystem_Create(&lpanel_viewport, 1.0f,
                                                (Vector2d){0.1f, 0.1f},
                                                &ui_default_palette, ui_standard_stack_spacing);
    if (!state_manager_panel)
    {
        DisposeUIElement(root);
        if (xml_views)
        {
            Deallocate((void **)&xml_views, sizeof(View *) * view_count);
        }
        return;
    }

    ViewHostSystem_InitRoot(state_manager_panel);      // Real UI_ELEMENT_ROOT + space + seed_box.
    AddElementToTree(root, state_manager_panel->root); // Re-parent; NEVER panel->root = root.

    // 4) Register the XML Views with the host.
    ViewHostSystem_InitViews(state_manager_panel, view_count);
    for (size_t i = 0; i < view_count; i++)
    {
        LArray_Push(&state_manager_panel->views, &xml_views[i]);
    }
    if (xml_views)
    {
        // Free the array (not the Views, which the host now owns).
        Deallocate((void **)&xml_views, sizeof(View *) * view_count);
    }

    // 5) Build the selector from the XML <Option> metadata.
    //    StateManager_OnViewSelected records the chosen tab and forwards to the host's handler.
    ViewSelector *view_selector =
        UILoader_BuildSelectorFromMarkup(state_manager_panel, StateManager_OnViewSelected);
    s_sm_selector = view_selector;
    s_sm_entity_tab = 0;
    s_sm_shown_view = -1;

    // 6) Honour initial-view="phys_view" (fall back to the first view).
    if (view_selector)
    {
        int initial = UILoader_ResolveViewIndexById(state_manager_panel, UILoader_GetInitialViewId());
        if (initial < 0)
        {
            initial = 0;
        }
        ViewHostSystem_SelectView(view_selector, (size_t)initial);
    }
    else
    {
        // Selector allocation failed: establish single-view visibility manually, mirroring the
        // lpanel fallback (never ViewHostSystem_FinaliseInit, which forces index 0).
        for (int i = 0; i < state_manager_panel->views.count; i++)
        {
            View *view = *((View **)LArray_Get(&state_manager_panel->views, i));
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

    // 7) ID-REGISTRATION + WIRING PASS: re-acquire every pointer the C refresh/handlers need.
    //    (Scrolling is now authored declaratively via scroll= on each <View> in the markup;
    //    the former imperative View_SetScrollableX force-enable has been removed.)
    StateManager_RegisterElements(root);

    // 8) Subscribe once so selection transitions can proactively refresh this panel.
    UIState_SetSelectionChangedCallback(HandleSelectionChanged, NULL);

    // 10) First frame should always render initialised state-manager values; final layout update.
    MarkStateManagerRefreshDirty();
    UpdateUISpace(state_manager_panel->root, state_manager_panel->seed_box);
}

void DrawStateManagerSystem(void)
{
    if (state_manager_panel)
    {
        Newtonoid2d *object = UIState_GetSelectedObject();

        // The CELL readouts are push-refreshed by RefreshCellView, but cell selection now flows
        // through the central SelectionSet (CommitClickSelection -> SelectionSet_Add), which does
        // NOT go through UIState_SetSelection, so the selection-changed callback never fires for a
        // cell-to-cell change and the readouts would stay stale. Detect a change in the selected
        // cell here by its canonical identity (owning world index + cell index; both -1 when no
        // cell is selected) and mark dirty so the readouts refresh. Using the (world,cell) pair
        // rather than cell index alone avoids missing a change between same-index cells in
        // different worlds. The warp fields are unaffected either way (per-frame dynamic queries).
        static int last_cell_world = -2;  // -2: never evaluated; -1: no cell selected.
        static int last_cell_lx = -2;
        static int last_cell_ly = -2;
        static int last_cell_index = -2;
        TargetRef primary = SelectionSet_Primary();
        int current_cell_world = (primary.kind == TARGET_KIND_CELL) ? primary.as.cell.world_index : -1;
        int current_cell_lx = (primary.kind == TARGET_KIND_CELL) ? primary.as.cell.lattice_x : -1;
        int current_cell_ly = (primary.kind == TARGET_KIND_CELL) ? primary.as.cell.lattice_y : -1;
        // The cell is now identified by (world, lattice x/y); the LIVE dense index is also tracked because
        // it is shown in the panel and is renumbered when the world grows on the left/top.
        int current_cell_index = (primary.kind == TARGET_KIND_CELL) ? UIState_GetSelectedCellIndex() : -1;
        if (current_cell_world != last_cell_world || current_cell_lx != last_cell_lx ||
            current_cell_ly != last_cell_ly || current_cell_index != last_cell_index)
        {
            last_cell_world = current_cell_world;
            last_cell_lx = current_cell_lx;
            last_cell_ly = current_cell_ly;
            last_cell_index = current_cell_index;
            MarkStateManagerRefreshDirty();
        }

        // The layout (tabs, chips, field lock, DELETE label) is a function of the set's size, its
        // per-kind counts and the primary's kind. SelectionSet_Add does not fire the selection
        // callback, so detect those changes here and mark dirty (same approach as the cell case).
        static int last_set_count = -1;
        static int last_primary_kind = -1;
        static int last_kind_counts[4] = {-1, -1, -1, -1};
        int set_count = SelectionSet_Count();
        int kind_counts[4] = {0};
        for (int i = 0; i < set_count; i++)
        {
            TargetRef member;
            if (SelectionSet_Get(i, &member) && member.kind >= TARGET_KIND_ENTITY && member.kind <= TARGET_KIND_CELL)
            {
                kind_counts[member.kind]++;
            }
        }
        if (set_count != last_set_count || (int)primary.kind != last_primary_kind ||
            memcmp(kind_counts, last_kind_counts, sizeof(kind_counts)) != 0)
        {
            last_set_count = set_count;
            last_primary_kind = (int)primary.kind;
            memcpy(last_kind_counts, kind_counts, sizeof(kind_counts));
            MarkStateManagerRefreshDirty();
        }

        // Dynamic entities update continuously; otherwise update when state is marked dirty.
        if (object || state_manager_refresh_dirty)
        {
            UpdateStateManagerSelectedObject();
            state_manager_refresh_dirty = false;
        }
        ViewHostSystem_Draw(state_manager_panel);
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

    ViewHostSystem *panel = state_manager_panel;
    state_manager_panel = NULL;
    ViewHostSystem_Destroy(panel);

    state_manager_refresh_dirty = true;
    ResetFlagButtons(s_sm_flags.entity_role, ARRAY_COUNT(s_sm_flags.entity_role));
    ResetFlagButtons(s_sm_flags.entity_capability, ARRAY_COUNT(s_sm_flags.entity_capability));
    ResetFlagButtons(s_sm_flags.entity_constraint, ARRAY_COUNT(s_sm_flags.entity_constraint));
    ResetFlagButtons(s_sm_flags.entity_status, ARRAY_COUNT(s_sm_flags.entity_status));
    ResetFlagButtons(s_sm_flags.collision_role_mask, ARRAY_COUNT(s_sm_flags.collision_role_mask));
    ResetFlagButtons(s_sm_flags.world, ARRAY_COUNT(s_sm_flags.world));
    ResetFlagButtons(s_sm_flags.cell, ARRAY_COUNT(s_sm_flags.cell));
    memset(&s_sm_ui, 0, sizeof(s_sm_ui));
    s_sm_selector = NULL; // Freed by ViewHostSystem_Destroy with the panel.
    s_sm_entity_tab = 0;
    s_sm_shown_view = -1;
    s_sm_fields_locked = false;
}
