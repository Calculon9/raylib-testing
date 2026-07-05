#include "system/panel_ui_helpers.h"

#include "system/str_helpers.h"

UIElement *CreatePanelTitleLabel(UIElement *parent,
                                 const char *text,
                                 Size size,
                                 Vector2d padding,
                                 Bitmap_Font font,
                                 ColourRgba border,
                                 ColourRgba fill)
{
    UIElement *title = CreateUIElementInTree(UI_ELEMENT_LABEL,
                                             size,
                                             parent,
                                             (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                             padding,
                                             border,
                                             fill);

    if (!title)
    {
        return NULL;
    }

    safe_strncpy(title->data.label.text.string, text, MAX_LABEL_CHARS);
    title->data.label.font = font;
    title->is_draggable = true;

    return title;
}

UIElement *CreatePanelLabeledField(UIElement *parent,
                                   const char *label_text,
                                   UIElementType input_type,
                                   Size row_size,
                                   Size textbox_size,
                                   Vector2d row_padding,
                                   Vector2d label_offset,
                                   ColourRgba row_border,
                                   ColourRgba row_fill,
                                   Vector2d cell_padding,
                                   ColourRgba cell_border,
                                   ColourRgba cell_fill,
                                   Bitmap_Font font)
{
    UIElement *tfield = CreateTextFieldInTree(row_size,
                                              parent,
                                              (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                              textbox_size,
                                              row_padding,
                                              label_offset,
                                              row_border,
                                              row_fill);
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

UIElement *CreatePanelButton(UIElement *parent,
                             UIElementType type,
                             const char *text,
                             Size size,
                             Vector2d padding,
                             ColourRgba border,
                             ColourRgba fill,
                             Bitmap_Font font,
                             UIEventHandler on_click,
                             void *user_data,
                             void *data_bind)
{
    UIElement *btn = CreateUIElementInTree(type,
                                           size,
                                           parent,
                                           (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                           padding,
                                           border,
                                           fill);
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

UIElement *CreatePanelContainer(UIElement *parent,
                                Size size,
                                Offset offset,
                                Vector2d padding,
                                ColourRgba border,
                                ColourRgba fill,
                                Spacing child_spacing,
                                bool is_draggable,
                                bool is_enabled)
{
    UIElement *cont = CreateUIElementInTree(UI_ELEMENT_CONTAINER,
                                            size,
                                            parent,
                                            offset,
                                            padding,
                                            border,
                                            fill);
    if (!cont)
    {
        return NULL;
    }

    cont->child_spacing = child_spacing;
    cont->is_draggable = is_draggable;
    cont->is_enabled = is_enabled;

    return cont;
}
