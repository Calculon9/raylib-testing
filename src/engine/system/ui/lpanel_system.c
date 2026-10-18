#include "system/ui/lpanel_system.h"
#include "system/ui/ui_loader.h"
#include "system/command_system.h"
#include "system/viewport_system.h"
#include "system/ui/ui_state.h"
#include "ui/ui.h"
#include "system/ui_system.h"
#include "entities/entity_factory.h"
#include "system/debug_overlay_system.h"
#include "ui/ui_constructors.h"
#include "system/view_host_system.h"
#include "system/systems.h"
#include "system/entities/ancestry.h"          // Ancestry_WalkUp / Ancestry_GetKind
#include "system/entities/relation_system.h"   // RelationSystem_GetRelation
#include "entities/entity_components.h"         // RelationType_ToString / RelationComponent
#include "entities/entity_id.h"                 // EntityId_GetSlotIndex / INVALID_ENTITY_ID
#include "world/target_resolver.h"              // TargetRef (primary selection payload)
#include "world/universe.h"                     // G_Universe / Universe_GetWorld
#include "ui/binding.h"                          // BindingValue / BIND_STRING
#include <stdint.h>
#include <string.h>
#include <stdio.h>                               // snprintf for the read-only label text

// ============================================================================
// Panel System
// ============================================================================
static ViewHostSystem *lpanel = NULL;

// ============================================================================
// Panel Selector State
// ============================================================================
// The STATE view's debug toggles and the DRAW view's edit fields are now authored
// in lpanel.xml and built by the generic UI loader; the former hard-coded toggle
// tables and view-container pointers were removed with the imperative builders.
static ViewSelector *lpanel_view_selector = NULL;

// ============================================================================
// UI Loader Resolvers (Application-Specific)
// ============================================================================

// Forward declaration: the debug query shim is defined lower down (near its historic
// neighbours) but the debug.* resolver branch below references it.
static BindingValue LPanel_QueryDebugEnabled(int overlay_key);

// ============================================================================
// Ancestry View State (module-owned, read-only)
// ============================================================================
// Fixed number of pre-declared ancestry line slots. Six generations comfortably covers
// selected entity -> parent entities -> owning world -> universe root world for the current
// depth model; any line beyond the actual chain length renders blank.
#define LPANEL_ANCESTRY_LINE_COUNT 6

// Panel-owned backing buffers for the ancestry view's read-only BIND_STRING labels. The binding
// core copies value.as.s into each textbox's own text buffer during the per-frame refresh, but
// the returned pointer must stay valid for the duration of that call, so storage is module-owned
// (never a stack local). One buffer per line slot, plus the header and relation summaries.
static char lpanel_ancestry_line_text[LPANEL_ANCESTRY_LINE_COUNT][96]; // One per ancestry line.
static char lpanel_ancestry_selected_text[96];                        // Header naming the rooted node.
static char lpanel_ancestry_relation_text[128];                       // Direct-relation summary.

// Forward declarations for the ancestry query shims + their shared selection-root helper. They
// are defined lower down alongside LPanel_QueryDebugEnabled, but the ancestry.* resolver branch
// below references them.
static EntityId     LPanel_AncestryRootId(void);
static BindingValue LPanel_QueryAncestryLine(int line_key);
static BindingValue LPanel_QueryAncestrySelected(int key);
static BindingValue LPanel_QueryAncestryRelation(int key);

/**
 * Map a toggle binding suffix to its DebugOverlayId.
 * The app owns these names (the loader stays domain-free). The authored names use three
 * namespaces that reflect each toggle's TRUE category and state owner (see the ownership
 * split in object_gizmos / world / universe / ui vs the diagnostics module):
 *   gizmo.*  -> per-object inspection overlays (axes/hull/aabb), owned by object_gizmos
 *   view.*   -> view/display settings (grids, grid labels, UI borders)
 *   debug.*  -> genuine developer diagnostics (the dashboard)
 * All three still resolve through the DebugOverlayId facade + IsDebugEnabled query (the facade
 * delegates to the real owners), so this mapping is purely about honest naming, not wiring.
 * Returns false (and leaves *out untouched) for an unrecognised suffix so the loader can warn.
 */
