/**********************************************************************************************
*
TOOLBAR SYSTEM MODULE (implementation)
*
Loads toolbar.xml, mounts it as a single-view ViewHostSystem against the far-left toolbar
viewport band, and wires each tool button as a radio-exclusive toggle. A button's binding= is a
live query source reporting whether that tool is the active one (BIND_INT 0/1, mirroring the
lpanel debug-toggle buttons), and its action= resolves through the command system to a tool
command that sets the active tool radio-exclusively.

The Nudge tool action itself (Toolbar_ApplyNudgeToEntity) is applied additively at the world
click-confirmed site in universe_system.c; it never suppresses the normal selection/drag/marquee
input.
*
**********************************************************************************************/
#include "system/ui/toolbar_system.h"

#include <string.h>
#include <math.h> // cosf / sinf

#include "system/ui/ui_icons.h"          // UIIcon_Draw* glyphs (relocated Nudge + nine toggles)
#include "system/ui/ui_loader.h"
#include "system/view_host_system.h"
#include "system/viewport_system.h"
#include "system/ui_system.h"
#include "system/command_system.h"      // CommandSystem_ResolveString (shared command resolver)
#include "system/debug_overlay_system.h" // DebugOverlayId / IsDebugEnabled (toggle display source)
#include "common/common.h"               // PI, ZERO_VECTOR_2D, LOG_ERROR
#include "ui/ui.h"
#include "ui/binding.h"
#include "entities/entity_registry.h"    // EntityRegistry_GetEntity / EntityRegistry_IsIdActive
#include "entities/entity_flags.h"       // ENTITY_CONSTRAINT_POSITION_LOCKED
#include "physics/newtonoid.h"           // Newtonoid2d / WakeUp
#include "math/helpers.h"                // GetRandomFloat

// ============================================================================
// Panel + tool tuning (module-owned)
// ============================================================================
static ViewHostSystem *toolbar = NULL;

// Single tunable linear-impulse magnitude for the Nudge tool (world units/sec), linear only (no
// torque). The debug spawn velocities range about +/-8, so this is a clearly visible but modest
// kick; tune the whole feel from this one place.
static const float NUDGE_IMPULSE_MAGNITUDE = 6.0f;

// ============================================================================
// Nudge tool action (public: called from the world click-confirmed commit site)
// ============================================================================

// Apply one random-direction linear impulse to the entity, additively. Wakes a sleeping body
// first (a sleeping body ignores small velocity changes), skips position-locked bodies (their
// velocity/momentum are zeroed each physics step anyway), and keeps momentum consistent with the
// new velocity for any same-frame reader (CalcVectors recomputes momentum = mass * velocity next
// step regardless). Linear only, no torque.
void Toolbar_ApplyNudgeToEntity(EntityId id)
{
    // Re-validate the id before resolving: the click-resolved ref was captured at press time, so
    // the entity could have been removed between press and this commit.
    if (!EntityRegistry_IsIdActive(id))
    {
        return;
    }

    Newtonoid2d *obj = EntityRegistry_GetEntity(id);
    if (!obj)
    {
        return;
    }

    // A position-locked body cannot be nudged: its velocity/momentum are wiped each step.
    if (obj->constraints & ENTITY_CONSTRAINT_POSITION_LOCKED)
    {
        return;
    }

    // Wake first so the impulse is not swallowed by the sleep threshold.
    WakeUp(obj);

    // Random unit direction scaled by the tunable magnitude, added on top of current velocity.
    float angle = GetRandomFloat(0.0f, 2.0f * PI);
    obj->velocity.x += cosf(angle) * NUDGE_IMPULSE_MAGNITUDE;
    obj->velocity.y += sinf(angle) * NUDGE_IMPULSE_MAGNITUDE;

    // Keep momentum in step with the new velocity (mirrors CalcVectors' momentum = mass * velocity
    // and the drag path's same-frame consistency).
    obj->momentum.x = obj->mass * obj->velocity.x;
    obj->momentum.y = obj->mass * obj->velocity.y;
}

// ============================================================================
// Binding query shim (returns BIND_INT 0/1, mirroring LPanel's debug-toggle query)
// ============================================================================

// Report whether the keyed tool is the active one, as a canonical 0/1 BIND_INT so the toggle
// button shows pressed/unpressed. The key carries the ActiveTool value.
static BindingValue Toolbar_QueryToolActive(int tool_key)
{
    return (BindingValue){ .type = BIND_INT,
                           .as.i = UIState_IsToolActive((ActiveTool)tool_key) ? 1 : 0 };
}

