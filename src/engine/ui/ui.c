/**********************************************************************************************

 **********************************************************************************************/
#include <stdio.h>
#include <stdarg.h> // UIHint_AppendSegment (variadic)
#include <string.h> // strlen
#include <float.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "ui/ui.h"
#include "system/ui_system.h" // UIPalette toggle fills + ui_default_palette for toggle-button fill
#include "raylib.h"

// Stacked-layout statistics for a FLOW_STACKED parent. Declared up here (rather than
// beside its collector) so the MEASURE pass can derive the stacked-fill remainder from the
// SAME helpers the DISTRIBUTE pass uses while keeping declaration-before-use in C.
typedef struct StackedLayoutStats
{
    int fill_count;
    int child_count;
    float occupied_height;
} StackedLayoutStats;

// Forward declarations for the shared stacked-layout helpers. The MEASURE pass
// (MeasureElementContent) is defined before these helpers but must call them so both passes
// derive the stacked-fill remainder from one source of truth (see stackwrap-unify-design.md).
static Vector2d ResolveGapStep(Layout layout, Vector2d content_area_local, OffsetMode offset_mode);
static StackedLayoutStats CollectStackedLayoutStats(const UIElement *parent, float content_area_h);
static float ResolveStackedFillHeightPerChild(StackedLayoutStats stats, float content_area_h, float gap_y);
static float ResolveStackedChildReceivedHeight(const UIElement *child,
                                               float parent_content_height,
                                               float fill_height_per_child);

static Vector2d GetContentArea(Vector2d dimensions, Vector2d padding)
{
    float content_w = fmaxf(0.0f, dimensions.x - (2.0f * padding.x));
    float content_h = fmaxf(0.0f, dimensions.y - (2.0f * padding.y));
    return (Vector2d){content_w, content_h};
}

// Advance through the sibling list while ignoring disabled layout children.
static UIElement *GetNextEnabledChild(UIElement *child)
{
    while (child && !child->is_enabled)
    {
        child = child->next_sibling;
    }

    return child;
}

#define ForEachEnabledChild(parent, child)                                                   \
    for (UIElement * (child) = GetNextEnabledChild((parent) ? (parent)->first_child : NULL); \
         (child) != NULL;                                                                    \
         (child) = GetNextEnabledChild((child)->next_sibling))

// Resolve the FINAL extent of a single axis from its per-axis mode. Used by the
// distribute / box-resolution sites where the measured content is trustworthy.
//   mode               : this axis's AxisSizeMode (width_mode for x, height_mode for y).
//   authored_dimension : Size.dimensions on this axis (literal for FIXED, 0..1 for PERCENT,
//                        MAX cap for HUG when > 0, ignored for FILL).
//   parent_extent      : parent content extent available on this axis.
//   measured_content   : this element's measured content extent on this axis. HUG only.
// Returns the resolved extent on this axis in world units, never negative.
static float ResolveAxisExtent(AxisSizeMode mode, float authored_dimension,
                               float parent_extent, float measured_content)
{
    switch (mode)
    {
    case AXIS_PERCENT:
        return fmaxf(0.0f, parent_extent * authored_dimension);
    case AXIS_FILL:
        return fmaxf(0.0f, parent_extent);
    case AXIS_HUG:
        if (authored_dimension > 0.0f) // > 0 == MAX cap
            return fminf(fmaxf(0.0f, measured_content), authored_dimension);
        return fmaxf(0.0f, measured_content); // uncapped
    case AXIS_FIXED:
    default:
        return fmaxf(0.0f, authored_dimension);
    }
}

// Resolve the AVAILABLE extent of a single axis during the MEASURE pass. Used by the two
// ResolveMeasurement* sites where a HUG axis has no trustworthy measured content yet, so
// HUG and FILL both fall back to the parent's available extent (reproducing the old
// code, where every content-driven/fill mode returned parent_content_* during measure).
static float ResolveMeasurementAxisAvailable(AxisSizeMode mode, float authored_dimension,
                                             float parent_extent)
{
    switch (mode)
    {
    case AXIS_PERCENT:
        return fmaxf(0.0f, parent_extent * authored_dimension);
    case AXIS_FILL:
    case AXIS_HUG:
        return fmaxf(0.0f, parent_extent); // availability, not final content size
    case AXIS_FIXED:
    default:
        return fmaxf(0.0f, authored_dimension);
    }
}

// Resolve the width available to an element during the measurement pass.
static float ResolveMeasurementWidth(const UIElement *element, float parent_content_width)
{
    if (!element)
    {
        return 0.0f;
    }

    // Width axis only.
    float avail = ResolveMeasurementAxisAvailable(element->size.width_mode,
                                                  element->size.dimensions.x,
                                                  parent_content_width);
    // Preserve the old SIZE_CONTENT_MAX width behaviour: an authored hug cap clamps the
    // available width during measurement too (the height site never did this).
    if (element->size.width_mode == AXIS_HUG && element->size.dimensions.x > 0.0f)
    {
        avail = fminf(avail, element->size.dimensions.x);
    }
    return avail;
}

// Resolve the height available to an element during the measurement pass.
// Mirrors ResolveMeasurementWidth: FILL/HUG take the parent's available height (used only
// as a wrap upper bound, never to force a size). No HUG-cap post-clamp here, matching the
// old code where the height axis never clamped availability.
static float ResolveMeasurementHeight(const UIElement *element, float parent_content_height)
{
    if (!element)
    {
        return 0.0f;
    }

    // Height axis only.
    return ResolveMeasurementAxisAvailable(element->size.height_mode,
                                           element->size.dimensions.y,
                                           parent_content_height);
}

// Resolve the measured size used when arranging a child in its parent's layout. Width is
// resolved from width_mode via the helper; height reproduces the pre-existing quirky table
// (a PERCENT height returns the RAW dimensions.y here, unscaled — see note below).
static Vector2d ResolveMeasuredChildSize(const UIElement *child, float parent_content_width)
{
    if (!child)
    {
        return ZERO_VECTOR_2D;
    }

    // Width axis: FILL -> parent width; PERCENT -> parent*frac; HUG -> measured; FIXED -> literal.
    float x = ResolveAxisExtent(child->size.width_mode, child->size.dimensions.x,
                                parent_content_width, child->measured_content_size.x);

    // Height axis: inlined to preserve the historic asymmetry. PERCENT returns the RAW
    // authored dimensions.y UNSCALED (pre-existing quirk; cf. old SIZE_PERCENT arm which
    // returned child->size.dimensions.y here). FILL/HUG return measured content; the final
    // filled height is applied later at arrange time. FIXED returns the literal.
    float y;
    switch (child->size.height_mode)
    {
    case AXIS_PERCENT:
        y = child->size.dimensions.y; // UNSCALED quirk — do NOT multiply by parent height
        break;
    case AXIS_FILL:
    case AXIS_HUG:
        y = child->measured_content_size.y;
        break;
    case AXIS_FIXED:
    default:
        y = child->size.dimensions.y;
        break;
    }

    return (Vector2d){x, y};
}

// Shared wrapping measure. Children fill "lines" along the main axis (x for inline wrap, y for
// stacked wrap); a line closes, and the next starts further along the cross axis, once the next
// child would overflow the main-axis limit. The measured size is the longest line along the main
// axis and the sum of line cross extents plus a cross gap between consecutive lines. Written in
// main/cross terms (mirroring DistributeChildrenWrap) so the two orientations cannot drift apart.
static Vector2d MeasureWrapContent(UIElement *element, float content_width, float content_height,
                                   bool stacked)
{
    if (!element)
    {
        return ZERO_VECTOR_2D;
    }

    float main_gap = stacked ? element->child_spacing.gap.y : element->child_spacing.gap.x;
    float cross_gap = stacked ? element->child_spacing.gap.x : element->child_spacing.gap.y;

    // Main-axis limit at which lines wrap.
    float main_limit = FLT_MAX;
    if (!stacked)
    {
        // Inline rows wrap at the available content width.
        main_limit = content_width > 0.0f ? content_width : FLT_MAX;
    }
    else if (element->size.height_mode == AXIS_HUG && element->size.dimensions.y > 0.0f)
    {
        // A capped HUG height uses its authored height (known at measure time).
        main_limit = fmaxf(0.0f, element->size.dimensions.y - (2.0f * element->padding.y));
    }
    else if (element->size.height_mode == AXIS_FILL && content_height > 0.0f && content_height < FLT_MAX)
    {
        // A FILL height uses the content height handed down from the parent, so measurement wraps
        // into the same number of columns the distribute pass produces against the real box.
        main_limit = fmaxf(0.0f, content_height);
    }
    // Any other stacked mode leaves the limit unbounded (single column).

    float measured_main = 0.0f;  // Longest line along the main axis.
    float measured_cross = 0.0f; // Sum of closed lines' cross extents plus gaps.
    float line_main = 0.0f;
    float line_cross = 0.0f;
    int closed_lines = 0;

    ForEachEnabledChild(element, child)
    {
        Vector2d child_size = ResolveMeasuredChildSize(child, content_width);
        float child_main = stacked ? child_size.y : child_size.x;
        float child_cross = stacked ? child_size.x : child_size.y;

        // Close the current line when this child would overflow a non-empty line.
        if (line_main > 0.0f && line_main + main_gap + child_main > main_limit)
        {
            measured_main = fmaxf(measured_main, line_main);
            measured_cross += line_cross + (closed_lines > 0 ? cross_gap : 0.0f);
            closed_lines++;
            line_main = 0.0f;
            line_cross = 0.0f;
        }

        line_main = (line_main > 0.0f ? line_main + main_gap : line_main) + child_main;
        line_cross = fmaxf(line_cross, child_cross);
    }

    // Close the final line.
    if (line_main > 0.0f || line_cross > 0.0f)
    {
        measured_main = fmaxf(measured_main, line_main);
        measured_cross += line_cross + (closed_lines > 0 ? cross_gap : 0.0f);
    }

    return stacked ? (Vector2d){measured_cross, measured_main}
                   : (Vector2d){measured_main, measured_cross};
}

