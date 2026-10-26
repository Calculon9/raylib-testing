/**********************************************************************************************
*
INFO STRIP SYSTEM MODULE (implementation)
*
Loads info_strip.xml, mounts it as a single-view ViewHostSystem against the info-strip viewport, and
feeds two read-only BIND_STRING labels: the hovered target and the current selection. Both are
expressed through the universal TargetRef resolver (Target_ResolveAtPoint / Target_Describe).
The hover ref is recomputed each frame from the pointer position by InfoStrip_UpdateHover,
which is strictly read-only with respect to input routing.
*
**********************************************************************************************/
#include "system/ui/info_strip_system.h"

#include <string.h>

#include "system/ui/ui_loader.h"
#include "system/view_host_system.h"
#include "system/viewport_system.h"
#include "system/ui_system.h"
#include "ui/ui.h"
#include "ui/binding.h"
#include "world/universe.h"
#include "world/world_internal.h" // ResolvePixelToWorldFrame
#include "world/target_resolver.h"
#include "entities/entity_registry.h" // EntityRegistry_IsIdActive
#include "system/ui/ui_state.h"       // G_UIState.selection_set (set tally)
#include <stdio.h>                     // snprintf for the count summary

// ============================================================================
// Panel + hover state (module-owned)
// ============================================================================
static ViewHostSystem *info_strip = NULL;

// The persistent hovered target. There is no engine-wide hover state today, so the strip owns
// it here: InfoStrip_UpdateHover writes it each frame; the hover query reads it. A plain value
// (not a pointer) keeps it stale-safe and avoids any allocation.
static TargetRef info_strip_hover_ref = {0};

// Stable backing buffers for the BIND_STRING query results. Binding_FormatValue copies the
// value.as.s string into the element's own text buffer during the per-frame refresh, but the
// pointer must stay valid for the duration of that call, so panel-owned storage is used
// (mirroring how textbox text buffers are owned). Sized generously for the composed labels.
static char info_strip_hover_text[160];
static char info_strip_selection_text[160];

// Retained pointer state for the cursor-coordinate shims. InfoStrip_UpdateHover captures the
// current pointer pixel and whether it lies inside the game viewport each frame; the two coord
// query shims then convert that pixel into world frames at binding-refresh time. Panel-owned
// backing buffers hold the formatted labels (BIND_STRING lifetime — never stack locals).
static Vector2d info_strip_pointer_pixel = {0};  // Last pointer pixel captured this frame.
static bool     info_strip_pointer_in_view = false; // Pointer inside game_viewport this frame.
static char     info_strip_rootcoords_text[64];  // Backing buffer for the Root coord label.
static char     info_strip_childcoords_text[64]; // Backing buffer for the Child coord label.
static char     info_strip_tile_text[32];        // Backing buffer for the active paint-tile label.

// ============================================================================
// Binding query shims (return BIND_STRING, mirroring LPanel's INT query shims)
// ============================================================================

// Compose the hover label from the stored hover ref. Returns a BIND_STRING value pointing at
// the panel-owned buffer; the key is unused (a single hover source).
static BindingValue InfoStrip_QueryHoverLabel(int key)
{
    (void)key;
    Target_Describe(info_strip_hover_ref, info_strip_hover_text, sizeof(info_strip_hover_text));
    return (BindingValue){ .type = BIND_STRING, .as.s = info_strip_hover_text };
}

