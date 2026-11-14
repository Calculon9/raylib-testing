/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 **********************************************************************************************/
#include "raylib.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "camera/camera.h"
#include "ui/ui.h"
#include "ui/text_region.h"
#include "system/draw_primitives.h"
#include "system/ui_system.h"
#include "system/debug_overlay_system.h"
#include "system/systems.h"
#include "system/utility_system.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
//
void DrawUIElement(UIElement *e, UIBox parent_box, Matrix3x3 M_ui_to_pixel, UIClipRect current_clip);
void DrawTextArea(UIElement *e);
void DrawTextAreaClipped(UIElement *e, UIClipRect clip);

// Screen pixels per UI pixel for the tree currently being drawn. UI content is laid out in UI
// pixels; DrawRootUIElement derives this integer factor from the root's UI-to-pixel transform so
// text glyphs, border lines and text insets are drawn at the same crisp integer scale as the boxes.
static int s_ui_pixel_scale = 1;

// Consume one wrapped text row and optionally copy its characters into a draw buffer.
static int ConsumeTextRow(const char **text_cursor, float available_width,
                          int glyph_advance, int glyph_cell_width,
                          char *row_buffer, size_t row_buffer_capacity,
                          float *out_row_width)
{
    if (!text_cursor || !*text_cursor || available_width <= 0.0f ||
        glyph_advance <= 0 || glyph_cell_width <= 0)
    {
        return 0;
    }

    const char *cursor = *text_cursor;
    int row_char_count = 0;
    int max_row_chars = row_buffer ? (int)row_buffer_capacity - 1 : 255;
    if (max_row_chars <= 0)
    {
        return 0;
    }

    float row_width = 0.0f;
    while (*cursor != '\0' && row_char_count < max_row_chars)
    {
        int char_width = row_char_count == 0 ? glyph_cell_width : glyph_advance;
        if (row_width + (float)char_width > available_width)
        {
            break;
        }

        if (row_buffer)
        {
            row_buffer[row_char_count] = *cursor;
        }
        row_width += (float)char_width;
        row_char_count++;
        cursor++;
    }

    if (row_buffer)
    {
        row_buffer[row_char_count] = '\0';
    }
    if (out_row_width)
    {
        *out_row_width = row_width;
    }

    *text_cursor = cursor;
    return row_char_count;
}

static int CountTextRows(const char *text, float available_width, int glyph_advance, int glyph_cell_width, int max_rows)
{
    const char *text_cursor = text;
    int row_count = 0;

    while (text_cursor && *text_cursor != '\0' && row_count < max_rows)
    {
        if (ConsumeTextRow(&text_cursor, available_width, glyph_advance,
                           glyph_cell_width, NULL, 0, NULL) == 0)
        {
            break;
        }

        row_count++;
    }

    return row_count;
}

// Faint outline drawn by the UI-borders debug overlay (F3) around elements that carry no
// visible border of their own, so the overlay still shows every element's bounds. Its colour is
// the active palette's debug_outline role.
// Horizontal text inset inside a bordered textbox: the border weight + a 4 px gap (5 px for the
// default 1 px border). The glyph's own 2 px blank column then places the first ink 7 px from the
// box edge, as in the mock.
#define UI_TEXTBOX_TEXT_GAP_PX 4.0f
// Glyphs in the "..." appended to read-only text that does not fit its box.
#define UI_ELLIPSIS_CHAR_COUNT 3

// Is this element a visible read-only text field? Read-only textboxes (UI_ELEMENT_TEXTBOX_O) that
// carry a visible box (fill and border both non-transparent) are drawn in the palette read-only
// style. Transparent read-only textboxes (panel headers, notes) have no box and stay untouched.
// This is the one place the read-only look is decided, so it applies to every panel and to fields
// locked at run time (multi-select) alike.
static bool IsReadOnlyFieldBox(const UIElement *e)
{
    return e->type == UI_ELEMENT_TEXTBOX_O && e->colour_border.a > 0 && e->colour_fill.a > 0;
}

// Resolve the fill to draw for an element: the palette read-only fill for a read-only text field,
// otherwise the element's own fill.
static ColourRgba ResolveDrawnFill(const UIElement *e)
{
    return IsReadOnlyFieldBox(e) ? UIPalette_GetRoleColours(UIPalette_GetActive(), UI_ROLE_READONLY_INPUT).fill : e->colour_fill;
}