// Measure a single-row text run's pixel width the SAME way the renderer draws it
// (ConsumeTextRow in ui_renderer.c): the first glyph occupies a full cell (glyph_cell_width,
// the glyph bitmap width) and every subsequent glyph steps by glyph_advance (cell width minus
// inter-glyph spacing). Using advance for the final glyph too would under-measure the run by
// (cell_width - advance), leaving the last character's cell poking past its box.
// Returns 0 for an empty string or a degenerate font (never a negative width).
static float MeasureTextRunPixels(const char *text, const Bitmap_Font *font)
{
    size_t glyph_count = text ? strlen(text) : 0;
    if (glyph_count == 0)
    {
        return 0.0f;
    }

    int glyph_advance = BitmapFont_GetGlyphAdvance(font);
    int glyph_cell_width = BitmapFont_GetGlyphCellWidth(font);
    if (glyph_advance <= 0 || glyph_cell_width <= 0)
    {
        return 0.0f;
    }

    // First glyph = full cell; remaining (glyph_count - 1) glyphs = one advance each.
    return (float)glyph_cell_width + (float)(glyph_count - 1) * (float)glyph_advance;
}

// Measure a leaf label's content extent from its text, in UI pixels (the layout unit).
//
// A childless UI_ELEMENT_LABEL has no measured geometry to hug (ForEachEnabledChild yields
// nothing), so the generic measure path would collapse its width to just padding and the
// title would vanish. This helper gives the label a real width from its text run. The height
// axis is left to the generic path (the title keeps its authored-fixed height), so only the
// width is text-driven.
//
//   element : the leaf label to measure. Must be a non-NULL UI_ELEMENT_LABEL.
//
// Returns a measured content size whose x is the text width plus this element's horizontal
// padding, and whose y is the generic padding-only height.
static Vector2d MeasureLeafLabelContent(const UIElement *element)
{
    // Padding-only fallback (today's leaf result): used when no usable text width exists.
    Vector2d fallback = {element->padding.x * 2.0f, element->padding.y * 2.0f};

    // Pixel width of the authored label text (empty label / degenerate font -> 0).
    float text_pixels = MeasureTextRunPixels(element->data.label.text.string,
                                             &element->data.label.font);
    if (text_pixels <= 0.0f)
    {
        return fallback; // Empty label contributes only its padding box.
    }

    // Fonts are measured at their authored scale, which is in UI pixels (the renderer multiplies
    // by the UI pixel scale when drawing), so the run width is already in layout units.
    float measured_x = text_pixels + (element->padding.x * 2.0f);

    // Height stays the padding-only contribution; the title's authored-fixed height is
    // resolved unchanged by the generic box-resolution path.
    return (Vector2d){measured_x, element->padding.y * 2.0f};
}

// Give every text-field row directly under `parent` one shared label column, sized to the
// longest label among those rows (the "longest label" rule of style v2), so no label is
// truncated and every input starts at the same x within a section.
//
// Rows are UI_ELEMENT_TEXTFIELD elements built by CreateLabeledFieldElement: first child =
// label (FIXED width column), second child = input (FILL width at a FIXED x offset). All rows
// are considered, including disabled ones, so the column does not jump when a hidden row
// appears. The column is the widest run + 2 px, converted to UI units with the owning root's
// ratio and clamped to 60 % of `content_width` so the input always keeps at least 40 %.
// Rows built with the legacy percent split (label width PERCENT) are left untouched.
// Recoverable no-ops: no rows, no root / unresolved pixel box (frame 0: the constructor's
// fallback width applies), or a malformed row missing a child.
static void AlignTextFieldLabelColumns(UIElement *parent, float content_width)
{
    // Pass 1: find the widest label run (pixels) among the direct text-field rows.
    float widest_px = 0.0f;
    bool has_rows = false;
    for (UIElement *row = parent->first_child; row; row = row->next_sibling)
    {
        if (row->type != UI_ELEMENT_TEXTFIELD)
        {
            continue;
        }
        UIElement *label = row->first_child;
        if (!label || !label->next_sibling || label->type != UI_ELEMENT_LABEL ||
            label->size.width_mode != AXIS_FIXED)
        {
            continue; // malformed or legacy percent-split row
        }
        has_rows = true;
        widest_px = fmaxf(widest_px, MeasureTextRunPixels(label->data.label.text.string,
                                                          &label->data.label.font));
    }
    if (!has_rows || widest_px <= 0.0f)
    {
        return;
    }

    // Text runs are measured in UI pixels (the layout unit). +2 px keeps the last glyph's cell
    // clear of the input's border.
    float label_units = widest_px + 2.0f;
    if (content_width > 0.0f)
    {
        label_units = fminf(label_units, 0.6f * content_width);
    }

    // Pass 2: apply the shared column width and move each input to start right after it.
    for (UIElement *row = parent->first_child; row; row = row->next_sibling)
    {
        if (row->type != UI_ELEMENT_TEXTFIELD)
        {
            continue;
        }
        UIElement *label = row->first_child;
        UIElement *input = label ? label->next_sibling : NULL;
        if (!label || !input || label->type != UI_ELEMENT_LABEL ||
            label->size.width_mode != AXIS_FIXED)
        {
            continue;
        }
        label->size.dimensions.x = label_units;
        input->authored_offset.offset.x = label_units;
        input->resolved_offset.offset.x = label_units;
    }
}

// Measure a subtree using the width (and available height) handed down by its parent.
// The height flows alongside the width so stacked-wrap sections can wrap columns at the
// same boundary the distribute pass uses, instead of always measuring a single column.
static Vector2d MeasureElementContent(UIElement *element, float parent_content_width, float parent_content_height)
{
    if (!element)
    {
        return ZERO_VECTOR_2D;
    }

    // Leaf labels carry no children to hug, so measure their width from their text instead
    // (keeps the shared container title from collapsing to zero width when hugging). Only
    // non-empty, childless labels take this path; every other element is unchanged.
    if (element->type == UI_ELEMENT_LABEL && element->first_child == NULL &&
        element->data.label.text.string[0] != '\0')
    {
        return MeasureLeafLabelContent(element);
    }

    float element_width = ResolveMeasurementWidth(element, parent_content_width);
    float element_height = ResolveMeasurementHeight(element, parent_content_height);
    float content_width = fmaxf(0.0f, element_width - (2.0f * element->padding.x));
    float content_height = fmaxf(0.0f, element_height - (2.0f * element->padding.y));

    // Size this element's text-field label column before its children are measured, so the
    // rows measure and lay out with the final column this frame.
    AlignTextFieldLabelColumns(element, content_width);

    Vector2d measured = ZERO_VECTOR_2D;
    float gap_x = element->child_spacing.gap.x;
    float gap_y = element->child_spacing.gap.y;
    int child_count = 0;

    if (element->child_spacing.flow == FLOW_STACKED)
    {
        // TWO-SUB-PASS MEASURE for stacked parents, so the stacked-fill remainder a
        // both-FILL child receives is derived from CURRENT-frame child measurements and
        // matches the DISTRIBUTE pass exactly (see stackwrap-unify-design.md Finding 1).

        // Sub-pass 1: measure every child with its plain available height (today's
        // behaviour) so each child's measured_content_size is this frame's value. Only
        // NON-both-FILL children (the fixed/HUG siblings, e.g. the selector) contribute to
        // the parent's measured size here; a both-FILL child's size is added in sub-pass 2.
        ForEachEnabledChild(element, child)
        {
            float child_width = ResolveMeasurementWidth(child, content_width);
            float child_height = ResolveMeasurementHeight(child, content_height);
            child->measured_content_size = MeasureElementContent(child, child_width, child_height);

            bool child_is_both_fill =
                child->size.width_mode == AXIS_FILL && child->size.height_mode == AXIS_FILL;
            if (!child_is_both_fill)
            {
                Vector2d child_size = ResolveMeasuredChildSize(child, content_width);
                measured.x = fmaxf(measured.x, child_size.x);
                measured.y += child_size.y;
            }

            child_count++;
        }

        // Derive the single stacked-fill remainder ONCE (O(N)) from the SAME function and
        // inputs DISTRIBUTE uses. The gap MUST be the RESOLVED OFFSET_FIXED step so a
        // PERCENT gap_mode (scaled by the content area) cannot re-diverge the remainder
        // vs DISTRIBUTE, which derives it the same way (see Finding 2). For NONE mode the
        // resolved value equals the raw member, so this is behaviour-preserving today.
        Vector2d measure_gap_step =
            ResolveGapStep(element->child_spacing,
                           (Vector2d){content_width, content_height}, OFFSET_FIXED);
        StackedLayoutStats stats = CollectStackedLayoutStats(element, content_height);
        float stacked_fill_height_per_child =
            ResolveStackedFillHeightPerChild(stats, content_height, measure_gap_step.y);

        // Sub-pass 2: re-measure ONLY both-FILL children against the stacked remainder so a
        // both-FILL child (the view) and its stack_wrap descendants wrap their columns
        // against the exact height the child's box will later resolve to. Each both-FILL
        // child's remainder-based size is added into the parent's measured size EXACTLY
        // once here (sub-pass 1 skipped it), so no child is double-counted.
        ForEachEnabledChild(element, child)
        {
            if (child->size.width_mode == AXIS_FILL && child->size.height_mode == AXIS_FILL)
            {
                float child_width = ResolveMeasurementWidth(child, content_width);
                float child_height = ResolveStackedChildReceivedHeight(child, content_height,
                                                                      stacked_fill_height_per_child);
                child->measured_content_size = MeasureElementContent(child, child_width, child_height);

                Vector2d child_size = ResolveMeasuredChildSize(child, content_width);
                measured.x = fmaxf(measured.x, child_size.x);
                measured.y += child_size.y;
            }
        }
    }
    else
    {
        ForEachEnabledChild(element, child)
        {
            float child_width = ResolveMeasurementWidth(child, content_width);
            float child_height = ResolveMeasurementHeight(child, content_height);
            child->measured_content_size = MeasureElementContent(child, child_width, child_height);
            Vector2d child_size = ResolveMeasuredChildSize(child, content_width);

            if (element->child_spacing.flow == FLOW_INLINE)
            {
                measured.x += child_size.x;
                measured.y = fmaxf(measured.y, child_size.y);
            }
            else if (element->child_spacing.flow == FLOW_INLINE_WRAP)
            {
                // Inline-wrap measurement is repeated below with row boundaries.
                measured = ZERO_VECTOR_2D;
            }
            else if (element->child_spacing.flow == FLOW_STACKED_WRAP)
            {
                // Stacked-wrap measurement is repeated below with column boundaries.
                measured = ZERO_VECTOR_2D;
            }
            else
            {
                measured.x = fmaxf(measured.x, child_size.x);
                measured.y = fmaxf(measured.y, child_size.y);
            }

            child_count++;
        }
    }

    if (element->child_spacing.flow == FLOW_INLINE_WRAP)
    {
        measured = MeasureWrapContent(element, content_width, content_height, false);
    }
    else if (element->child_spacing.flow == FLOW_STACKED_WRAP)
    {
        measured = MeasureWrapContent(element, content_width, content_height, true);
    }
    // else if (element->child_spacing.flow == FLOW_STACKED_SMART_WRAP)
    // {
    //     Implement a "Claimed Space" system where you have allocated, claimed & unclaimed. If there's unclaimed space then another element can go there (if it fits).
    // }
    else if (child_count > 1 && element->child_spacing.flow == FLOW_INLINE)
    {
        measured.x += gap_x * (child_count - 1);
    }
    else if (child_count > 1 && element->child_spacing.flow == FLOW_STACKED)
    {
        measured.y += gap_y * (child_count - 1);
    }

    measured.x += element->padding.x * 2.0f;
    measured.y += element->padding.y * 2.0f;

    // Per-axis hug cap. A HUG axis with an authored dimension > 0 clamps its measured
    // content to that cap (folds the old SIZE_CONTENT_MAX clamp into the generic hug-cap).
    // This stays behaviour-preserving because every non-cap HUG element carries
    // dimensions == {0,0}, so the > 0 guard fires solely for the old capped-hug case.
    if (element->size.width_mode == AXIS_HUG && element->size.dimensions.x > 0.0f)
    {
        measured.x = fminf(measured.x, element->size.dimensions.x);
    }
    if (element->size.height_mode == AXIS_HUG && element->size.dimensions.y > 0.0f)
    {
        measured.y = fminf(measured.y, element->size.dimensions.y);
    }

    return measured;
}