// Compose the selection label from the central selection set. An empty set reports "nothing";
// exactly one member is described with the existing single-target label; more than one member is
// summarised as a count tally by TargetKind (e.g. "Selection: 3 entities, 1 world"). The set is
// the source of truth, so the primary member is read via SelectionSet_Primary for the single case.
static BindingValue InfoStrip_QuerySelectionLabel(int key)
{
    (void)key;

    int count = SelectionSet_Count();

    if (count <= 0)
    {
        // Empty selection: keep the existing neutral "none" output.
        Target_Describe((TargetRef){ .kind = TARGET_KIND_NONE }, info_strip_selection_text,
                        sizeof(info_strip_selection_text));
        return (BindingValue){ .type = BIND_STRING, .as.s = info_strip_selection_text };
    }

    if (count == 1)
    {
        // Single member: preserve today's single-target label via the set's primary.
        Target_Describe(SelectionSet_Primary(), info_strip_selection_text,
                        sizeof(info_strip_selection_text));
        return (BindingValue){ .type = BIND_STRING, .as.s = info_strip_selection_text };
    }

    // More than one member: tally kinds over the set and compose a count summary.
    int entity_count = 0;
    int world_count = 0;
    int cell_count = 0;
    TargetRef *member = NULL;
    LArray_ForEach(&G_UIState.selection_set, TargetRef*, member)
    {
        switch (member->kind)
        {
            case TARGET_KIND_ENTITY: entity_count++; break;
            case TARGET_KIND_WORLD:  world_count++;  break;
            case TARGET_KIND_CELL:   cell_count++;   break;
            default: break;
        }
    }

    // Build "Selection: N entities, M worlds, K cells", omitting zero-count kinds and pluralising
    // per tally. Writes are length-checked so the panel-owned buffer never overflows.
    int offset = 0;
    offset += snprintf(info_strip_selection_text + offset,
                       sizeof(info_strip_selection_text) - (size_t)offset, "Selection: ");

    bool first = true;
    if (entity_count > 0 && offset < (int)sizeof(info_strip_selection_text))
    {
        offset += snprintf(info_strip_selection_text + offset,
                           sizeof(info_strip_selection_text) - (size_t)offset,
                           "%d %s", entity_count, entity_count == 1 ? "entity" : "entities");
        first = false;
    }
    if (world_count > 0 && offset < (int)sizeof(info_strip_selection_text))
    {
        offset += snprintf(info_strip_selection_text + offset,
                           sizeof(info_strip_selection_text) - (size_t)offset,
                           "%s%d %s", first ? "" : ", ", world_count,
                           world_count == 1 ? "world" : "worlds");
        first = false;
    }
    if (cell_count > 0 && offset < (int)sizeof(info_strip_selection_text))
    {
        snprintf(info_strip_selection_text + offset,
                 sizeof(info_strip_selection_text) - (size_t)offset,
                 "%s%d %s", first ? "" : ", ", cell_count,
                 cell_count == 1 ? "cell" : "cells");
    }

    return (BindingValue){ .type = BIND_STRING, .as.s = info_strip_selection_text };
}

// Compose the ROOT cursor-coordinate label. Always present: converts the last captured pointer
// pixel into the root world's frame via ResolvePixelToWorldFrame and reports "Root: (x, y)" with
// one decimal per axis. When the pointer is outside the game viewport the strip shows "Root: -"
// (dash) so no stale coordinate is displayed, mirroring the child field's empty convention.
// Returns a BIND_STRING value pointing at the panel-owned buffer; the key is unused.
static BindingValue InfoStrip_QueryRootCoords(int key)
{
    (void)key;

    if (!info_strip_pointer_in_view)
    {
        // Pointer not over the world view: show a dash rather than a stale coordinate.
        snprintf(info_strip_rootcoords_text, sizeof(info_strip_rootcoords_text), "Root: -");
        return (BindingValue){ .type = BIND_STRING, .as.s = info_strip_rootcoords_text };
    }

    // Pixel -> root-world frame (universe space), reusing the existing conversion helper.
    Vector2d uni = ResolvePixelToWorldFrame(&G_Universe.root_world, info_strip_pointer_pixel);
    snprintf(info_strip_rootcoords_text, sizeof(info_strip_rootcoords_text),
             "Root: (%.1f, %.1f)", (double)uni.x, (double)uni.y);
    return (BindingValue){ .type = BIND_STRING, .as.s = info_strip_rootcoords_text };
}