// ============================================================================
// Debug-toggle display machinery (rehomed from the retired lpanel STATE view)
// ============================================================================

// Map a toggle binding suffix to its DebugOverlayId. The three authored namespaces name each
// toggle's true category and state owner (gizmo.* object overlays, view.* view/display settings,
// debug.* developer diagnostics); all resolve through the DebugOverlayId facade + IsDebugEnabled,
// so this mapping is purely about honest naming, not wiring. Returns false (leaving *out
// untouched) for an unrecognised suffix so the caller can report the binding as unresolved.
static bool Toolbar_ResolveOverlayName(const char *suffix, DebugOverlayId *out)
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

// Query shim matching BindingQueryFn (BindingValue(int)): report whether the keyed debug overlay
// is enabled, normalised to a canonical 0/1 BIND_INT so the bound toggle renders pressed/unpressed
// (and its base-name text + amber-on/dim-off fill). The key carries the DebugOverlayId.
static BindingValue Toolbar_QueryDebugEnabled(int overlay_key)
{
    return (BindingValue){ .type = BIND_INT,
                           .as.i = IsDebugEnabled((DebugOverlayId)overlay_key) ? 1 : 0 };
}

// ============================================================================
// UI loader resolvers (application-specific)
// ============================================================================

// Resolve a toolbar button's binding= name to a live query source. "tool.nudge" maps to the Nudge
// tool's active-state query (radio-exclusive, unchanged). The gizmo.* / view.* / debug.* toggle
// namespaces (rehomed from the lpanel STATE view) strip their prefix, resolve the suffix to a
// DebugOverlayId and return a query over Toolbar_QueryDebugEnabled; an unknown overlay name is
// unresolved. Anything else is unresolved.
static UIBinding Toolbar_ResolveBinding(const char *binding_string, UILoaderContext *ctx)
{
    (void)ctx;

    if (!binding_string)
    {
        return (UIBinding){ .kind = UI_BIND_SRC_NONE };
    }

    // Nudge tool: radio-exclusive active-state source (kept unchanged).
    if (!strcmp(binding_string, "tool.nudge"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = Toolbar_QueryToolActive,
            .query_key = (int)TOOL_NUDGE,
            .value_type = BIND_INT,
        };
    }

    // Debug-toggle namespaces: the prefix names the toggle's true category, the suffix is the
    // overlay name. Each resolves to a live query over IsDebugEnabled (via the overlay facade).
    const char *suffix = NULL;
    if      (!strncmp(binding_string, "gizmo.", 6)) suffix = binding_string + 6;
    else if (!strncmp(binding_string, "view.", 5))  suffix = binding_string + 5;
    else if (!strncmp(binding_string, "debug.", 6)) suffix = binding_string + 6;

    if (suffix)
    {
        DebugOverlayId id;
        if (Toolbar_ResolveOverlayName(suffix, &id))
        {
            return (UIBinding){
                .kind = UI_BIND_SRC_QUERY,
                .query = Toolbar_QueryDebugEnabled,
                .query_key = (int)id,
                .value_type = BIND_INT,
            };
        }
        // Unknown overlay name -> unresolved (loader warns).
        return (UIBinding){ .kind = UI_BIND_SRC_NONE };
    }

    return (UIBinding){ .kind = UI_BIND_SRC_NONE };
}

// Resolve a toolbar button's action= string to a command code (shared command resolver). The
// toolbar's "tool-nudge" action maps to CMD_TOOL_NUDGE, which ExecuteCommand dispatches to the
// radio-exclusive active-tool toggle. Mirrors LPanel_ResolveCommand's use of the shared resolver.
static int Toolbar_ResolveCommand(const char *cmd_string, UILoaderContext *ctx)
{
    (void)ctx;
    return CommandSystem_ResolveString(cmd_string);
}

// ============================================================================
// Lifecycle
// ============================================================================