static float ResolveOffsetToPercent(float offset_fixed, float content_extent)
{
    return content_extent > 0.0f ? (offset_fixed / content_extent) : 0.0f;
}

static float ResolveOffsetToFixed(float offset, float content_extent, OffsetMode mode)
{
    float safe_offset = fmaxf(0.0f, offset);
    if (mode == OFFSET_PERCENT)
    {
        return content_extent * safe_offset;
    }

    return safe_offset;
}

static Vector2d ResolveGapStep(Layout layout, Vector2d content_area_local, OffsetMode offset_mode)
{
    Vector2d result = layout.gap;

    if (layout.gap_mode == PERCENT && offset_mode == OFFSET_FIXED)
    {
        result.x *= content_area_local.x;
        result.y *= content_area_local.y;
    }
    else if (layout.gap_mode == NONE && offset_mode == OFFSET_PERCENT)
    {
        result.x = content_area_local.x > 0.0f ? result.x / content_area_local.x : 0.0f;
        result.y = content_area_local.y > 0.0f ? result.y / content_area_local.y : 0.0f;
    }

    return result; // If modes match, no conversion needed, just return the raw gap
}

// Resolve the vertical size used while distributing a child within a parent.
static float ResolveChildHeight(const UIElement *child, float content_area_h, float fill_height)
{
    if (!child)
    {
        return 0.0f;
    }

    // Height axis only. FILL uses the stacked remainder (fill_height), NOT content_area_h.
    switch (child->size.height_mode)
    {
    case AXIS_FILL:
        return fmaxf(0.0f, fill_height);
    case AXIS_PERCENT:
        return content_area_h * child->size.dimensions.y;
    case AXIS_HUG:
        return child->measured_content_size.y;
    case AXIS_FIXED:
    default:
        return child->size.dimensions.y;
    }
}

static Vector2d ResolveChildSizeFixed(const UIElement *child, Vector2d content_area_local, float consumed_fixed_y)
{
    if (!child)
    {
        return ZERO_VECTOR_2D;
    }

    // Width axis via the helper: FILL -> full content width; PERCENT -> content*frac;
    // HUG -> measured content; FIXED -> literal.
    float x = ResolveAxisExtent(child->size.width_mode, child->size.dimensions.x,
                                content_area_local.x, child->measured_content_size.x);

    // Height axis inlined because of the FILL stacked-remainder and the FIXED delegation.
    float y;
    switch (child->size.height_mode)
    {
    case AXIS_PERCENT:
        y = content_area_local.y * child->size.dimensions.y;
        break;
    case AXIS_FILL:
        y = fmaxf(0.0f, content_area_local.y - consumed_fixed_y); // stacked-fill remainder
        break;
    case AXIS_HUG:
        y = child->measured_content_size.y;
        break;
    case AXIS_FIXED:
    default:
        y = ResolveChildHeight(child, content_area_local.y,
                               content_area_local.y - consumed_fixed_y);
        break;
    }

    return (Vector2d){x, y};
}

static StackedLayoutStats CollectStackedLayoutStats(const UIElement *parent, float content_area_h)
{
    StackedLayoutStats stats = {0};
    if (!parent)
    {
        return stats;
    }

    ForEachEnabledChild(parent, child)
    {
        stats.child_count++;
        // Only a both-FILL child participates in the stacked fill distribution (reproduces
        // the old SIZE_FILL-only gating; a mixed hug/fill child is handled per axis).
        if (child->size.width_mode == AXIS_FILL && child->size.height_mode == AXIS_FILL)
        {
            stats.fill_count++;
        }
        else
        {
            stats.occupied_height += ResolveChildHeight(child, content_area_h, 0.0f);
        }
    }

    return stats;
}

static float ResolveStackedFillHeightPerChild(StackedLayoutStats stats, float content_area_h, float gap_y)
{
    if (stats.child_count <= 0 || stats.fill_count <= 0)
    {
        return 0.0f;
    }

    float total_gap = gap_y * (stats.child_count - 1);
    float remaining_space = fmaxf(0.0f, content_area_h - stats.occupied_height - total_gap);
    return remaining_space / stats.fill_count;
}

// Resolve the content height a single child receives from a stacked (FLOW_STACKED)
// parent, i.e. the stacked-fill remainder for a both-FILL child, matching exactly what
// DistributeChildrenStacked caches and ResolveElementBox consumes. This is the SINGLE
// source of truth shared by the MEASURE pass (MeasureElementContent) and the
// DISTRIBUTE/box-resolve pass, so a both-FILL child (the view) receives the identical
// height in both passes and its stack_wrap descendant wraps against the same limit.
//
// The caller MUST have already computed the stacked stats ONCE for this parent and pass
// in the shared fill_height_per_child; this helper does NOT rescan the sibling list, so
// routing every child through it stays O(N) per parent (not O(N^2)).
//
// STRUCTURAL CONTRACT (see .agents/tasks/stackwrap-unify-design.md): equality across
// passes holds only if BOTH callers pass (1) the SAME parent_content_height for the
// stacked parent — for the real chain the stacked parent is the XML <ViewHost> host,
// whose own content height is equal across passes because its parent (panel->root,
// parent==NULL, no padding/override asymmetry) hands it the full content height as its
// single both-FILL child; (2) a fill_height_per_child derived from the SAME-FRAME child
// measurements — the MEASURE caller must measure the parent's children BEFORE computing
// the stacked stats, so the HUG selector's measured_content_size.y is this frame's value,
// not the previous frame's; and (3) the SAME resolved gap step — both callers must
// feed the remainder helper a gap derived via ResolveGapStep(child_spacing, ...,
// OFFSET_FIXED), not the raw member gap, so PERCENT-mode gap (scaled by content
// area) cannot re-diverge the remainder.
//
//   child                : the child whose received height we want.
//   parent_content_height: the stacked parent's content area (box height - 2*padding) in
//                          the CALLER'S pass. In MEASURE this is MeasureElementContent's
//                          local content_height; in DISTRIBUTE it is content_area_local.y.
//   fill_height_per_child: the parent's single stacked-remainder-per-fill-child, computed
//                          ONCE by the caller via CollectStackedLayoutStats +
//                          ResolveStackedFillHeightPerChild over same-frame measurements
//                          and the resolved OFFSET_FIXED gap step.
// Returns the height the child should see on its height axis:
//   * both-FILL child  -> fill_height_per_child (same value cached_stacked_fill_height holds);
//   * any other child  -> ResolveChildHeight (fixed/percent/hug/measured), unchanged.
//
// NOTE (scope): this unifies the stacked-remainder height only. A single-FILL-axis
// descendant (e.g. a stack_wrap section) whose resolved inline-wrap offset_y > 0 still
// diverges by that offset in DISTRIBUTE; that is a separate asymmetry, out of scope here
// (see "Out-of-scope / documented boundary" in the design note).
static float ResolveStackedChildReceivedHeight(const UIElement *child,
                                               float parent_content_height,
                                               float fill_height_per_child)
{
    if (!child)
    {
        return 0.0f;
    }

    // A both-FILL child receives the stacked remainder the caller already computed once;
    // anything else keeps its own per-axis height (fixed/percent/hug), identical to
    // ResolveChildHeight's result. No sibling rescan here -> O(1) per child.
    if (child->size.width_mode == AXIS_FILL && child->size.height_mode == AXIS_FILL)
    {
        return fmaxf(0.0f, fill_height_per_child);
    }
    return ResolveChildHeight(child, parent_content_height, fill_height_per_child);
}

static void DistributeChildrenNormal(UIElement *parent, Vector2d gap_step_fixed, Vector2d gap_step_percent)
{
    int child_count = 0;
    ForEachEnabledChild(parent, child)
    {
        Vector2d step = child->resolved_offset.offset_mode == OFFSET_PERCENT ? gap_step_percent : gap_step_fixed;
        Vector2d distributed = (Vector2d){step.x * child_count, step.y * child_count};

        child->resolved_offset.offset.x = child->authored_offset.offset.x + distributed.x;
        child->resolved_offset.offset.y = child->authored_offset.offset.y + distributed.y;
        child_count++;
    }
}