static bool LPanel_ResolveOverlayName(const char *suffix, DebugOverlayId *out)
{
    if (!suffix || !out)
        return false;

    // debug.* (diagnostics)
    if (!strcmp(suffix, "dashboard"))                 { *out = DEBUG_DASHBOARD;            return true; }
    // view.* (view/display settings)
    if (!strcmp(suffix, "viewport-grid"))             { *out = DEBUG_VIEWPORT_GRID;        return true; }
    if (!strcmp(suffix, "world-grid"))                { *out = DEBUG_WORLD_GRID;           return true; }
    if (!strcmp(suffix, "world-grid-labels"))         { *out = DEBUG_WORLD_GRID_LABELS;    return true; }
    if (!strcmp(suffix, "universe-grid-labels"))      { *out = DEBUG_UNIVERSE_GRID_LABELS; return true; }
    if (!strcmp(suffix, "ui-borders"))                { *out = DEBUG_UI_BORDERS;           return true; }
    // gizmo.* (object inspection overlays)
    if (!strcmp(suffix, "object-axes"))               { *out = DEBUG_OBJECT_AXES;          return true; }
    if (!strcmp(suffix, "object-hull"))               { *out = DEBUG_OBJECT_HULL;          return true; }
    if (!strcmp(suffix, "object-aabb"))               { *out = DEBUG_OBJECT_AABB;          return true; }

    return false; // Unrecognised overlay name
}

/**
 * Resolve data bindings for entity creation UI.
 * Maps binding strings like "physics.width" to entity_create_params fields, and the toggle
 * namespaces "gizmo.<name>" / "view.<name>" / "debug.<name>" to a live query source over
 * IsDebugEnabled (the overlay facade delegates to each toggle's real owner).
 */
static UIBinding LPanel_ResolveBinding(const char *binding_string, UILoaderContext *ctx)
{
    UIBinding binding = {NULL, FLOAT};  // Default: unresolved

    if (!binding_string)
        return binding;

    // Handle the ancestry.* read-only labels FIRST. This branch is self-contained and does NOT
    // depend on entity_create_params, so it must resolve even when no create-params exist. Each
    // name maps to a BIND_STRING query source (mirroring the info strip's read-only shims): a
    // header, a relation summary, and six line slots keyed 0..5 by the trailing digit.
    if (!strcmp(binding_string, "ancestry.selected"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = LPanel_QueryAncestrySelected,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }
    if (!strcmp(binding_string, "ancestry.relation"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = LPanel_QueryAncestryRelation,
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
            if (line_key >= 0 && line_key < LPANEL_ANCESTRY_LINE_COUNT)
            {
                return (UIBinding){
                    .kind = UI_BIND_SRC_QUERY,
                    .query = LPanel_QueryAncestryLine,
                    .query_key = line_key,
                    .value_type = BIND_STRING,
                };
            }
        }
        // Malformed ancestry.lineN name -> unresolved (loader warns).
        return (UIBinding){ .kind = UI_BIND_SRC_NONE };
    }

    // Handle the toggle namespaces FIRST (before the physics dot-parse). This branch is
    // self-contained and does NOT depend on entity_create_params, so it must run even when
    // entity_create_params is NULL. The three prefixes name the toggle's true category
    // (gizmo./view./debug.); the suffix after the dot is the overlay name. Each returns a
    // query source over IsDebugEnabled and returns before the dot-parse logic.
    const char *suffix = NULL;
    if      (!strncmp(binding_string, "gizmo.", 6)) suffix = binding_string + 6;
    else if (!strncmp(binding_string, "view.", 5))  suffix = binding_string + 5;
    else if (!strncmp(binding_string, "debug.", 6)) suffix = binding_string + 6;

    if (suffix)
    {
        DebugOverlayId id;
        if (LPanel_ResolveOverlayName(suffix, &id))
        {
            return (UIBinding){
                .kind = UI_BIND_SRC_QUERY,
                .query = LPanel_QueryDebugEnabled, // existing shim, kept
                .query_key = (int)id,
                .value_type = BIND_INT,
            };
        }
        // Unknown overlay name -> unresolved (loader warns).
        return (UIBinding){ .kind = UI_BIND_SRC_NONE };
    }

    if (!G_UIState.entity_create_params)
        return binding;
    
    // Parse binding string format: "component.field"
    const char *dot_pos = strchr(binding_string, '.');
    if (!dot_pos)
        return binding;  // Invalid format
    
    int component_len = dot_pos - binding_string;
    const char *field_name = dot_pos + 1;
    
    // Handle physics.* bindings for Newtonoid2dParams
    if (component_len == 7 && !strncmp(binding_string, "physics", 7))
    {
        Newtonoid2dParams *physics = &G_UIState.entity_create_params->physics;
        
        // Map field names to addresses and types
        if (!strcmp(field_name, "vertice_count"))
        {
            binding.address = &physics->vertice_count;
            binding.data_type = INT;
        }
        else if (!strcmp(field_name, "width"))
        {
            binding.address = &physics->width;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "height"))
        {
            binding.address = &physics->height;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "mass"))
        {
            binding.address = &physics->mass;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "restitution"))
        {
            binding.address = &physics->restitution;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "friction"))
        {
            binding.address = &physics->friction;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "radius"))
        {
            binding.address = &physics->radius;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "anchor_position"))
        {
            binding.address = &physics->anchor_position;
            binding.data_type = VECTOR2D;
        }
        else if (!strcmp(field_name, "velocity"))
        {
            binding.address = &physics->velocity;
            binding.data_type = VECTOR2D;
        }
        else if (!strcmp(field_name, "acceleration"))
        {
            binding.address = &physics->acceleration;
            binding.data_type = VECTOR2D;
        }
        else if (!strcmp(field_name, "momentum"))
        {
            binding.address = &physics->momentum;
            binding.data_type = VECTOR2D;
        }
    }
    // TODO: Add support for other components (portal.*, rotor.*, gear.*, health.*)
    
    return binding;
}

