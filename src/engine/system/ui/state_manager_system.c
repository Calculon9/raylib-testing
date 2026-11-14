#include "system/ui/state_manager_system.h"
#include "system/ui/panel_shared.h"            // Flag families, header block, field lock, selection helpers
#include <string.h>
#include "system/command_system.h"
#include "system/ui/ui_loader.h"
#include "system/view_host_system.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/utility_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"
#include "ui/ui_input.h"                         // ResetUIInputState (drop focus when a body hides)
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
#define SM_SHOWN_UNKNOWN (-2)                // No layout applied yet.
// The inputs that decide the layout (tabs, field lock, DELETE label). The layout is re-applied only
// when this changes, as the World Manager does with its own signature.
typedef struct StateManagerLayoutSignature
{
    int entity_count;      // Entities in the selection set
    EntityId primary;      // Primary entity id (INVALID_ENTITY_ID when none): names the DELETE label
    bool has_target;       // A live primary entity exists (a stale ref counts as none)
} StateManagerLayoutSignature;
static StateManagerLayoutSignature s_sm_layout_signature;
static bool s_sm_layout_signature_valid = false; // False until the first layout has been applied.
static ViewSelector *s_sm_selector = NULL;   // The entity tab selector (NULL if allocation failed).
static int s_sm_entity_tab = 0;              // Entity tab remembered across empty selections.
static int s_sm_shown_view = SM_SHOWN_UNKNOWN; // View currently shown.
static bool s_sm_fields_locked = false;      // Cached lock state, so types only change on a flip.

// COMP action button widths (UI pixels). Buttons cannot hug their caption, so each width is derived
// from the caption (text width + 2 x PAD) and written on a state flip; the header's toggle simply
// fills the rest of the inline row, so no panel width appears here.
#define SM_COMP_ACTION_REMOVE_W 78.0f
#define SM_COMP_ACTION_ADD_W 48.0f

// Which caption/colour an action button currently shows (cached so it is only rewritten on a flip).
enum
{
    SM_ACTION_UNSET = 0,
    SM_ACTION_ADD,
    SM_ACTION_REMOVE,
};

// A COMP header's action button together with the toggle Button to its left. The action button is
// resized and recoloured when its state flips.
typedef struct StateManagerActionPair
{
    UIElement *lead;     // toggle Button (expand / collapse caption)
    UIElement *button;   // ADD / REMOVE action Button
    int state;           // SM_ACTION_* currently displayed
} StateManagerActionPair;

// One attachable component type in the COMP view. Every type has an ATTACHED card and an
// AVAILABLE card in the markup; exactly one of the two is shown (see RefreshComponentsView).
typedef struct StateManagerComponentSection
{
    EntityComponentType type;
    UIElement *section;             // comp_sec_<type>: the ATTACHED card
    UIElement *body;                // comp_body_<type>: the TextFields; NULL for types without fields
    UIElement *note;                // comp_note_<type>: "values: primary only"; NULL without a body
    UIElement *avail_row;           // comp_avail_<type>: the AVAILABLE card
    StateManagerActionPair header;  // comp_tgl_<type> + comp_btn_<type>
    UIElement *avail_button;        // comp_avail_btn_<type>: the Available card's ADD button
    bool expanded;                  // panel-local and per component TYPE (kept across selections)
    bool body_was_shown;            // edge detect so a hiding body can drop input focus
} StateManagerComponentSection;

// Number of attachable component types listed in the COMP view.
#define SM_COMP_SECTION_COUNT 4

// Fixed GEOMETRY-view vertex-row slot count. One row per potential surface vector, so a selection
// with more than this many vertices is capped (rows beyond the pool are not shown); matches the
// MAX_SHAPE_VERTICES vertex budget so no in-range vertex is dropped.
#define STATE_MANAGER_GEOMETRY_ROW_COUNT MAX_SHAPE_VERTICES
// SHAPE-view vertex rows [0, this) may be enabled; rows at or beyond it are known to be disabled, so
// the per-frame refresh only disables rows that were shown last frame. The pool size means "unknown"
// (a freshly built panel), which disables every unused row once.
static int s_sm_geometry_rows_enabled = STATE_MANAGER_GEOMETRY_ROW_COUNT;

// Fixed number of pre-declared ancestry line slots (see the Ancestry View State section below).
#define STATE_MANAGER_ANCESTRY_LINE_COUNT 6
// UIElement and String64 references owned by the State Manager panel.

typedef struct StateManagerUI
{
    // COMP view: one entry per attachable type in fixed order (PORTAL, ROTOR, GEAR, HEALTH), plus
    // the empty-state lines of the two halves. The refresh shows/hides these elements.
    StateManagerComponentSection comp_sections[SM_COMP_SECTION_COUNT];
    UIElement *comp_attached_empty; // "none attached" line in the Attached half
    UIElement *comp_avail_empty;    // "all attached" line in the Available half

    // Header (selection-driven title / sub-line and multi-select tint) and view-switching state.
    PanelHeader header;             // entity_header container + title + sub-line
    UIElement *delete_button;       // sm_delete (label reflects the selection)
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

    // Health/damage readouts (inside the HEALTH body of the COMP view)
    UIElement *health_tbox;
    UIElement *max_health_tbox;
    UIElement *damage_tbox;

    // Component readouts (portal fields live in the PORTAL body; the relation line is in the TREE view)
    UIElement *comp_portal_cooldown_tbox;   // Editable; binding-owned, no String64
    UIElement *comp_portal_target_tbox;   // Portal Dest. field (editable; binding-owned, no String64)
    UIElement *comp_portal_entrant_roles_tbox;
    String64 *comp_portal_entrant_roles_str;
    UIElement *comp_relation_type_tbox;
    String64 *comp_relation_type_str;

    // GEOMETRY view (v1): vertex-count readout + fixed readonly vertex-row pool.
    UIElement *geo_vertex_count_tbox;
    UIElement *geo_vertex_tbox[STATE_MANAGER_GEOMETRY_ROW_COUNT];
    // ANCESTRY view: fixed line slots, disabled beyond the chain length so they take no space.
    UIElement *ancestry_line_tbox[STATE_MANAGER_ANCESTRY_LINE_COUNT];
} StateManagerUI;