// Place a child at its authored offset plus a flow cursor measured in fixed units. A child
// authored in OFFSET_PERCENT receives the cursor converted to a fraction of the content area, so
// every flow keeps each child's authored offset units.
static void ApplyFlowCursor(UIElement *child, Vector2d cursor_fixed, Vector2d content_area_local)
{
    if (child->resolved_offset.offset_mode == OFFSET_PERCENT)
    {
        child->resolved_offset.offset = (Vector2d){
            child->authored_offset.offset.x + ResolveOffsetToPercent(cursor_fixed.x, content_area_local.x),
            child->authored_offset.offset.y + ResolveOffsetToPercent(cursor_fixed.y, content_area_local.y)};
    }
    else
    {
        child->resolved_offset.offset = (Vector2d){
            child->authored_offset.offset.x + cursor_fixed.x,
            child->authored_offset.offset.y + cursor_fixed.y};
    }
}

static void DistributeChildrenStacked(UIElement *parent, Vector2d content_area_local, Vector2d gap_step_fixed)
{
    StackedLayoutStats stats = CollectStackedLayoutStats(parent, content_area_local.y);
    if (stats.child_count == 0)
        return;

    float fill_height_per_child = ResolveStackedFillHeightPerChild(stats, content_area_local.y, gap_step_fixed.y);

    // PASS 2: Position elements using a single unified cursor
    float cursor_y = 0.0f;

    ForEachEnabledChild(parent, child)
    {
        ApplyFlowCursor(child, (Vector2d){0.0f, cursor_y}, content_area_local);

        // Height this child receives from the stacked distribution â€” via the SAME helper
        // the MEASURE pass uses, so the cached stacked-fill height can never diverge from
        // the measured column-height limit. Reuses the single fill_height_per_child
        // computed once above (O(1) here).
        float child_h = ResolveStackedChildReceivedHeight(child, content_area_local.y, fill_height_per_child);
        if (child->size.width_mode == AXIS_FILL && child->size.height_mode == AXIS_FILL)
        {
            // Cache here so this child's own ResolveElementBox call doesn't rescan every sibling.
            // Only both-FILL children get the cached stacked remainder (old SIZE_FILL path).
            child->cached_stacked_fill_height = fill_height_per_child;
        }

        // Advance single unified cursor
        cursor_y += child_h + gap_step_fixed.y;
    }
}

// Width a non-FILL inline child advances the cursor by (percent / hug / fixed).
static float ResolveInlineChildWidth(const UIElement *child, float content_width)
{
    switch (child->size.width_mode)
    {
    case AXIS_PERCENT:
        return content_width * child->size.dimensions.x;
    case AXIS_HUG:
        return child->measured_content_size.x;
    case AXIS_FIXED:
    case AXIS_FILL:
    default:
        return child->size.dimensions.x;
    }
}

// Lay children out left to right. Children with width_mode AXIS_FILL share the row width left
// over after the non-FILL children and the gaps (equal shares), so a "fill" caption beside a
// fixed action button leaves the button flush right. The share is cached on the child for
// ResolveElementBox; with no FILL child nothing changes from the plain cursor advance.
static void DistributeChildrenInline(UIElement *parent, Vector2d content_area_local, Vector2d gap_step_fixed)
{
    // PASS 1: total the fixed-extent children, count the FILL children and the enabled children.
    int child_count = 0;
    int fill_count = 0;
    float non_fill_width = 0.0f;
    ForEachEnabledChild(parent, child)
    {
        child_count++;
        if (child->size.width_mode == AXIS_FILL)
        {
            fill_count++;
        }
        else
        {
            non_fill_width += ResolveInlineChildWidth(child, content_area_local.x);
        }
    }

    float fill_width = 0.0f;
    if (fill_count > 0)
    {
        float gaps = gap_step_fixed.x * (float)(child_count - 1);
        fill_width = fmaxf(0.0f, content_area_local.x - non_fill_width - gaps) / (float)fill_count;
    }

    // PASS 2: place each child at the running cursor.
    float cursor_x = 0.0f;
    ForEachEnabledChild(parent, child)
    {
        ApplyFlowCursor(child, (Vector2d){cursor_x, 0.0f}, content_area_local);

        float child_width;
        if (child->size.width_mode == AXIS_FILL)
        {
            child->cached_inline_fill_width = fill_width;
            child_width = fill_width;
        }
        else
        {
            child_width = ResolveInlineChildWidth(child, content_area_local.x);
        }
        cursor_x += child_width + gap_step_fixed.x;
    }
}
// Shared wrapping flow. Children fill "lines" along the main axis (x for inline wrap, y for
// stacked wrap) and start a new line, offset along the cross axis, once the next child would
// overflow the content area. `stacked` selects the main axis; all the arithmetic below is written
// in main/cross terms so the two flows cannot drift apart.
static void DistributeChildrenWrap(UIElement *parent, Vector2d content_area_local,
                                   Vector2d gap_step_fixed, bool stacked)
{
    float main_limit = stacked ? content_area_local.y : content_area_local.x;
    float main_gap = stacked ? gap_step_fixed.y : gap_step_fixed.x;
    float cross_gap = stacked ? gap_step_fixed.x : gap_step_fixed.y;

    float main_cursor = 0.0f;  // Extent used on the current line.
    float cross_cursor = 0.0f; // Cross-axis start of the current line.
    float line_cross = 0.0f;   // Largest cross extent on the current line.

    ForEachEnabledChild(parent, child)
    {
        Vector2d child_size = ResolveChildSizeFixed(child, content_area_local, 0.0f);
        float child_main = stacked ? child_size.y : child_size.x;
        float child_cross = stacked ? child_size.x : child_size.y;

        // Wrap to a new line when this child would overflow a non-empty line.
        if (main_cursor > 0.0f && main_cursor + main_gap + child_main > main_limit)
        {
            main_cursor = 0.0f;
            cross_cursor += line_cross + cross_gap;
            line_cross = 0.0f;
        }

        float child_main_pos = main_cursor > 0.0f ? main_cursor + main_gap : main_cursor;
        Vector2d cursor = stacked ? (Vector2d){cross_cursor, child_main_pos}
                                  : (Vector2d){child_main_pos, cross_cursor};
        ApplyFlowCursor(child, cursor, content_area_local);

        main_cursor = child_main_pos + child_main;
        line_cross = fmaxf(line_cross, child_cross);
    }
}

static void DistributeChildrenWithContentArea(UIElement *e, Vector2d content_area_local)
{
    if (!e || !e->first_child)
    {
        return;
    }

    Layout s = e->child_spacing;
    if (s.flow == FLOW_NONE)
    {
        return;
    }

    Vector2d gap_step_fixed = ResolveGapStep(s, content_area_local, OFFSET_FIXED);
    Vector2d gap_step_percent = ResolveGapStep(s, content_area_local, OFFSET_PERCENT);

    if (s.flow == FLOW_NORMAL)
    {
        DistributeChildrenNormal(e, gap_step_fixed, gap_step_percent);
    }
    else if (s.flow == FLOW_STACKED)
    {
        DistributeChildrenStacked(e, content_area_local, gap_step_fixed);
    }
    else if (s.flow == FLOW_INLINE)
    {
        DistributeChildrenInline(e, content_area_local, gap_step_fixed);
    }
    else if (s.flow == FLOW_INLINE_WRAP)
    {
        DistributeChildrenWrap(e, content_area_local, gap_step_fixed, false);
    }
    else if (s.flow == FLOW_STACKED_WRAP)
    {
        DistributeChildrenWrap(e, content_area_local, gap_step_fixed, true);
    }
}

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

// Pool for UIElement instances (uses memory pool API)
#include "memory/cmemory.h"
static Pool *ui_element_pool = NULL;

// Returns the global UI element pool so external systems (e.g. total UI reset) can destroy it.
Pool *GetUIElementPool(void)
{
    return ui_element_pool;
}

// Sets the global UI element pool; used after destroying the old pool during a total UI reset.
void SetUIElementPool(Pool *pool)
{
    ui_element_pool = pool;
}

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------

UIElement *CreateUIElement(UIElementType type, Size size, Offset parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill)
{
    // Fast-path: allocate from a simple static pool to reduce heap churn for UI elements
    // Increased from 512 to 2048 for Phase 2 optimization (reduced heap allocation overhead)
    if (!ui_element_pool)
        ui_element_pool = PoolCreate(sizeof(UIElement), 2048);

    UIElement *e = NULL;
    if (ui_element_pool)
        e = (UIElement *)PoolAlloc(ui_element_pool);

    if (!e)
        e = AllocateBytes(sizeof(UIElement));

    e->colour_fill = colour_fill;
    e->colour_border = colour_border;
    e->border_weight = 1; // Historic 1 px border; surfaces and border-weight= override it
    e->size = size;
    e->text_horizontal_alignment = UI_TEXT_ALIGN_LEFT;
    e->text_vertical_alignment = UI_TEXT_VERTICAL_ALIGN_CENTRE;
    e->padding = padding;
    e->measured_content_size = ZERO_VECTOR_2D;
    e->cached_stacked_fill_height = 0.0f;
    e->cached_inline_fill_width = 0.0f;
    e->resolved_offset = parent_offset;
    e->authored_offset = parent_offset;
    e->type = type;
    e->is_enabled = true;
    // MUST explicitly set these to NULL
    e->first_child = NULL;
    e->next_sibling = NULL;
    e->parent = NULL;
    // Default values for interactive elements
    e->is_focused = false;
    e->is_dirty = true;
    e->is_draggable = false;
    e->child_spacing = UI_SPACING_NONE;
    e->scroll_offset = ZERO_VECTOR_2D;
    e->is_scrollable_x = false;
    e->is_scrollable_y = false;
    // Explicitly clear the hover-hint fields: pooled elements can be recycled, so never rely on
    // the allocator having zeroed them.
    e->hint = NULL;
    e->on_hint = NULL;

    return e;
}

