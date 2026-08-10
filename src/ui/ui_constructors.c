#include "ui/ui_constructors.h"

#include "system/utility_system.h"
#include "system/ui_system.h"

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
        palette = &ui_default_palette;
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
        palette = &ui_default_palette;
    }

    return CreateUILabel(parent, text, size, padding,
                         FONT_LARGE_WITH_COLOUR(palette->text),
                         COLOURLESS_RGBA, COLOURLESS_RGBA);
}

UIElement *CreateUILabeledField(UIElement *parent, const char *label_text, UIElementType input_type,
                                Size row_size, Size textbox_size, Vector2d row_padding,
                                ColourRgba row_border, ColourRgba row_fill, Vector2d cell_padding, 
                                ColourRgba cell_border, ColourRgba cell_fill,
                                Bitmap_Font label_font, Bitmap_Font font)
{
    UIElement *tfield = CreateTextFieldInTree(row_size, parent, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                              textbox_size, row_padding, true, row_border, row_fill, font);
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
        palette = &ui_default_palette;
    }

    ColourRgba row_border;
    ColourRgba row_fill;
    UIPalette_GetSurfaceColours(palette, UI_PALETTE_SURFACE_FIELD_ROW, &row_border, &row_fill);

    ColourRgba input_border;
    ColourRgba input_fill;
    UIPalette_GetSurfaceColours(palette, UI_PALETTE_SURFACE_INPUT,
                                &input_border, &input_fill);

    return CreateUILabeledField(parent, label_text, input_type, row_size,
                                ui_standard_textfield_input_size, row_padding, row_border, row_fill,
                                ZERO_VECTOR_2D, input_border, input_fill,
                                FONT_MEDIUM_WITH_COLOUR(palette->label_text),
                                FONT_MEDIUM_WITH_COLOUR(palette->text));
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

    safe_strncpy(btn->data.button.label.string, text, MAX_LABEL_CHARS);
    btn->data.button.font = font;
    SetUIElementTextHorizontalAlignment(btn, UI_TEXT_ALIGN_CENTRE);
    SetUIElementTextVerticalAlignment(btn, UI_TEXT_VERTICAL_ALIGN_CENTRE);
    btn->data.button.on_click = on_click;
    btn->data.button.user_data = user_data;
    btn->data.button.data_bind = data_bind;
    btn->is_draggable = true;

    return btn;
}

UIElement *CreateUIButtonDefault(UIElement *parent, UIElementType type, const char *text,
                                 Size size, Vector2d padding, const UIPalette *palette, UIEventHandler on_click,
                                 void *user_data, void *data_bind)
{
    if (!palette)
    {
        palette = &ui_default_palette;
    }

    ColourRgba border;
    ColourRgba fill;
    UIPalette_GetSurfaceColours(palette, UI_PALETTE_SURFACE_BUTTON,
                                &border, &fill);

    return CreateUIButton(parent, type, text, size, padding,
                          border, fill, FONT_MEDIUM_WITH_COLOUR(palette->text),
                          on_click, user_data, data_bind);
}

UIElement *CreateUIHoverItemDefault(UIElement *parent, const char *text, Size size, Vector2d padding,
                                    const UIPalette *palette, UIEventHandler on_hover, void *user_data)
{
    if (!palette)
    {
        palette = &ui_default_palette;
    }

    ColourRgba border;
    ColourRgba fill;
    UIPalette_GetSurfaceColours(palette, UI_PALETTE_SURFACE_BUTTON, &border, &fill);

    UIElement *item = CreateUIElementInTree(
        UI_ELEMENT_HOVER_ITEM, size, parent,
        (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, padding, border, fill);
    if (!item)
    {
        return NULL;
    }

    safe_strncpy(item->data.hover_item.label.string, text, MAX_LABEL_CHARS);
    item->data.hover_item.font = FONT_MEDIUM_WITH_COLOUR(palette->text);
    item->data.hover_item.on_hover = on_hover;
    item->data.hover_item.data_bind = NULL;
    item->data.hover_item.user_data = user_data;
    item->data.hover_item.normal_fill = fill;
    item->data.hover_item.hover_fill = palette->button_fill;
    item->is_draggable = false;
    SetUIElementTextHorizontalAlignment(item, UI_TEXT_ALIGN_LEFT);
    SetUIElementTextVerticalAlignment(item, UI_TEXT_ALIGN_CENTRE);
    return item;
}

UIElement *CreateUIContainer(UIElement *parent, Size size, Offset offset,
                             Vector2d padding, const UIPalette *palette,
                             UIPaletteSurface surface, Spacing child_spacing,
                             bool is_draggable, bool is_enabled)
{
    if (!palette)
    {
        palette = &ui_default_palette;
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