// Resolve the border colour to draw for an element (style v2 border model). A border is
// drawn whenever its alpha is non-zero, independent of the debug overlay; constructors decide
// which elements carry one. With the UI-borders overlay on, borderless elements get the faint
// debug outline instead, so the overlay still shows element bounds. Read-only text fields use the
// palette read-only border.
static ColourRgba ResolveDrawnBorderColour(const UIElement *e)
{
    // Read-only text fields use the palette read-only border (same 1 px geometry as editable ones).
    if (IsReadOnlyFieldBox(e))
    {
        return UIPalette_GetRoleColours(UIPalette_GetActive(), UI_ROLE_READONLY_INPUT).border;
    }
    ColourRgba colour_border = e->colour_border;
    if (colour_border.a == 0 && IsDebugEnabled(DEBUG_UI_BORDERS))
    {
        colour_border = UIPalette_GetActive()->debug_outline;
    }
    return colour_border;
}

// Resolve the border thickness to draw, in pixels. The element's weight is clamped to half the
// shorter side so opposite edges never overdraw; the faint debug outline of a borderless element
// is always 1 px so the overlay does not change with the weight.
static float ResolveDrawnBorderWeight(const UIElement *e, const UIBox *box)
{
    // Authored weights are UI pixels; the debug outline is always one screen pixel.
    float weight = (e->colour_border.a == 0) ? 1.0f : (float)(e->border_weight * s_ui_pixel_scale);
    float half_short_side = fminf(box->dimensions.x, box->dimensions.y) * 0.5f;
    return fminf(weight, half_short_side);
}

// Draw an element's background fill and its border (drawn inside the box, so a bordered
// element keeps its layout size). A zero border weight draws no line.
void DrawElementBox(UIElement *e)
{
    UIBox box = e->screen_box;
    ColourRgba colour_fill = ResolveDrawnFill(e);
    ColourRgba colour_border = ResolveDrawnBorderColour(e);
    float weight = ResolveDrawnBorderWeight(e, &box);
    DrawRectangleRec((Rectangle){box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y},
                     ToRaylibColor(colour_fill));
    if (weight > 0.0f)
    {
        DrawRectangleLinesEx((Rectangle){box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y},
                             weight, ToRaylibColor(colour_border));
    }
}
// Draw an element's background and border clipped against an active clip rectangle.
void DrawElementBoxClipped(UIElement *e, UIClipRect clip)
{
    if (!e || !UIClipRect_IntersectsBox(clip, e->screen_box))
    {
        return;
    }

    if (UIClipRect_ContainsBox(clip, e->screen_box))
    {
        DrawElementBox(e);
        return;
    }

    UIBox clipped_box = UIBox_Clip(e->screen_box, clip);
    if (clipped_box.dimensions.x <= 0.0f || clipped_box.dimensions.y <= 0.0f)
    {
        return;
    }

    ColourRgba colour_fill = ResolveDrawnFill(e);
    ColourRgba colour_border = ResolveDrawnBorderColour(e);

    // Draw the clipped background fill.
    if (colour_fill.a > 0)
    {
        DrawRectangleRec((Rectangle){clipped_box.coords.x, clipped_box.coords.y,
                                     clipped_box.dimensions.x, clipped_box.dimensions.y},
                         ToRaylibColor(colour_fill));
    }

    // Only draw borders along the original element edges if they fall within the clip rect.
    float weight = ResolveDrawnBorderWeight(e, &e->screen_box);
    if (colour_border.a > 0 && weight > 0.0f)
    {
        Color border_color = ToRaylibColor(colour_border);
        const float epsilon = 0.5f;

        // Draw top border if not clipped away.
        if (fabsf(clipped_box.coords.y - e->screen_box.coords.y) <= epsilon)
        {
            DrawRectangleRec((Rectangle){clipped_box.coords.x, clipped_box.coords.y,
                                         clipped_box.dimensions.x, fminf(weight, clipped_box.dimensions.y)},
                             border_color);
        }

        // Draw bottom border if not clipped away.
        if (fabsf((clipped_box.coords.y + clipped_box.dimensions.y) -
                  (e->screen_box.coords.y + e->screen_box.dimensions.y)) <= epsilon)
        {
            DrawRectangleRec((Rectangle){clipped_box.coords.x,
                                         clipped_box.coords.y + clipped_box.dimensions.y - fminf(weight, clipped_box.dimensions.y),
                                         clipped_box.dimensions.x, fminf(weight, clipped_box.dimensions.y)},
                             border_color);
        }

        // Draw left border if not clipped away.
        if (fabsf(clipped_box.coords.x - e->screen_box.coords.x) <= epsilon)
        {
            DrawRectangleRec((Rectangle){clipped_box.coords.x, clipped_box.coords.y,
                                         fminf(weight, clipped_box.dimensions.x), clipped_box.dimensions.y},
                             border_color);
        }

        // Draw right border if not clipped away.
        if (fabsf((clipped_box.coords.x + clipped_box.dimensions.x) -
                  (e->screen_box.coords.x + e->screen_box.dimensions.x)) <= epsilon)
        {
            DrawRectangleRec((Rectangle){clipped_box.coords.x + clipped_box.dimensions.x - fminf(weight, clipped_box.dimensions.x),
                                         clipped_box.coords.y,
                                         fminf(weight, clipped_box.dimensions.x), clipped_box.dimensions.y},
                             border_color);
        }
    }
}

