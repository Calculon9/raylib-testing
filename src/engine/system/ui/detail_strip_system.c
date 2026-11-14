/**********************************************************************************************
*
DETAIL STRIP SYSTEM MODULE (implementation)
*
Loads detail_strip.xml, mounts it as a single-view ViewHostSystem against the detail-strip
viewport, and feeds ONE read-only BIND_STRING textbox with a composed description string.

What the string says is decided each frame by DetailStrip_ComposeText, in priority order:
  1. HOVER HINT - the element under the pointer (or its nearest ancestor carrying a hint). The
     static text comes from the markup hint= attribute (UIElement.hint) and any live values are
     appended by the element's on_hint callback (e.g. "Tile: grass (1) | ON").
  2. TOOL CONTEXT - when nothing hintable is hovered: TOOL_SPAWN shows the active prefab,
     TOOL_CELL_PAINT shows the active tile, TOOL_NUDGE / TOOL_NONE show the em-dash placeholder.
Segments are joined with " | " by UIHint_AppendSegment, which also truncates with "..." so the
text never overruns the textbox it is displayed in.

The textbox text buffer is a String64, so the visible string is capped at 63 characters; hints
and context segments are written to stay well inside that.
*
**********************************************************************************************/
#include "system/ui/detail_strip_system.h"

#include <string.h>
#include <stdio.h> // snprintf

#include "system/ui/ui_loader.h"
#include "system/view_host_system.h"
#include "system/viewport_system.h"
#include "system/ui_system.h"
#include "ui/ui.h"
#include "ui/binding.h"
#include "entities/entity_catalogue.h" // Catalogue_GetParams / Catalogue_GetId
#include "entities/entity_factory.h"   // EntityCreateParams / CREATION_COMPONENT_*
#include "math/geometry.h"             // ShapeType enum
#include "world/world.h"               // TilePalette_NameForType / TilePalette_ColourForType
#include "common/common.h"             // LOG_ERROR

// Em-dash placeholder shown when there is nothing to describe.
#define DETAIL_STRIP_EMPTY "-"

// ============================================================================
// Panel state (module-owned)
// ============================================================================
static ViewHostSystem *detail_strip = NULL;

// Panel-owned backing buffer for the BIND_STRING query result (BIND_STRING lifetime - never a
// stack local). Sized to the textbox's own String64 so truncation happens here, with an
// ellipsis, instead of silently inside the binding copy.
static char detail_text[sizeof(String64)];

// ============================================================================
// Tool-context helpers (the fallback when no hintable element is hovered)
// ============================================================================

// Resolve the active spawn prefab. Returns its params (NULL when the catalogue is empty) and,
// when out_id is non-NULL, writes the prefab's id ("" when empty).
static const EntityCreateParams *DetailStrip_ActivePrefabParams(const char **out_id)
{
    int idx = UIState_GetActivePrefab();
    const EntityCreateParams *p = Catalogue_GetParams(idx); // NULL when catalogue empty.
    if (out_id)
    {
        *out_id = Catalogue_GetId(idx); // "" when empty.
    }
    return p;
}

// Map a ShapeType to a short label. There is NO existing ShapeType->string helper in the tree, so
// this file-local map covers every ShapeType value (SHAPE_AUTO .. SHAPE_GEAR) with a "?" default.
static const char *DetailStrip_ShapeName(ShapeType shape)
{
    switch (shape)
    {
        case SHAPE_AUTO:                 return "auto";
        case SHAPE_TRIANGLE:             return "triangle";
        case SHAPE_SQUARE:               return "square";
        case SHAPE_CIRCLE:               return "circle";
        case SHAPE_ELLIPSE:              return "ellipse";
        case SHAPE_POLYGON:              return "polygon";
        case SHAPE_RECTANGLE:            return "rectangle";
        case SHAPE_TRIANGLE_EQUILATERAL: return "triangle-eq";
        case SHAPE_TRIANGLE_ISOSCELES:   return "triangle-iso";
        case SHAPE_ARROW:                return "arrow";
        case SHAPE_ROTOR:                return "rotor";
        case SHAPE_GEAR:                 return "gear";
        default:                         return "?";
    }
}