/**
 * Resolve action strings to command codes (adapter for CommandSystem_ResolveString).
 * This is a wrapper around the universal command system resolver, allowing application-specific
 * logic to be added here if needed in the future.
 */
static int LPanel_ResolveCommand(const char *cmd_string, UILoaderContext *ctx)
{
    (void)ctx;  // Currently unused; provided for future application-specific context
    return CommandSystem_ResolveString(cmd_string);
}

/**
 * Resolve a left-panel view-type string to its ViewType code.
 * Maps the two lpanel view-type names authored in lpanel.xml; any other string is
 * reported as unresolved and defaults to the first view type.
 */
static ViewType LPanel_ResolveViewType(const char *type_string, bool *resolved, UILoaderContext *ctx)
{
    (void)ctx;  // Currently unused; provided for future application-specific context

    if (resolved)
    {
        *resolved = true;
    }

    if (type_string && !strcmp(type_string, "LPANEL_STATE_VIEW"))
    {
        return LPANEL_STATE_VIEW;
    }
    if (type_string && !strcmp(type_string, "LPANEL_DRAW_VIEW"))
    {
        return LPANEL_DRAW_VIEW;
    }
    if (type_string && !strcmp(type_string, "LPANEL_ANCESTRY_VIEW"))
    {
        return LPANEL_ANCESTRY_VIEW;
    }

    // Unrecognised: report failure and default to the first view type.
    if (resolved)
    {
        *resolved = false;
    }
    return LPANEL_STATE_VIEW;
}

// Recalculate the left-panel UI tree immediately for layout debugging.
static void HandleLPanelUIRefreshClick(UIElement *button)
{
    (void)button;

    if (lpanel && lpanel->root)
    {
        UpdateUISpace(lpanel->root, lpanel->seed_box);
    }
}