// Horizontal gap (UI px) between a stacked vector box's "x" / "y" prefix and its value.
#define UI_VECTOR_PREFIX_ADVANCE_PX 16.0f
// Gap (UI px) kept between a field's value text and its right-aligned unit suffix.
#define UI_FIELD_UNIT_GAP_PX 4.0f

// Rewrite the exponent of a "%e" string in place as the compact form: "1.23e+05" -> "1.23e5",
// "1.2e-07" -> "1.2e-7" (sign kept only when negative, leading zeros dropped).
static void CompactExponent(char *text, size_t text_bytes)
{
    char *marker = strchr(text, 'e');
    if (!marker || marker[1] == '\0')
    {
        return;
    }
    bool negative = (marker[1] == '-');
    const char *digits = marker + 2;
    while (*digits == '0' && digits[1] != '\0')
    {
        digits++;
    }
    // Copy the digits out first: the rewrite below overwrites the buffer they live in.
    char exponent[8];
    snprintf(exponent, sizeof(exponent), "%s%s", negative ? "-" : "", digits);
    size_t mantissa_bytes = (size_t)(marker - text) + 1; // mantissa plus the 'e'
    if (mantissa_bytes < text_bytes)
    {
        snprintf(marker + 1, text_bytes - mantissa_bytes, "%s", exponent);
    }
}

// Fit an unfocused numeric value into `max_chars` glyphs. Only text that parses fully as a number
// and is too long is rewritten (non-numeric text such as "N/A" is left alone): first with fewer
// decimal places, then, when even the integer part is too wide, in compact scientific notation
// (for example 1.23e5, dropping mantissa digits as needed). Returns true and fills `out` when the
// text was rewritten. The stored text is never changed, so a focused field still shows the full value.
static bool FitNumericText(const char *text, int max_chars, char *out, size_t out_bytes)
{
    size_t length = strlen(text);
    if (max_chars <= 0 || (int)length <= max_chars)
    {
        return false;
    }

    char *end = NULL;
    double value = strtod(text, &end);
    if (end == text || *end != '\0')
    {
        return false;
    }

    // Step 1: shed decimal places, one at a time, until the text fits.
    const char *point = strchr(text, '.');
    int decimals = point ? (int)(length - (size_t)(point - text) - 1) : 0;
    for (int places = decimals - 1; places >= 0; places--)
    {
        snprintf(out, out_bytes, "%.*f", places, value);
        if ((int)strlen(out) <= max_chars)
        {
            return true;
        }
    }

    // Step 2: scientific notation, keeping as many mantissa digits as fit (the last attempt, with
    // no mantissa decimals, is the shortest form and is used even if the field is very narrow).
    for (int places = 2; places >= 0; places--)
    {
        snprintf(out, out_bytes, "%.*e", places, value);
        CompactExponent(out, out_bytes);
        if ((int)strlen(out) <= max_chars)
        {
            break;
        }
    }
    return true;
}

// Split a formatted vector "(x,y)" into its two component strings. The no-value placeholder
// (UI_NO_VALUE_TEXT) is shown in BOTH components; any other text without a comma (for example
// "N/A") lands whole in `x_out` with an empty `y_out`.
static void SplitVectorText(const char *text, char *x_out, char *y_out, size_t out_bytes)
{
    const char *start = (text[0] == '(') ? text + 1 : text;
    const char *comma = strchr(start, ',');
    if (!comma)
    {
        snprintf(x_out, out_bytes, "%s", text);
        snprintf(y_out, out_bytes, "%s", strcmp(text, UI_NO_VALUE_TEXT) == 0 ? text : "");
        return;
    }
    size_t x_length = (size_t)(comma - start);
    if (x_length >= out_bytes)
    {
        x_length = out_bytes - 1;
    }
    memcpy(x_out, start, x_length);
    x_out[x_length] = '\0';
    snprintf(y_out, out_bytes, "%s", comma + 1);
    size_t y_length = strlen(y_out);
    if (y_length > 0 && y_out[y_length - 1] == ')')
    {
        y_out[y_length - 1] = '\0';
    }
}

// Draw `text` in the muted palette colour (muted_text, brown on cream) at a pixel position, clipped.
static void DrawMutedTextClipped(const char *text, Vector2d origin, UIClipRect clip)
{
    Bitmap_Font font = FONT_MEDIUM_WITH_COLOUR(UIPalette_GetActive()->muted_text);
    font.scale *= s_ui_pixel_scale;
    DrawTextCustomClipped(text, origin, font.scale, font, font.colour, clip);
}

