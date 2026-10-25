/**********************************************************************************************
*
UI ICONS MODULE (implementation)
*
One home and one visual language for the toolbar's procedural glyphs. The relocated Nudge tool
glyph plus nine toggle glyphs, each attached to a button's UIElement.on_draw hook by id in
InitToolbar. Every glyph is centred in e->screen_box, sized off the box HEIGHT (the toolbar tiles
are wide and short, so height is the limiting axis), monochrome, and composed from the same raylib
primitives the Nudge glyph has always used. All ten buttons share one flat surface=input base; the
ON/OFF state of each toggle is shown the Nudge way, through the GLYPH INK (bright button_fill when
ON, normal text ink when OFF via UIIcon_StateInk), not an amber box-fill swap.
*
**********************************************************************************************/
#include "system/ui/ui_icons.h"

#include <math.h> // fmaxf / cosf / sinf

#include "raylib.h"                 // DrawLineEx / DrawRectangleLinesEx / DrawCircleV, Vector2 / Color
#include "system/draw_primitives.h" // ToRaylibColor (ColourRgba -> raylib Color)
#include "system/ui_system.h"       // ui_default_palette, UIState_IsToolActive, TOOL_NUDGE
#include "common/common.h"          // PI (float)
#include "ui/ui.h"                  // UIElement / UIBox
#include "ui/binding.h"             // Binding_ReadSource / BindingValue / BIND_SRC_NONE (toggle truth)

// ============================================================================
// Shared glyph helpers (file-local)
// ============================================================================

// A centred, square glyph box fitted inside a (wider) element box, plus a derived stroke width.
// The glyph is sized off the element HEIGHT so it stays proportional on the wide-short toolbar
// tiles, exactly like the Nudge glyph. cx/cy are the glyph centre, half is half the glyph extent.
typedef struct UIIconFit
{
    float cx;     // Centre x (pixels).
    float cy;     // Centre y (pixels).
    float half;   // Half the glyph's square extent (pixels).
    float stroke; // Line stroke width shared by the glyph's primitives.
} UIIconFit;

// Compute the centred glyph fit for an element, sizing off box HEIGHT (glyph_size = box_h * 0.8f)
// and the Nudge stroke rule (stroke = max(1.5, glyph_size * 0.12)). Returns false (leaving *out
// untouched) when the element is null or the glyph would be sub-pixel, so each glyph can early-out
// just like the Nudge glyph does.
static bool UIIcon_Fit(const UIElement *e, UIIconFit *out)
{
    if (!e || !out)
    {
        return false;
    }

    float box_x = e->screen_box.coords.x;
    float box_y = e->screen_box.coords.y;
    float box_w = e->screen_box.dimensions.x;
    float box_h = e->screen_box.dimensions.y;

    float glyph_size = box_h * 0.8f; // Composition height; width is the full (wider) box.
    if (glyph_size < 1.0f)
    {
        return false; // Too small to render legibly.
    }

    out->cx = box_x + box_w * 0.5f;        // Centre horizontally in the wide tile.
    out->cy = box_y + box_h * 0.5f;        // Centre vertically.
    out->half = glyph_size * 0.5f;         // Half the square glyph extent.
    out->stroke = fmaxf(1.5f, glyph_size * 0.12f);
    return true;
}

