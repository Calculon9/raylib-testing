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

#include "raylib.h"                      // DrawLineEx / DrawRectangleLinesEx / DrawCircleV, Vector2 / Color
#include "system/draw_primitives.h"      // ToRaylibColor (ColourRgba -> raylib Color)
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
// Procedural icon (on_draw hook for the Nudge tool button)
// ============================================================================

// Draw the Nudge pictogram inside the element's box: an arrow pushing a small box with a couple
// of motion dots trailing behind it. Monochrome, composed from raylib primitives and centred in
// e->screen_box with padding. Attached via UIElement.on_draw so the renderer calls it on top of
// the (flat) button box; no label is drawn because the button's text is empty.
static void Toolbar_DrawNudgeIcon(UIElement *e)
{
    if (!e)
    {
        return;
    }

    // Pixel geometry of the host element.
    float box_x = e->screen_box.coords.x;
    float box_y = e->screen_box.coords.y;
    float box_w = e->screen_box.dimensions.x;
    float box_h = e->screen_box.dimensions.y;

    // The toolbar tile is wide and short, so size the glyph off its HEIGHT (the limiting axis)
    // and let it span most of that height; the composition is wider than tall and is centred
    // horizontally in the (wider) box. Using ~0.8 of the height fills the tile instead of leaving
    // the glyph tiny. glyph_size is the composition HEIGHT; width is derived from the layout.
    float glyph_size = box_h * 0.8f;
    if (glyph_size < 1.0f)
    {
        return; // Too small to render legibly.
    }
    // The composition spans ~1.6x its height horizontally (arrow + box + trailing dots). Centre
    // that extent in the box.
    float glyph_w = glyph_size * 1.6f;
    float glyph_x = box_x + (box_w - glyph_w) * 0.5f;    // Left edge of the glyph extent.
    float glyph_y = box_y + (box_h - glyph_size) * 0.5f; // Top edge of the glyph box.

    // Active-tool feedback is the ICON's look (no text toggle on this button): draw in the bright
    // button fill when Nudge is the active tool, otherwise the normal text ink. This is the
    // radio-selected indicator for an icon-only button.
    bool active = UIState_IsToolActive(TOOL_NUDGE);
    Color ink = active ? ToRaylibColor(ui_default_palette.button_fill)
                       : ToRaylibColor(ui_default_palette.text);
    float stroke = fmaxf(1.5f, glyph_size * 0.12f);

    // Vertical centre line of the whole composition.
    float cy = glyph_y + glyph_size * 0.5f;

    // Horizontal layout as fractions of the full composition WIDTH (glyph_w), left -> right:
    //   arrow shaft : 0.00 .. 0.32
    //   arrowhead tip: 0.38
    //   box         : 0.42 .. 0.70 (its left side meets the arrow tip)
    //   dots        : 0.82 and 0.96 (motion trail to the right of the box)
    float arrow_start_x = glyph_x + glyph_w * 0.00f;
    float arrow_tip_x   = glyph_x + glyph_w * 0.38f;
    float box_left      = glyph_x + glyph_w * 0.42f;
    float box_right     = glyph_x + glyph_w * 0.70f;

    // --- Arrow shaft: a short horizontal line ending at the box's left edge. ---
    DrawLineEx((Vector2){arrow_start_x, cy}, (Vector2){arrow_tip_x, cy}, stroke, ink);

    // --- Arrowhead: two short diagonal strokes meeting at the tip (upper and lower). The head
    //     reach scales with the composition HEIGHT so it stays proportional to the box. ---
    float head_len = glyph_size * 0.18f; // Diagonal reach back from the tip.
    DrawLineEx((Vector2){arrow_tip_x, cy},
               (Vector2){arrow_tip_x - head_len, cy - head_len}, stroke, ink);
    DrawLineEx((Vector2){arrow_tip_x, cy},
               (Vector2){arrow_tip_x - head_len, cy + head_len}, stroke, ink);

    // --- Box: a square outline whose left side sits just past the arrow tip. Its height is a
    //     fraction of the composition HEIGHT so it reads as a chunky box. ---
    float box_half = glyph_size * 0.28f; // Half the box height (square box).
    Rectangle box_rect = { box_left, cy - box_half, box_right - box_left, box_half * 2.0f };
    DrawRectangleLinesEx(box_rect, stroke, ink);

    // --- Motion dots: two small filled dots trailing to the right of the box. ---
    float dot_radius = fmaxf(1.0f, glyph_size * 0.09f);
    float dot1_x = glyph_x + glyph_w * 0.82f;
    float dot2_x = glyph_x + glyph_w * 0.96f;
    DrawCircleV((Vector2){dot1_x, cy}, dot_radius, ink);
    DrawCircleV((Vector2){dot2_x, cy}, dot_radius, ink);
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

    // 2b) Attach the procedural Nudge icon to the (icon-only) tool button by id. The button's
    //     label text is empty in the markup, so the renderer draws the on_draw glyph on top of
    //     the flat box. The binding="tool.nudge" / action="tool-nudge" wiring is untouched.
    UIElement *nudge_btn = UILoader_FindById(root, "tool_nudge");
    if (nudge_btn)
    {
        nudge_btn->on_draw = Toolbar_DrawNudgeIcon;
    }

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