// Draw the muted unit suffix (for example "rad") right-aligned inside a textbox, centred vertically.
// No-op when the element has no unit.
static void DrawFieldUnitClipped(const UIElement *e, UIClipRect clip)
{
    const char *unit = e->data.textbox.unit;
    if (unit[0] == '\0')
    {
        return;
    }
    Bitmap_Font font = e->data.textbox.font;
    font.scale *= s_ui_pixel_scale;
    int advance = BitmapFont_GetGlyphAdvance(&font);
    int cell = BitmapFont_GetGlyphCellWidth(&font);
    int row_height = BitmapFont_GetRowHeight(&font);
    if (advance <= 0 || row_height <= 0)
    {
        return;
    }
    float run_width = (float)(((int)strlen(unit) - 1) * advance + cell);
    float inset = ((float)e->border_weight + UI_TEXTBOX_TEXT_GAP_PX) * (float)s_ui_pixel_scale;
    Vector2d origin = {
        e->screen_box.coords.x + e->screen_box.dimensions.x - inset - run_width,
        e->screen_box.coords.y + fmaxf(0.0f, (e->screen_box.dimensions.y - (float)row_height) / 2.0f)};
    DrawMutedTextClipped(unit, origin, clip);
}

// Draw a stacked-vector textbox as two component boxes (x on the first row, y on the second, the
// same width, one control row each). Each box shows a muted "x" / "y" prefix and its value, which
// starts UI_VECTOR_PREFIX_ADVANCE_PX after the prefix. The element keeps ONE box, text buffer, binding
// and hit area (so editing, focus and refresh are unchanged); only its drawing is split. This
// temporarily points screen_box and the text buffer at each component and restores both before
// returning, so nothing else observes the change.
static void DrawStackedVectorClipped(UIElement *e, UIClipRect clip)
{
    const UIBox full = e->screen_box;
    float row_height = ui_standard_control_size.dimensions.y * (float)s_ui_pixel_scale;
    if (row_height * 2.0f > full.dimensions.y)
    {
        row_height = full.dimensions.y * 0.5f; // box shorter than two rows: share it equally
    }

    // Take the components from the buffer before it is borrowed for drawing.
    char components[2][sizeof(String64)];
    SplitVectorText(e->data.textbox.text.string, components[0], components[1], sizeof(components[0]));
    const String64 saved_text = e->data.textbox.text;
    static const char *prefixes[2] = {"x", "y"};

    for (int i = 0; i < 2; i++)
    {
        // Row 0 sits at the top of the box and row 1 at the bottom; the spare height is the gap.
        e->screen_box.coords = (Vector2d){full.coords.x, full.coords.y + (float)i * (full.dimensions.y - row_height)};
        e->screen_box.dimensions = (Vector2d){full.dimensions.x, row_height};
        DrawElementBoxClipped(e, clip);

        // Muted prefix at the normal text origin, vertically centred in the row.
        Bitmap_Font font = e->data.textbox.font;
        font.scale *= s_ui_pixel_scale;
        int row_text_height = BitmapFont_GetRowHeight(&font);
        float inset = ((float)e->border_weight + UI_TEXTBOX_TEXT_GAP_PX) * (float)s_ui_pixel_scale;
        Vector2d prefix_origin = {
            e->screen_box.coords.x + inset,
            e->screen_box.coords.y + fmaxf(0.0f, (row_height - (float)row_text_height) / 2.0f)};
        DrawMutedTextClipped(prefixes[i], prefix_origin, clip);

        // The value is drawn by the normal text path in a box shifted past the prefix.
        float shift = UI_VECTOR_PREFIX_ADVANCE_PX * (float)s_ui_pixel_scale;
        e->screen_box.coords.x += shift;
        e->screen_box.dimensions.x = fmaxf(0.0f, e->screen_box.dimensions.x - shift);
        snprintf(e->data.textbox.text.string, sizeof(e->data.textbox.text.string), "%s", components[i]);
        DrawTextAreaClipped(e, clip);
    }

    e->screen_box = full;
    e->data.textbox.text = saved_text;
}

// Draw the amber accent bar of a section title (a label with accent_bar set): UI_TITLE_BAR_WIDTH_PX x
// UI_TITLE_BAR_HEIGHT_PX at the label's top-left, clipped. The label is one title row tall, so the
// bar lines up with its text. No-op for every other element.
static void DrawTitleAccentBarClipped(const UIElement *e, UIClipRect clip)
{
    if (e->type != UI_ELEMENT_LABEL || !e->data.label.accent_bar)
    {
        return;
    }
    float scale = (float)s_ui_pixel_scale;
    float left = fmaxf(e->screen_box.coords.x, clip.min.x);
    float top = fmaxf(e->screen_box.coords.y, clip.min.y);
    float right = fminf(e->screen_box.coords.x + UI_TITLE_BAR_WIDTH_PX * scale, clip.max.x);
    float bottom = fminf(e->screen_box.coords.y + UI_TITLE_BAR_HEIGHT_PX * scale, clip.max.y);
    if (right > left && bottom > top)
    {
        DrawRectangleRec((Rectangle){left, top, right - left, bottom - top},
                         ToRaylibColor(UIPalette_GetActive()->button_fill));
    }
}

