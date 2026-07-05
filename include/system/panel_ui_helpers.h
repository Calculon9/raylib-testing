/**********************************************************************************************
*
PANEL UI HELPERS MODULE
*
**********************************************************************************************/
#ifndef PANEL_UI_HELPERS_H
#define PANEL_UI_HELPERS_H

#include "ui/ui.h"
#include "ui/text_region.h"

UIElement *CreatePanelTitleLabel(UIElement *parent,
                                 const char *text,
                                 Size size,
                                 Vector2d padding,
                                 Bitmap_Font font,
                                 ColourRgba border,
                                 ColourRgba fill);

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
                                   Bitmap_Font font);

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
                             void *data_bind);

UIElement *CreatePanelContainer(UIElement *parent,
                                          Size size,
                                          Offset offset,
                                          Vector2d padding,
                                          ColourRgba border,
                                          ColourRgba fill,
                                          Spacing child_spacing,
                                          bool is_draggable,
                                          bool is_enabled);

#endif