// Compose the CHILD cursor-coordinate label. Shown only when the pointer hovers a nested/child
// world (same detection the hover field uses via Universe_FindWorldAt): over a nested world the
// pointer is converted into THAT world's local frame and reported as "Child: (x, y)". Over the
// root world or empty space it shows "Child: -". Sentinel normalisation matches the resolver:
// Universe_FindWorldAt returns a nested index in 0..world_count-1, or -1 for the none/root case;
// only a valid nested index populates the coordinates, so -1 (and the root) both render the dash.
// The current/selected world is deliberately NOT used as a fallback. Returns a BIND_STRING value
// pointing at the panel-owned buffer; the key is unused.
static BindingValue InfoStrip_QueryChildCoords(int key)
{
    (void)key;

    if (!info_strip_pointer_in_view)
    {
        // Pointer not over the world view: nothing to report for the child frame.
        snprintf(info_strip_childcoords_text, sizeof(info_strip_childcoords_text), "Child: -");
        return (BindingValue){ .type = BIND_STRING, .as.s = info_strip_childcoords_text };
    }

    // Resolve which world sits under the pointer, using the SAME detection the hover field uses:
    // pixel -> root frame, then Universe_FindWorldAt to find any nested world at that point.
    Vector2d uni = ResolvePixelToWorldFrame(&G_Universe.root_world, info_strip_pointer_pixel);
    int idx = Universe_FindWorldAt(&G_Universe, uni);

    // Only a confirmed nested index (0..world_count-1) yields a child-frame coordinate; the -1
    // none case and the root fall through to the dash.
    if (idx >= 0 && idx < Universe_GetWorldCount(&G_Universe))
    {
        World2d *child_world = Universe_GetWorld(&G_Universe, idx);
        // Convert the SAME pointer pixel into the nested world's local frame.
        Vector2d child = ResolvePixelToWorldFrame(child_world, info_strip_pointer_pixel);
        snprintf(info_strip_childcoords_text, sizeof(info_strip_childcoords_text),
                 "Child: (%.1f, %.1f)", (double)child.x, (double)child.y);
    }
    else
    {
        // Root world or empty space: no child frame to report.
        snprintf(info_strip_childcoords_text, sizeof(info_strip_childcoords_text), "Child: -");
    }

    return (BindingValue){ .type = BIND_STRING, .as.s = info_strip_childcoords_text };
}

// Compose the ACTIVE PAINT tile-type label. Read-only: reports the UIState active tile type so the
// user always knows what a paint click will stamp. Returns a BIND_STRING value pointing at the
// panel-owned buffer; the key is unused.
static BindingValue InfoStrip_QueryActiveTile(int key)
{
    (void)key;
    snprintf(info_strip_tile_text, sizeof(info_strip_tile_text), "Tile: %d",
             UIState_GetActiveTileType());
    return (BindingValue){ .type = BIND_STRING, .as.s = info_strip_tile_text };
}

// ============================================================================
// UI loader resolvers (application-specific)
// ============================================================================

