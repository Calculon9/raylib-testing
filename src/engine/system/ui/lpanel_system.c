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
#include <stdint.h>
#include <string.h>

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