// Write a prefab's component names, space separated, into `out` ("none" when it carries none).
//
//   p   : the prefab's creation params (non-NULL).
//   out : destination buffer.
//   cap : capacity of out in bytes.
static void DetailStrip_FormatComponents(const EntityCreateParams *p, char *out, size_t cap)
{
    static const struct
    {
        int flag;
        const char *name;
    } components[] = {
        { CREATION_COMPONENT_PORTAL, "portal" },
        { CREATION_COMPONENT_ROTOR,  "rotor"  },
        { CREATION_COMPONENT_GEAR,   "gear"   },
        { CREATION_COMPONENT_HEALTH, "health" },
    };

    out[0] = '\0';
    size_t used = 0;
    for (size_t i = 0; i < sizeof(components) / sizeof(components[0]); i++)
    {
        if (((int)p->component_flags & components[i].flag) && used + 1 < cap)
        {
            int written = snprintf(out + used, cap - used, "%s%s", used ? " " : "", components[i].name);
            if (written < 0)
            {
                break;
            }
            used += (size_t)written;
            if (used >= cap)
            {
                used = cap - 1; // snprintf truncated; stay inside the buffer.
            }
        }
    }
    if (used == 0)
    {
        snprintf(out, cap, "none");
    }
}

// Append the active tool's context (spawn prefab / paint tile) to `out`. Writes the em-dash
// placeholder for TOOL_NUDGE and TOOL_NONE, which have no payload to show.
//
//   out : destination text (appended to via UIHint_AppendSegment).
//   cap : capacity of out in bytes.
static void DetailStrip_ComposeToolContext(char *out, size_t cap)
{
    switch (UIState_GetActiveTool())
    {
        case TOOL_SPAWN:
        {
            const char *id = NULL;
            const EntityCreateParams *p = DetailStrip_ActivePrefabParams(&id);
            if (!p)
            {
                UIHint_AppendSegment(out, cap, "Spawn: " DETAIL_STRIP_EMPTY);
                break;
            }

            // Compact one-line summary: "Spawn: crate | square | portal rotor | 2x2 m=1.00".
            char components[48];
            DetailStrip_FormatComponents(p, components, sizeof(components));
            UIHint_AppendSegment(out, cap, "Spawn: %s", (id && *id) ? id : DETAIL_STRIP_EMPTY);
            UIHint_AppendSegment(out, cap, "%s", DetailStrip_ShapeName(p->physics.shape_type));
            UIHint_AppendSegment(out, cap, "%s", components);
            UIHint_AppendSegment(out, cap, "%.0fx%.0f m=%.2f", (double)p->physics.width,
                                 (double)p->physics.height, (double)p->physics.mass);
            break;
        }
        case TOOL_CELL_PAINT:
        {
            int type = UIState_GetActiveTileType();
            // The fallback only feeds the "RGB r,g,b" hint text (never a drawn colour), so it is the
            // neutral COLOURLESS_RGBA rather than a palette role.
            ColourRgba c = TilePalette_ColourForType(type, COLOURLESS_RGBA);
            UIHint_AppendSegment(out, cap, "Paint: %s (%d)", TilePalette_NameForType(type), type);
            UIHint_AppendSegment(out, cap, "RGB %d,%d,%d", (int)c.r, (int)c.g, (int)c.b);
            break;
        }
        default: // TOOL_NUDGE / TOOL_NONE - no payload to show.
            UIHint_AppendSegment(out, cap, DETAIL_STRIP_EMPTY);
            break;
    }
}

// ============================================================================
// Hover resolution + composition
// ============================================================================

// Find the element whose hint should describe the current hover: the hovered element itself, or
// the nearest enabled ancestor carrying a static hint or an on_hint callback. Returns NULL when
// the pointer is not over a hintable part of the UI.
static const UIElement *DetailStrip_FindHintElement(void)
{
    for (const UIElement *e = G_UIState.hovered_element; e != NULL; e = e->parent)
    {
        if (e->is_enabled && (e->hint != NULL || e->on_hint != NULL))
        {
            return e;
        }
    }
    return NULL;
}