static StateManagerUI s_sm_ui = {0};

// All entity flag definitions and their bound controls stay together as module state. (World and
// cell flags live in the World Manager; the family machinery is shared via panel_shared.)
typedef struct StateManagerFlags
{
    FlagButton entity_role[5];
    FlagButton entity_capability[4];
    FlagButton entity_constraint[2];
    FlagButton entity_status[3];
    FlagButton collision_role_mask[5];
} StateManagerFlags;

static StateManagerFlags s_sm_flags = {
    .entity_role = {
        {"sm_role_wall", ENTITY_ROLE_WALL, NULL},
        {"sm_role_newtonoid", ENTITY_ROLE_NEWTONOID, NULL},
        {"sm_role_projectile", ENTITY_ROLE_PROJECTILE, NULL},
        {"sm_role_effect", ENTITY_ROLE_EFFECT, NULL},
        {"sm_role_camera", ENTITY_ROLE_CAMERA, NULL},
    },
    .entity_capability = {
        {"sm_cap_damageable", ENTITY_CAPABILITY_DAMAGEABLE, NULL},
        {"sm_cap_velocity", ENTITY_CAPABILITY_VELOCITY_ALIGNED, NULL},
        {"sm_cap_affect_owner", ENTITY_CAPABILITY_AFFECT_OWNER, NULL},
        {"sm_cap_sensor", ENTITY_CAPABILITY_SENSOR, NULL},
    },
    .entity_constraint = {
        {"sm_con_position_locked", ENTITY_CONSTRAINT_POSITION_LOCKED, NULL},
        {"sm_con_no_contact_response", ENTITY_CONSTRAINT_NO_CONTACT_RESPONSE, NULL},
    },
    .entity_status = {
        {"sm_status_alive", ENTITY_STATUS_FLAG_ALIVE, NULL},
        {"sm_status_sleeping", ENTITY_STATUS_FLAG_SLEEPING, NULL},
        {"sm_status_clocked", ENTITY_STATUS_FLAG_CLOCKED, NULL},
    },
    .collision_role_mask = {
        {"sm_coll_wall", ENTITY_ROLE_WALL, NULL},
        {"sm_coll_newtonoid", ENTITY_ROLE_NEWTONOID, NULL},
        {"sm_coll_projectile", ENTITY_ROLE_PROJECTILE, NULL},
        {"sm_coll_effect", ENTITY_ROLE_EFFECT, NULL},
        {"sm_coll_camera", ENTITY_ROLE_CAMERA, NULL},
    },
};
// ============================================================================
// Ancestry View State (module-owned, read-only)
// ============================================================================
// Fixed number of pre-declared ancestry line slots. Six generations comfortably covers
// selected entity -> parent entities -> owning world -> universe root world for the current
// depth model; any line beyond the actual chain length renders blank.

// Panel-owned backing buffers for the ancestry view's read-only BIND_STRING labels. The binding
// core copies value.as.s into each textbox's own text buffer during the per-frame refresh, but
// the returned pointer must stay valid for the duration of that call, so storage is module-owned
// (never a stack local). One buffer per line slot, plus the header and relation summaries.
static char state_manager_ancestry_line_text[STATE_MANAGER_ANCESTRY_LINE_COUNT][96]; // One per ancestry line.
static char state_manager_ancestry_selected_text[96];                               // Header naming the rooted node.
static char state_manager_ancestry_relation_text[128];                              // Direct-relation summary.

/**
 * Resolve the EntityId the ancestry walk should root on, from the primary ENTITY of the current
 * selection. Selection is read fresh every call (never cached) so the view always reflects the
 * live set. With no entity selected there is no frame-bearing node, so INVALID_ENTITY_ID is
 * returned (callers render the neutral "No selection" state). This is the single source of truth
 * the ancestry shims share, so selection resolution is not duplicated across them.
 */
static EntityId StateManager_AncestryRootId(void)
{
    TargetRef ref;
    if (!PanelShared_PrimaryOfKind(TARGET_KIND_ENTITY, &ref))
    {
        return INVALID_ENTITY_ID;
    }
    return ref.as.entity;
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
                 "Selected: %s", UI_NO_VALUE_TEXT);
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
                 "Relation: %s", UI_NO_VALUE_TEXT);
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

