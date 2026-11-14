#include "system/ui/panel_shared.h"
#include <stdio.h>
#include "raylib.h"                      // DrawRectangleRec (header rule)
#include "system/draw_primitives.h"      // ToRaylibColor
#include "ui/ui_input.h"                 // ResetUIInputState (drop focus when fields lock)
#include "ui/cfont.h"                    // FONT_LARGE_WITH_COLOUR / FONT_MEDIUM_WITH_COLOUR
#include "system/ui/ui_loader.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "world/universe.h"
#include "entities/entity_registry.h"

// ============================================================================
// Flag families (shared read / write / walk / query)
// ============================================================================
//
// Each FlagFamily names one bitmask on a selection member (entity, world or cell). The generic
// refresh walk composes "<label>: ON/OFF" through PanelShared_QueryFlag and the click handler
// toggles through the same accessors, so adding a family means one enum value and one case in
// each accessor. Members of the wrong kind for a family are skipped, which is what lets one
// walker serve a mixed selection and both panels.

// Resolve an ENTITY ref to its live entity, or NULL for a wrong-kind or stale ref. Re-resolved on
// every call; pointers are never cached across frames.
Newtonoid2d *PanelShared_ResolveEntityRef(TargetRef ref)
{
    if (ref.kind != TARGET_KIND_ENTITY)
    {
        return NULL;
    }
    return Universe_GetEntityByID(&G_Universe, ref.as.entity, NULL);
}

// Collapse a tally to the toggle state: 0 off (or nothing applicable), 1 all on, otherwise a
// packed "<on>/<applicable>" count (BIND_TOGGLE_MIXED_PACK) when the applicable targets disagree.
// Shared by flag and component queries so the display and the click decision always agree.
int PanelShared_TriState(int applicable, int on)
{
    if (applicable == 0 || on == 0)
    {
        return 0;
    }
    return (on == applicable) ? 1 : BIND_TOGGLE_MIXED_PACK(on, applicable);
}