// ============================================================================
// Root Layout
// ============================================================================
// ============================================================================
// Toggle Button Layout
// ============================================================================
// ============================================================================
// State View Layout
// ============================================================================s
// Query shim matching BindingQueryFn (BindingValue(int)). The binding core stays decoupled
// from the debug subsystem: it only knows "call this fn with this int key". Here the key is a
// DebugOverlayId, so the shim is a thin, type-clean adapter over IsDebugEnabled that normalises
// the on/off state to a canonical 0/1 BindingValue INT (preserving the toggle's ON/OFF display).
static BindingValue LPanel_QueryDebugEnabled(int overlay_key)
{
    return (BindingValue){ .type = BIND_INT, .as.i = IsDebugEnabled((DebugOverlayId)overlay_key) ? 1 : 0 };
}

// ============================================================================
// Ancestry View Query Shims (read-only BIND_STRING sources)
// ============================================================================

/**
 * Resolve the EntityId the ancestry walk should root on, from the current PRIMARY selection.
 * Selection is read fresh every call (never cached) so the view always reflects the live set.
 * An entity selection roots on itself; a world selection roots on that world's own EntityId
 * (its chain is itself -> universe root world); a cell / empty selection has no frame-bearing
 * node, so INVALID_ENTITY_ID is returned (callers render the neutral "No selection" state).
 * This is the single source of truth the ancestry shims share, so selection resolution is not
 * duplicated across them.
 */
static EntityId LPanel_AncestryRootId(void)
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
 * Query shim for one ancestry line slot (line_key = 0..LPANEL_ANCESTRY_LINE_COUNT-1).
 * Rooted fresh each frame via LPanel_AncestryRootId, it walks up to the root world into a small
 * stack buffer and formats the requested generation as "<KIND> #<slot>". Slots beyond the actual
 * chain length (or when nothing is selected) render blank: the buffer is set to an empty string
 * and returned as BIND_STRING "" rather than BIND_NONE, because BIND_NONE leaves the textbox's
 * last-good text untouched (stale) instead of clearing it.
 */
static BindingValue LPanel_QueryAncestryLine(int line_key)
{
    // Clamp defensively; the resolver already bounds the key, but keep the array access safe.
    if (line_key < 0 || line_key >= LPANEL_ANCESTRY_LINE_COUNT)
    {
        return (BindingValue){ .type = BIND_STRING, .as.s = "" };
    }

    char *buf = lpanel_ancestry_line_text[line_key];
    buf[0] = '\0'; // Default to blank so an unused slot always clears.

    EntityId root = LPanel_AncestryRootId();
    if (root == INVALID_ENTITY_ID)
    {
        // Nothing frame-bearing is selected: this slot stays blank.
        return (BindingValue){ .type = BIND_STRING, .as.s = buf };
    }

    // Walk the chain fresh every frame. max_generations is a HOP count, so slots-1 hops yields up
    // to LPANEL_ANCESTRY_LINE_COUNT ids (start first), matching the slot capacity.
    EntityId chain[LPANEL_ANCESTRY_LINE_COUNT];
    int count = Ancestry_WalkUp(root, LPANEL_ANCESTRY_LINE_COUNT - 1, chain,
                                LPANEL_ANCESTRY_LINE_COUNT);

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
    snprintf(buf, sizeof(lpanel_ancestry_line_text[line_key]), "%s #%u",
             label, EntityId_GetSlotIndex(chain[line_key]));

    return (BindingValue){ .type = BIND_STRING, .as.s = buf };
}

/**
 * Query shim for the ancestry header line. Names the rooted (primary-selected) node, or shows
 * the neutral "No selection" state when nothing frame-bearing is selected. Resolved fresh each
 * frame; returns BIND_STRING pointing at the panel-owned header buffer.
 */
static BindingValue LPanel_QueryAncestrySelected(int key)
{
    (void)key; // Single header source; key is unused.

    EntityId root = LPanel_AncestryRootId();
    if (root == INVALID_ENTITY_ID)
    {
        // Neutral empty state.
        snprintf(lpanel_ancestry_selected_text, sizeof(lpanel_ancestry_selected_text),
                 "No selection");
        return (BindingValue){ .type = BIND_STRING, .as.s = lpanel_ancestry_selected_text };
    }

    AncestryNodeKind kind = Ancestry_GetKind(root);
    const char *label = (kind == ANCESTRY_KIND_WORLD)  ? "WORLD"
                      : (kind == ANCESTRY_KIND_ENTITY) ? "ENTITY"
                                                       : "INVALID";
    snprintf(lpanel_ancestry_selected_text, sizeof(lpanel_ancestry_selected_text),
             "Selected: %s #%u", label, EntityId_GetSlotIndex(root));

    return (BindingValue){ .type = BIND_STRING, .as.s = lpanel_ancestry_selected_text };
}

