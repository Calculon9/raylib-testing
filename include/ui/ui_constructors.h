/**********************************************************************************************
*
UI CONSTRUCTORS MODULE
*
**********************************************************************************************/
#ifndef UI_CONSTRUCTORS_H
#define UI_CONSTRUCTORS_H

#include "ui/ui.h"
#include "ui/text_region.h"
#include "system/ui_system.h"

typedef struct UIFieldSpec
{
    const char *label;
    UIElementType type;
    Size size;
    DataType data_type;
    UIElement **target;
    String64 **text_target;
} UIFieldSpec;

UIElement *CreateUILabel(UIElement *parent, const char *text, Size size,
                         Vector2d padding, Bitmap_Font font,
                         ColourRgba border, ColourRgba fill);

UIElement *CreateUILabelDefault(UIElement *parent, const char *text,
                                Size size, Vector2d padding,
                                const UIPalette *palette);

UIElement *CreateUILabelTitleDefault(UIElement *parent, const char *text,
                                     Size size, Vector2d padding,
                                     const UIPalette *palette);

UIElement *CreateUILabeledField(UIElement *parent, const char *label_text,
                                UIElementType input_type, Size row_size,
                                Size textbox_size, Vector2d row_padding,
                                ColourRgba row_border, ColourRgba row_fill,
                                Vector2d cell_padding,
                                ColourRgba cell_border, ColourRgba cell_fill,
                                Bitmap_Font label_font, Bitmap_Font font);

UIElement *CreateUILabeledFieldDefault(UIElement *parent, const char *label_text,
                                       UIElementType input_type, Size row_size,
                                       Vector2d row_padding,
                                       const UIPalette *palette);

UIElement *CreateUIButton(UIElement *parent, UIElementType type,
                          const char *text, Size size,
                          Vector2d padding, ColourRgba border,
                          ColourRgba fill, Bitmap_Font font,
                          UIEventHandler on_click, void *user_data,
                          void *data_bind);

UIElement *CreateUIButtonDefault(UIElement *parent, UIElementType type,
                                 const char *text, Size size,
                                 Vector2d padding, const UIPalette *palette,
                                 UIEventHandler on_click,
                                 void *user_data, void *data_bind);

UIElement *CreateUIHoverItemDefault(UIElement *parent, const char *text,
                                    Size size, Vector2d padding,
                                    const UIPalette *palette,
                                    UIEventHandler on_hover,
                                    void *user_data);

UIElement *CreateUIContainer(UIElement *parent, Size size, Offset offset,
                             Vector2d padding, const UIPalette *palette,
                             UIPaletteSurface surface, Spacing child_spacing,
                             bool is_draggable, bool is_enabled);

void InitUIFields(UIElement *parent, const UIFieldSpec *specs, size_t count,
                  Vector2d row_padding, const UIPalette *palette);

#endif