// Build the toolbar from toolbar.xml and mount it as a single-view strip against the far-left
// toolbar viewport band.
void InitToolbar(void)
{
    if (toolbar)
    {
        return;
    }

    // 1) Load the authoritative markup with the toolbar's own resolvers (no view-type resolver:
    //    the toolbar is a single Container root, not a multi-view ViewHost).
    UIElement *root = UILoader_LoadFromFileWithResolvers(
        "C:\\Projects\\raylib-testing\\src\\engine\\ui\\components\\toolbar.xml",
        &ui_default_palette,
        Toolbar_ResolveBinding,
        Toolbar_ResolveCommand,
        NULL,
        NULL);

    if (!root)
    {
        // XML is the single authoritative source; a load failure is an asset/build error.
        LOG_ERROR("InitToolbar: failed to load toolbar.xml; tool toolbar will not render");
        return;
    }

    // 2) Build the host against the dedicated far-left toolbar viewport band (carved by
    //    InitViewportLayout), build its real root, and re-parent the XML container under it
    //    (NEVER toolbar->root = root - that is the dangling-root bug).
    toolbar = ViewHostSystem_Create(&toolbar_viewport, 1.0f, ZERO_VECTOR_2D,
                                    &ui_default_palette, ui_standard_stack_spacing);
    if (!toolbar)
    {
        DisposeUIElement(root);
        return;
    }

    ViewHostSystem_InitRoot(toolbar);      // Real UI_ELEMENT_ROOT + space + seed_box.
    AddElementToTree(root, toolbar->root);  // Re-parent XML container under the real root.

    // 2b) Attach the procedural glyphs to their buttons by id (the glyphs now live in ui_icons.c).
    //     Each button's label text is empty in the markup, so the renderer draws the on_draw glyph
    //     on top of the flat box. Every binding=/action= wiring is untouched; only the on_draw hook
    //     is added, one guarded line per button exactly as the Nudge button is wired.
    UIElement *nudge_btn = UILoader_FindById(root, "tool_nudge");
    if (nudge_btn)
    {
        nudge_btn->on_draw = UIIcon_DrawNudge;
    }

    // Cell paint tool glyph, wired exactly like the Nudge button (one guarded line). The button has
    // no binding=, so its active-state feedback comes from the on_draw glyph reading the active tool.
    UIElement *cell_paint_btn = UILoader_FindById(root, "tool_cell_paint");
    if (cell_paint_btn)
    {
        cell_paint_btn->on_draw = UIIcon_DrawCellPaint;
    }

    // Tile-type CYCLE button: plain ACTION (no binding=), so its feedback is the on_draw glyph,
    // which tints to the active tile type's palette colour. action="cycle-tile-type" resolves to
    // CMD_CYCLE_TILE_TYPE through the shared command system on click.
    UIElement *tile_cycle_btn = UILoader_FindById(root, "tool_cycle_tile_type");
    if (tile_cycle_btn)
    {
        tile_cycle_btn->on_draw = UIIcon_DrawTileCycle;
    }

    // Toggle glyphs: look each up by id and attach its on_draw. The toggles keep their binding=,
    // so each glyph tints itself from e->colour_fill (the refresh sets it to toggle_on_fill/off).
    UIElement *b;
    if ((b = UILoader_FindById(root, "toggle_dashboard")))            b->on_draw = UIIcon_DrawDashboard;
    if ((b = UILoader_FindById(root, "toggle_viewport_grid")))        b->on_draw = UIIcon_DrawViewportGrid;
    if ((b = UILoader_FindById(root, "toggle_world_grid")))           b->on_draw = UIIcon_DrawWorldGrid;
    if ((b = UILoader_FindById(root, "toggle_world_grid_labels")))    b->on_draw = UIIcon_DrawWorldGridLabels;
    if ((b = UILoader_FindById(root, "toggle_universe_grid_labels"))) b->on_draw = UIIcon_DrawUniverseGridLabels;
    if ((b = UILoader_FindById(root, "toggle_ui_borders")))           b->on_draw = UIIcon_DrawUIBorders;
    if ((b = UILoader_FindById(root, "toggle_object_axes")))          b->on_draw = UIIcon_DrawObjectAxes;
    if ((b = UILoader_FindById(root, "toggle_object_hull")))          b->on_draw = UIIcon_DrawObjectHull;
    if ((b = UILoader_FindById(root, "toggle_object_aabb")))          b->on_draw = UIIcon_DrawObjectAABB;

    // 3) Final layout update over the host's valid coordinate space.
    UpdateUISpace(toolbar->root, toolbar->seed_box);
}

// Draw the toolbar (null-guarded so a load failure degrades to no rendering).
void DrawToolbar(void)
{
    if (!toolbar)
    {
        return;
    }

    ViewHostSystem_Draw(toolbar);
}

// Destroy the toolbar and clear its cached reference.
void DestroyToolbar(void)
{
    ViewHostSystem *panel = toolbar;
    toolbar = NULL;
    ViewHostSystem_Destroy(panel);
}

// Return the toolbar's root UIElement for input hit-testing, or NULL when not loaded.
UIElement *GetToolbarRoot(void)
{
    return toolbar ? toolbar->root : NULL;
}