void DrawTextArea(UIElement *e){
    DrawTextAreaClipped(e, UI_UNCONSTRAINED_CLIP);
}

// Draw text within an element's screen bounds, clipped against an active clip rectangle.
void DrawTextAreaClipped(UIElement *e, UIClipRect clip)
{
    if (!e || UIClipRect_IsEmpty(clip))
    {
        return;
    }

    if (!UIClipRect_IntersectsBox(clip, e->screen_box))
    {
        return;
    }

    // Data Setup - Abstract the difference between Label, Button, and Textbox.
    char *text_ptr = NULL;
    Bitmap_Font font = {0};
    if (IsTextbox(e))
    {
        text_ptr = e->data.textbox.text.string;
        font = e->data.textbox.font;
    }
    else if (IsBtn(e))
    {
        text_ptr = e->data.button.display_text.string;
        font = e->data.button.font;
    }
    else if (e->type == UI_ELEMENT_LABEL)
    {
        text_ptr = e->data.label.text.string;
        font = e->data.label.font;
    }
    if (!text_ptr || text_ptr[0] == '\0')
    {
        return;
    }
    // Fonts are authored at their UI-pixel scale; draw them at the UI pixel scale so text grows with
    // the UI in whole-pixel steps. Every metric below derives from this scaled copy.
    font.scale *= s_ui_pixel_scale;
    Vector2d available_space = e->screen_box.dimensions;

    // Text origin: a textbox with a visible border insets its text from both sides so the
    // characters (and the cursor, which follows the drawn run) clear the border. Transparent
    // textboxes, labels and buttons keep the flush/centred origin at the box edge.
    float text_origin_x = e->screen_box.coords.x;
    if (IsTextbox(e) && e->colour_border.a > 0)
    {
        float text_inset = ((float)e->border_weight + UI_TEXTBOX_TEXT_GAP_PX) * (float)s_ui_pixel_scale;
        text_origin_x += text_inset;
        available_space.x = fmaxf(0.0f, available_space.x - 2.0f * text_inset);
    }

    // Accent-bar titles start their text UI_TITLE_TEXT_INDENT_PX right of the bar's left edge.
    if (e->type == UI_ELEMENT_LABEL && e->data.label.accent_bar)
    {
        float title_indent = UI_TITLE_TEXT_INDENT_PX * (float)s_ui_pixel_scale;
        text_origin_x += title_indent;
        available_space.x = fmaxf(0.0f, available_space.x - title_indent);
    }

    // Metrics calculation
    int glyph_advance = BitmapFont_GetGlyphAdvance(&font);
    int glyph_cell_width = BitmapFont_GetGlyphCellWidth(&font);
    int row_height = BitmapFont_GetRowHeight(&font);
    if (glyph_advance <= 0 || row_height <= 0)
    {
        return;
    }

    // Unit suffix: keep the value text clear of the right-aligned unit drawn after the text.
    if (IsTextbox(e) && e->data.textbox.unit[0] != '\0')
    {
        float unit_reserve = ((float)(strlen(e->data.textbox.unit) * (size_t)glyph_advance)) +
                             UI_FIELD_UNIT_GAP_PX * (float)s_ui_pixel_scale;
        available_space.x = fmaxf(0.0f, available_space.x - unit_reserve);
    }

    // Fit-to-field: an unfocused numeric value that is too wide is shown with fewer decimals, then
    // in scientific notation. Stacked-vector components are scalars too. The stored text is untouched.
    char fitted_text[sizeof(String64)];
    if (IsTextbox(e) && !e->is_focused &&
        (e->data.textbox.data_type == FLOAT || e->data.textbox.data_type == INT || e->data.textbox.stacked_vector))
    {
        int max_chars = (available_space.x >= (float)glyph_cell_width)
                            ? (int)((available_space.x - (float)glyph_cell_width) / (float)glyph_advance) + 1
                            : 0;
        if (FitNumericText(text_ptr, max_chars, fitted_text, sizeof(fitted_text)))
        {
            text_ptr = fitted_text;
        }
    }
    // Ellipsis: a read-only, non-numeric textbox that is one text row tall and whose text is wider
    // than the box shows the head of the text followed by "..." instead of being cut off mid-word.
    // Editable fields are untouched (they scroll/clip as they are typed into), and numeric fields
    // keep the fit-to-field formatting above, so the two rules never apply to the same text.
    // The stored text is never changed.
    char ellipsised_text[sizeof(String64)];
    if (e->type == UI_ELEMENT_TEXTBOX_O && !e->data.textbox.stacked_vector &&
        e->data.textbox.data_type != FLOAT && e->data.textbox.data_type != INT &&
        available_space.y < (float)(2 * row_height))
    {
        int max_chars = (available_space.x >= (float)glyph_cell_width)
                            ? (int)((available_space.x - (float)glyph_cell_width) / (float)glyph_advance) + 1
                            : 0;
        size_t text_length = strlen(text_ptr);
        if (max_chars > UI_ELLIPSIS_CHAR_COUNT && text_length > (size_t)max_chars &&
            (size_t)max_chars < sizeof(ellipsised_text))
        {
            // Keep the leading glyphs that leave room for the dots, then append them.
            size_t kept = (size_t)(max_chars - UI_ELLIPSIS_CHAR_COUNT);
            memcpy(ellipsised_text, text_ptr, kept);
            memcpy(ellipsised_text + kept, "...", UI_ELLIPSIS_CHAR_COUNT + 1);
            text_ptr = ellipsised_text;
        }
    }
    int rows_that_fit = (int)(available_space.y / row_height);
    int text_row_count = CountTextRows(text_ptr, available_space.x, glyph_advance, glyph_cell_width, rows_that_fit);
    float vertical_offset = 0.0f;
    float text_height = (float)(text_row_count * row_height);
    if (e->text_vertical_alignment == UI_TEXT_VERTICAL_ALIGN_CENTRE)
    {
        vertical_offset = fmaxf(0.0f, (available_space.y - text_height) / 2.0f);
    }
    else if (e->text_vertical_alignment == UI_TEXT_VERTICAL_ALIGN_BOTTOM)
    {
        vertical_offset = fmaxf(0.0f, available_space.y - text_height);
    }
    const char *text_cursor = text_ptr;
    int current_row = 0;
    float last_row_x_end = text_origin_x;
    bool fully_contained = UIClipRect_ContainsBox(clip, e->screen_box);

    // Segmenting and Drawing
    // We'll draw row-by-row to save memory (no need for a massive 2D array)
    while (text_cursor && *text_cursor != '\0' && current_row < rows_that_fit)
    {
        char row_buffer[256] = {0}; // Local buffer for the current line
        float current_row_width = 0.0f;
        int row_char_count = ConsumeTextRow(
            &text_cursor, available_space.x, glyph_advance, glyph_cell_width,
            row_buffer, sizeof(row_buffer), &current_row_width);
        if (row_char_count == 0)
        {
            break;
        }

        // Draw the row
        Vector2d draw_pos = {
            text_origin_x,
            e->screen_box.coords.y + vertical_offset + (float)(current_row * row_height)};

        if (e->text_horizontal_alignment == UI_TEXT_ALIGN_CENTRE)
        {
            draw_pos.x += fmaxf(0.0f, (available_space.x - current_row_width) / 2.0f);
        }
        else if (e->text_horizontal_alignment == UI_TEXT_ALIGN_RIGHT)
        {
            draw_pos.x += fmaxf(0.0f, available_space.x - current_row_width);
        }

        if (fully_contained)
        {
            // Fast path: element is entirely inside the clip rectangle
            last_row_x_end = DrawTextCustom(row_buffer, draw_pos, font.scale, font, font.colour);
        }
        else
        {
            float row_bottom = draw_pos.y + (float)row_height;
            float row_right = draw_pos.x + current_row_width;

            // Check if this row overlaps the clipping rectangle
            if (row_bottom > clip.min.y && draw_pos.y < clip.max.y &&
                row_right > clip.min.x && draw_pos.x < clip.max.x)
            {
                if (draw_pos.x >= clip.min.x && row_right <= clip.max.x &&
                    draw_pos.y >= clip.min.y && row_bottom <= clip.max.y)
                {
                    last_row_x_end = DrawTextCustom(row_buffer, draw_pos, font.scale, font, font.colour);
                }
                else
                {
                    last_row_x_end = DrawTextCustomClipped(row_buffer, draw_pos, font.scale, font, font.colour, clip);
                }
            }
            else
            {
                last_row_x_end = draw_pos.x + current_row_width;
            }
        }

        current_row++;
    }

    const char *remaining_text = text_cursor;
    if (remaining_text[0] != '\0')
    {
        if (frame_counter.total_frames % 1200 == 0)
        {
            LOG_WARN("Text was truncated for %s. Rendered rows: %d, Rows that fit: %d. Remaining text: \"%s\"",
                     GetElementTypeName(e->type), current_row, rows_that_fit, remaining_text);
        }
    }

    // Cursor Rendering
    if (e->is_focused && (frame_counter.total_frames % 60 < 30) && IsTextbox(e))
    {
        // Place cursor at the end of the last drawn character
        // Note: Subtract 1 from current_row because it was incremented after the last draw
        float adjusted_x = last_row_x_end;
        float adjusted_y = text_cursor != text_ptr
                               ? e->screen_box.coords.y + vertical_offset + (float)((current_row - 1) * row_height)
                               : e->screen_box.coords.y + vertical_offset;
        Vector2d cursor_pos = {adjusted_x, adjusted_y};

        if (fully_contained)
        {
            DrawTextCustom("|", cursor_pos, font.scale, font, font.colour);
        }
        else if (cursor_pos.x >= clip.min.x && cursor_pos.x < clip.max.x &&
                 cursor_pos.y + (float)row_height > clip.min.y && cursor_pos.y < clip.max.y)
        {
            DrawTextCustomClipped("|", cursor_pos, font.scale, font, font.colour, clip);
        }
    }
}

