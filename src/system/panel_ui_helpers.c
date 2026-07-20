#include "system/panel_ui_helpers.h"

#include "system/str_helpers.h"
#include "system/ui_system.h"

UIElement *CreatePanelTitleLabel(UIElement *parent, const char *text, Size size,
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

UIElement *CreatePanelTitleLabelDefault(UIElement *parent, const char *text, Size size, Vector2d padding)
{
    return CreatePanelTitleLabel(parent, text, size, padding, FONT_BASIC, COLOURLESS_RGBA, COLOURLESS_RGBA);
}

UIElement *CreatePanelLabeledField(UIElement *parent, const char *label_text, UIElementType input_type,
                                   Size row_size, Size textbox_size, Vector2d row_padding,
                                   Vector2d label_offset, ColourRgba row_border, ColourRgba row_fill,
                                   Vector2d cell_padding, ColourRgba cell_border, ColourRgba cell_fill,
                                   Bitmap_Font font)
{
    UIElement *tfield = CreateTextFieldInTree(row_size, parent,
                                              (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                              textbox_size, row_padding, label_offset,
                                              row_border, row_fill);
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
    label_child->colour_border = cell_border;
    label_child->colour_fill = cell_fill;
    label_child->data.label.font = font;
    safe_strncpy(label_child->data.label.text.string, label_text, MAX_LABEL_CHARS);

    input_child->padding = cell_padding;
    input_child->colour_border = cell_border;
    input_child->colour_fill = cell_fill;
    input_child->data.textbox.font = font;
    input_child->type = input_type;
    input_child->is_draggable = true;

    return input_child;
}

UIElement *CreatePanelLabeledFieldDefault(UIElement *parent, const char *label_text,
                                          UIElementType input_type, Size row_size,
                                          Vector2d row_padding, ColourRgba row_border,
                                          ColourRgba row_fill)
{
    return CreatePanelLabeledField(parent, label_text, input_type, row_size,
                                   tbox_default_size, row_padding, tbox_tlabel_default_offset.offset,
                                   row_border, row_fill, tbox_default_padding,
                                   tbox_default_colour_border, tbox_default_colour_fill,
                                   FONT_BASIC);
}

UIElement *CreatePanelButton(UIElement *parent, UIElementType type, const char *text,
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
    btn->data.button.on_click = on_click;
    btn->data.button.user_data = user_data;
    btn->data.button.data_bind = data_bind;
    btn->is_draggable = true;

    return btn;
}

UIElement *CreatePanelButtonDefault(UIElement *parent, UIElementType type,
                                    const char *text, Size size, Vector2d padding,
                                    UIEventHandler on_click, void *user_data,
                                    void *data_bind)
{
    return CreatePanelButton(parent, type, text, size, padding,
                             btn_default_colour_border, btn_default_colour_fill,
                             FONT_BASIC, on_click, user_data, data_bind);
}

UIElement *CreatePanelContainer(UIElement *parent, Size size, Offset offset,
                                Vector2d padding, ColourRgba border,
                                ColourRgba fill, Spacing child_spacing,
                                bool is_draggable, bool is_enabled)
{
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

void InitPanelFields(UIElement *parent, const PanelFieldSpec *specs, size_t count,
                     Vector2d row_padding, ColourRgba row_border, ColourRgba row_fill)
{
    if (!parent || !specs)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        UIElement *input_child = CreatePanelLabeledFieldDefault(parent, specs[i].label,
                                    specs[i].type, specs[i].size,
                                    row_padding, row_border, row_fill);
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