// Resolve the strip's binding names to query sources returning BIND_STRING values. Five names
// are recognised: "infostrip.hover", "infostrip.selection", "infostrip.rootcoords",
// "infostrip.childcoords" and "infostrip.tile"; anything else is unresolved.
static UIBinding InfoStrip_ResolveBinding(const char *binding_string, UILoaderContext *ctx)
{
    (void)ctx;

    if (!binding_string)
    {
        return (UIBinding){ .kind = UI_BIND_SRC_NONE };
    }

    if (!strcmp(binding_string, "infostrip.hover"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = InfoStrip_QueryHoverLabel,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }

    if (!strcmp(binding_string, "infostrip.selection"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = InfoStrip_QuerySelectionLabel,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }

    if (!strcmp(binding_string, "infostrip.rootcoords"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = InfoStrip_QueryRootCoords,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }

    if (!strcmp(binding_string, "infostrip.childcoords"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = InfoStrip_QueryChildCoords,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }

    if (!strcmp(binding_string, "infostrip.tile"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = InfoStrip_QueryActiveTile,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }

    return (UIBinding){ .kind = UI_BIND_SRC_NONE };
}

// The strip has no command buttons; every action name resolves to none.
static int InfoStrip_ResolveCommand(const char *cmd_string, UILoaderContext *ctx)
{
    (void)cmd_string;
    (void)ctx;
    return 0;
}

// ============================================================================
// Per-frame hover update (read-only)
// ============================================================================

// Recompute the hovered TargetRef from the pointer position. It only READS
// input->pointer_position, converts it to the universe root frame with ResolvePixelToWorldFrame
// (as the diagnostics hover probe does), and resolves a target. It never consumes input and is
// not part of the route-claim protocol.
void InfoStrip_UpdateHover(const InputFrame *input)
{
    if (!input)
    {
        // No input this frame: the coord shims must not report a stale pixel.
        info_strip_pointer_in_view = false;
        return;
    }

    // Capture the pointer pixel for the coord shims, and compute the viewport gate ONCE so the
    // hover ref and the coord fields share a single source of truth for "pointer in view".
    info_strip_pointer_pixel = input->pointer_position;
    info_strip_pointer_in_view =
        ViewportRegion_ContainsPixel(&game_viewport, input->pointer_position);

    // Gate on the game viewport: only the world view should contribute a hover target. When the
    // pointer is over a UI panel or outside the world view entirely, report "nothing" rather
    // than narrating the world content occluded beneath the panel.
    if (!info_strip_pointer_in_view)
    {
        info_strip_hover_ref = (TargetRef){0};
        info_strip_hover_ref.kind = TARGET_KIND_NONE;
        return;
    }

    // Pixel -> universe (root-world-local) point, matching the existing hover/pick convention.
    Vector2d universe_point = ResolvePixelToWorldFrame(&G_Universe.root_world, input->pointer_position);
    info_strip_hover_ref = Target_ResolveAtPoint(&G_Universe, universe_point);
}

// ============================================================================
// Lifecycle
// ============================================================================

// Load info_strip.xml and mount it as a single-view strip against the game viewport.
void InitInfoStrip(void)
{
    if (info_strip)
    {
        return;
    }

    // 1) Load the authoritative markup with the strip's own resolvers (no view-type resolver:
    //    the strip is a single Container root, not a multi-view ViewHost).
    UIElement *root = UILoader_LoadFromFileWithResolvers(
        "C:\\Projects\\raylib-testing\\src\\engine\\ui\\components\\info_strip.xml",
        &ui_default_palette,
        InfoStrip_ResolveBinding,
        InfoStrip_ResolveCommand,
        NULL,
        NULL);

    if (!root)
    {
        // XML is the single authoritative source; a load failure is an asset/build error.
        LOG_ERROR("InitInfoStrip: failed to load info_strip.xml; info strip will not render");
        return;
    }

    // 2) Build the host against the dedicated info-strip viewport band (carved from the top of
    //    the central column by InitViewportLayout), build its real root, and re-parent the XML
    //    container under it (NEVER info_strip->root = root - that is the dangling-root bug).
    //    Mounting against info_strip_viewport (not game_viewport) keeps the strip in its own
    //    reserved screen region so it never overlaps the world render area.
    info_strip = ViewHostSystem_Create(&info_strip_viewport, 1.0f, ZERO_VECTOR_2D,
                                       &ui_default_palette, ui_standard_stack_spacing);
    if (!info_strip)
    {
        DisposeUIElement(root);
        return;
    }

    ViewHostSystem_InitRoot(info_strip);     // Real UI_ELEMENT_ROOT + space + seed_box.
    AddElementToTree(root, info_strip->root); // Re-parent XML container under the real root.

    // 3) Final layout update over the host's valid coordinate space.
    UpdateUISpace(info_strip->root, info_strip->seed_box);
}

// Draw the info strip (null-guarded so a load failure degrades to no rendering).
void DrawInfoStrip(void)
{
    if (!info_strip)
    {
        return;
    }

    ViewHostSystem_Draw(info_strip);
}

// Destroy the info strip and clear its cached references.
void DestroyInfoStrip(void)
{
    ViewHostSystem *panel = info_strip;
    info_strip = NULL;
    ViewHostSystem_Destroy(panel);

    info_strip_hover_ref = (TargetRef){0};
    info_strip_hover_text[0] = '\0';
    info_strip_selection_text[0] = '\0';

    // Reset the coord shim state so a later re-init starts clean (no stale pixel/label).
    info_strip_rootcoords_text[0] = '\0';
    info_strip_childcoords_text[0] = '\0';
    info_strip_tile_text[0] = '\0';
    info_strip_pointer_in_view = false;
    info_strip_pointer_pixel = (Vector2d){0};
}