// Read the bitmask a family targets on one member. Returns false when the ref is the wrong kind
// for the family (entity families need ENTITY, WORLD needs WORLD, CELL needs CELL) or no longer
// resolves, which is how a mixed selection skips inapplicable members. Guard is `!= NULL` only
// (no id check), so a non-NULL object with an invalid id still displays its real flags.
bool PanelShared_ReadFlags(TargetRef ref, FlagFamily family, uint32_t *out)
{
    switch (family)
    {
    case FLAG_FAMILY_ROLE:
    case FLAG_FAMILY_CAPABILITY:
    case FLAG_FAMILY_CONSTRAINT:
    case FLAG_FAMILY_STATUS:
    case FLAG_FAMILY_COLLISION_MASK:
    {
        Newtonoid2d *o = PanelShared_ResolveEntityRef(ref);
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

// Store a bitmask into the field a family targets on one member. Returns false when the ref is
// the wrong kind or stale (same rules as PanelShared_ReadFlags).
bool PanelShared_WriteFlags(TargetRef ref, FlagFamily family, uint32_t value)
{
    switch (family)
    {
    case FLAG_FAMILY_ROLE:
    case FLAG_FAMILY_CAPABILITY:
    case FLAG_FAMILY_CONSTRAINT:
    case FLAG_FAMILY_STATUS:
    case FLAG_FAMILY_COLLISION_MASK:
    {
        Newtonoid2d *o = PanelShared_ResolveEntityRef(ref);
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

// Set `bit` of `family` on one target to `value`. Holds the ROLE exclusivity and SLEEPING
// (sleep/wake keeps the body's physics state consistent) special cases in one place.
static void PanelShared_ApplyFlagToTarget(TargetRef ref, FlagFamily family, uint32_t bit, bool value)
{
    uint32_t flags = 0;
    if (!PanelShared_ReadFlags(ref, family, &flags))
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
        PanelShared_WriteFlags(ref, family, value ? ((flags & ~PANEL_SHARED_ROLE_MASK) | bit) : (flags & ~bit));
    }
    else if (family == FLAG_FAMILY_STATUS && bit == ENTITY_STATUS_FLAG_SLEEPING)
    {
        // A status read succeeded, so the entity resolves.
        Newtonoid2d *object = PanelShared_ResolveEntityRef(ref);
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
        PanelShared_WriteFlags(ref, family, value ? (flags | bit) : (flags & ~bit));
    }
}

// Tally (and optionally apply to) one target. Inapplicable targets are not counted.
static void PanelShared_VisitFlagTarget(FlagTally *tally, TargetRef ref, FlagFamily family,
                                        uint32_t bit, bool apply, bool value)
{
    uint32_t flags = 0;
    if (!PanelShared_ReadFlags(ref, family, &flags))
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
        PanelShared_ApplyFlagToTarget(ref, family, bit, value);
    }
}

// Visit every applicable target of a family in the SelectionSet, tallying the bit state; when
// `apply` is true also writes `value` to each. The only place that enumerates and filters
// targets for flags. Re-resolves every ref (stale ones are skipped) and does not allocate.
// World flags fall back to the context world when the set holds neither a world nor a cell (an
// entity or nothing selected), so the World Manager can still edit the world being shown.
FlagTally PanelShared_WalkFlag(FlagFamily family, uint32_t bit, bool apply, bool value)
{
    FlagTally tally = {0};
    bool saw_world_or_cell = false;

    int count = SelectionSet_Count();
    for (int i = 0; i < count; i++)
    {
        TargetRef ref;
        if (!SelectionSet_Get(i, &ref))
        {
            continue;
        }
        saw_world_or_cell = saw_world_or_cell || ref.kind == TARGET_KIND_WORLD || ref.kind == TARGET_KIND_CELL;
        PanelShared_VisitFlagTarget(&tally, ref, family, bit, apply, value);
    }

    if (family == FLAG_FAMILY_WORLD && !saw_world_or_cell)
    {
        TargetRef ref = { .kind = TARGET_KIND_WORLD, .as.world_index = UIState_GetContextWorldIndex() };
        PanelShared_VisitFlagTarget(&tally, ref, family, bit, apply, value);
    }
    return tally;
}

// Is-set query shared by every flag family; returns BIND_INT 0 off, 1 on, BIND_TOGGLE_MIXED when
// the selected targets disagree. Key from PANEL_FLAG_KEY.
BindingValue PanelShared_QueryFlag(int key)
{
    FlagTally t = PanelShared_WalkFlag((FlagFamily)(key >> 16), (uint32_t)(key & 0xFFFF), false, false);
    return (BindingValue){ .type = BIND_INT, .as.i = PanelShared_TriState(t.applicable, t.on) };
}

// Click a flag across the whole selection. If the flag is ON for every applicable target it is
// cleared on all; otherwise it is set on all (never a per-object XOR). ROLE is exclusive, so a
// click always makes every applicable entity exactly that role. The "<label>: ON/OFF" text is
// refreshed by the per-frame binding walk, so no dirty mark is needed here.
void PanelShared_HandleFlagClick(UIElement *button)
{
    const FlagButton *spec = (const FlagButton *)button->interaction_user_data;
    if (!spec)
    {
        return;
    }

    FlagTally tally = PanelShared_WalkFlag(spec->family, spec->flag, false, false);
    if (tally.applicable == 0)
    {
        return;
    }

    bool value = (spec->family == FLAG_FAMILY_ROLE) ? true : (tally.on != tally.applicable);
    PanelShared_WalkFlag(spec->family, spec->flag, true, value);
}

// Wire a family of flag buttons acquired from the loaded tree: store the element into each
// buttons[i].button, attach the click handler + user_data, and attach the query SOURCE Binding
// (keyed on the flag bit) so the generic refresh walk composes "<label>: ON/OFF". A missing id is
// logged and left NULL so the feature degrades to an inert button.
void PanelShared_WireFlags(UIElement *root, FlagButton *buttons, const char **ids, size_t count,
                           FlagFamily family)
{
    for (size_t i = 0; i < count; i++)
    {
        buttons[i].family = family;
        buttons[i].button = UILoader_FindById(root, ids[i]);
        if (!buttons[i].button)
        {
            LOG_ERROR("PanelShared: flag button id '%s' not found in markup", ids[i]);
            continue;
        }
        buttons[i].button->on_click = PanelShared_HandleFlagClick;
        buttons[i].button->interaction_user_data = (void *)&buttons[i];

        // Query SOURCE restores the "<label>: ON/OFF" display (sink stays NONE; the handler
        // owns the write path).
        Binding b = {
            .source = { .kind = BIND_SRC_QUERY, .value_type = BIND_INT,
                        .query = PanelShared_QueryFlag,
                        .query_key = PANEL_FLAG_KEY(family, buttons[i].flag) },
        };
        buttons[i].button->data.button.binding = Binding_Create(b);
    }
}

// ============================================================================
// Header block
// ============================================================================

// Header on_draw hook: a 2 px coral rule along the bottom edge of the header box (the palette
// container border colour). The pixel scale is recovered from the box (screen height / UI height),
// so the rule stays 2 UI pixels at every UI scale.
static void PanelShared_DrawHeaderRule(UIElement *box)
{
    float scale = (box->local_box.dimensions.y > 0.0f)
                      ? box->screen_box.dimensions.y / box->local_box.dimensions.y
                      : 1.0f;
    float thickness = PANEL_HEADER_RULE_PX * scale;
    DrawRectangleRec((Rectangle){box->screen_box.coords.x,
                                 box->screen_box.coords.y + box->screen_box.dimensions.y - thickness,
                                 box->screen_box.dimensions.x, thickness},
                     ToRaylibColor(UIPalette_GetActive()->container_border));
}

// Bind a header block to its markup ids. The title switches to FONT_LARGE. The box is restyled as
// the shared header look: the palette header_fill, no border, and a coral rule along its bottom
// edge (on_draw hook); that fill is captured so an untinted header can be restored. The owning
// host's palette is stored for the tint pass. Missing ids are logged and left NULL.
void PanelShared_HeaderBind(PanelHeader *header, UIElement *root, const UIPalette *palette,
                            const char *box_id, const char *title_id, const char *sub_id)
{
    header->palette = palette ? palette : UIPalette_GetActive();
    header->box = UILoader_FindById(root, box_id);
    header->title = UILoader_FindById(root, title_id);
    header->sub = UILoader_FindById(root, sub_id);
    if (!header->box || !header->title || !header->sub)
    {
        LOG_ERROR("PanelShared: header ids '%s' / '%s' / '%s' not all found in markup",
                  box_id, title_id, sub_id);
    }
    if (header->box)
    {
        header->box->colour_fill = UIPalette_GetRoleColours(header->palette, UI_ROLE_HEADER).fill;
        header->box->colour_border = COLOURLESS_RGBA;
        header->box->on_draw = PanelShared_DrawHeaderRule;
        header->default_fill = header->box->colour_fill;
    }
    if (header->title)
    {
        // Titles use FONT_LARGE; the tint pass re-applies it with the right ink colour.
        header->title->data.textbox.font = FONT_LARGE_WITH_COLOUR(header->palette->header_text);
    }
}

// Tint (or restore) the header box with existing palette colours: amber button_fill when several
// targets are selected, otherwise the default header fill. Only the fill swaps; the ink is the
// header_text colour in both states (dark reads on both amber and the lighter sage) and the coral
// bottom rule is kept.
void PanelShared_HeaderSetTint(PanelHeader *header, bool tinted)
{
    if (!header->box)
    {
        return;
    }

    ColourRgba ink = header->palette->header_text;
    header->box->colour_fill = tinted ? UIPalette_GetRoleColours(header->palette, UI_ROLE_ACTIVE).fill : header->default_fill;
    if (header->title)
    {
        header->title->data.textbox.font = FONT_LARGE_WITH_COLOUR(ink);
    }
    if (header->sub)
    {
        header->sub->data.textbox.font = FONT_MEDIUM_WITH_COLOUR(ink);
    }
}

// ============================================================================
// Field lock, buttons, views, selection helpers
// ============================================================================

// Lock (read-only) or unlock editable fields. Value edits are not bulk-editable, so in multi-select
// they show the primary's values and ignore input. IsEditableTextbox is the single choke point for
// input and keys on the element type, so swapping SAFE_IO <-> O is enough. Only acts when the state
// (held in the caller's `cached`) flips.
void PanelShared_SetFieldsLocked(UIElement **fields, size_t count, bool *cached, bool locked)
{
    if (locked == *cached)
    {
        return;
    }
    *cached = locked;

    for (size_t i = 0; i < count; i++)
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

// Set a button's authored and drawn text together (chips and DELETE carry no binding).
void PanelShared_SetButtonText(UIElement *button, const char *text)
{
    if (!button)
    {
        return;
    }
    snprintf(button->data.button.text.string, sizeof(button->data.button.text.string), "%s", text);
    snprintf(button->data.button.display_text.string, sizeof(button->data.button.display_text.string), "%s", text);
}

// Show exactly one of a host's views by index (used for tab-less views); -1 shows none. Mirrors the
// host's own single-view activation.
void PanelShared_ShowOnlyView(ViewHostSystem *host, int view_index)
{
    if (!host)
    {
        return;
    }
    for (int i = 0; i < host->views.count; i++)
    {
        View *view = *((View **)LArray_Get(&host->views, i));
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

// Find the last SelectionSet member of `kind` (the "primary of that kind", consistent with
// SelectionSet_Primary being the last added). Returns false when the set holds none.
bool PanelShared_PrimaryOfKind(TargetKind kind, TargetRef *out)
{
    bool found = false;
    int count = SelectionSet_Count();
    for (int i = 0; i < count; i++)
    {
        TargetRef ref;
        if (SelectionSet_Get(i, &ref) && ref.kind == kind)
        {
            if (out)
            {
                *out = ref;
            }
            found = true;
        }
    }
    return found;
}

// Count the SelectionSet members of `kind` in one pass (no allocation).
int PanelShared_CountKind(TargetKind kind)
{
    int total = 0;
    int count = SelectionSet_Count();
    for (int i = 0; i < count; i++)
    {
        TargetRef ref;
        if (SelectionSet_Get(i, &ref) && ref.kind == kind)
        {
            total++;
        }
    }
    return total;
}