/**
 * Query shim for the direct-relation summary line. Reads the rooted node's relation fresh each
 * frame (never caching the RelationComponent* across frames): "Relation: -" when nothing is
 * selected, "Relation: none" when the node has no relation, otherwise
 * "Relation: <type> -> #<target slot>" with an " (inactive)" suffix when the relation is dormant.
 * Returns BIND_STRING pointing at the panel-owned relation buffer.
 */
static BindingValue LPanel_QueryAncestryRelation(int key)
{
    (void)key; // Single relation source; key is unused.

    EntityId root = LPanel_AncestryRootId();
    if (root == INVALID_ENTITY_ID)
    {
        // Neutral empty state (nothing selected).
        snprintf(lpanel_ancestry_relation_text, sizeof(lpanel_ancestry_relation_text),
                 "Relation: -");
        return (BindingValue){ .type = BIND_STRING, .as.s = lpanel_ancestry_relation_text };
    }

    RelationComponent *rel = RelationSystem_GetRelation(root);
    if (!rel)
    {
        // Node exists but carries no relation.
        snprintf(lpanel_ancestry_relation_text, sizeof(lpanel_ancestry_relation_text),
                 "Relation: none");
        return (BindingValue){ .type = BIND_STRING, .as.s = lpanel_ancestry_relation_text };
    }

    // Describe the relation by type and target slot; flag an inactive relation for honesty.
    snprintf(lpanel_ancestry_relation_text, sizeof(lpanel_ancestry_relation_text),
             "Relation: %s -> #%u%s", RelationType_ToString(rel->type),
             EntityId_GetSlotIndex(rel->target_entity), rel->is_active ? "" : " (inactive)");

    return (BindingValue){ .type = BIND_STRING, .as.s = lpanel_ancestry_relation_text };
}