// Build the strip's text for this frame into `out`. A hovered hint wins; otherwise the active
// tool's context is shown. `out` is always left NUL-terminated.
//
//   out : destination buffer.
//   cap : capacity of out in bytes.
static void DetailStrip_ComposeText(char *out, size_t cap)
{
    out[0] = '\0';

    const UIElement *hinted = DetailStrip_FindHintElement();
    if (hinted)
    {
        if (hinted->hint)
        {
            UIHint_AppendSegment(out, cap, "%s", hinted->hint);
        }
        if (hinted->on_hint)
        {
            hinted->on_hint(hinted, out, cap); // Appends live values (state, selection).
        }
        if (out[0] != '\0')
        {
            return;
        }
    }

    DetailStrip_ComposeToolContext(out, cap);
}

// ============================================================================
// Binding query shim (returns BIND_STRING, mirroring the info strip's shims)
// ============================================================================

// The strip's single text source. Recomposes the string each refresh and returns a BIND_STRING
// value pointing at the panel-owned buffer; the key is unused.
static BindingValue DetailStrip_QueryText(int key)
{
    (void)key;
    DetailStrip_ComposeText(detail_text, sizeof(detail_text));
    return (BindingValue){ .type = BIND_STRING, .as.s = detail_text };
}

// ============================================================================
// UI loader resolvers (application-specific)
// ============================================================================

// Resolve the strip's binding name to its query source. Only "detailstrip.text" is recognised;
// anything else is unresolved.
static UIBinding DetailStrip_ResolveBinding(const char *binding_string, UILoaderContext *ctx)
{
    (void)ctx;

    if (binding_string && !strcmp(binding_string, "detailstrip.text"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = DetailStrip_QueryText,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }

    return (UIBinding){ .kind = UI_BIND_SRC_NONE };
}

// The strip has no command buttons; every action name resolves to none.
static int DetailStrip_ResolveCommand(const char *cmd_string, UILoaderContext *ctx)
{
    (void)cmd_string;
    (void)ctx;
    return 0;
}

// ============================================================================
// Lifecycle
// ============================================================================

// Load detail_strip.xml and mount it as a single-view strip against the detail-strip viewport.
void InitDetailStrip(void)
{
    if (detail_strip)
    {
        return;
    }

    // 1) Load the authoritative markup with the strip's own resolvers. The path is the main-repo
    //    component location, matching the convention used by info_strip_system.c / toolbar_system.c
    //    / state_manager_system.c (they all load from src/engine/ui/components/).
    UIElement *root = UILoader_LoadFromFileWithResolvers(
        UI_COMPONENTS_DIR "/detail_strip.xml",
        UIPalette_GetActive(),
        DetailStrip_ResolveBinding,
        DetailStrip_ResolveCommand,
        NULL,
        NULL);

    if (!root)
    {
        // XML is the single authoritative source; a load failure is an asset/build error.
        LOG_ERROR("InitDetailStrip: failed to load detail_strip.xml; detail strip will not render");
        return;
    }

    // 2) Build the host against the dedicated detail-strip viewport band (carved from the bottom of
    //    the central column by InitViewportLayout), build its real root, and re-parent the XML
    //    container under it. Mounting against detail_strip_viewport keeps the strip in its own
    //    reserved screen region so it never overlaps the world render area.
    detail_strip = ViewHostSystem_Create(&detail_strip_viewport, 1.0f, ui_strip_host_padding,
                                         UIPalette_GetActive(), ui_standard_stack_spacing);
    if (!detail_strip)
    {
        DisposeUIElement(root);
        return;
    }

    ViewHostSystem_InitRoot(detail_strip);      // Real UI_ELEMENT_ROOT + space + seed_box.
    AddElementToTree(root, detail_strip->root);  // Re-parent XML container under the real root.

    // 3) Final layout update over the host's valid coordinate space.
    UpdateUISpace(detail_strip->root, detail_strip->seed_box);
}

// Draw the detail strip (null-guarded so a load failure degrades to no rendering).
void DrawDetailStrip(void)
{
    if (!detail_strip)
    {
        return;
    }

    ViewHostSystem_Draw(detail_strip);
}

// Destroy the detail strip and clear its cached references.
void DestroyDetailStrip(void)
{
    ViewHostSystem *panel = detail_strip;
    detail_strip = NULL;
    ViewHostSystem_Destroy(panel);

    // Reset the text buffer so a later re-init starts clean (no stale label).
    detail_text[0] = '\0';
}
