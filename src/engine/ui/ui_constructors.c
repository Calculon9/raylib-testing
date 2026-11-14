#include "ui/ui_constructors.h"

#include "system/systems.h"
#include "system/utility_system.h"
#include "system/ui_system.h"
#include <ctype.h>

UIElement *CreateUILabel(UIElement *parent, const char *text, Size size,
                         Vector2d padding, Bitmap_Font font,
                         ColourRgba border, ColourRgba fill)
{
    UIElement *title = CreateUIElementInTree(UI_ELEMENT_LABEL, size, parent,
                                             (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                             padding, border, fill);

    if (!title)
    {
        return NULL;
    }

    safe_strncpy(title->data.label.text.string, text, MAX_LABEL_CHARS);
    title->data.label.font = font;
    title->is_draggable = true;

    return title;
}

UIElement *CreateUILabelDefault(UIElement *parent, const char *text, Size size,
                                Vector2d padding, const UIPalette *palette)
{
    if (!palette)
    {
        palette = UIPalette_GetActive();
    }

    return CreateUILabel(parent, text, size, padding,
                         FONT_MEDIUM_WITH_COLOUR(palette->text),
                         COLOURLESS_RGBA, COLOURLESS_RGBA);
}

UIElement *CreateUILabelTitleDefault(UIElement *parent, const char *text,
                                     Size size, Vector2d padding,
                                     const UIPalette *palette)
{
    if (!palette)
    {
        palette = UIPalette_GetActive();
    }

    // Section titles are UPPER CASE by the style guide. Folding here, once, in the standard title
    // constructor means every title (markup style="title" or C-built) follows automatically, and the
    // hug width is measured on the folded text. ASCII fold via toupper on unsigned char (the ctype
    // contract); the copy is bounded by MAX_LABEL_CHARS, the label buffer size.
    char upper_text[MAX_LABEL_CHARS] = {0};
    if (text)
    {
        for (size_t i = 0; i + 1 < sizeof(upper_text) && text[i] != '\0'; i++)
        {
            upper_text[i] = (char)toupper((unsigned char)text[i]);
        }
    }

    UIElement *title = CreateUILabel(parent, upper_text, size, padding,
                                     FONT_LARGE_WITH_COLOUR(palette->title_text),
                                     COLOURLESS_RGBA, COLOURLESS_RGBA);
    if (title)
    {
        // The renderer draws the amber accent bar and indents the text (UI_TITLE_* in ui.h).
        title->data.label.accent_bar = true;
    }
    return title;
}

// Build the UIElement-based labelled field used by the shared field constructor.
static UIElement *CreateLabeledFieldElement(UIElement *parent, Size size,
                                            Size textbox_size, Vector2d padding,
                                            ColourRgba border, ColourRgba fill,
                                            Bitmap_Font font)
{
    UIElement *field = CreateUIElementInTree(
        UI_ELEMENT_TEXTFIELD, size, parent,
        (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, padding, border, fill);
    if (!field)
    {
        return NULL;
    }

    UIElement *label = CreateUIElement(
        UI_ELEMENT_LABEL, (Size){ZERO_VECTOR_2D, AXIS_PERCENT, AXIS_PERCENT},
        (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, ZERO_VECTOR_2D, border, fill);
    UIElement *textbox = CreateUIElement(
        UI_ELEMENT_TEXTBOX_SAFE_IO, (Size){ZERO_VECTOR_2D, AXIS_PERCENT, AXIS_PERCENT},
        (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, ZERO_VECTOR_2D, border, fill);
    if (!label || !textbox)
    {
        return NULL;
    }

    SetUIElementTextVerticalAlignment(label, UI_TEXT_VERTICAL_ALIGN_CENTRE);
    SetUIElementTextVerticalAlignment(textbox, UI_TEXT_VERTICAL_ALIGN_CENTRE);
    label->data.label.font = font;
    textbox->data.textbox.font = font;

    // Every former SIZE_PERCENT value maps to (AXIS_PERCENT, AXIS_PERCENT), so the width
    // axis is the behaviour-equivalent test for the percent label/textbox split.
    if (textbox_size.width_mode == AXIS_PERCENT)
    {
        label->size.dimensions = (Vector2d){1.0f - textbox_size.dimensions.x, 1.0f};
        label->resolved_offset.offset = ZERO_VECTOR_2D;

        textbox->size.dimensions = (Vector2d){textbox_size.dimensions.x, 1.0f};
        textbox->resolved_offset.offset = (Vector2d){1.0f - textbox_size.dimensions.x, 0.0f};

        label->authored_offset = label->resolved_offset;
        textbox->authored_offset = textbox->resolved_offset;
    }
    else if (textbox_size.width_mode == AXIS_FILL)
    {
        // Label column + filling input (style v2). The label is a FIXED-width column (the
        // fallback width here; the per-section layout pass in ui.c widens it to the section's
        // longest label), and the input sits at a FIXED x offset equal to that width with a
        // FILL width, so it extends from the column to the row's inner right edge.
        const float label_width = ui_standard_textfield_label_width;
        label->size = (Size){{label_width, 1.0f}, AXIS_FIXED, AXIS_PERCENT};
        label->resolved_offset = (Offset){ZERO_VECTOR_2D, OFFSET_FIXED};

        textbox->size = (Size){{1.0f, 1.0f}, AXIS_FILL, AXIS_PERCENT};
        textbox->resolved_offset = (Offset){{label_width, 0.0f}, OFFSET_FIXED};

        label->authored_offset = label->resolved_offset;
        textbox->authored_offset = textbox->resolved_offset;
    }

    AddElementToTree(label, field);
    AddElementToTree(textbox, field);
    return field;
}

UIElement *CreateUILabeledField(UIElement *parent, const char *label_text, UIElementType input_type,
                                Size row_size, Size textbox_size, Vector2d row_padding,
                                ColourRgba row_border, ColourRgba row_fill, Vector2d cell_padding, 
                                ColourRgba cell_border, ColourRgba cell_fill,
                                Bitmap_Font label_font, Bitmap_Font font)
{
    UIElement *tfield = CreateLabeledFieldElement(parent, row_size, textbox_size,
                                                  row_padding, row_border, row_fill, font);
    if (!tfield)
    {
        return NULL;
    }

    tfield->is_draggable = true;

    UIElement *label_child = tfield->first_child;
    UIElement *input_child = (label_child) ? label_child->next_sibling : NULL;
    if (!label_child || !input_child)
    {
        return NULL;
    }

    label_child->padding = cell_padding;
    label_child->colour_border = row_border;
    label_child->colour_fill = row_fill;
    label_child->data.label.font = label_font;
    safe_strncpy(label_child->data.label.text.string, label_text, MAX_LABEL_CHARS);

    input_child->padding = cell_padding;
    input_child->colour_border = cell_border;
    input_child->colour_fill = cell_fill;
    input_child->data.textbox.font = font;
    input_child->type = input_type;
    input_child->is_draggable = true;

    return input_child;
}

UIElement *CreateUILabeledFieldDefault(UIElement *parent, const char *label_text,
                                       UIElementType input_type, Size row_size,
                                       Vector2d row_padding, const UIPalette *palette)
{
    if (!palette)
    {
        palette = UIPalette_GetActive();
    }

    // The row and its label cell are TRANSPARENT: borders are drawn whenever their alpha is
    // non-zero, and only the input box should show one (the FIELD_ROW border would outline
    // every row). Markup can still opt a row into surface="field_row".
    ColourRgba row_border;
    ColourRgba row_fill;
    UIPalette_GetSurfaceColours(palette, UI_PALETTE_SURFACE_TRANSPARENT, &row_border, &row_fill);

    ColourRgba input_border;
    ColourRgba input_fill;
    UIPalette_GetSurfaceColours(palette, UI_PALETTE_SURFACE_INPUT,
                                &input_border, &input_fill);

    UIElement *input = CreateUILabeledField(parent, label_text, input_type, row_size,
                                ui_standard_textfield_input_size, row_padding, row_border, row_fill,
                                ZERO_VECTOR_2D, input_border, input_fill,
                                FONT_MEDIUM_WITH_COLOUR(palette->label_text),
                                FONT_MEDIUM_WITH_COLOUR(palette->text));
    if (input)
    {
        input->border_weight = UIPalette_GetSurfaceBorderWeight(palette, UI_PALETTE_SURFACE_INPUT);
    }
    return input;
}

UIElement *CreateUIButton(UIElement *parent, UIElementType type, const char *text,
                          Size size, Vector2d padding, ColourRgba border,
                          ColourRgba fill, Bitmap_Font font, UIEventHandler on_click,
                          void *user_data, void *data_bind)
{
    UIElement *btn = CreateUIElementInTree(type, size, parent,
                                           (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                           padding, border, fill);
    if (!btn)
    {
        return NULL;
    }

    // Seed both the authored base text and the drawn text; with no binding they stay equal.
    safe_strncpy(btn->data.button.text.string, text, MAX_LABEL_CHARS);
    safe_strncpy(btn->data.button.display_text.string, text, MAX_LABEL_CHARS);
    btn->data.button.font = font;
    SetUIElementTextHorizontalAlignment(btn, UI_TEXT_ALIGN_CENTRE);
    SetUIElementTextVerticalAlignment(btn, UI_TEXT_VERTICAL_ALIGN_CENTRE);
    btn->on_click = on_click;                 // base handler (presence-based dispatch)
    btn->interaction_user_data = user_data;   // base opaque per-element state
    btn->data.button.data_bind = data_bind;   // retained in ButtonData for the view-selector click
    btn->is_draggable = true;

    return btn;
}

UIElement *CreateUIButtonDefault(UIElement *parent, UIElementType type, const char *text,
                                 Size size, Vector2d padding, const UIPalette *palette, UIEventHandler on_click,
                                 void *user_data, void *data_bind)
{
    if (!palette)
    {
        palette = UIPalette_GetActive();
    }

    ColourRgba border;
    ColourRgba fill;
    UIPalette_GetSurfaceColours(palette, UI_PALETTE_SURFACE_BUTTON,
                                &border, &fill);

    UIElement *btn = CreateUIButton(parent, type, text, size, padding,
                          border, fill, FONT_MEDIUM_WITH_COLOUR(palette->text),
                          on_click, user_data, data_bind);
    if (btn)
    {
        btn->border_weight = UIPalette_GetSurfaceBorderWeight(palette, UI_PALETTE_SURFACE_BUTTON);
    }
    return btn;
}

// Create a hover-reactive LABEL: a text label whose base on_hover fires while the
// pointer is over it. There is no built-in tint (decision 1b); any visual change is
// the handler's responsibility. Former UI_ELEMENT_HOVER_ITEM behaviour folds into a
// LABEL plus a base interaction handler.
UIElement *CreateUIHoverItemDefault(UIElement *parent, const char *text, Size size, Vector2d padding,
                                    const UIPalette *palette, UIEventHandler on_hover, void *user_data)
{
    if (!palette)
    {
        palette = UIPalette_GetActive();
    }

    ColourRgba border;
    ColourRgba fill;
    UIPalette_GetSurfaceColours(palette, UI_PALETTE_SURFACE_BUTTON, &border, &fill);

    UIElement *item = CreateUIElementInTree(
        UI_ELEMENT_LABEL, size, parent,
        (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, padding, border, fill);
    if (!item)
    {
        return NULL;
    }

    // Former hover-item text + font map onto the LABEL's label buffer and font.
    safe_strncpy(item->data.label.text.string, text, MAX_LABEL_CHARS);
    item->data.label.font = FONT_MEDIUM_WITH_COLOUR(palette->text);
    item->border_weight = UIPalette_GetSurfaceBorderWeight(palette, UI_PALETTE_SURFACE_BUTTON);
    item->on_hover = on_hover;                 // base handler (presence-based dispatch)
    item->interaction_user_data = user_data;   // base opaque per-element state
    item->is_draggable = false;
    SetUIElementTextHorizontalAlignment(item, UI_TEXT_ALIGN_LEFT);
    SetUIElementTextVerticalAlignment(item, UI_TEXT_VERTICAL_ALIGN_CENTRE);
    return item;
}

UIElement *CreateUIContainer(UIElement *parent, Size size, Offset offset,
                             Vector2d padding, const UIPalette *palette,
                             UIPaletteSurface surface, Layout child_spacing,
                             bool is_draggable, bool is_enabled)
{
    if (!palette)
    {
        palette = UIPalette_GetActive();
    }

    ColourRgba border;
    ColourRgba fill;
    UIPalette_GetSurfaceColours(palette, surface, &border, &fill);

    UIElement *cont = CreateUIElementInTree(UI_ELEMENT_CONTAINER, size, parent, offset,
                                            padding, border, fill);
    if (!cont)
    {
        return NULL;
    }

    cont->border_weight = UIPalette_GetSurfaceBorderWeight(palette, surface);
    cont->child_spacing = child_spacing;
    cont->is_draggable = is_draggable;
    cont->is_enabled = is_enabled;

    return cont;
}

void InitUIFields(UIElement *parent, const UIFieldSpec *specs, size_t count,
                  Vector2d row_padding, const UIPalette *palette)
{
    if (!parent || !specs)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        UIElement *input_child = CreateUILabeledFieldDefault(parent, specs[i].label,
                                                             specs[i].type, specs[i].size,
                                                             row_padding, palette);
        if (!input_child)
        {
            continue;
        }

        input_child->type = specs[i].type;
        if (specs[i].data_type >= FLOAT && specs[i].data_type <= STRING256)
        {
            input_child->data.textbox.data_type = specs[i].data_type;
        }

        if (specs[i].data_bind)
        {
            BindTextboxData(input_child, specs[i].data_type, specs[i].data_bind);
        }

        if (specs[i].target)
        {
            *specs[i].target = input_child;
        }

        if (specs[i].text_target)
        {
            *specs[i].text_target = &input_child->data.textbox.text;
        }
    }
}