void DrawTextBoxText(UIElement *e)
{
}

// NEED TO CREATE BOX FOR ENTIRE SCREEN AND PASS THAT AS THE PARENT BOX FOR THE ROOT ELEMENT, SO THAT THE ROOT ELEMENT AND ALL OTHER ELEMENTS CAN CALCULATE THEIR POSITIONS AND DIMENSIONS BASED ON THAT, RATHER THAN HAVING THE ROOT ELEMENT CALCULATE ITS POSITION AND DIMENSIONS BASED ON A ZERO VECTOR, WHICH IS NOT FLEXIBLE IF WE WANT TO CHANGE THE PANEL OR ROOT ELEMENT PROPERTIES LATER
void DrawRootUIElement(UIElement *root_element, UIBox seed_box, Matrix3x3 M_ui_to_pixel)
{
    if (!root_element)
    {
        return;
    }

    Matrix2x2 basis_matrix = {
        {M_ui_to_pixel.col1.x, M_ui_to_pixel.col1.y},
        {M_ui_to_pixel.col2.x, M_ui_to_pixel.col2.y}};
    Vector2d basis_tfrm = VectorSum_2d(basis_matrix.col1, basis_matrix.col2);

    // One UI pixel spans basis_tfrm.x screen pixels; round to the integer UI pixel scale (>= 1).
    int pixel_scale = (int)roundf(fabsf(basis_tfrm.x));
    s_ui_pixel_scale = pixel_scale < 1 ? 1 : pixel_scale;

    // Resolve rendered position and dimensions of panel element
    root_element->screen_box.coords = TransformCoordinates(M_ui_to_pixel, seed_box.coords);
    root_element->screen_box.dimensions = (Vector2d){seed_box.dimensions.x * basis_tfrm.x, seed_box.dimensions.y * basis_tfrm.y};

    // Draw background & border
    DrawElementBox(root_element);

    // Recursively draw children using an unconstrained root clip
    ForEachChild(root_element, child)
    {
        DrawUIElement(child, root_element->screen_box, M_ui_to_pixel, UI_UNCONSTRAINED_CLIP);
    }
}