// Resolve the ink a toggle glyph draws in, mirroring the Nudge button EXACTLY: the glyph sits on a
// flat surface=input box (no amber fill swap), so ON/OFF is conveyed by the ink alone - bright
// button_fill when the toggle is ON, normal text ink when OFF. The toggle's live on/off truth is
// read from its OWN binding source (the same IsDebugEnabled query the per-frame refresh uses), so
// the ink can never drift from the binding and no suffix->id map is duplicated. A button with no
// readable source, or a non-int / zero read, falls back to the normal text ink - the same fallback
// the Nudge glyph uses when inactive. The element carries no palette handle, so ui_default_palette
// is read directly, matching the toolbar's existing fallback. The Nudge glyph does NOT use this
// helper: it owns its own ink path (button_fill/text, keyed on UIState_IsToolActive) in-line.
static Color UIIcon_StateInk(const UIElement *e)
{
    bool on = false; // OFF unless the toggle's own binding reports a non-zero (ON) value.
    // Button-type check is inlined (not IsBtn, which takes a non-const pointer) so this helper can
    // keep its const UIElement * signature; it mirrors IsBtn's three button-type cases exactly.
    bool is_button = e && (e->type == UI_ELEMENT_BUTTON_SIMPLE ||
                           e->type == UI_ELEMENT_BUTTON_ENUMERATE ||
                           e->type == UI_ELEMENT_BUTTON_SUBMIT);
    if (is_button)
    {
        const Binding *binding = e->data.button.binding;
        if (binding && binding->source.kind != BIND_SRC_NONE)
        {
            BindingValue value = Binding_ReadSource(&binding->source);
            on = (value.type != BIND_NONE && value.as.i != 0);
        }
    }

    // Identical inks to the Nudge button: bright amber ON, normal near-black ink OFF.
    return on ? ToRaylibColor(ui_default_palette.button_fill)
              : ToRaylibColor(ui_default_palette.text);
}

// ============================================================================
// Relocated Nudge tool glyph
// ============================================================================

// Draw the Nudge pictogram inside the element's box: an arrow pushing a small box with a couple
// of motion dots trailing behind it. Monochrome, composed from raylib primitives and centred in
// e->screen_box with padding. Attached via UIElement.on_draw so the renderer calls it on top of
// the (flat) button box; no label is drawn because the button's text is empty. The ink follows
// the active-tool state directly (bright button_fill when active, normal text ink otherwise)
// because the Nudge button has no binding= and thus no toggle fill to read.
void UIIcon_DrawNudge(UIElement *e)
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

    // Size the glyph off the box HEIGHT (the limiting axis for the wide-short tile).
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
    // button fill when Nudge is the active tool, otherwise the normal text ink.
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
    float arrow_tip_x   = glyph_x + glyph_w * 0.32f;
    float box_left      = glyph_x + glyph_w * 0.38f;
    float box_right     = glyph_x + glyph_w * 0.62f;

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
    float box_half = glyph_size * 0.24f; // Half the box height (square box).
    Rectangle box_rect = { box_left, cy - box_half, box_right - box_left, box_half * 2.0f };
    DrawRectangleLinesEx(box_rect, stroke, ink);

    // --- Motion dots: two small filled dots trailing to the right of the box. ---
    float dot_radius = fmaxf(1.0f, glyph_size * 0.09f);
    float dot1_x = glyph_x + glyph_w * 0.7f;
    float dot2_x = glyph_x + glyph_w * 0.85f;
    DrawCircleV((Vector2){dot1_x, cy}, dot_radius, ink);
    DrawCircleV((Vector2){dot2_x, cy}, dot_radius, ink);
}

// ============================================================================
// Cell paint tool glyph
// ============================================================================

// Draw the Cell Paint pictogram: a small 2x2 grid of cells with one cell filled, reading as
// "paint a cell". Monochrome, centred in e->screen_box, composed from raylib primitives with no
// heap allocation. Like the Nudge glyph the ink follows the active-tool state directly (bright
// button_fill when TOOL_CELL_PAINT is active, normal text ink otherwise) because this button has
// no binding= to read. Attached via UIElement.on_draw, mirroring the other toolbar glyphs.
void UIIcon_DrawCellPaint(UIElement *e)
{
    UIIconFit f;
    if (!UIIcon_Fit(e, &f))
    {
        return;
    }

    // Active-tool feedback is the icon's ink (no text toggle on this button).
    bool active = UIState_IsToolActive(TOOL_CELL_PAINT);
    Color ink = active ? ToRaylibColor(ui_default_palette.button_fill)
                       : ToRaylibColor(ui_default_palette.text);

    // 2x2 grid geometry centred on the fit.
    float left = f.cx - f.half;
    float top = f.cy - f.half;
    float span = f.half * 2.0f;
    float half_span = span * 0.5f;

    // Outer frame.
    Rectangle frame = { left, top, span, span };
    DrawRectangleLinesEx(frame, f.stroke, ink);

    // Interior divider lines (one vertical, one horizontal) splitting the frame into 2x2 cells.
    DrawLineEx((Vector2){ f.cx, top }, (Vector2){ f.cx, top + span }, f.stroke, ink);
    DrawLineEx((Vector2){ left, f.cy }, (Vector2){ left + span, f.cy }, f.stroke, ink);

    // One filled cell (top-left quadrant) to read as "a painted cell". Inset slightly so the fill
    // sits inside the stroked borders rather than overlapping them.
    float inset = f.stroke;
    Rectangle filled = { left + inset, top + inset, half_span - inset * 1.5f, half_span - inset * 1.5f };
    DrawRectangleRec(filled, ink);
}

