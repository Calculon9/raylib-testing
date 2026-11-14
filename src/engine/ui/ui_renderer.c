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
static void DrawUIElement(UIElement *e, Matrix3x3 M_ui_to_pixel, UIClipRect current_clip);

// Screen pixels per UI pixel for the tree currently being drawn. UI content is laid out in UI
// pixels; DrawRootUIElement derives this integer factor from the root's UI-to-pixel transform so
// text glyphs, border lines and text insets are drawn at the same crisp integer scale as the boxes.
static int s_ui_pixel_scale = 1;

// The live palette for the tree currently being drawn. Fetched once per draw pass by
// DrawRootUIElement (the palette never changes during a draw traversal) instead of once per helper call.
static const UIPalette *s_ui_palette = NULL;

// Consume one wrapped text row from [*text_cursor, text_end) and optionally copy its characters
// into a draw buffer (which is always NUL-terminated when given).
static int ConsumeTextRow(const char **text_cursor, const char *text_end, float available_width,
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
    while (cursor < text_end && row_char_count < max_row_chars)
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

// Count how many wrapped rows [text, text_end) needs at `available_width`, up to `max_rows`.
static int CountTextRows(const char *text, const char *text_end, float available_width,
                         int glyph_advance, int glyph_cell_width, int max_rows)
{
    const char *text_cursor = text;
    int row_count = 0;

    while (text_cursor && text_cursor < text_end && row_count < max_rows)
    {
        if (ConsumeTextRow(&text_cursor, text_end, available_width, glyph_advance,
                           glyph_cell_width, NULL, 0, NULL) == 0)
        {
            break;
        }

        row_count++;
    }

    return row_count;
}

// Horizontal text inset inside a bordered textbox: the border weight + a 4 px gap (5 px for the
// default 1 px border). The glyph's own 2 px blank column then places the first ink 7 px from the
// box edge, as in the mock.
#define UI_TEXTBOX_TEXT_GAP_PX 4.0f
// Glyphs in the "..." appended to read-only text that does not fit its box.
#define UI_ELLIPSIS_CHAR_COUNT 3

// Text inset (screen pixels) from a bordered textbox's edge: the border weight plus the text gap,
// at the current UI pixel scale. Shared by the text run, the unit suffix and the vector prefix.
static float TextInsetPx(const UIElement *e)
{
    return ((float)e->border_weight + UI_TEXTBOX_TEXT_GAP_PX) * (float)s_ui_pixel_scale;
}

// How many glyphs fit in `width` pixels: the first glyph needs a full cell and every further one an
// advance. Zero when not even one cell fits. Shared by the fit-to-field and ellipsis rules.
static int MaxGlyphsInWidth(float width, int glyph_advance, int glyph_cell_width)
{
    return (width >= (float)glyph_cell_width)
               ? (int)((width - (float)glyph_cell_width) / (float)glyph_advance) + 1
               : 0;
}

// Is this element a visible read-only text field? Read-only textboxes (UI_ELEMENT_TEXTBOX_O) that
// carry a visible box (fill and border both non-transparent) are drawn in the palette read-only
// style. Transparent read-only textboxes (panel headers, notes) have no box and stay untouched.
// This is the one place the read-only look is decided, so it applies to every panel and to fields
// locked at run time (multi-select) alike.
static bool IsReadOnlyFieldBox(const UIElement *e)
{
    return e->type == UI_ELEMENT_TEXTBOX_O && e->colour_border.a > 0 && e->colour_fill.a > 0;
}

// The fill and border colour an element's box is drawn with.
typedef struct DrawnBoxColours
{
    ColourRgba fill;
    ColourRgba border;
} DrawnBoxColours;

// Resolve the fill and border to draw for an element, once per element per draw (style v2 border
// model). A read-only text field uses the palette read-only fill and border (same 1 px geometry as
// editable ones). Otherwise the element's own colours are used: a border is drawn whenever its
// alpha is non-zero, independent of the debug overlay, and with the UI-borders overlay on a
// borderless element gets the faint debug outline so the overlay still shows its bounds.
static DrawnBoxColours ResolveDrawnBoxColours(const UIElement *e)
{
    if (IsReadOnlyFieldBox(e))
    {
        UIRoleColours readonly = UIPalette_GetRoleColours(s_ui_palette, UI_ROLE_READONLY_INPUT);
        return (DrawnBoxColours){readonly.fill, readonly.border};
    }
    DrawnBoxColours colours = {e->colour_fill, e->colour_border};
    if (colours.border.a == 0 && IsDebugEnabled(DEBUG_UI_BORDERS))
    {
        colours.border = s_ui_palette->debug_outline;
    }
    return colours;
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

// Draw a box's background fill and its border (drawn inside the box, so a bordered element keeps
// its layout size). A zero border weight draws no line. `box` is passed in so a stacked vector can
// draw each component row without moving the element's own screen box.
static void DrawElementBox(const UIElement *e, UIBox box, DrawnBoxColours colours)
{
    float weight = ResolveDrawnBorderWeight(e, &box);
    DrawRectangleRec((Rectangle){box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y},
                     ToRaylibColor(colours.fill));
    if (weight > 0.0f)
    {
        DrawRectangleLinesEx((Rectangle){box.coords.x, box.coords.y, box.dimensions.x, box.dimensions.y},
                             weight, ToRaylibColor(colours.border));
    }
}

// Draw a box's background and border clipped against an active clip rectangle.
static void DrawElementBoxClipped(const UIElement *e, UIBox box, UIClipRect clip, DrawnBoxColours colours)
{
    if (!UIClipRect_IntersectsBox(clip, box))
    {
        return;
    }

    if (UIClipRect_ContainsBox(clip, box))
    {
        DrawElementBox(e, box, colours);
        return;
    }

    UIBox clipped_box = UIBox_Clip(box, clip);
    if (clipped_box.dimensions.x <= 0.0f || clipped_box.dimensions.y <= 0.0f)
    {
        return;
    }

    // Draw the clipped background fill.
    if (colours.fill.a > 0)
    {
        DrawRectangleRec((Rectangle){clipped_box.coords.x, clipped_box.coords.y,
                                     clipped_box.dimensions.x, clipped_box.dimensions.y},
                         ToRaylibColor(colours.fill));
    }

    // Only draw borders along the original element edges if they fall within the clip rect.
    float weight = ResolveDrawnBorderWeight(e, &box);
    if (colours.border.a > 0 && weight > 0.0f)
    {
        Color border_color = ToRaylibColor(colours.border);
        const float epsilon = 0.5f;

        // Draw top border if not clipped away.
        if (fabsf(clipped_box.coords.y - box.coords.y) <= epsilon)
        {
            DrawRectangleRec((Rectangle){clipped_box.coords.x, clipped_box.coords.y,
                                         clipped_box.dimensions.x, fminf(weight, clipped_box.dimensions.y)},
                             border_color);
        }

        // Draw bottom border if not clipped away.
        if (fabsf((clipped_box.coords.y + clipped_box.dimensions.y) -
                  (box.coords.y + box.dimensions.y)) <= epsilon)
        {
            DrawRectangleRec((Rectangle){clipped_box.coords.x,
                                         clipped_box.coords.y + clipped_box.dimensions.y - fminf(weight, clipped_box.dimensions.y),
                                         clipped_box.dimensions.x, fminf(weight, clipped_box.dimensions.y)},
                             border_color);
        }

        // Draw left border if not clipped away.
        if (fabsf(clipped_box.coords.x - box.coords.x) <= epsilon)
        {
            DrawRectangleRec((Rectangle){clipped_box.coords.x, clipped_box.coords.y,
                                         fminf(weight, clipped_box.dimensions.x), clipped_box.dimensions.y},
                             border_color);
        }

        // Draw right border if not clipped away.
        if (fabsf((clipped_box.coords.x + clipped_box.dimensions.x) -
                  (box.coords.x + box.dimensions.x)) <= epsilon)
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

// Fit an unfocused numeric value [text, text + length) into `max_chars` glyphs. Only text that
// parses fully as a number and is too long is rewritten (non-numeric text such as "N/A" is left
// alone): first with fewer decimal places, then, when even the integer part is too wide, in compact
// scientific notation (for example 1.23e5, dropping mantissa digits as needed). Returns true and
// fills `out` (NUL-terminated) when the text was rewritten. The stored text is never changed, so a
// focused field still shows the full value. The length check is O(1); the NUL-terminated copy the
// number parse needs is only made for text that is actually too wide.
static bool FitNumericText(const char *text, size_t length, int max_chars, char *out, size_t out_bytes)
{
    if (max_chars <= 0 || (int)length <= max_chars)
    {
        return false;
    }

    char number[sizeof(String64)];
    if (length >= sizeof(number))
    {
        return false;
    }
    memcpy(number, text, length);
    number[length] = '\0';

    char *end = NULL;
    double value = strtod(number, &end);
    if (end == number || *end != '\0')
    {
        return false;
    }

    // Step 1: shed decimal places, one at a time, until the text fits.
    const char *point = strchr(number, '.');
    int decimals = point ? (int)(length - (size_t)(point - number) - 1) : 0;
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

// A run of text inside a larger string: a pointer and a length, so no copy is needed.
typedef struct TextSpan
{
    const char *start;
    size_t length;
} TextSpan;

// Split a formatted vector "(x,y)" into its two component spans, pointing into `text` (no copy).
// The no-value placeholder (UI_NO_VALUE_TEXT) is shown in BOTH components; any other text without
// a comma (for example "N/A") lands whole in `x_out` with an empty `y_out`.
static void SplitVectorText(const char *text, TextSpan *x_out, TextSpan *y_out)
{
    const char *start = (text[0] == '(') ? text + 1 : text;
    const char *comma = strchr(start, ',');
    if (!comma)
    {
        *x_out = (TextSpan){text, strlen(text)};
        *y_out = (strcmp(text, UI_NO_VALUE_TEXT) == 0) ? *x_out : (TextSpan){text, 0};
        return;
    }
    *x_out = (TextSpan){start, (size_t)(comma - start)};
    size_t y_length = strlen(comma + 1);
    // Drop the closing bracket of "(x,y)".
    if (y_length > 0 && comma[y_length] == ')')
    {
        y_length--;
    }
    *y_out = (TextSpan){comma + 1, y_length};
}

// Draw `text` in the muted palette colour (muted_text, brown on cream) at a pixel position, clipped.
static void DrawMutedTextClipped(const char *text, Vector2d origin, UIClipRect clip)
{
    Bitmap_Font font = FONT_MEDIUM_WITH_COLOUR(s_ui_palette->muted_text);
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
    float inset = TextInsetPx(e);
    Vector2d origin = {
        e->screen_box.coords.x + e->screen_box.dimensions.x - inset - run_width,
        e->screen_box.coords.y + fmaxf(0.0f, (e->screen_box.dimensions.y - (float)row_height) / 2.0f)};
    DrawMutedTextClipped(unit, origin, clip);
}

// Draw a text run inside `box`, clipped against an active clip rectangle: the text-run core shared
// by every text element and by each component of a stacked vector. `font` is already scaled to the
// UI pixel scale and every metric below derives from it. Handles the box inset, the accent-bar
// indent, the unit reserve, fit-to-field and ellipsis, row wrapping, alignment and the cursor. The
// text is a pointer and length into the element's own buffer, so nothing is copied or swapped.
static void DrawTextRunClipped(UIElement *e, UIClipRect clip, const char *text, size_t text_length,
                               Bitmap_Font font, UIBox box)
{
    if (UIClipRect_IsEmpty(clip) || !UIClipRect_IntersectsBox(clip, box) || text_length == 0)
    {
        return;
    }

    const bool is_textbox = IsTextbox(e);
    Vector2d available_space = box.dimensions;

    // Text origin: a textbox with a visible border insets its text from both sides so the
    // characters (and the cursor, which follows the drawn run) clear the border. Transparent
    // textboxes, labels and buttons keep the flush/centred origin at the box edge.
    float text_origin_x = box.coords.x;
    if (is_textbox && e->colour_border.a > 0)
    {
        float text_inset = TextInsetPx(e);
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
    if (is_textbox && e->data.textbox.unit[0] != '\0')
    {
        float unit_reserve = ((float)(strlen(e->data.textbox.unit) * (size_t)glyph_advance)) +
                             UI_FIELD_UNIT_GAP_PX * (float)s_ui_pixel_scale;
        available_space.x = fmaxf(0.0f, available_space.x - unit_reserve);
    }

    // The glyph budget of the line, shared by the fit-to-field and ellipsis rules below.
    const int max_glyphs = MaxGlyphsInWidth(available_space.x, glyph_advance, glyph_cell_width);

    // Fit-to-field: an unfocused numeric value that is too wide is shown with fewer decimals, then
    // in scientific notation. Stacked-vector components are scalars too. The stored text is untouched.
    char fitted_text[sizeof(String64)];
    if (is_textbox && !e->is_focused &&
        (e->data.textbox.data_type == FLOAT || e->data.textbox.data_type == INT || e->data.textbox.stacked_vector))
    {
        if (FitNumericText(text, text_length, max_glyphs, fitted_text, sizeof(fitted_text)))
        {
            text = fitted_text;
            text_length = strlen(fitted_text);
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
        if (max_glyphs > UI_ELLIPSIS_CHAR_COUNT && text_length > (size_t)max_glyphs &&
            (size_t)max_glyphs < sizeof(ellipsised_text))
        {
            // Keep the leading glyphs that leave room for the dots, then append them.
            size_t kept = (size_t)(max_glyphs - UI_ELLIPSIS_CHAR_COUNT);
            memcpy(ellipsised_text, text, kept);
            memcpy(ellipsised_text + kept, "...", UI_ELLIPSIS_CHAR_COUNT + 1);
            text = ellipsised_text;
            text_length = (size_t)max_glyphs;
        }
    }
    const char *text_end = text + text_length;
    int rows_that_fit = (int)(available_space.y / row_height);
    int text_row_count = CountTextRows(text, text_end, available_space.x, glyph_advance, glyph_cell_width, rows_that_fit);
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
    const char *text_cursor = text;
    int current_row = 0;
    float last_row_x_end = text_origin_x;
    bool fully_contained = UIClipRect_ContainsBox(clip, box);

    // Segmenting and Drawing
    // We'll draw row-by-row to save memory (no need for a massive 2D array). The row buffer is
    // reused for every row; ConsumeTextRow writes the terminator, so only the first byte is primed.
    char row_buffer[256];
    row_buffer[0] = '\0';
    while (text_cursor < text_end && current_row < rows_that_fit)
    {
        float current_row_width = 0.0f;
        int row_char_count = ConsumeTextRow(
            &text_cursor, text_end, available_space.x, glyph_advance, glyph_cell_width,
            row_buffer, sizeof(row_buffer), &current_row_width);
        if (row_char_count == 0)
        {
            break;
        }

        // Draw the row
        Vector2d draw_pos = {
            text_origin_x,
            box.coords.y + vertical_offset + (float)(current_row * row_height)};

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

    if (text_cursor < text_end)
    {
        if (frame_counter.total_frames % 1200 == 0)
        {
            LOG_WARN("Text was truncated for %s. Rendered rows: %d, Rows that fit: %d. Remaining text: \"%.*s\"",
                     GetElementTypeName(e->type), current_row, rows_that_fit,
                     (int)(text_end - text_cursor), text_cursor);
        }
    }

    // Cursor Rendering
    if (e->is_focused && (frame_counter.total_frames % 60 < 30) && is_textbox)
    {
        // Place cursor at the end of the last drawn character
        // Note: Subtract 1 from current_row because it was incremented after the last draw
        float adjusted_x = last_row_x_end;
        float adjusted_y = text_cursor != text
                               ? box.coords.y + vertical_offset + (float)((current_row - 1) * row_height)
                               : box.coords.y + vertical_offset;
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

// Draw an element's own text (label, button or textbox caption/value) in its screen box, clipped.
// Picks the text and font for the element type, scales the font to the UI pixel scale and runs the
// shared text-run core. Elements with no text, or other types, draw nothing.
static void DrawTextAreaClipped(UIElement *e, UIClipRect clip)
{
    const char *text = NULL;
    Bitmap_Font font = {0};
    if (IsTextbox(e))
    {
        text = e->data.textbox.text.string;
        font = e->data.textbox.font;
    }
    else if (IsBtn(e))
    {
        text = e->data.button.display_text.string;
        font = e->data.button.font;
    }
    else if (e->type == UI_ELEMENT_LABEL)
    {
        text = e->data.label.text.string;
        font = e->data.label.font;
    }
    if (!text || text[0] == '\0')
    {
        return;
    }
    // Fonts are authored at their UI-pixel scale; draw them at the UI pixel scale so text grows with
    // the UI in whole-pixel steps.
    font.scale *= s_ui_pixel_scale;
    DrawTextRunClipped(e, clip, text, strlen(text), font, e->screen_box);
}

// Draw a stacked-vector textbox as two component boxes (x on the first row, y on the second, the
// same width, one control row each). Each box shows a muted "x" / "y" prefix and its value, which
// starts UI_VECTOR_PREFIX_ADVANCE_PX after the prefix. The element keeps ONE box, text buffer, binding
// and hit area (so editing, focus and refresh are unchanged); only its drawing is split. The two
// components are spans into the element's own text, drawn through the text-run core with a
// per-component box, so neither the element's screen box nor its text is ever modified.
static void DrawStackedVectorClipped(UIElement *e, UIClipRect clip, DrawnBoxColours colours)
{
    const UIBox full = e->screen_box;
    float row_height = ui_standard_control_size.dimensions.y * (float)s_ui_pixel_scale;
    if (row_height * 2.0f > full.dimensions.y)
    {
        row_height = full.dimensions.y * 0.5f; // box shorter than two rows: share it equally
    }

    TextSpan components[2];
    SplitVectorText(e->data.textbox.text.string, &components[0], &components[1]);
    static const char *prefixes[2] = {"x", "y"};

    // Metrics shared by both rows: the scaled value font, the prefix inset and the value shift.
    Bitmap_Font font = e->data.textbox.font;
    font.scale *= s_ui_pixel_scale;
    const int row_text_height = BitmapFont_GetRowHeight(&font);
    const float inset = TextInsetPx(e);
    const float shift = UI_VECTOR_PREFIX_ADVANCE_PX * (float)s_ui_pixel_scale;

    for (int i = 0; i < 2; i++)
    {
        // Row 0 sits at the top of the box and row 1 at the bottom; the spare height is the gap.
        UIBox row = {
            {full.coords.x, full.coords.y + (float)i * (full.dimensions.y - row_height)},
            {full.dimensions.x, row_height}};
        DrawElementBoxClipped(e, row, clip, colours);

        // Muted prefix at the normal text origin, vertically centred in the row.
        Vector2d prefix_origin = {
            row.coords.x + inset,
            row.coords.y + fmaxf(0.0f, (row_height - (float)row_text_height) / 2.0f)};
        DrawMutedTextClipped(prefixes[i], prefix_origin, clip);

        // The value is drawn by the text-run core in a box shifted past the prefix.
        UIBox value_box = row;
        value_box.coords.x += shift;
        value_box.dimensions.x = fmaxf(0.0f, row.dimensions.x - shift);
        DrawTextRunClipped(e, clip, components[i].start, components[i].length, font, value_box);
    }
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
                         ToRaylibColor(s_ui_palette->button_fill));
    }
}

// Draw a whole UI tree: derive the integer UI-pixel scale from the UI-to-pixel transform, place the
// root at seed_box and draw it and its descendants.
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

    // The palette is fetched once for the whole draw pass.
    s_ui_palette = UIPalette_GetActive();

    // Resolve rendered position and dimensions of panel element
    root_element->screen_box.coords = TransformCoordinates(M_ui_to_pixel, seed_box.coords);
    root_element->screen_box.dimensions = (Vector2d){seed_box.dimensions.x * basis_tfrm.x, seed_box.dimensions.y * basis_tfrm.y};

    // Draw background & border
    DrawElementBox(root_element, root_element->screen_box, ResolveDrawnBoxColours(root_element));

    // Recursively draw children using an unconstrained root clip
    ForEachChild(root_element, child)
    {
        DrawUIElement(child, M_ui_to_pixel, UI_UNCONSTRAINED_CLIP);
    }
}

// Draw one element (box, text, custom hook) and recurse into its children under the propagated clip.
static void DrawUIElement(UIElement *e, Matrix3x3 M_ui_to_pixel, UIClipRect current_clip)
{
    if (!e || !e->is_enabled)
    {
        return;
    }

    Matrix2x2 basis_matrix = {
        {M_ui_to_pixel.col1.x, M_ui_to_pixel.col1.y},
        {M_ui_to_pixel.col2.x, M_ui_to_pixel.col2.y}};
    Vector2d basis_tfrm = VectorSum_2d(basis_matrix.col1, basis_matrix.col2);

    // Resolve the element's screen box from its local box.
    e->screen_box.coords = TransformCoordinates(M_ui_to_pixel, e->local_box.coords);
    e->screen_box.dimensions = (Vector2d){e->local_box.dimensions.x * basis_tfrm.x, e->local_box.dimensions.y * basis_tfrm.y};

    // Prune elements completely outside the current clipping rectangle if they cannot scroll children into view.
    if (!(e->is_scrollable_x || e->is_scrollable_y) &&
        !UIClipRect_IntersectsBox(current_clip, e->screen_box))
    {
        return;
    }

    // A stacked-vector field that is not being edited draws as two component boxes; every other
    // element (and a focused vector, which shows its full "(x,y)" edit text) draws as one box.
    bool draw_stacked_vector = IsTextbox(e) && e->data.textbox.stacked_vector && !e->is_focused;

    // Draw background & border with clipping against the current clip rectangle. The drawn colours
    // (including the read-only role) are resolved once here and shared by every box drawn below.
    const DrawnBoxColours box_colours = ResolveDrawnBoxColours(e);
    if (draw_stacked_vector)
    {
        DrawStackedVectorClipped(e, current_clip, box_colours);
    }
    else
    {
        DrawElementBoxClipped(e, e->screen_box, current_clip, box_colours);
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
            DrawUIElement(child, M_ui_to_pixel, child_clip);
        }
    }
}