void DrawUIElement(UIElement *e, UIBox parent_box, Matrix3x3 M_ui_to_pixel, UIClipRect current_clip)
{
    if (!e || !e->is_enabled)
    {
        return;
    }

    Matrix2x2 basis_matrix = {
        {M_ui_to_pixel.col1.x, M_ui_to_pixel.col1.y},
        {M_ui_to_pixel.col2.x, M_ui_to_pixel.col2.y}};
    Vector2d basis_tfrm = VectorSum_2d(basis_matrix.col1, basis_matrix.col2);

    // Calculate local offset scaled to pixel space
    Vector2d scaled_local_offset = {
        e->local_box.coords.x * basis_tfrm.x,
        e->local_box.coords.y * basis_tfrm.y};

    // Add to parent's screen coordinates
    e->screen_box.coords = TransformCoordinates(M_ui_to_pixel, e->local_box.coords);
    e->screen_box.dimensions = (Vector2d){e->local_box.dimensions.x * basis_tfrm.x, e->local_box.dimensions.y * basis_tfrm.y};

    if (IsBtn(e))
    {
        // LOG_INFO("I'm a textbox, check my dimensions.");
    }

    // Prune elements completely outside the current clipping rectangle if they cannot scroll children into view.
    if (!(e->is_scrollable_x || e->is_scrollable_y) &&
        !UIClipRect_IntersectsBox(current_clip, e->screen_box))
    {
        return;
    }

    // A stacked-vector field that is not being edited draws as two component boxes; every other
    // element (and a focused vector, which shows its full "(x,y)" edit text) draws as one box.
    bool draw_stacked_vector = IsTextbox(e) && e->data.textbox.stacked_vector && !e->is_focused;

    // Draw background & border with clipping against the current clip rectangle
    if (draw_stacked_vector)
    {
        DrawStackedVectorClipped(e, current_clip);
    }
    else
    {
        DrawElementBoxClipped(e, current_clip);
    }
    DrawTitleAccentBarClipped(e, current_clip);

    if (!draw_stacked_vector && (IsTextbox(e) || IsBtn(e) || e->type == UI_ELEMENT_LABEL))
    {
        // Draw text only if element intersects the visible clip; partly visible text is clipped
        // to it so a scrolled-out row cannot spill over neighbouring content
        if (UIClipRect_IntersectsBox(current_clip, e->screen_box))
        {
            DrawTextAreaClipped(e, current_clip);
            if (IsTextbox(e))
            {
                DrawFieldUnitClipped(e, current_clip);
            }
        }
    }

    // Optional custom-draw hook: presence-dispatched, runs on top of the element's box and
    // independently of child recursion. Mirror the text branch's clip guard so a clipped
    // element draws no glyph. The hook reads e->screen_box for geometry.
    if (e->on_draw != NULL && UIClipRect_IntersectsBox(current_clip, e->screen_box))
    {
        e->on_draw(e);
    }

    // Calculate clip rectangle for children; scrollable containers constrain all descendants.
    UIClipRect child_clip = current_clip;
    if (e->is_scrollable_x || e->is_scrollable_y)
    {
        child_clip = UIClipRect_Intersect(current_clip, UIClipRect_FromBox(e->screen_box));
    }

    if (!UIClipRect_IsEmpty(child_clip))
    {
        // Recursively draw children elements within the propagated clip boundary
        ForEachChild(e, child)
        {
            DrawUIElement(child, e->screen_box, M_ui_to_pixel, child_clip);
        }
    }
}