// ============================================================================
// Toggle glyphs (ink from UIIcon_StateInk: bright button_fill ON / normal text ink OFF)
// ============================================================================

// Dashboard (debug.dashboard): a gauge dial. A thin ring (circle outline, approximated by a
// slightly larger filled disc with a smaller background-coloured disc punched out is avoided to
// keep a single ink; instead a ring is drawn as a line loop around the rim) plus one short radial
// needle from the centre to the upper-right rim. Reads as a speedometer/dashboard dial.
void UIIcon_DrawDashboard(UIElement *e)
{
    UIIconFit f;
    if (!UIIcon_Fit(e, &f))
    {
        return;
    }
    Color ink = UIIcon_StateInk(e);
    
    // Dial rim: a ring drawn as a closed line loop of N points around the centre (a single-ink
    // outline, respecting the stroke width without a filled disc).
    float radius = f.half * 0.85f;
    const int sides = 16; // Enough to read as a smooth circle at tile size.
    Vector2 prev = { f.cx + radius, f.cy };
    for (int i = 1; i <= sides; i++)
    {
        float a = (float)i / (float)sides * 2.0f * PI;
        Vector2 cur = { f.cx + cosf(a) * radius, f.cy + sinf(a) * radius };
        DrawLineEx(prev, cur, f.stroke, ink);
        prev = cur;
    }

    // Needle: a short radial line from the centre to the upper-right rim (about -45 degrees).
    float needle = radius * 0.9f;
    Vector2 tip = { f.cx + cosf(-PI * 0.25f) * needle, f.cy + sinf(-PI * 0.25f) * needle };
    DrawLineEx((Vector2){ f.cx, f.cy }, tip, f.stroke, ink);
}

// Viewport Grid (view.viewport-grid): a screen frame (one rect outline) with a single centred
// cross (one H + one V line). Reads as "one screen, one divider" - the coarsest grid-family glyph.
void UIIcon_DrawViewportGrid(UIElement *e)
{
    UIIconFit f;
    if (!UIIcon_Fit(e, &f))
    {
        return;
    }
    Color ink = UIIcon_StateInk(e);

    // Screen frame.
    Rectangle frame = { f.cx - f.half, f.cy - f.half, f.half * 2.0f, f.half * 2.0f };
    DrawRectangleLinesEx(frame, f.stroke, ink);

    // Single centred cross.
    DrawLineEx((Vector2){ f.cx, f.cy - f.half }, (Vector2){ f.cx, f.cy + f.half }, f.stroke, ink);
    DrawLineEx((Vector2){ f.cx - f.half, f.cy }, (Vector2){ f.cx + f.half, f.cy }, f.stroke, ink);
}

// Draw a 3x3 grid (outline + two interior H + two interior V lines) centred on the fit. Shared by
// World Grid and World Grid Labels so the dense grid reads identically; the Labels variant adds
// tick dots on top. interior intersection coordinates are the thirds of the frame.
static void UIIcon_DrawGrid3x3(const UIIconFit *f, Color ink)
{
    float left = f->cx - f->half;
    float top = f->cy - f->half;
    float span = f->half * 2.0f;
    float third = span / 3.0f;

    // Outer frame.
    Rectangle frame = { left, top, span, span };
    DrawRectangleLinesEx(frame, f->stroke, ink);

    // Two interior vertical lines and two interior horizontal lines at the thirds.
    for (int i = 1; i <= 2; i++)
    {
        float vx = left + third * (float)i;
        float hy = top + third * (float)i;
        DrawLineEx((Vector2){ vx, top }, (Vector2){ vx, top + span }, f->stroke, ink);
        DrawLineEx((Vector2){ left, hy }, (Vector2){ left + span, hy }, f->stroke, ink);
    }
}

