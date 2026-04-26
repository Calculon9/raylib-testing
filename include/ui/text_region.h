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

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct
{
    DynamicArray text_fields;
    ColourRgba colour_border_outer;
    ColourRgba colour_border_inner;
    ColourRgba colour_fill_outer;
    ColourRgba colour_fill_inner;
    Vector2d text_field_spacing;
    Vector2d padding_inner;
    Vector2d padding_outer;
    Vector2d origin;
    float width, height;
} TextFieldsContainer;

typedef struct
{
    char text[64];  // Storage for up to 63 chars + null terminator
    TextLabel label; // Optional label for the text box
    TextFieldsContainer *parent; // The "Backlink" for bubbling up changes to the text field container (e.g., for re-rendering when text changes)
    int cursor_pos; // You'll need this to know where to add/delete chars
    int max_len;

    ColourRgba colour_border_outer;
    ColourRgba colour_border_inner;
    ColourRgba colour_fill_outer;
    ColourRgba colour_fill_inner;
    Vector2d padding_inner;
    Vector2d padding_outer;
    Vector2d origin;
    float width, height;
    bool is_focused, is_read_only;
} TextField;

typedef struct
{
    char text[64];  // Storage for up to 63 chars + null terminator
    TextField *parent; // The "Backlink" for bubbling up changes to the text field container (e.g., for re-rendering when text changes)
    ColourRgba colour_border;
    ColourRgba colour_fill;
    Vector2d padding;
    Vector2d origin;
    float width, height;
    bool is_read_only;
} TextLabel;

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
TextField CreateTextField(float width, float height, Vector2d origin_coords, Vector2d padding_inner, Vector2d padding_outer, ColourRgba colour_border_outer, ColourRgba colour_fill_outer, ColourRgba colour_border_inner, ColourRgba colour_fill_inner);
TextFieldsContainer CreateTextFieldContainer(float width, float height, Vector2d origin_coords, Vector2d padding_inner, Vector2d padding_outer, ColourRgba colour_border_outer, ColourRgba colour_fill_outer, ColourRgba colour_border_inner, ColourRgba colour_fill_inner);
ShortString GetText_TextField(TextField *text_box);
Vector2d *GetTextFieldVertices(TextField text_box);
bool IsFocused(Vector2d pixel_coords, Vector2d *vertices, int vertex_count);
// bool IsFocused(Vector2d pixel_coords, Polygon *polygon);
float DrawTextCustom(const char *text, float x, float y, char scale, Bitmap_Font font, ColourRgba colour);
int MeasureTextWidth(const char *text, char font_spacing, char scale);
// Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis);
// Field UpdateFieldCellValues(Field field);

#endif