/**********************************************************************************************
*
TEXT REGION MODULE
*
**********************************************************************************************/
#ifndef TEXT_REGION_H
#define TEXT_REGION_H
#include "common/common.h"
#include "math/cvectors.h"
#include "math/geometry.h"
#include "colour/colour.h"
#include "ui/cfont.h"
#include "ui/ui.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define MAX_TEXTBOX_CHARS (int)sizeof(String64)
#define MAX_LABEL_CHARS (int)sizeof(String64)
//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// typedef struct TextLabel;
typedef struct TextField TextField;
// typedef struct TextFieldsContainer TextFieldsContainer;

typedef struct TextFieldsContainer
{
    DArray text_fields;
    ColourRgba colour_border;
    ColourRgba colour_fill;
    Vector2d text_field_spacing;
    Vector2d padding;
    Vector2d resolved_origin;
    Vector2d field_spacing;
    float width, height;
} TextFieldsContainer;

typedef struct TextLabel
{
    char text[64];     // Storage for up to 63 chars + null terminator
    TextField *parent; // The "Backlink" for bubbling up changes to the text field container (e.g., for re-rendering when text changes)
    ColourRgba colour_border;
    ColourRgba colour_fill;
    Vector2d padding;
    Vector2d resolved_origin;
    Vector2d parent_offset;
    float width, height;
} TextLabel;

typedef struct TextBox
{
    char text[64];     // Storage for up to 63 chars + null terminator
    TextField *parent; // The "Backlink" for bubbling up changes to the text field container (e.g., for re-rendering when text changes)
    int cursor_pos;    // You'll need this to know where to add/delete chars
    int max_len;

    //ColourRgba colour_border_outer;
    ColourRgba colour_border;
    ColourRgba colour_fill;
    //ColourRgba colour_fill_inner;
    Vector2d padding;
    //Vector2d padding_outer;
    Vector2d resolved_origin;
    Vector2d parent_offset;
    float width, height;
    bool is_focused, is_read_only;
} TextBox;

typedef struct TextField
{
    TextBox text_box;            // Storage for up to 63 chars + null terminator
    Vector2d parent_offset;
    TextLabel label;             // Optional label for the text box
    TextFieldsContainer *parent; // The "Backlink" for bubbling up changes to the text field container (e.g., for re-rendering when text changes)
    Vector2d resolved_origin;
    float width, height;
} TextField;

typedef struct
{
    Polygon text_box_perimeter;
    ColourRgba colour_border;
    ColourRgba colour_fill;
    float width, height, padding_inner, padding_outer;
} TextPolygon;

typedef struct
{
    char text[32];
} ShortString;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
//TextField CreateTextField(float width, float height, Vector2d origin_coords, Vector2d padding_inner, Vector2d padding_outer, ColourRgba colour_border_outer, ColourRgba colour_fill_outer, ColourRgba colour_border_inner, ColourRgba colour_fill_inner);
UIElement *CreateTextFieldInTree(Size size, UIElement *parent, Offset parent_offset, Size tbox_size, Vector2d padding, Vector2d label_tbox_offset, ColourRgba colour_border, ColourRgba colour_fill);
UIElement *CreateTextFieldContainerInTree(Size size, UIElement *parent, Offset parent_offset, Vector2d padding, Spacing child_spacing, ColourRgba colour_border, ColourRgba colour_fill);
//ShortString GetText_TextField(TextField *text_box);
//void *GetTextFieldVertices(TextField text_box, Vector2d out_vertices[4]);
bool IsFocused(Vector2d pixel_coords, Vector2d *vertices, int vertex_count);
// bool IsFocused(Vector2d pixel_coords, Polygon *polygon);
float DrawTextCustom(const char *text, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour);
int GetTextWidth(char *text, char font_spacing, char scale);


#endif