// World Grid (view.world-grid): a dense 3x3 grid, no ticks. Distinguished from Viewport (single
// cross) by density and from Universe (one coarse cell) by its finer cells.
void UIIcon_DrawWorldGrid(UIElement *e)
{
    UIIconFit f;
    if (!UIIcon_Fit(e, &f))
    {
        return;
    }
    UIIcon_DrawGrid3x3(&f, UIIcon_StateInk(e));
}

// World Grid Labels (view.world-grid-labels): the dense 3x3 grid plus small tick dots at the four
// interior intersections, connoting labels. The ticks separate it from the plain World Grid glyph.
void UIIcon_DrawWorldGridLabels(UIElement *e)
{
    UIIconFit f;
    if (!UIIcon_Fit(e, &f))
    {
        return;
    }
    Color ink = UIIcon_StateInk(e);
    UIIcon_DrawGrid3x3(&f, ink);

    // Tick dots at the four interior intersections (the 1/3 and 2/3 crossings).
    float left = f.cx - f.half;
    float top = f.cy - f.half;
    float third = (f.half * 2.0f) / 3.0f;
    float dot = fmaxf(1.0f, f.half * 0.1f); // ~glyph_size * 0.05f.
    for (int gx = 1; gx <= 2; gx++)
    {
        for (int gy = 1; gy <= 2; gy++)
        {
            DrawCircleV((Vector2){ left + third * (float)gx, top + third * (float)gy }, dot, ink);
        }
    }
}

// Universe Grid Labels (view.universe-grid-labels): a coarse single-cell grid (one big rect + one
// centred cross) plus tick dots at the cross centre and the four corners. Distinguished from World
// by coarseness (one cell vs 3x3) and from Viewport by the tick dots.
void UIIcon_DrawUniverseGridLabels(UIElement *e)
{
    UIIconFit f;
    if (!UIIcon_Fit(e, &f))
    {
        return;
    }
    Color ink = UIIcon_StateInk(e);

    // Coarse single cell + centred cross.
    Rectangle frame = { f.cx - f.half, f.cy - f.half, f.half * 2.0f, f.half * 2.0f };
    DrawRectangleLinesEx(frame, f.stroke, ink);
    DrawLineEx((Vector2){ f.cx, f.cy - f.half }, (Vector2){ f.cx, f.cy + f.half }, f.stroke, ink);
    DrawLineEx((Vector2){ f.cx - f.half, f.cy }, (Vector2){ f.cx + f.half, f.cy }, f.stroke, ink);

    // Tick dots: centre + four corners (coarse, labelled).
    float dot = fmaxf(1.0f, f.half * 0.1f);
    DrawCircleV((Vector2){ f.cx, f.cy }, dot, ink);
    DrawCircleV((Vector2){ f.cx - f.half, f.cy - f.half }, dot, ink);
    DrawCircleV((Vector2){ f.cx + f.half, f.cy - f.half }, dot, ink);
    DrawCircleV((Vector2){ f.cx - f.half, f.cy + f.half }, dot, ink);
    DrawCircleV((Vector2){ f.cx + f.half, f.cy + f.half }, dot, ink);
}

// UI Borders (view.ui-borders): a visibly THICK rect outline with a thinner concentric inset rect,
// reading as "borders on". The double outline distinguishes it from the plain AABB rect.
void UIIcon_DrawUIBorders(UIElement *e)
{
    UIIconFit f;
    if (!UIIcon_Fit(e, &f))
    {
        return;
    }
    Color ink = UIIcon_StateInk(e);

    // Outer frame at a thick stroke.
    Rectangle outer = { f.cx - f.half, f.cy - f.half, f.half * 2.0f, f.half * 2.0f };
    DrawRectangleLinesEx(outer, f.stroke * 2.0f, ink);

    // Inner concentric inset frame at the base stroke.
    float inset = f.half * 0.55f;
    Rectangle inner = { f.cx - inset, f.cy - inset, inset * 2.0f, inset * 2.0f };
    DrawRectangleLinesEx(inner, f.stroke, ink);
}