void InitLPanel()
{
    // Load the left panel from its authoritative XML markup, supplying the
    // application-specific binding / command / view-type resolvers.
    UIElement *root = UILoader_LoadFromFileWithResolvers(
        "C:\\Projects\\raylib-testing\\src\\engine\\ui\\components\\lpanel.xml",
        &ui_default_palette,
        LPanel_ResolveBinding,
        LPanel_ResolveCommand,
        LPanel_ResolveViewType,
        NULL
    );

    if (!root)
    {
        // XML is the single authoritative source; a load failure is an asset/build
        // error. Leave lpanel NULL (DrawLPanel null-guards) rather than diverging
        // to a hard-coded panel.
        LOG_ERROR("InitLPanel: failed to load lpanel.xml; left panel will not render");
        return;
    }

    // Toggle buttons now carry their live debug-state source declaratively (binding="debug.*"
    // resolved in BuildButton via LPanel_ResolveBinding), so no C post-pass is needed.

    // Extract the Views from the XML tree; each now carries its correct ViewType
    // resolved from the XML type= via LPanel_ResolveViewType.
    size_t view_count = 0;
    View **xml_views = UILoader_ExtractViews(root, &view_count);

    // Create the host, then build its real root (with a valid coordinate space /
    // seed box) and re-parent the XML <ViewHost> container under it. The XML tree
    // drives everything inside the single real root.
    lpanel = ViewHostSystem_Create(&lpanel_viewport, 1.0f, (Vector2d){0.1f, 0.1f},
                                   &ui_default_palette, ui_standard_stack_spacing);
    if (!lpanel)
    {
        DisposeUIElement(root);
        if (xml_views)
        {
            Deallocate((void **)&xml_views, sizeof(View *) * view_count);
        }
        return;
    }

    ViewHostSystem_InitRoot(lpanel);        // Build the real UI_ELEMENT_ROOT + space + seed_box.
    AddElementToTree(root, lpanel->root);   // Re-parent XML container under the real root.
                                            // NEVER lpanel->root = root (the dangling-root bug).

    // Register the XML-created Views with the host.
    ViewHostSystem_InitViews(lpanel, view_count);
    for (size_t i = 0; i < view_count; i++)
    {
        LArray_Push(&lpanel->views, &xml_views[i]);
    }
    if (xml_views)
    {
        // Free the array (not the Views, which the host now owns).
        Deallocate((void **)&xml_views, sizeof(View *) * view_count);
    }

    // Build and register the selector from the XML <Option> metadata, wiring each
    // option button to drive ViewHostSystem_SelectView.
    lpanel_view_selector = UILoader_BuildSelectorFromMarkup(lpanel, ViewHostSystem_HandleViewSelected);

    if (lpanel_view_selector)
    {
        // Honour ViewHost initialView="state_view" (falling back to the first view),
        // via the single styling + selection call.
        int initial = UILoader_ResolveViewIndexById(lpanel, UILoader_GetInitialViewId());
        if (initial < 0)
        {
            initial = 0;
        }
        ViewHostSystem_SelectView(lpanel_view_selector, (size_t)initial);
    }
    else
    {
        // Selector allocation failed. XML view containers default enabled, so
        // establish single-view visibility manually via the public primitives
        // (SetPanelActiveView is static and not reachable here). Never call
        // ViewHostSystem_FinaliseInit (it forces index 0 and fights initialView).
        for (int i = 0; i < lpanel->views.count; i++)
        {
            View *view = *((View **)LArray_Get(&lpanel->views, i));
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

    // Final layout update over the host's valid coordinate space.
    UpdateUISpace(lpanel->root, lpanel->seed_box);
}

void DrawLPanel(void)
{
    if (!lpanel)
    {
        return;
    }

    ViewHostSystem_Draw(lpanel);
}

Frame2d *GetLPanelSpaceFrame(void)
{
    return ViewHostSystem_GetSpaceFrame(lpanel);
}

bool SetLPanelSpaceBasis(Vector2d basis_u, Vector2d basis_v)
{
    return ViewHostSystem_SetSpaceBasis(lpanel, basis_u, basis_v);
}

void ResetLPanelSpaceBasis(void)
{
    ViewHostSystem_ResetSpaceBasis(lpanel);
}

UIElement *GetLPanelRoot(void)
{
    return lpanel ? lpanel->root : NULL;
}

ViewHostSystem *GetLPanelViewHost(void)
{
    return lpanel;
}

// Destroy the left panel and clear its cached UI references.
void DestroyLPanel(void)
{
    ViewHostSystem *panel = lpanel;
    lpanel = NULL;
    ViewHostSystem_Destroy(panel);

    lpanel_view_selector = NULL;
    G_UIState.lpanel_views = NULL;

    G_UIState.edit_edge_count_tbox = NULL;
    G_UIState.edit_vertice_count_tbox = NULL;
    G_UIState.edit_width_tbox = NULL;
    G_UIState.edit_height_tbox = NULL;
    G_UIState.edit_mass_tbox = NULL;
    G_UIState.edit_restitution_tbox = NULL;
    G_UIState.edit_friction_tbox = NULL;
    G_UIState.edit_pos_c_tbox = NULL;
    G_UIState.edit_vel_tbox = NULL;
    G_UIState.edit_accel_tbox = NULL;
    G_UIState.edit_moment_tbox = NULL;
    G_UIState.edit_edge_count_str = NULL;
    G_UIState.edit_vertice_count_str = NULL;
    G_UIState.edit_width_str = NULL;
    G_UIState.edit_height_str = NULL;
    G_UIState.edit_mass_str = NULL;
    G_UIState.edit_restitution_str = NULL;
    G_UIState.edit_friction_str = NULL;
    G_UIState.edit_pos_c_str = NULL;
    G_UIState.edit_vel_str = NULL;
    G_UIState.edit_accel_str = NULL;
    G_UIState.edit_moment_str = NULL;
}
