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

// ============================================================================
// UI loader resolvers (application-specific)
// ============================================================================

// Resolve the strip's binding names to query sources returning BIND_STRING values. Two names
// are recognised: "infostrip.hover" and "infostrip.selection"; anything else is unresolved.
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
        return;
    }

    // Gate on the game viewport: only the world view should contribute a hover target. When the
    // pointer is over a UI panel or outside the world view entirely, report "nothing" rather
    // than narrating the world content occluded beneath the panel.
    if (!ViewportRegion_ContainsPixel(&game_viewport, input->pointer_position))
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
}