// Draws Left Panel
// void DrawPanelRegion(UIElement *element, Camera2d camera)
// {
//     // Need to convert world coordinates to screen coordinates
//     Space2d panel_space = element->data.root.space;
//     Basis2d basis = panel_space.basis;

//     // The world position of the coordinate space object is the origin of the coordinate space, so (0,0).
//     // But to make it more flexible for different coordinate space origins, we will add the world position to the start and end points of the lines to get their actual coordinates in world space, and then convert those to screen coordinates using the basis transform matrix
//     Vector2d origin = panel_space.bounds_origin;
//     Vector2d end = VectorSum_2d(origin, panel_space.resolution_ixj);
//     Vector2d basis_scale = BasisTransform_2d_Scale(camera.source_basis, camera.destination_basis); // Need to scale dimensions from world units to pixel units using the camera's basis transform

//     // Resolve rendered position and dimensions of panel element
//     UIBox panel_box = ResolveElementBox(element, ZERO_VECTOR_2D, basis_scale);

//     // First: Draw background & border
//     ColourRgba colour_fill = element->colour_fill;
//     ColourRgba colour_border = element->colour_border;
//     DrawRectangle(panel_box.coords.x, panel_box.coords.y, panel_box.dimensions.x, panel_box.dimensions.y, (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a});
//     DrawRectangleLines(panel_box.coords.x, panel_box.coords.y, panel_box.dimensions.x, panel_box.dimensions.y, (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a});

//     // Draw the text boxes in the Properties container
//     DrawTextFieldsContainer(lpanel_properties_tcont, camera_lpanel);

//     // Draw the text boxes in the Stats container
//     // DrawTextFieldsContainer(&lpanel_stats_tcont, camera_lpanel);

//     // Memory display - Consumed memory in bytes out of the total allocated bytes
//     // snprintf(text, sizeof(text), "Memory Consumed (bytes): %zu", GetCurrentMemoryAllocated()); // Format the FPS value into the buffer
//     // DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + 2 * lineSpacing.y}, font.baseSize * 2.0f, 2, (Color)BEIGE);
// }

// void DrawPanelRegion_Stats(Space2d panel_space, Color fill_colour)
// {
//     Vector2 pos = {20, 100};
//     Vector2 lineSpacing = {0, 40};

//     // Count display
//     char text[32];
//     // snprintf(text, sizeof(text), "Polygonoids: %d", GetPolygonoidCount());                                                                                 // Format the FPS value into the buffer
//     // DrawTextEx(font_default, text, pos, font_default.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a}); // Buffer to hold the text
//     //  snprintf(text, sizeof(text), "Circloids: %d", GetCircloidCount()); // Format the FPS value into the buffer
//     //  DrawTextEx(font, text, pos, font.baseSize * 2.0f, 2, (Color)BEIGE);

//     // FPS display
//     snprintf(text, sizeof(text), "FPS: %.1f", frame_counter.fps); // Format the FPS value into the buffer
//     DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + lineSpacing.y}, font.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a});

//     // Memory display - Total allocated memory in bytes
//     snprintf(text, sizeof(text), "Memory (bytes): %zu", GetCurrentMemoryAllocated()); // Format the FPS value into the buffer
//     DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + 2 * lineSpacing.y}, font.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a});
// }