// Append one printf-formatted segment to a hint string. A " | " separator is inserted when the
// buffer already holds text. The result is always NUL-terminated and never exceeds `cap` bytes;
// if the segment does not fit, the visible tail is replaced with "..." so truncation is obvious.
// Later appends to an already-truncated buffer keep the ellipsis (the operation is idempotent).
//
//   buf : destination hint buffer (must already be NUL-terminated, or empty).
//   cap : total capacity of buf in bytes (including the terminator).
//   fmt : printf-style format for the segment text.
void UIHint_AppendSegment(char *buf, size_t cap, const char *fmt, ...)
{
    // Need room for at least one character plus the "..." marker and terminator.
    if (!buf || !fmt || cap < 5)
    {
        return;
    }

    // Format the segment into scratch space first so the separator logic sees its true length.
    char segment[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(segment, sizeof(segment), fmt, args);
    va_end(args);

    size_t used = strlen(buf);
    const char *separator = (used > 0) ? " | " : "";

    // snprintf truncates safely at cap-1; its return value tells us whether anything was lost.
    int wanted = snprintf(buf + used, cap - used, "%s%s", separator, segment);
    if (wanted < 0 || (size_t)wanted >= cap - used)
    {
        // Overflow: stamp an ellipsis over the last three visible characters.
        buf[cap - 4] = '.';
        buf[cap - 3] = '.';
        buf[cap - 2] = '.';
        buf[cap - 1] = '\0';
    }
}

UIElement *CreateUIElementInTree(UIElementType type, Size size, UIElement *parent, Offset parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill)
{
    UIElement *e = CreateUIElement(type, size, parent_offset, padding, colour_border, colour_fill);

    AddElementToTree(e, parent);

    return e;
}

void AddElementToTree(UIElement *e, UIElement *parent)
{
    if (!e)
    {
        return;
    }
    
    // NULL parent indicates this is a root element
    if (!parent)
    {
        e->parent = NULL;
        e->next_sibling = NULL;
        return;
    }
    
    e->parent = parent;
    e->next_sibling = NULL;

    // Make the provided child the youngest sibling, get the next_sibling starting at the first_child
    if (parent->first_child == NULL)
    {
        // This is the only child
        parent->first_child = e;
    }
    else
    {
        UIElement *last_sibling = GetLastChild(parent);
        if (last_sibling)
        {
            last_sibling->next_sibling = e;
        }
        else
        {
            parent->first_child = e;
            printf("ERROR adding [%s] as child to [%s]: Parent has child but last_sibling of new element not found. Likely a bug in GetLastChild.\n", GetElementTypeName(e->type), GetElementTypeName(parent->type));
        }
    }
}

void RemoveElementFromTree(UIElement *e)
{
    if (!e)
    {
        return;
    }
    UIElement *p = e->parent;
    UIElement *prev_sibling = GetPreviousSibling(p);

    // Remove element from siblings list by pointing prev_sibling to the removed element's next_sibling
    if (prev_sibling)
    {
        prev_sibling->next_sibling = e->next_sibling;
    }
    else
    {
        // The element we want to remove must've been the first_child, need to update this
        // if (next_sibling)
        {
            p->first_child = e->next_sibling;
        }
    }
    // Unlink the element from the tree completely
    e->parent = NULL;
    e->next_sibling = NULL;
    // Dispose of the element
}

UIElement *GetLastChild(UIElement *e)
{
    if (!e || !e->first_child)
    {
        return NULL;
    }

    UIElement *next_child = e->first_child;
    while (next_child->next_sibling)
    {
        next_child = next_child->next_sibling;
    }

    return next_child;
}

UIElement *GetPreviousSibling(UIElement *e)
{
    if (!e)
    {
        return NULL;
    }
    UIElement *p = e->parent;
    if (!p)
    {
        return NULL;
    }

    UIElement *prev_child = p->first_child;
    UIElement *next_child = prev_child;
    while (next_child && next_child != e)
    {
        prev_child = next_child;
        next_child = next_child->next_sibling;
    }

    if (next_child != e)
    {
        return NULL;
    }

    return prev_child;
}

bool ElementHasSibling(UIElement *e)
{
    if (!e || !e->parent)
        return false;
    return (e->next_sibling != NULL) || (e->parent->first_child != e);
}

// Resolve boxes and distribute children after the subtree has been measured.
// This needs to be called BEFORE the child's box is resolved, so that the child can use the parent's child_spacing to determine its position
// Purpose: Distribute the children of a parent container according to its configured spacing rules. This function modifies the parent_offset of each child based on the parent's child_spacing settings.
static void LayoutSubtree(UIElement *e, UIBox parent_box)
{
    if (!e)
        return;

    // Resolve THIS element's box based on parent context
    // ResolveElementBox looks UP at parent_box to figure out e's own dimensions.
    // UI_DistributeChildren looks DOWN at its children using e's own resolved content area to position them.
    e->local_box = ResolveElementBox(e, parent_box);

    // Now that e's box is resolved we can distribute immediate children inside its resovled content area
    if (e->first_child)
    {
        Vector2d content_area = GetContentArea(e->local_box.dimensions, e->padding);
        DistributeChildrenWithContentArea(e, content_area);
    }

    // Recurse down into children
    ForEachChild(e, child)
    {
        LayoutSubtree(child, e->local_box);
    }
}

// Measure the subtree once, then resolve every element using the cached measurements.
void UI_LayoutSubtree(UIElement *e, UIBox parent_box)
{
    if (!e)
        return;

    e->measured_content_size = MeasureElementContent(e, parent_box.dimensions.x, parent_box.dimensions.y);
    LayoutSubtree(e, parent_box);
}

// looks DOWN at its children using e's own resolved content area to position them.
// ResolveElementBox looks UP at parent_box to figure out e's own dimensions.
void UI_DistributeChildren(UIElement *e)
{
    if (!e || !e->first_child)
        return;

    Vector2d content_area = GetContentArea(e->local_box.dimensions, e->padding);
    DistributeChildrenWithContentArea(e, content_area);
}

// Call this to kick of the recursive distribution of children for a given parent element and its resolved box. This function will traverse the entire subtree of the parent element, applying the appropriate spacing rules to each child based on the parent's configuration.
//  Correct top-down recursion

// True if this element OR any descendant has allow_overflow set. The hit-test uses this to
// decide whether to keep descending a child branch when the pointer is outside the current
// element's own box: an overflow element may draw and be hit outside its ancestors' boxes,
// so a branch containing one must still be visited. Only enabled elements are considered,
// mirroring the hit-test's own is_enabled gate (a disabled subtree is never hit).
static bool SubtreeHasOverflow(const UIElement *e)
{
    if (!e || !e->is_enabled)
    {
        return false;
    }
    if (e->allow_overflow)
    {
        return true;
    }
    ForEachChild(e, child)
    {
        if (SubtreeHasOverflow(child))
        {
            return true;
        }
    }
    return false;
}

// Resolve the deepest interactive UIElement under pixel coordinates, respecting unconstrained bounds.
UIElement *GetElementAt(UIElement *e, Vector2d pixel_coords)
{
    return GetElementAtClipped(e, pixel_coords, UI_UNCONSTRAINED_CLIP);
}

// Resolve the deepest interactive UIElement under pixel coordinates, respecting an active clip rectangle.
UIElement *GetElementAtClipped(UIElement *e, Vector2d pixel_coords, UIClipRect clip)
{
    if (!e || !e->is_enabled)
    {
        return NULL;
    }

    // Verify pointer coordinates fall within the active clipping rectangle.
    if (pixel_coords.x < clip.min.x || pixel_coords.x > clip.max.x ||
        pixel_coords.y < clip.min.y || pixel_coords.y > clip.max.y)
    {
        return NULL;
    }

    // Is the pointer over THIS element's own box? An allow_overflow element ANYWHERE in a
    // child's subtree (not just an immediate child) may extend outside e's box, so being
    // outside e does NOT prove the pointer misses every descendant: we must keep descending
    // into any child branch that contains an overflow element. Only when the pointer is
    // outside e AND no child subtree contains an overflow element can we short-circuit
    // (the common, fast case). SubtreeHasOverflow walks the branch to answer that.
    bool over_self = IsMouseOverElement(e, pixel_coords);
    if (!over_self)
    {
        bool branch_overflows = false;
        ForEachChild(e, child)
        {
            if (SubtreeHasOverflow(child))
            {
                branch_overflows = true;
                break;
            }
        }
        if (!branch_overflows)
        {
            return NULL;
        }
    }

    // If this container is scrollable, constrain child hit-testing to the container's visible bounds.
    UIClipRect child_clip = clip;
    if (e->is_scrollable_x || e->is_scrollable_y)
    {
        child_clip = UIClipRect_Intersect(clip, UIClipRect_FromBox(e->screen_box));
    }

    // Check children (last sibling is usually drawn on top, so top-most child wins). When the
    // pointer is outside e's own box, only child branches that CONTAIN an overflow element are
    // eligible (a fully-clamped branch cannot hold a pointer that e itself does not).
    UIElement *found = NULL;
    if (!UIClipRect_IsEmpty(child_clip))
    {
        ForEachChild(e, child)
        {
            if (!over_self && !SubtreeHasOverflow(child))
            {
                continue;
            }
            UIElement *clicked = GetElementAtClipped(child, pixel_coords, child_clip);
            if (clicked)
            {
                found = clicked;
            }
        }
    }

    // A child hit wins. Otherwise e is the hit ONLY if the pointer is actually over e's box
    // (an overflow-only descent where the pointer missed every overflow child returns NULL,
    // since the pointer is not over e itself).
    if (found)
    {
        return found;
    }
    return over_self ? e : NULL;
}

void DisableElement(UIElement *element)
{
    element->is_enabled = false;
}

void SetEnabledState(UIElement *element, bool is_enabled)
{
    if (element)
    {
        element->is_enabled = is_enabled;
    }
}

void SetParentEnabledState(UIElement *child, bool is_enabled)
{
    if (child)
    {
        UIElement *parent = child->parent;
        if (parent)
        {
            parent->is_enabled = is_enabled;
            return;
        }
    }
}

void EnableElement(UIElement *element)
{
    element->is_enabled = true;
}

void ToggleElementEnabled(UIElement *element)
{
    element->is_enabled = !element->is_enabled;
}

// Scroll a horizontally scrollable element and clamp its offset to measured content bounds.
void ScrollUIElementX(UIElement *element, float delta)
{
    if (!element || !element->is_scrollable_x)
    {
        return;
    }

    float max_scroll = fmaxf(0.0f, element->measured_content_size.x - element->local_box.dimensions.x);
    element->scroll_offset.x = fminf(max_scroll, fmaxf(0.0f, element->scroll_offset.x + delta));
}

// Scroll a vertically scrollable element and clamp its offset to measured content bounds.
void ScrollUIElementY(UIElement *element, float delta)
{
    if (!element || !element->is_scrollable_y)
    {
        return;
    }

    float max_scroll = fmaxf(0.0f, element->measured_content_size.y - element->local_box.dimensions.y);
    element->scroll_offset.y = fminf(max_scroll, fmaxf(0.0f, element->scroll_offset.y + delta));
}

// Re-clamp the scroll offset of every enabled scrollable element below (and including) root
// to its measured content bounds. Views are skipped because View_UpdateScrollBounds owns them.
// Run after layout each frame so an offset cannot stay stranded past the end when content shrinks.
void UI_ClampScrollOffsets(UIElement *root)
{
    if (!root || !root->is_enabled)
    {
        return;
    }

    if (root->type != UI_ELEMENT_VIEW)
    {
        if (root->is_scrollable_x)
        {
            float max_scroll = fmaxf(0.0f, root->measured_content_size.x - root->local_box.dimensions.x);
            root->scroll_offset.x = fminf(max_scroll, fmaxf(0.0f, root->scroll_offset.x));
        }
        if (root->is_scrollable_y)
        {
            float max_scroll = fmaxf(0.0f, root->measured_content_size.y - root->local_box.dimensions.y);
            root->scroll_offset.y = fminf(max_scroll, fmaxf(0.0f, root->scroll_offset.y));
        }
    }

    ForEachEnabledChild(root, child)
    {
        UI_ClampScrollOffsets(child);
    }
}
void SetUIElementTextHorizontalAlignment(UIElement *element, UITextHorizontalAlignment alignment)
{
    if (!element)
    {
        return;
    }

    element->text_horizontal_alignment = alignment;
}

void SetUIElementTextVerticalAlignment(UIElement *element, UITextVerticalAlignment alignment)
{
    if (!element)
    {
        return;
    }

    element->text_vertical_alignment = alignment;
}

bool IsTextbox(UIElement *e)
{
    if (!e)
        return false;
    return (e->type == UI_ELEMENT_TEXTBOX_O ||
            e->type == UI_ELEMENT_TEXTBOX_SAFE_IO ||
            e->type == UI_ELEMENT_TEXTBOX_IO);
}

// Return whether the element accepts keyboard input and commits edited text.
bool IsEditableTextbox(const UIElement *e)
{
    return e && (e->type == UI_ELEMENT_TEXTBOX_IO ||
                 e->type == UI_ELEMENT_TEXTBOX_SAFE_IO);
}

bool IsBtn(UIElement *e)
{
    if (!e)
        return false;
    return (e->type == UI_ELEMENT_BUTTON_SIMPLE ||
            e->type == UI_ELEMENT_BUTTON_ENUMERATE ||
            e->type == UI_ELEMENT_BUTTON_SUBMIT);
}

// Disposes Element - assumes it has already been removed from its UI Tree if in one
void DisposeUIElement(UIElement *e)
{
    if (!e)
        return;

    // 1. Recursively destroy the first child (and all its siblings)
    // This goes "deep" before it stays "wide"
    if (e->first_child)
    {
        DisposeUIElement(e->first_child);
    }

    // 2. Recursively destroy the next sibling
    // This ensures every element in the horizontal list is freed
    if (e->next_sibling)
    {
        DisposeUIElement(e->next_sibling);
    }

    // 3. Cleanup element-specific resources (e.g., attached binders)
    if (IsTextbox(e) && e->data.textbox.binder)
    {
        Binder_Destroy(e->data.textbox.binder);
        e->data.textbox.binder = NULL;
    }

    // Free any heap binding attached to a textbox (mirrors the binder free above and the button
    // binding free below). Binding_Destroy NULLs the pointer; pool-zeroing defaults untouched
    // slots to NULL, so this is a no-op for textboxes that were never migrated.
    if (IsTextbox(e) && e->data.textbox.binding)
    {
        Binding_Destroy(&e->data.textbox.binding);
    }

    // Free any heap binding attached to a button (mirrors the textbox binder cleanup above).
    // Pool-zeroing guarantees binding == NULL for untouched buttons, so this is a no-op there.
    if (IsBtn(e) && e->data.button.binding)
    {
        Binding_Destroy(&e->data.button.binding);
    }

    // 4. Finally, free the current element
    // Now that children and siblings are gone, it's safe to delete this one
    // If the element points into our static pool, return it to the free list; otherwise free the heap allocation.
    if (ui_element_pool && PoolOwns(ui_element_pool, e))
    {
        // reset minimal state and return to pool
        e->first_child = NULL;
        e->next_sibling = NULL;
        e->parent = NULL;
        e->is_focused = false;
        e->is_dirty = false;
        e->is_draggable = false;
        e->is_enabled = false;
        PoolFree(ui_element_pool, e);
    }
    else
    {
        Deallocate((void **)&e, sizeof(UIElement));
    }
}

// Refresh a single element's display from its attached binding's source (data -> UI).
// Pure pull: reads current truth each call, so a widget bound to a query (e.g. a debug toggle)
// never goes stale no matter what changed the underlying state. Does nothing for elements with
// no binding or a source-less (command-only / sink-only) binding, so it is cheap to call on
// every element every frame.
// EXCEPTION: a bound button that also carries an on_draw glyph (the toolbar's icon toggles) keeps
// its authored flat surface=input fill and conveys ON/OFF through the glyph ink instead, mirroring
// the Nudge button; for those the amber box-fill swap is deliberately skipped (see below).
void UIElement_RefreshBinding(UIElement *e)
{
    if (!e)
        return;

    if (IsBtn(e))
    {
        const Binding *b = e->data.button.binding;
        // Only buttons whose binding actually has a readable source derive their text; a plain
        // command-sink button (no source) keeps the authored text it was built with.
        if (!b || b->source.kind == BIND_SRC_NONE)
            return;

        BindingValue value = Binding_ReadSource(&b->source);
        if (value.type == BIND_NONE)
            return;

        // Toggle presentation: the drawn label is just the authored base name; the ON/OFF state
        // is carried by the element fill instead of a ": ON/OFF" suffix. The authored base lives
        // in `text` (left intact); the drawn result goes to `display_text`. Fill is read from the
        // active palette's toggle-on/off fields (bright amber ON, dimmed amber OFF). The element
        // carries no palette handle, so ui_default_palette is read directly - matching the
        // toolbar's existing palette fallback.
        bool on = (value.as.i != 0);
        safe_strncpy(e->data.button.display_text.string, e->data.button.text.string,
                     sizeof(e->data.button.display_text.string));
        if (BIND_TOGGLE_IS_MIXED(value.as.i))
        {
            // Bulk selection whose targets disagree: suffix the label with the "<on>/<applicable>"
            // count and show the OFF fill (a click sets everything ON). snprintf truncates safely
            // if the label is long.
            snprintf(e->data.button.display_text.string, sizeof(e->data.button.display_text.string),
                     "%s: %d/%d", e->data.button.text.string,
                     BIND_TOGGLE_ON(value.as.i), BIND_TOGGLE_TOTAL(value.as.i));
            if (e->on_draw == NULL)
            {
                // MIXED uses the inactive pair (OFF fill + light ink).
                UIPalette_GetToggleColours(&ui_default_palette, false,
                                           &e->colour_fill, &e->data.button.font.colour);
            }
            return;
        }
        // Icon toggles (an on_draw glyph present) keep their authored flat surface=input fill and
        // show ON/OFF via the glyph ink (UIIcon_StateInk), exactly like the Nudge button - so the
        // amber box-fill and caption-ink swap is skipped for them, leaving the shared flat base
        // intact. Text toggles use the shared two-state pair (ON amber + dark ink, OFF dimmed
        // fill + light ink) so the caption always contrasts with its fill.
        if (e->on_draw == NULL)
        {
            UIPalette_GetToggleColours(&ui_default_palette, on,
                                       &e->colour_fill, &e->data.button.font.colour);
        }
        return;
    }

    if (IsTextbox(e))
    {
        const Binding *b = e->data.textbox.binding;
        // Unbound or source-less textboxes stay on the legacy RefreshTextboxFields path.
        if (!b || b->source.kind == BIND_SRC_NONE)
            return;

        // CRITICAL invariant: never overwrite text the user is actively editing. This skip used
        // to live in RefreshTextboxFields and must be preserved now that the pull drives textboxes.
        if (e->is_focused)
            return;

        // Read/display direction: source -> format -> text buffer. A BIND_NONE read (e.g. a
        // dynamic query returning "nothing selected") leaves the last-good display untouched.
        Binding_RefreshText(b, e->data.textbox.text.string, sizeof(e->data.textbox.text.string));
        return;
    }

    // NOTE: migrated textboxes refresh here via the generic pull (skipped while focused), reached
    // every frame by ViewHostSystem_RefreshBindings. Textboxes without a binding stay on the explicit
    // RefreshTextboxFields path that panels call.
}

void GetUIElementVertices(UIElement *e, Vector2d out_vertices[4])
{
    out_vertices[0] = e->screen_box.coords;
    out_vertices[1] = (Vector2d){e->screen_box.coords.x + e->screen_box.dimensions.x, e->screen_box.coords.y};
    out_vertices[2] = (Vector2d){e->screen_box.coords.x + e->screen_box.dimensions.x, e->screen_box.coords.y + e->screen_box.dimensions.y};
    out_vertices[3] = (Vector2d){e->screen_box.coords.x, e->screen_box.coords.y + e->screen_box.dimensions.y};
}

bool IsMouseOverElement(UIElement *e, Vector2d mouse_pos)
{
    return (mouse_pos.x >= e->screen_box.coords.x &&
            mouse_pos.x <= e->screen_box.coords.x + e->screen_box.dimensions.x &&
            mouse_pos.y >= e->screen_box.coords.y &&
            mouse_pos.y <= e->screen_box.coords.y + e->screen_box.dimensions.y);
}

// looks UP at parent_box to figure out e's own dimensions.
UIBox ResolveElementBox(UIElement *element, UIBox parent_box)
{
    if (!element)
        return ZERO_BOX;

    UIBox box;
    box.coords = parent_box.coords; // Start at parent origin
    float p_mid_x = parent_box.dimensions.x / 2;
    float p_mid_y = parent_box.dimensions.y / 2;

    // Calculate the available content area inside the parent
    float content_area_w = parent_box.dimensions.x;
    float content_area_h = parent_box.dimensions.y;

    // Apply any padding to correct the available area
    if (element->parent)
    {
        // Account for padding and parent scroll offset
        float pad_x = element->parent->padding.x;
        float pad_y = element->parent->padding.y;

        box.coords.x += pad_x - element->parent->scroll_offset.x;
        box.coords.y += pad_y - element->parent->scroll_offset.y;

        content_area_w -= (pad_x * 2.0f);
        content_area_h -= (pad_y * 2.0f);
    }

    // Resolve the local Offset (Relative to the content area)
    float adj_offset_x = ResolveOffsetToFixed(element->resolved_offset.offset.x, content_area_w, element->resolved_offset.offset_mode);
    float adj_offset_y = ResolveOffsetToFixed(element->resolved_offset.offset.y, content_area_h, element->resolved_offset.offset_mode);

    // Apply the offset to the final coordinates
    box.coords.x += adj_offset_x;
    box.coords.y += adj_offset_y;

    // Resolve Size with Right/Bottom clamping. The "space left" is the content area minus
    // how far we shifted in.
    float remaining_w = fmaxf(0.0f, content_area_w - adj_offset_x);
    float remaining_h = fmaxf(0.0f, content_area_h - adj_offset_y);

    // The cached_stacked_fill_height override applies ONLY when BOTH axes fill, reproducing
    // the old SIZE_FILL-only path exactly. A mixed hug/fill element never picks it up.
    bool both_fill = (element->size.width_mode == AXIS_FILL &&
                      element->size.height_mode == AXIS_FILL);

    if (both_fill)
    {
        // Old SIZE_FILL path, verbatim: fill remaining extent, with the stacked-fill override.
        box.dimensions.x = remaining_w;
        box.dimensions.y = remaining_h;

        if (element->parent && element->parent->child_spacing.flow == FLOW_STACKED)
        {
            // The parent's own distribution pass already computed this for every FILL
            // child in one scan; reuse it instead of rescanning all siblings here.
            float fill_height_per_child = element->cached_stacked_fill_height;
            if (fill_height_per_child > 0.0f)
            {
                box.dimensions.y = fill_height_per_child;
            }
        }
        // (No right/bottom clamp for the both-FILL case, matching today.)
    }
    else
    {
        // Resolve each axis independently. A single FILL axis uses the "remaining" extent
        // (old SIZE_HUG_* path), NOT the stacked-fill override. PERCENT/HUG/FIXED go via
        // ResolveAxisExtent against the content area.
        // A FILL width inside an inline parent takes the share the parent's distribution cached
        // (0 means none was cached, so the plain remaining extent applies).
        float fill_width = remaining_w;
        if (element->parent && element->parent->child_spacing.flow == FLOW_INLINE &&
            element->cached_inline_fill_width > 0.0f)
        {
            fill_width = element->cached_inline_fill_width;
        }
        box.dimensions.x = (element->size.width_mode == AXIS_FILL)
                               ? fill_width
                               : ResolveAxisExtent(element->size.width_mode,
                                                   element->size.dimensions.x,
                                                   content_area_w,
                                                   element->measured_content_size.x);

        box.dimensions.y = (element->size.height_mode == AXIS_FILL)
                               ? remaining_h
                               : ResolveAxisExtent(element->size.height_mode,
                                                   element->size.dimensions.y,
                                                   content_area_h,
                                                   element->measured_content_size.y);

        // Right/bottom clamp, per axis, only for the non-FILL axes and only when the parent
        // is not scrollable along that axis. An element with allow_overflow opts out of the
        // clamp entirely so it may extend past the parent's content box (fan-out submenus,
        // tooltips, dropdowns); its authored/measured size is kept intact on both axes.
        if (element->size.width_mode != AXIS_FILL && !element->allow_overflow &&
            (!element->parent || !element->parent->is_scrollable_x))
        {
            box.dimensions.x = fminf(box.dimensions.x, remaining_w);
        }
        if (element->size.height_mode != AXIS_FILL && !element->allow_overflow &&
            (!element->parent || !element->parent->is_scrollable_y))
        {
            box.dimensions.y = fminf(box.dimensions.y, remaining_h);
        }
    }

    return box;
}

bool UI_AABB_Intersects(UIBox a, UIBox b)
{
    return (a.coords.x < b.coords.x + b.dimensions.x &&
            a.coords.x + a.dimensions.x > b.coords.x &&
            a.coords.y < b.coords.y + b.dimensions.y &&
            a.coords.y + a.dimensions.y > b.coords.y);
}

// Convert a UIBox into an axis-aligned minimum/maximum clipping rectangle.
UIClipRect UIClipRect_FromBox(UIBox box)
{
    return (UIClipRect){
        .min = box.coords,
        .max = (Vector2d){box.coords.x + box.dimensions.x, box.coords.y + box.dimensions.y}};
}

// Compute the overlapping intersection of two clipping rectangles.
UIClipRect UIClipRect_Intersect(UIClipRect a, UIClipRect b)
{
    UIClipRect result;
    result.min.x = fmaxf(a.min.x, b.min.x);
    result.min.y = fmaxf(a.min.y, b.min.y);
    result.max.x = fminf(a.max.x, b.max.x);
    result.max.y = fminf(a.max.y, b.max.y);
    if (result.max.x < result.min.x)
    {
        result.max.x = result.min.x;
    }
    if (result.max.y < result.min.y)
    {
        result.max.y = result.min.y;
    }
    return result;
}

// Return true if the clipping rectangle has zero or negative area.
bool UIClipRect_IsEmpty(UIClipRect rect)
{
    return (rect.max.x <= rect.min.x) || (rect.max.y <= rect.min.y);
}

// Check whether a UIBox is entirely contained within a clipping rectangle.
bool UIClipRect_ContainsBox(UIClipRect clip, UIBox box)
{
    if (box.dimensions.x <= 0.0f || box.dimensions.y <= 0.0f)
    {
        return true;
    }
    return (box.coords.x >= clip.min.x &&
            box.coords.x + box.dimensions.x <= clip.max.x &&
            box.coords.y >= clip.min.y &&
            box.coords.y + box.dimensions.y <= clip.max.y);
}

// Check whether a UIBox partially or fully overlaps a clipping rectangle.
bool UIClipRect_IntersectsBox(UIClipRect clip, UIBox box)
{
    if (box.dimensions.x <= 0.0f || box.dimensions.y <= 0.0f)
    {
        return false;
    }
    return (box.coords.x < clip.max.x &&
            box.coords.x + box.dimensions.x > clip.min.x &&
            box.coords.y < clip.max.y &&
            box.coords.y + box.dimensions.y > clip.min.y);
}

// Trim a UIBox against a clipping rectangle, returning the clamped visible box.
UIBox UIBox_Clip(UIBox box, UIClipRect clip)
{
    float min_x = fmaxf(box.coords.x, clip.min.x);
    float min_y = fmaxf(box.coords.y, clip.min.y);
    float max_x = fminf(box.coords.x + box.dimensions.x, clip.max.x);
    float max_y = fminf(box.coords.y + box.dimensions.y, clip.max.y);
    if (max_x < min_x)
    {
        max_x = min_x;
    }
    if (max_y < min_y)
    {
        max_y = min_y;
    }
    return (UIBox){
        .coords = (Vector2d){min_x, min_y},
        .dimensions = (Vector2d){max_x - min_x, max_y - min_y}};
}

const char *GetElementTypeName(UIElementType type)
{
    switch (type)
    {
    case UI_ELEMENT_ROOT:
        return "ROOT";
    case UI_ELEMENT_TEXTFIELD:
        return "TEXTFIELD";
    case UI_ELEMENT_CONTAINER:
        return "CONTAINER";
    case UI_ELEMENT_LABEL:
        return "LABEL";
    case UI_ELEMENT_TEXTBOX_O:
        return "TEXTBOX_O";
    case UI_ELEMENT_TEXTBOX_IO:
        return "TEXTBOX_IO";
    case UI_ELEMENT_TEXTBOX_SAFE_IO:
        return "TEXTBOX_SAFE_IO";
    case UI_ELEMENT_BUTTON_SIMPLE:
        return "BUTTON_SIMPLE";
    case UI_ELEMENT_BUTTON_ENUMERATE:
        return "BUTTON_ENUMERATE";
    case UI_ELEMENT_BUTTON_SUBMIT:
        return "BUTTON_SUBMIT";
    case UI_ELEMENT_IMAGE:
        return "IMAGE";
    default:
        return "UNKNOWN_TYPE";
    }
}

// // This needs to be called after ResolveElementDimensions because the resolved dimensions are required to determine mid-points which are used here
// Offset ResolveElementOffset(UIElement *element, Vector2d available_area, Vector2d basis_scale)
// {
//     // 1. Calculate the available content area inside the parent
//     Offset p_offset = {0};
//     float offset_x = fmaxf(0.0f, element->parent_offset.offset.x);
//     float offset_y = fmaxf(0.0f, element->parent_offset.offset.y);
//     float pixel_offset_x, pixel_offset_y;

//     float p_mid_x = available_area.x / 2;
//     float p_mid_y = available_area.y / 2;

//     if (element->parent_offset.offset_mode == ALIGNED_CENTRE)
//     {
//         // This means the mid-point of the element needs to equal mid-point of parent
//         pixel_offset_x;
//     }
//     else if (element->parent_offset.offset_mode == OFFSET_PERCENT)
//     {
//         pixel_offset_x = available_area * offset_x;
//         pixel_offset_y = content_area_h * offset_y;
//     }
//     else if (element->parent_offset.offset_mode == OFFSET_FIXED)
//     {
//         if (element->parent)
//         {
//             // Account for padding
//             float pad_x = element->parent->padding.x * basis_scale.x;
//             float pad_y = element->parent->padding.y * basis_scale.y;

//             p_offset.offset.x += pad_x;
//             p_offset.offset.y += pad_y;

//             content_area_w -= (pad_x * 2.0f);
//             content_area_h -= (pad_y * 2.0f);
//         }
//     }
// }

// UIElement *CreateTextField(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d label_tbox_offset, Vector2d label_tbox_padding, char max_label_chars, char max_text_box_chars)
// {
//     UIElement *tf = AllocateBytes(sizeof(UIElement));
//     tf->origin = origin_coords;
//     tf->width = width;
//     tf->height = height;
//     tf->parent_offset = parent_offset;
//     tf->children = *NewLArray(2, sizeof(UIElement));
//     // tf.parent = parent;

//     // Determine if Label is above or inline
//     // If the vertical offset is small, we assume they sit side-by-side
//     bool label_is_inline = (label_tbox_offset.y < height / 4);

//     UIElement tb = {0};
//     UIElement tl = {0};
//     // Set backlink for the label to the TextField we are currently building
//     // Note: We'll set tf.label = tl at the end, copying the data.
//     tl.parent = tf;
//     tb.parent = tf; // TextBox bubbles to the Container

//     //tb.max_len = max_text_box_chars;

//     if (label_is_inline)
//     {
//         // 1. Calculate Width Proportions based on char counts
//         float total_chars = (float)(max_text_box_chars + max_label_chars);
//         float tl_w = (max_label_chars / total_chars) * (width - label_tbox_offset.x);
//         float tb_w = width - tl_w - label_tbox_offset.x;

//         // 2. Set Label Dimensions & Origin (Left Side)
//         tl.width = tl_w;
//         tl.height = height;
//         tl.origin = origin_coords;
//         tl.parent_offset = (Vector2d){0, 0};

//         // 3. Set TextBox Dimensions & Origin (Right Side, shifted by label + offset)
//         tb.width = tb_w;
//         tb.height = height;
//         tb.origin.x = origin_coords.x + tl_w + label_tbox_offset.x;
//         tb.origin.y = origin_coords.y;
//         tb.parent_offset = (Vector2d){tl_w + label_tbox_offset.x, 0};
//     }
//     else
//     {
//         // Stacked Layout: Label is above the TextBox
//         // Label takes full width, height is determined by the offset
//         tl.width = width;
//         tl.height = label_tbox_offset.y;
//         tl.origin = origin_coords;
//         tl.parent_offset = (Vector2d){0, 0};

//         // TextBox takes full width, starts below the label offset
//         tb.width = width;
//         tb.height = height - label_tbox_offset.y;
//         tb.origin.x = origin_coords.x;
//         tb.origin.y = origin_coords.y + label_tbox_offset.y;
//         tb.parent_offset = (Vector2d){0, label_tbox_offset.y};
//     }

//     // Assign the sub-structs to the main TextField
//     ((UIElement *)(tf->children.items))[0] = tl;
//     ((UIElement *)(tf->children.items))[1] = tb;

//     return tf;
// }

// TextBox *CreateTextBox(float width, float height, Vector2d origin_coords, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill)
// {
//     TextBox *t = AllocateBytes(sizeof(TextBox));
//     t->origin = origin_coords;
//     t->colour_fill = colour_fill;
//     t->colour_border = colour_border;
//     t->width = width;
//     t->height = height;
//     t->padding = padding;
//     t->is_focused = false;
//     t->is_read_only = false;
//     t->cursor_pos = 0;
//     TextField *parent; // The "Backlink" for bubbling up changes to the text field container (e.g., for re-rendering when text changes)

//     return t;
// }

// TextFieldsContainer *CreateTextFieldContainer(float width, float height, Vector2d origin_coords, Vector2d padding, Vector2d field_spacing, ColourRgba colour_border, ColourRgba colour_fill)
// {
//     TextFieldsContainer *tc = AllocateBytes(sizeof(TextFieldsContainer));
//     tc->origin = origin_coords;
//     tc->colour_fill = colour_fill;
//     tc->colour_border = colour_border;
//     tc->width = width;
//     tc->height = height;
//     tc->padding = padding;
//     tc->field_spacing = field_spacing;

//     tc->text_fields = *NEW_DYNAMIC_ARRAY(4, sizeof(TextField)); // Start with capacity for 4 text fields, will grow as needed
//     return tc;
// }

// ShortString GetText_TextField(TextField *text_box)
// {
//     // ShortString str[64] = {0};
//     // strncpy(str, text_box->text, sizeof(str) - 1); // Copy text with safety check to prevent overflow

//     // return str;
// }

// int MeasureTextWidth(const char *text, char font_spacing, char scale)
// {
//     if (text == NULL)
//         return 0;

//     int char_count = strlen(text);
//     if (char_count == 0)
//         return 0;

//     // 8 pixels for the char + 1 pixel for spacing = 9 total per char
//     // Note: We subtract the very last spacing pixel for a perfect fit
//     // (8 pixels per char + x pixel spacing) * scale
//     int width = char_count * ((8 * scale) + (scale * font_spacing));
//     // int width = char_count * ((8 + font_spacing) * scale); (8 * scale) + (scale * font.spacing);
//     //  int width = (char_count * (8 * scale));// + ((char_count - 1) * scale);

//     return width;
// }

// Vector2d GetTextCenterPos(const char *text, float fontSize, Vector2d origin)
// {
//     // 1. Calculate the center of the specific cell (c, r)
//     // float centerX = origin.x + (c + 0.5f) * u.x + (r + 0.5f) * v.x;
//     // float centerY = origin.y + (c + 0.5f) * u.y + (r + 0.5f) * v.y;

//     // // 2. Measure the text dimensions
//     // Vector2d textSize = MeasureTextEx(font, text, fontSize, 1.0f);

//     // // 3. Subtract half dimensions to get the starting (top-left) point
//     // Vector2d startPos;
//     // startPos.x = centerX - (textSize.x / 2.0f);
//     // startPos.y = centerY - (textSize.y / 2.0f);

//     // return startPos;
// }

// // Custom text drawing function that uses our Bitmap_Font and supports scaling and color. Coordinate origin is the top-left corner of the text in pixels.
// float DrawTextCustom(const char *text, float origin_x, float origin_y, char scale, Bitmap_Font font, ColourRgba colour)
// {
//     float current_x = origin_x;
//     while (*text)
//     {
//         DrawChar(*text, current_x, origin_y, scale, font, colour);

//         // Move to the next character slot
//         // 8 pixels wide * scale + 1 pixel of "letter spacing"
//         current_x += (8 * scale) + (scale * font.spacing); // - (scale * scale) + 1; // Subtracting "scale" removes embedded white-space in each character
//         text++;
//     }
//     return current_x;
// }

// void DrawChar(char c, float origin_x, float origin_y, int scale, Bitmap_Font font, ColourRgba colour)
// {
//     // Cast to unsigned to handle extended ASCII safely
//     unsigned char u_c = (unsigned char)c;

//     for (int row = 0; row < 8; row++)
//     {
//         unsigned char row_data = font.bitmap[u_c][row];

//         for (int col = 0; col < 8; col++)
//         {
//             // We use a bitmask (0x80 is 10000000) and shift it right to check each bit in the byte.
//             // The >> is the Bitwise Right Shift. It literally pushes the bits to the right by the number of places specified by col.
//             if (row_data & (0x80 >> col))
//             {
//                 // If the bit is 1, draw a "pixel" scaled up
//                 DrawRectangle(origin_x + (col * scale), origin_y + (row * scale), scale, scale, (Color){colour.r, colour.g, colour.b, colour.a});
//             }
//         }
//     }
// }

// void GetTextFieldVertices(TextField text_box, Vector2d out_vertices[4])
// {
//     out_vertices[0] = text_box.origin;
//     out_vertices[1] = (Vector2d){text_box.origin.x + text_box.width, text_box.origin.y};
//     out_vertices[2] = (Vector2d){text_box.origin.x + text_box.width, text_box.origin.y + text_box.height};
//     out_vertices[3] = (Vector2d){text_box.origin.x, text_box.origin.y + text_box.height};
// }

// void EvalChildrenOriginsFromParentOffset(TextFieldsContainer *tfcont)
// {
//     // TextFieldsContainer origin and additional space + offsets that will affect the origin of its children
//     // There is no TextField padding (currently), so nothing funnels down to children, just the predetermined parent_offsets specified when the children were initialised
//     Vector2d tfcont_origin = tfcont->origin;
//     Collection tfield_coll = tfcont->text_fields.coll;

//     for (int i = 0; i < tfield_coll.count; i++)
//     {
//         TextField *tfield = ((TextField **)tfield_coll.items)[i];
//         Vector2d tfield_origin_total = VectorSum_2d(VectorSum_2d(tfield->parent_offset, tfcont->origin), tfcont->padding); // offset rel to parent (container) + parent padding
//         tfield->origin = tfield_origin_total;
//     }
// }

// void EvalTextFieldChildrenOrigins(TextField *text_field)
// {
//     // TextField origin and additional space + offsets that will affect the origin of its children
//     // There is no TextField padding (currently), so nothing funnels down to children, just the predetermined parent_offsets specified when the children were initialised
//     Vector2d tfield_origin = text_field->origin;
//     Vector2d tbox_parent_offset = text_field->text_box.parent_offset;
//     Vector2d tlabel_parent_offset = text_field->label.parent_offset;

//     text_field->text_box.origin = VectorSum_2d(tfield_origin, tbox_parent_offset);
//     text_field->label.origin = VectorSum_2d(tfield_origin, tlabel_parent_offset);
// }

// bool DisposeUITree(UIElement *e)
// {
//     // Remove from parent's children array
//     if (e->parent != NULL)
//     {
//         LArray *siblings = &e->parent->children;
//         for (size_t i = 0; i < siblings->count; i++)
//         {
//             if (((UIElement **)siblings->items)[i] == e)
//             {
//                 LArray_RemoveAt(siblings, i);
//                 break;
//             }
//         }
//     }

//     // Free the element's own resources
//     // Note: If the element has its own children, you may want to recursively destroy them here as well. We will.
//     for (size_t i = 0; i < e->children.count; i++)
//     {
//         DisposeUIElement(((UIElement **)e->children.items)[i]);
//     }
//     free(e);
//     return true;
// }