// Resolve a state-manager view-type string to its ViewType code. The application owns the
// enum names so the loader stays domain-free. An Unrecognised string reports failure and defaults to the first view.
static ViewType StateManager_ResolveViewType(const char *type_string, bool *resolved,
                                             UILoaderContext *ctx)
{
    (void)ctx; // Currently unused; provided for future application-specific context.

#define SM_VIEW_ENTRY(name) {#name, name}
    static const struct { const char *name; ViewType type; } views[] = {
        SM_VIEW_ENTRY(STATE_MANAGER_PHYSICS_VIEW),
        SM_VIEW_ENTRY(STATE_MANAGER_ATTRIBUTES_VIEW),
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
// Supplied to the loader to future-proof a declarative command button; no button uses it (DELETE is wired in C - see StateManager_RegisterElements).
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

    // Ancestry view read-only labels: each maps to a BIND_STRING query source over the ancestry
    // shims. A header, a relation summary, and
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

// The primary selected entity (the last ENTITY member of the SelectionSet), or NULL when no valid
// entity is selected. Re-resolved on every call so a stale ref is never dereferenced.
static Newtonoid2d *StateManager_SelectedObject(void)
{
    TargetRef ref;
    if (!PanelShared_PrimaryOfKind(TARGET_KIND_ENTITY, &ref))
    {
        return NULL;
    }
    Newtonoid2d *object = PanelShared_ResolveEntityRef(ref);
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
// keys only on selection; visibility is owned by the HEALTH body of the COMP view.
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

// Resolve an ENTITY ref to a live entity with a valid id (the stricter guard the component
// handlers use), or NULL.
static Newtonoid2d *StateManager_ResolveLiveEntity(TargetRef ref)
{
    Newtonoid2d *o = PanelShared_ResolveEntityRef(ref);
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

// Query for the Entity Manager header (key HEADER_KEY_TITLE or HEADER_KEY_SUB). The entity members
// of the selection decide the text: none, several (bulk edit) or the single entity. Always returns
// BIND_STRING (never BIND_NONE) so stale text is cleared. Stale or unresolvable refs print "-"
// rather than being dereferenced.
static BindingValue StateManager_QueryHeader(int key)
{
    bool want_sub = (key == HEADER_KEY_SUB);
    char *out = want_sub ? state_manager_header_sub_text : state_manager_header_title_text;
    size_t out_bytes = want_sub ? sizeof(state_manager_header_sub_text)
                                : sizeof(state_manager_header_title_text);
    out[0] = '\0';

    int entity_count = PanelShared_CountKind(TARGET_KIND_ENTITY);
    TargetRef ref = { .kind = TARGET_KIND_NONE };

    // A lone stale ref (entity deleted or destroyed) counts as no selection, like the field refresh.
    if (entity_count <= 0 || !PanelShared_PrimaryOfKind(TARGET_KIND_ENTITY, &ref) ||
        (entity_count == 1 && !StateManager_SelectedObject()))
    {
        snprintf(out, out_bytes, "%s", want_sub ? "click an entity" : "NO ENTITY SELECTED");
    }
    else if (entity_count > 1)
    {
        // The sub-line also carries the "values come from the primary" note (the PHYS note it replaces),
        // by naming the primary entity whose values the read-only fields show.
        if (want_sub)
        {
            snprintf(out, out_bytes, "%d SELECTED  PRIMARY #%d", entity_count, (int)ref.as.entity);
        }
        else
        {
            snprintf(out, out_bytes, "MULTIPLE");
        }
    }
    else if (!want_sub)
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
            snprintf(out, out_bytes, "SLOT %u GEN %u WORLD %d",
                     (unsigned)EntityId_GetSlotIndex(ref.as.entity),
                     (unsigned)EntityId_GetGeneration(ref.as.entity), world_index);
        }
        else
        {
            snprintf(out, out_bytes, "-");
        }
    }

    return (BindingValue){ .type = BIND_STRING, .as.s = out };
}

// ============================================================================
// DELETE
// ============================================================================

// Delete the primary ENTITY of the selection through the deferred command queue. The shared submit
// handler reads the single-select primary, which is empty when the set's primary is a world or
// cell, so the Entity Manager resolves its own primary entity here.
static void HandleEntityDeleteClick(UIElement *button)
{
    (void)button;
    TargetRef ref;
    if (PanelShared_PrimaryOfKind(TARGET_KIND_ENTITY, &ref))
    {
        EnqueueDeleteEntity(ref.as.entity);
    }
}
// Build a default-initialised component payload for an addable type. Returns false if the
// type cannot be attached with pure defaults (RELATION/WORLD/NONE). This is the single source
// of truth for the "attach with defaults" path: both the PHYS-view component toggle and the
// COMP-view ADD buttons call it, so their defaults can never drift apart. ROTOR/GEAR keep the
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

// Return the UK-English all-caps display label for a component type. RELATION is never listed in the
// COMP view (it is not attachable), but keeps a label for completeness.
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

// Click on a COMP action button (header REMOVE/ADD or Available ADD). Applies the bulk rule: ADD to
// the selected entities lacking the component unless every one already has it, in which case REMOVE
// from all. It reads the live tally (the same one the label is drawn from), so the click always
// does what the button says. The section arrives through interaction_user_data.
static void HandleComponentActionClick(UIElement *button)
{
    if (!button || !button->interaction_user_data)
    {
        return;
    }

    const StateManagerComponentSection *section = (const StateManagerComponentSection *)button->interaction_user_data;
    FlagTally tally = StateManager_TallyComponent(section->type);
    if (tally.applicable == 0)
    {
        return;
    }
    // value = (on != applicable): not on everywhere -> add to those lacking; else remove from all.
    StateManager_ApplyComponentToSelection(section->type, tally.on != tally.applicable);
}

// Click on a header's toggle Button: expand or collapse that component type's body. Ignored when the
// type has no fields or the primary entity lacks the component (nothing to show). Clicks do not
// bubble, so the action button and the body's TextFields can never reach this handler.
static void HandleComponentToggleClick(UIElement *button)
{
    if (!button || !button->interaction_user_data)
    {
        return;
    }

    StateManagerComponentSection *section = (StateManagerComponentSection *)button->interaction_user_data;
    Newtonoid2d *primary = StateManager_SelectedObject();
    if (!section->body || !primary || !EntityRegistry_HasComponent(primary->id, section->type))
    {
        return;
    }
    section->expanded = !section->expanded;
}
// ============================================================================
// Element id-registration + wiring (post-load)
// ============================================================================

// Look up an element by id in the loaded tree, logging (and tolerating) a miss. The Refresh*
// helpers null-guard every pointer, so a missing id degrades to an inert field, not a crash.
static UIElement *StateManager_Find(UIElement *root, const char *id)
{
    return PanelShared_Find(root, "StateManager", id);
}

// Find an element whose id is "<prefix><key>" (for example "comp_sec_" + "portal"), logging a miss.
static UIElement *StateManager_FindComponentElement(UIElement *root, const char *prefix, const char *key)
{
    char id[48];
    snprintf(id, sizeof(id), "%s%s", prefix, key);
    return StateManager_Find(root, id);
}

// Acquire and wire the COMP view. One table row per attachable type, in the markup order PORTAL,
// ROTOR, GEAR, HEALTH; every id is "comp_<role>_<key>". A type with data fields sets has_fields and
// gains a comp_body_<key> and comp_note_<key> in the markup; ROTOR and GEAR have none yet, so their
// body stays NULL and the section shows no arrow (the refresh derives "has fields" from the body).
static void StateManager_RegisterComponentSections(UIElement *root)
{
    static const struct
    {
        EntityComponentType type;
        const char *key;
        bool has_fields;
    } s_comp_ids[SM_COMP_SECTION_COUNT] = {
        {ENTITY_COMPONENT_PORTAL, "portal", true},
        {ENTITY_COMPONENT_ROTOR, "rotor", false}, // ROTOR fields will go here: set has_fields once the markup has a body
        {ENTITY_COMPONENT_GEAR, "gear", false},   // GEAR fields will go here: set has_fields once the markup has a body
        {ENTITY_COMPONENT_HEALTH, "health", true},
    };

    s_sm_ui.comp_attached_empty = StateManager_Find(root, "comp_attached_empty");
    s_sm_ui.comp_avail_empty = StateManager_Find(root, "comp_avail_empty");

    for (size_t i = 0; i < ARRAY_COUNT(s_comp_ids); i++)
    {
        StateManagerComponentSection *section = &s_sm_ui.comp_sections[i];
        const char *key = s_comp_ids[i].key;
        section->type = s_comp_ids[i].type;
        section->section = StateManager_FindComponentElement(root, "comp_sec_", key);
        section->avail_row = StateManager_FindComponentElement(root, "comp_avail_", key);
        section->header.lead = StateManager_FindComponentElement(root, "comp_tgl_", key);
        section->header.button = StateManager_FindComponentElement(root, "comp_btn_", key);
        section->avail_button = StateManager_FindComponentElement(root, "comp_avail_btn_", key);
        if (s_comp_ids[i].has_fields)
        {
            section->body = StateManager_FindComponentElement(root, "comp_body_", key);
            section->note = StateManager_FindComponentElement(root, "comp_note_", key);
            PanelShared_SetNoteText(section->note);
        }

        // Every button carries its section so one handler per role serves all four types.
        UIElement *buttons[] = {section->header.button, section->avail_button};
        for (size_t b = 0; b < ARRAY_COUNT(buttons); b++)
        {
            if (buttons[b])
            {
                buttons[b]->on_click = HandleComponentActionClick;
                buttons[b]->interaction_user_data = (void *)section;
            }
        }

        UIElement *toggle = section->header.lead;
        if (toggle)
        {
            toggle->on_click = HandleComponentToggleClick;
            toggle->interaction_user_data = (void *)section;
            // Header caption: left-aligned and flush with the card's inner edge (the markup has no
            // alignment or padding attribute for a Button).
            SetUIElementTextHorizontalAlignment(toggle, UI_TEXT_ALIGN_LEFT);
            toggle->padding = ZERO_VECTOR_2D;
        }
    }
}

// Re-acquire every UIElement* the per-frame C refresh and the C click handlers need, from the
// loaded tree, and wire the C-owned clicks (flags / component toggles / DELETE). Any id that
// fails to resolve is logged and left NULL; the Refresh* helpers null-guard every pointer.
static void StateManager_RegisterElements(UIElement *root)
{
    // --- Header (title / sub-line / tint box) and tab row ---
    PanelShared_HeaderBind(&s_sm_ui.header, root, state_manager_panel->palette, "entity_header", "sm_header_title", "sm_header_sub");
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

    // --- Component property textboxes + their String64 display buffers ---
    // The String64 is the inline data.textbox.text buffer the field draws; the former
    // UIFieldSpec out-param captured exactly that address.
    // Portal cooldown is EDITABLE (io="safe" in the markup): bind it dynamically with a
    // query SOURCE + write SINK, exactly like the damage field, so commits write back to the
    // selected object's portal->cooldown_frames. The target is reached via the selection ->
    // attached portal (can go stale), so a dynamic query/write pair is used rather than an
    // address binding. The binding owns the displayed value, so the per-frame refresh never blits it.
    s_sm_ui.comp_portal_cooldown_tbox = StateManager_Find(root, "sm_portal_cooldown");
    if (s_sm_ui.comp_portal_cooldown_tbox)
    {
        BindTextboxDynamic(s_sm_ui.comp_portal_cooldown_tbox, BIND_INT, 0,
                           StateManager_QueryPortalField, StateManager_WritePortalField, PORTAL_FIELD_COOLDOWN);
    }

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

    // --- COMP view: sections, bodies and Available rows, found through the id family table ---
    StateManager_RegisterComponentSections(root);

    // --- Flag buttons: acquire + wire handler + user_data + query SOURCE per family ---
    PanelShared_WireFlags(root, s_sm_flags.entity_role, ARRAY_COUNT(s_sm_flags.entity_role), FLAG_FAMILY_ROLE);
    PanelShared_WireFlags(root, s_sm_flags.entity_capability, ARRAY_COUNT(s_sm_flags.entity_capability), FLAG_FAMILY_CAPABILITY);
    PanelShared_WireFlags(root, s_sm_flags.entity_constraint, ARRAY_COUNT(s_sm_flags.entity_constraint), FLAG_FAMILY_CONSTRAINT);
    PanelShared_WireFlags(root, s_sm_flags.entity_status, ARRAY_COUNT(s_sm_flags.entity_status), FLAG_FAMILY_STATUS);
    PanelShared_WireFlags(root, s_sm_flags.collision_role_mask, ARRAY_COUNT(s_sm_flags.collision_role_mask), FLAG_FAMILY_COLLISION_MASK);

    // --- DELETE: C-wired (deferred command-queue delete of the primary entity); no action= in the XML ---
    s_sm_ui.delete_button = StateManager_Find(root, "sm_delete");
    if (s_sm_ui.delete_button)
    {
        s_sm_ui.delete_button->on_click = HandleEntityDeleteClick;
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
    // --- ANCESTRY view: the dense "ancestry_lineN" slot pool, enabled per chain length. ---
    for (int i = 0; i < STATE_MANAGER_ANCESTRY_LINE_COUNT; i++)
    {
        char id[24];
        snprintf(id, sizeof(id), "ancestry_line%d", i);
        s_sm_ui.ancestry_line_tbox[i] = StateManager_Find(root, id);
    }
}

// ============================================================================
// View Refresh
// ============================================================================
// (The views come from state_manager.xml; the pointers/handlers are re-acquired in
// StateManager_RegisterElements.)

// Populate the portal and relation value fields from the primary entity. Portal field visibility is
// owned by the PORTAL body of the COMP view; the relation line (TREE view) hides itself when absent.
static void RefreshComponentsSection(const Newtonoid2d *object)
{
    bool is_valid = (object != NULL && object->id != INVALID_ENTITY_ID);

    if (!is_valid)
    {
        ClearString64(s_sm_ui.comp_portal_entrant_roles_str);
        ClearString64(s_sm_ui.comp_relation_type_str);
        // The TREE "Relation: -" line covers the empty state, so hide the relation row.
        SetParentEnabledState(s_sm_ui.comp_relation_type_tbox, false);
        return; // No components to populate.
    }

    EntityDescription desc = EntityRegistry_Describe(object);

    // Populate portal component fields if attached.
    PortalComponent *portal = (PortalComponent *)desc.components[ENTITY_COMPONENT_PORTAL];
    if (portal)
    {
        if (s_sm_ui.comp_portal_entrant_roles_str)
        {
            UpdateString64(s_sm_ui.comp_portal_entrant_roles_str->string, "%d", portal->entrant_roles);
        }
    }
    else
    {
        ClearString64(s_sm_ui.comp_portal_entrant_roles_str);
    }

    // Toggle Relation property field row visibility
    bool has_relation = (desc.components[ENTITY_COMPONENT_RELATION] != NULL);
    SetParentEnabledState(s_sm_ui.comp_relation_type_tbox, has_relation);

    // Populate relation component fields if attached.
    RelationComponent *relation = (RelationComponent *)desc.components[ENTITY_COMPONENT_RELATION];
    if (relation)
    {
        if (s_sm_ui.comp_relation_type_str)
        {
            UpdateString64(s_sm_ui.comp_relation_type_str->string, "%s", RelationType_ToString(relation->type));
        }
    }
    else
    {
        ClearString64(s_sm_ui.comp_relation_type_str);
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
        {s_sm_ui.mass_tbox, FLOAT, object ? (void *)&object->mass : NULL, 2, UI_NO_VALUE_TEXT},
        {s_sm_ui.restitution_tbox, FLOAT, object ? (void *)&object->restitution : NULL, 2, UI_NO_VALUE_TEXT},
        {s_sm_ui.friction_tbox, FLOAT, object ? (void *)&object->friction : NULL, 2, UI_NO_VALUE_TEXT},
        {s_sm_ui.pos_tl_tbox, VECTOR2D, object ? (void *)&object->bounds_origin : NULL, 0, UI_NO_VALUE_TEXT},
        {s_sm_ui.pos_c_tbox, VECTOR2D, object ? (void *)&object->anchor_position : NULL, 0, UI_NO_VALUE_TEXT},
        {s_sm_ui.vel_tbox, VECTOR2D, object ? (void *)&object->velocity : NULL, 0, UI_NO_VALUE_TEXT},
        {s_sm_ui.accel_tbox, VECTOR2D, object ? (void *)&object->acceleration : NULL, 0, UI_NO_VALUE_TEXT},
        {s_sm_ui.moment_tbox, VECTOR2D, object ? (void *)&object->momentum : NULL, 0, UI_NO_VALUE_TEXT},
        {s_sm_ui.angular_velocity_tbox, FLOAT, object ? (void *)&object->angular_velocity : NULL, 2, UI_NO_VALUE_TEXT},
        {s_sm_ui.angular_acceleration_tbox, FLOAT, object ? (void *)&object->angular_acceleration : NULL, 2, UI_NO_VALUE_TEXT},

        {s_sm_ui.health_tbox, FLOAT, health ? (void *)&health->current_health : NULL, 2, UI_NO_VALUE_TEXT},
        {s_sm_ui.max_health_tbox, FLOAT, health ? (void *)&health->max_health : NULL, 2, UI_NO_VALUE_TEXT},
        // {s_sm_ui.damage_tbox, FLOAT, object ? (void *)&object->damage : NULL, 2, UI_NO_VALUE_TEXT}, // MIGRATED to BindTextboxDynamic
    };
    RefreshTextboxFields(state_fields, ARRAY_COUNT(state_fields));

    // Refresh geometry fields, synchronising object rotation when valid. With no object every
    // field gets a NULL source and shows the "-" placeholder. The rotation field keeps precision 0 in that case,
    // as before.
    if (object)
    {
        SyncNewtonoidRotation(object);
    }
    TextboxField geometry_fields[] = {
        {s_sm_ui.rotation_tbox, FLOAT, object ? (void *)&object->rotation : NULL, object ? 3 : 0, UI_NO_VALUE_TEXT},
        {s_sm_ui.basis_u_tbox, VECTOR2D, object ? (void *)&object->local_axis_x : NULL, 0, UI_NO_VALUE_TEXT},
        {s_sm_ui.basis_v_tbox, VECTOR2D, object ? (void *)&object->local_axis_y : NULL, 0, UI_NO_VALUE_TEXT},
        {s_sm_ui.geometry_center_tbox, VECTOR2D, object ? (void *)&object->local_geometry_center : NULL, 0, UI_NO_VALUE_TEXT},
    };
    RefreshTextboxFields(geometry_fields, ARRAY_COUNT(geometry_fields));

    // The dynamically bound fields (damage, portal cooldown / destination) keep their last value
    // when the binding query returns nothing, so write the placeholder by hand while nothing is
    // selected. They are locked then, so no edit can be in progress; the binding pull restores the
    // real value as soon as an entity (with that component) is selected.
    if (!object)
    {
        WriteTextboxText(s_sm_ui.damage_tbox, UI_NO_VALUE_TEXT);
        WriteTextboxText(s_sm_ui.comp_portal_cooldown_tbox, UI_NO_VALUE_TEXT);
        WriteTextboxText(s_sm_ui.comp_portal_target_tbox, UI_NO_VALUE_TEXT);
    }

    // Refresh the component value fields (portal, relation).
    RefreshComponentsSection(object);
}

// (The ATTRI view needs no refresh function: its flag-button ON/OFF labels are driven entirely by
// the generic refresh walk via each button's query-source Binding.)

// Show a header action pair as ADD (amber) or REMOVE (danger): caption, palette colours and the
// caption-derived button width (the toggle to its left fills the rest of the row). Colours come from the shared palette,
// never literals. A no-op when the state is unchanged, so nothing is rewritten per frame.
static void StateManager_SetActionState(StateManagerActionPair *pair, bool remove)
{
    int wanted = remove ? SM_ACTION_REMOVE : SM_ACTION_ADD;
    if (!pair->button || pair->state == wanted)
    {
        return;
    }
    pair->state = wanted;

    PanelShared_SetButtonText(pair->button, remove ? "REMOVE" : "ADD");
    // The button takes the whole DANGER (REMOVE) or ACTIVE (ADD) role: fill, border and paired text.
    UIElement_ApplyRoleColours(pair->button, state_manager_panel->palette,
                               remove ? UI_ROLE_DANGER : UI_ROLE_ACTIVE);

    float action_width = remove ? SM_COMP_ACTION_REMOVE_W : SM_COMP_ACTION_ADD_W;
    pair->button->size.dimensions.x = action_width;
}

// Show or hide a section's body (and its "values: primary only" note, which only multi-select needs).
// When a body that was showing disappears, drop input focus and any half-typed edit so a hidden field
// cannot keep receiving keys.
static void StateManager_ShowSectionBody(StateManagerComponentSection *section, bool visible, bool show_note)
{
    SetEnabledState(section->body, visible);
    SetEnabledState(section->note, visible && show_note);
    if (section->body_was_shown && !visible)
    {
        ResetUIInputState();
    }
    section->body_was_shown = visible;
}

// Refresh the COMP view: the four attachable components in two fixed halves (Attached, Available).
// Every type has a card in both halves in the markup; this only enables one of the two.
//  * Membership is read live from the tally each frame (ATTACHED when any selected entity has the
//    type), so ADD / REMOVE moves the card between halves at once, in the same fixed slot.
//  * The attached card's caption, count, arrow, action button and body visibility come from the same
//    tally, so the button always tells the truth about the next click.
// With no entity selected every card is hidden and both empty-state lines are shown.
static void RefreshComponentsView(const Newtonoid2d *object)
{
    if (!object || object->id == INVALID_ENTITY_ID)
    {
        for (size_t i = 0; i < ARRAY_COUNT(s_sm_ui.comp_sections); i++)
        {
            StateManagerComponentSection *section = &s_sm_ui.comp_sections[i];
            SetEnabledState(section->section, false);
            SetEnabledState(section->avail_row, false);
            if (section->body)
            {
                StateManager_ShowSectionBody(section, false, false);
            }
        }
        // Both halves show their empty-state lines; no card means no ADD / REMOVE button either.
        SetEnabledState(s_sm_ui.comp_attached_empty, true);
        SetEnabledState(s_sm_ui.comp_avail_empty, true);
        return;
    }

    int attached_count = 0;
    for (size_t i = 0; i < ARRAY_COUNT(s_sm_ui.comp_sections); i++)
    {
        StateManagerComponentSection *section = &s_sm_ui.comp_sections[i];

        // Live state: the same tally and predicate the click handler uses (all on -> REMOVE).
        FlagTally tally = StateManager_TallyComponent(section->type);
        bool attached = tally.on > 0;
        bool all_on = (PanelShared_TriState(tally.applicable, tally.on) == 1);
        bool multi = tally.applicable > 1;

        // Exactly one of the two cards is shown for each type.
        SetEnabledState(section->section, attached);
        SetEnabledState(section->avail_row, !attached);

        if (!attached)
        {
            if (section->body)
            {
                StateManager_ShowSectionBody(section, false, false);
            }
            continue;
        }
        attached_count++;

        // ATTACHED: header row, plus the body when it has fields, the primary has the component and
        // the card is expanded. Values shown in the body are the primary's.
        bool expandable = section->body && EntityRegistry_HasComponent(object->id, section->type);
        const char *arrow = !expandable ? "  " : (section->expanded ? "v " : "> ");
        char caption[48];
        if (multi)
        {
            snprintf(caption, sizeof(caption), "%s%s %d/%d", arrow, ComponentTypeName(section->type),
                     tally.on, tally.applicable);
        }
        else
        {
            snprintf(caption, sizeof(caption), "%s%s", arrow, ComponentTypeName(section->type));
        }
        PanelShared_SetButtonText(section->header.lead, caption);
        StateManager_SetActionState(&section->header, all_on);

        if (section->body)
        {
            StateManager_ShowSectionBody(section, expandable && section->expanded, multi);
        }
    }

    // Empty-state lines for a half with no cards.
    SetEnabledState(s_sm_ui.comp_attached_empty, attached_count == 0);
    SetEnabledState(s_sm_ui.comp_avail_empty, attached_count == SM_COMP_SECTION_COUNT);
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

    // Vertex-count readout: a NULL value pointer makes the field render the "-" placeholder.
    int count = is_valid ? (int)object->surface.surface_vectors.count : 0;
    TextboxField count_field[] = {
        {s_sm_ui.geo_vertex_count_tbox, INT, is_valid ? (void *)&count : NULL, 0, UI_NO_VALUE_TEXT},
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
        else if (i < s_sm_geometry_rows_enabled)
        {
            // Only a row that was shown last frame needs disabling now; the rest already are.
            SetParentEnabledState(row, false);
        }
    }
    s_sm_geometry_rows_enabled = verts ? shown : 0;
}

// ============================================================================
// Selection-driven layout (header tint, tab row, per-entity views, field lock)
// ============================================================================
// The panel shows the ENTITY members of the SelectionSet only (worlds and cells are edited in the
// World Manager). One entity gets the five-tab row; several get a bulk-edit layout with the
// single-entity tabs hidden; none gets an empty view area. The panel never modifies the set.

// View indices in markup order; option index == view index, which the selector relies on.
enum
{
    SM_VIEW_PHYS = 0,
    SM_VIEW_FLAGS,
    SM_VIEW_COMP,
    SM_VIEW_SHAPE,
    SM_VIEW_TREE,
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

// Apply everything the entity selection decides: header tint, tab row and shown view, the field
// lock (multi-select or nothing selected) and DELETE's visibility and label. With nothing selected
// the panel keeps its full structure (tabs, views) with "-" placeholders and locked read-only
// fields. Called at the top of every refresh.
static void StateManager_ApplyLayout(void)
{
    if (!state_manager_panel || !s_sm_selector || s_sm_selector->count <= SM_VIEW_TREE)
    {
        return;
    }

    int entity_count = PanelShared_CountKind(TARGET_KIND_ENTITY);
    bool multi = entity_count > 1;
    // A live primary entity exists: a stale ref (deleted or destroyed entity) counts as none.
    bool has_target = StateManager_SelectedObject() != NULL;

    // Re-apply only when an input changed (this runs every frame while an entity is selected, and
    // the layout does string formatting and several enable/disable calls).
    TargetRef primary_ref;
    StateManagerLayoutSignature signature;
    memset(&signature, 0, sizeof(signature)); // Zero the padding too, so the memcmp below is exact.
    signature.entity_count = entity_count;
    signature.primary = PanelShared_PrimaryOfKind(TARGET_KIND_ENTITY, &primary_ref) ? primary_ref.as.entity
                                                                                    : INVALID_ENTITY_ID;
    signature.has_target = has_target;
    if (s_sm_layout_signature_valid && memcmp(&signature, &s_sm_layout_signature, sizeof(signature)) == 0)
    {
        return;
    }
    s_sm_layout_signature = signature;
    s_sm_layout_signature_valid = true;

    // Header tint: amber while several entities are being edited together.
    PanelShared_HeaderSetTint(&s_sm_ui.header, multi);

    // Tab row: always shown. In multi-select the SHAPE and TREE tabs are hidden (single-entity views).
    SetEnabledState(s_sm_selector->buttons[SM_VIEW_SHAPE], !multi);
    SetEnabledState(s_sm_selector->buttons[SM_VIEW_TREE], !multi);

    // Shown view: the remembered tab, clamped to the bulk-capable tabs in multi-select.
    int desired = s_sm_entity_tab;
    if (multi && desired > SM_VIEW_COMP)
    {
        desired = SM_VIEW_PHYS;
    }
    if (desired != s_sm_shown_view)
    {
        PanelShared_SelectViewKeepingTab(s_sm_selector, (size_t)desired, &s_sm_entity_tab);
        s_sm_shown_view = desired;
    }

    // Value fields are read-only in multi-select (the header sub-line names the primary they show)
    // and while nothing is selected (they show the "-" placeholder and have no target to write to).
    UIElement *fields[] = {
        s_sm_ui.mass_tbox, s_sm_ui.restitution_tbox, s_sm_ui.friction_tbox, s_sm_ui.pos_c_tbox,
        s_sm_ui.vel_tbox, s_sm_ui.accel_tbox, s_sm_ui.moment_tbox, s_sm_ui.angular_velocity_tbox,
        s_sm_ui.rotation_tbox, s_sm_ui.basis_u_tbox, s_sm_ui.basis_v_tbox,
        s_sm_ui.health_tbox, s_sm_ui.max_health_tbox, s_sm_ui.damage_tbox,
        s_sm_ui.comp_portal_cooldown_tbox, s_sm_ui.comp_portal_target_tbox,
    };
    PanelShared_SetFieldsLocked(fields, ARRAY_COUNT(fields), &s_sm_fields_locked, multi || !has_target);

    // DELETE has nothing to act on without a target, so it is hidden (a disabled element takes no space).
    SetEnabledState(s_sm_ui.delete_button, has_target);
    // DELETE acts on the primary entity only, so the multi-select label says so.
    if (s_sm_ui.delete_button)
    {
        if (multi && signature.primary != INVALID_ENTITY_ID)
        {
            char label[32];
            snprintf(label, sizeof(label), "DELETE PRIMARY #%d", (int)signature.primary);
            PanelShared_SetButtonText(s_sm_ui.delete_button, label);
        }
        else
        {
            PanelShared_SetButtonText(s_sm_ui.delete_button, "DELETE");
        }
    }
}

/**
 * Enable only the ancestry line slots the selected node's chain actually fills. Disabled rows take
 * no layout space, so unused slots no longer show as blank read-only fields (the same approach the
 * SHAPE view uses for unused vertex rows). The text itself still comes from the binding shim.
 */
static void RefreshAncestryView(void)
{
    int count = 0;
    EntityId root = StateManager_AncestryRootId();
    if (root != INVALID_ENTITY_ID)
    {
        // Same walk as StateManager_QueryAncestryLine so the visible rows match its text.
        EntityId chain[STATE_MANAGER_ANCESTRY_LINE_COUNT];
        count = Ancestry_WalkUp(root, STATE_MANAGER_ANCESTRY_LINE_COUNT - 1, chain,
                                STATE_MANAGER_ANCESTRY_LINE_COUNT);
    }
    for (int i = 0; i < STATE_MANAGER_ANCESTRY_LINE_COUNT; i++)
    {
        SetEnabledState(s_sm_ui.ancestry_line_tbox[i], i < count);
    }
}
// Push the primary selected entity and its capability state into the UI views.
void UpdateStateManagerSelectedObject(void)
{
    StateManager_ApplyLayout();

    Newtonoid2d *object = StateManager_SelectedObject();

    RefreshPhysView(object);
    RefreshComponentsView(object);
    RefreshGeometryView(object);
    RefreshAncestryView();
}
// ============================================================================
// Module Lifecycle
// ============================================================================

// Build the Entity Mgmt panel: load state_manager.xml, build its host on the former left-panel
// viewport slot (&lpanel_viewport) through the shared panel builder, re-acquire every element by id
// and wire the C-owned behaviour, then lay the panel out over the valid viewport space.
void InitStateManagerSystem(void)
{
    // Tab state is reset first: the selection callback fires while the builder shows the initial view.
    s_sm_entity_tab = 0;
    s_sm_shown_view = SM_SHOWN_UNKNOWN;

    const PanelMarkupDesc desc = {
        .file = UI_COMPONENTS_DIR "/state_manager.xml",
        .viewport = &lpanel_viewport,
        .host_padding = {5.0f, 5.0f},
        .resolve_binding = StateManager_ResolveBinding,
        .resolve_command = StateManager_ResolveCommand,
        .resolve_view_type = StateManager_ResolveViewType,
        .on_view_selected = StateManager_OnViewSelected,   // records the chosen tab
    };
    UIElement *root = NULL;
    state_manager_panel = PanelShared_BuildFromMarkup(&desc, &s_sm_selector, &root);
    if (!state_manager_panel)
    {
        return;
    }

    // ID-REGISTRATION + WIRING PASS: re-acquire every pointer the C refresh/handlers need.
    // (Scrolling is authored declaratively via scroll= on each <View> in the markup.)
    StateManager_RegisterElements(root);

    // Subscribe once so selection transitions can proactively refresh this panel.
    UIState_SetSelectionChangedCallback(HandleSelectionChanged, NULL);

    // First frame should always render initialised state-manager values; final layout update.
    s_sm_layout_signature_valid = false;
    s_sm_geometry_rows_enabled = STATE_MANAGER_GEOMETRY_ROW_COUNT;
    MarkStateManagerRefreshDirty();
    UpdateUISpace(state_manager_panel->root, state_manager_panel->seed_box);
}
void DrawStateManagerSystem(void)
{
    if (state_manager_panel)
    {
        Newtonoid2d *object = StateManager_SelectedObject();

        // The layout (tabs, field lock, DELETE label) is a function of how many entities are selected
        // and which one is primary. SelectionSet_Add does not fire the selection callback, so detect
        // those changes here and mark dirty.
        static int last_entity_count = -1;
        static EntityId last_primary_entity = INVALID_ENTITY_ID;
        int entity_count = PanelShared_CountKind(TARGET_KIND_ENTITY);
        EntityId primary_entity = object ? object->id : INVALID_ENTITY_ID;
        if (entity_count != last_entity_count || primary_entity != last_primary_entity)
        {
            last_entity_count = entity_count;
            last_primary_entity = primary_entity;
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

// Debug-basis hooks: expose the panel host's coordinate space to the diagnostics basis editor.
Frame2d *GetStateManagerSpaceFrame(void)
{
    return ViewHostSystem_GetSpaceFrame(state_manager_panel);
}

bool SetStateManagerSpaceBasis(Vector2d basis_u, Vector2d basis_v)
{
    return ViewHostSystem_SetSpaceBasis(state_manager_panel, basis_u, basis_v);
}

void ResetStateManagerSpaceBasis(void)
{
    ViewHostSystem_ResetSpaceBasis(state_manager_panel);
}

UIElement *GetStateManagerRoot(void)
{
    return state_manager_panel ? state_manager_panel->root : NULL;
}

// Destroy the state-manager panel and clear its cached UI references.
void DestroyStateManagerSystem(void)
{
    UIState_SetSelectionChangedCallback(NULL, NULL);

    ViewHostSystem *panel = state_manager_panel;
    state_manager_panel = NULL;
    ViewHostSystem_Destroy(panel);

    state_manager_refresh_dirty = true;
    PanelShared_ResetFlagButtons(s_sm_flags.entity_role, ARRAY_COUNT(s_sm_flags.entity_role));
    PanelShared_ResetFlagButtons(s_sm_flags.entity_capability, ARRAY_COUNT(s_sm_flags.entity_capability));
    PanelShared_ResetFlagButtons(s_sm_flags.entity_constraint, ARRAY_COUNT(s_sm_flags.entity_constraint));
    PanelShared_ResetFlagButtons(s_sm_flags.entity_status, ARRAY_COUNT(s_sm_flags.entity_status));
    PanelShared_ResetFlagButtons(s_sm_flags.collision_role_mask, ARRAY_COUNT(s_sm_flags.collision_role_mask));
    memset(&s_sm_ui, 0, sizeof(s_sm_ui));
    s_sm_selector = NULL; // Freed by ViewHostSystem_Destroy with the panel.
    s_sm_entity_tab = 0;
    s_sm_shown_view = SM_SHOWN_UNKNOWN;
    s_sm_fields_locked = false;
    s_sm_layout_signature_valid = false;
    s_sm_geometry_rows_enabled = STATE_MANAGER_GEOMETRY_ROW_COUNT;
}