// Object Axes (gizmo.object-axes): two perpendicular arrows from a common origin at the glyph's
// lower-left - a horizontal (X) and a vertical (Y) shaft, each capped with two short diagonal
// arrowhead strokes (reusing the Nudge arrowhead construction). Reads as a coordinate gizmo.
void UIIcon_DrawObjectAxes(UIElement *e)
{
    UIIconFit f;
    if (!UIIcon_Fit(e, &f))
    {
        return;
    }
    Color ink = UIIcon_StateInk(e);

    // Common origin at the lower-left of the glyph box.
    Vector2 origin = { f.cx - f.half, f.cy + f.half };
    Vector2 x_tip = { f.cx + f.half, f.cy + f.half }; // X shaft -> right.
    Vector2 y_tip = { f.cx - f.half, f.cy - f.half };  // Y shaft -> up.

    DrawLineEx(origin, x_tip, f.stroke, ink);
    DrawLineEx(origin, y_tip, f.stroke, ink);

    // Arrowheads: two short diagonal strokes back from each tip.
    float head = f.half * 0.35f;
    // X arrowhead (pointing right).
    DrawLineEx(x_tip, (Vector2){ x_tip.x - head, x_tip.y - head }, f.stroke, ink);
    DrawLineEx(x_tip, (Vector2){ x_tip.x - head, x_tip.y + head }, f.stroke, ink);
    // Y arrowhead (pointing up).
    DrawLineEx(y_tip, (Vector2){ y_tip.x - head, y_tip.y + head }, f.stroke, ink);
    DrawLineEx(y_tip, (Vector2){ y_tip.x + head, y_tip.y + head }, f.stroke, ink);
}

// Object Hull (gizmo.object-hull): a convex polygon outline (a hexagon). The vertices are computed
// inline around the glyph centre (no heap allocation, so CreateVertices_* is intentionally not
// used) and drawn as a closed DrawLineEx loop so the outline respects the stroke width.
void UIIcon_DrawObjectHull(UIElement *e)
{
    UIIconFit f;
    if (!UIIcon_Fit(e, &f))
    {
        return;
    }
    Color ink = UIIcon_StateInk(e);

    const int sides = 6;             // Hexagon.
    float radius = f.half * 0.95f;
    float start = -PI * 0.5f;        // First vertex at the top for a flat-ish, upright hull.

    Vector2 first = { f.cx + cosf(start) * radius, f.cy + sinf(start) * radius };
    Vector2 prev = first;
    for (int i = 1; i < sides; i++)
    {
        float a = start + (float)i / (float)sides * 2.0f * PI;
        Vector2 cur = { f.cx + cosf(a) * radius, f.cy + sinf(a) * radius };
        DrawLineEx(prev, cur, f.stroke, ink);
        prev = cur;
    }
    DrawLineEx(prev, first, f.stroke, ink); // Close the loop.
}

// Object AABB (gizmo.object-aabb): an axis-aligned rect outline (base stroke) with a small filled
// dot centred inside it. The inner dot distinguishes it from UI Borders (thick/concentric frame).
void UIIcon_DrawObjectAABB(UIElement *e)
{
    UIIconFit f;
    if (!UIIcon_Fit(e, &f))
    {
        return;
    }
    Color ink = UIIcon_StateInk(e);

    Rectangle box = { f.cx - f.half, f.cy - f.half, f.half * 2.0f, f.half * 2.0f };
    DrawRectangleLinesEx(box, f.stroke, ink);

    float dot = fmaxf(1.5f, f.half * 0.22f);
    DrawCircleV((Vector2){ f.cx, f.cy }, dot, ink);
}
