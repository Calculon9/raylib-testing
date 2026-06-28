/**********************************************************************************************

 **********************************************************************************************/
#include <stdio.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "ui/text_region.h"
#include "system/ui_system.h"
#include "colour/colour.h"
#include "raylib.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
void DrawChar(char c, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour);

// Takes in pixel coord point and determines if they are in the target region
bool IsFocused(Vector2d pixel_coords, Vector2d *vertices, int vertex_count)
{
    // Vector2d *vertices = polygon->vertices.coll.items;
    return IsPointInPolygon(pixel_coords, vertices, ZERO_VECTOR_2D, vertex_count);
}

// Ensure the dimensions and label_tbox_offset values are the same SIZE_MODE (e.g., fixed, percentage)
UIElement *CreateTextFieldInTree(Size size, UIElement *parent, Offset parent_offset, Size tbox_size, Vector2d padding, Vector2d label_tbox_offset, ColourRgba colour_border, ColourRgba colour_fill)
{
    UIElement *tf = CreateUIElementInTree(UI_ELEMENT_TEXTFIELD, size, parent, parent_offset, padding, colour_border, colour_fill);
    tf->type = UI_ELEMENT_TEXTFIELD;

    UIElement *tl = CreateUIElement(UI_ELEMENT_LABEL, (Size){ZERO_VECTOR_2D, SIZE_PERCENT}, (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, ZERO_VECTOR_2D, colour_border, colour_fill);
    UIElement *tb = CreateUIElement(UI_ELEMENT_TEXTBOX_SAFE_IO, (Size){ZERO_VECTOR_2D, SIZE_PERCENT}, (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, ZERO_VECTOR_2D, colour_border, colour_fill);
    tl->type = UI_ELEMENT_LABEL;
    tb->type = UI_ELEMENT_TEXTBOX_SAFE_IO;

    // Determine layout style
    bool label_is_inline = (label_tbox_offset.y < size.dimensions.y / 4);

    if (tbox_size.size_mode == SIZE_PERCENT)
    {
        if (label_is_inline) // Inline layout
        {
            // 1. Percentage-based Inline Layout (e.g., 40% Label, 60% TextBox)
            tl->size.dimensions.x = 1.0 - tbox_size.dimensions.x; // 40% of parent width
            tl->size.dimensions.y = 1.0f;                       // 100% of parent height
            tl->parent_offset.offset = (Vector2d){0, 0};

            tb->size.dimensions.x = tbox_size.dimensions.x; // 60% of parent width
            tb->size.dimensions.y = 1.0f;
            // Offset starts where the label ends (40% mark)
            // Note: Using percentage for offset requires Resolve function to handle it!
            tb->parent_offset.offset = (Vector2d){1.0 - tbox_size.dimensions.x, 0};
        }
        else // Stacked Layout: Label top (e.g. 30%), TextBox bottom (70%)
        {

            tl->size.dimensions.x = 1.0f;
            tl->size.dimensions.y = 1.0 - tbox_size.dimensions.y;
            tl->parent_offset.offset = (Vector2d){0, 0};

            tb->size.dimensions.x = 1.0f;
            tb->size.dimensions.y = tbox_size.dimensions.y;
            tb->parent_offset.offset = (Vector2d){0, 1.0 - tbox_size.dimensions.y};
        }
    }

    AddElementToTree(tl, tf);
    AddElementToTree(tb, tf);

    return tf;
}

// UIElement *CreateTextFieldInTree(UIElement *parent, Vector2d dimensions, Vector2d parent_offset, Vector2d padding, Vector2d label_tbox_offset, Vector2d label_tbox_padding, ColourRgba colour_border, ColourRgba colour_fill, int max_label_chars, int max_text_box_chars)
// {
//     UIElement *tf = CreateUIElementInTree(parent, dimensions, parent_offset, padding, colour_border, colour_fill);
//     //tf->children = *NewLArray(2, sizeof(UIElement *));
//     tf->type = UI_ELEMENT_TEXTFIELD;

//     // Determine if Label is above or inline
//     // If the vertical offset is small, we assume they sit side-by-side
//     bool label_is_inline = (label_tbox_offset.y < dimensions.y / 4);

//     // Initialise the tbox and label - init values to be overwritten as below.
//     UIElement *tb = CreateUIElement(dimensions, ZERO_VECTOR_2D, ZERO_VECTOR_2D, COLOUR_ERROR, COLOUR_WARNING);// AllocateBytes(sizeof(UIElement *)); // CreateUIElement(parent, width, height, origin_coords, parent_offset, padding, colour_border, colour_fill);
//     UIElement *tl = CreateUIElement(dimensions, ZERO_VECTOR_2D, ZERO_VECTOR_2D, COLOUR_ERROR, COLOUR_WARNING);// AllocateBytes(sizeof(UIElement *)); // CreateUIElement(parent, width, height, origin_coords, parent_offset, padding, colour_border, colour_fill);
//     tb->type = UI_ELEMENT_TEXTBOX;
//     tl->type = UI_ELEMENT_LABEL;

//     if (label_is_inline)
//     {
//         // 1. Calculate Width Proportions based on char counts
//         float total_chars = (float)(max_text_box_chars + max_label_chars);
//         float tl_w = (max_label_chars / total_chars) * (dimensions.x - label_tbox_offset.x);
//         float tb_w = dimensions.x - tl_w - label_tbox_offset.x;

//         // 2. Set Label Dimensions & Origin (Left Side)
//         tl->dimensions.x = tl_w;
//         tl->dimensions.y = dimensions.y;
//         tl->parent_offset = (Vector2d){0, 0};

//         // 3. Set TextBox Dimensions & Origin (Right Side, shifted by label + offset)
//         tb->dimensions.x = tb_w;
//         tb->dimensions.y = dimensions.y;
//         tb->parent_offset = (Vector2d){tl_w + label_tbox_offset.x, 0};
//     }
//     else
//     {
//         // Stacked Layout: Label is above the TextBox
//         // Label takes full width, height is determined by the offset
//         tl->dimensions.x = dimensions.x;
//         tl->dimensions.y = label_tbox_offset.y;
//         tl->parent_offset = (Vector2d){0, 0};

//         // TextBox takes full width, starts below the label offset
//         tb->dimensions.x = dimensions.x;
//         tb->dimensions.y = dimensions.y - label_tbox_offset.y;
//         tb->parent_offset = (Vector2d){0, label_tbox_offset.y};
//     }
//     AddElementToTree(tl, tf);
//     AddElementToTree(tb, tf);
//     //tl->parent = tf;
//     //tb->parent = tf; // TextBox bubbles to the Container
//     // Assign the tbox and label as the TextField's children
//     //LArray_Push(&tf->children, &tl);
//     //LArray_Push(&tf->children, &tb);

//     return tf;
// }

// void DisposeTextField(UIElement *tf)
// {
//     if (tf == NULL)
//         return;

//     DisposeUIElement(((UIElement **)(tf->children.items))[0]); // Label
//     DisposeUIElement(((UIElement **)(tf->children.items))[1]); // TextBox
//     DisposeUIElement(tf);
// }

UIElement *CreateTextFieldContainerInTree(Size size, UIElement *parent, Offset parent_offset, Vector2d padding, Spacing child_spacing, ColourRgba colour_border, ColourRgba colour_fill)
{
    UIElement *tc = CreateUIElementInTree(UI_ELEMENT_CONTAINER, size, parent, parent_offset, padding, colour_border, colour_fill);
    tc->child_spacing = child_spacing;
    // tc->children = *NewLArray(4, sizeof(UIElement *));

    return tc;
}

// TextField *CreateTextField(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d label_tbox_offset, Vector2d label_tbox_padding, char max_label_chars, char max_text_box_chars)
// {
//     TextField *tf = AllocateBytes(sizeof(TextField));
//     tf->origin = origin_coords;
//     tf->width = width;
//     tf->height = height;
//     tf->parent_offset = parent_offset;
//     // tf.parent = parent;

//     // Determine if Label is above or inline
//     // If the vertical offset is small, we assume they sit side-by-side
//     bool label_is_inline = (label_tbox_offset.y < height / 4);

//     TextBox tb = {0};
//     TextLabel tl = {0};
//     // Set backlink for the label to the TextField we are currently building
//     // Note: We'll set tf.label = tl at the end, copying the data.
//     tl.parent = tf;
//     tb.parent = tf; // TextBox bubbles to the Container

//     tb.max_len = max_text_box_chars;

//     if (label_is_inline)
//     {
//         // 1. Calculate Width Proportions based on char counts
//         float total_chars = (float)(max_text_box_chars + max_label_chars);
//         float tl_w = (max_label_chars / total_chars) * (width - label_tbox_offset.x);
//         float tb_w = width - tl_w - label_tbox_offset.x;

//         // 2. Set Label Dimensions & Origin (Left Side)
//         tl.width = tl_w;
//         tl.height = height;
//         tl.origin = origin_coords;
//         tl.parent_offset = (Vector2d){0, 0};

//         // 3. Set TextBox Dimensions & Origin (Right Side, shifted by label + offset)
//         tb.width = tb_w;
//         tb.height = height;
//         tb.origin.x = origin_coords.x + tl_w + label_tbox_offset.x;
//         tb.origin.y = origin_coords.y;
//         tb.parent_offset = (Vector2d){tl_w + label_tbox_offset.x, 0};
//     }
//     else
//     {
//         // Stacked Layout: Label is above the TextBox
//         // Label takes full width, height is determined by the offset
//         tl.width = width;
//         tl.height = label_tbox_offset.y;
//         tl.origin = origin_coords;
//         tl.parent_offset = (Vector2d){0, 0};

//         // TextBox takes full width, starts below the label offset
//         tb.width = width;
//         tb.height = height - label_tbox_offset.y;
//         tb.origin.x = origin_coords.x;
//         tb.origin.y = origin_coords.y + label_tbox_offset.y;
//         tb.parent_offset = (Vector2d){0, label_tbox_offset.y};
//     }

//     // Assign the sub-structs to the main TextField
//     tf->label = tl;
//     tf->text_box = tb;

//     return tf;
// }

// UIElement *CreateTextBox(float width, float height, Vector2d origin_coords, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill)
// {
//     TextBox *t = AllocateBytes(sizeof(TextBox));
//     t->origin = origin_coords;
//     t->colour_fill = colour_fill;
//     t->colour_border = colour_border;
//     t->width = width;
//     t->height = height;
//     t->padding = padding;
//     t->is_focused = false;
//     t->is_read_only = false;
//     t->cursor_pos = 0;
//     TextField *parent; // The "Backlink" for bubbling up changes to the text field container (e.g., for re-rendering when text changes)

//     return t;
// }

ShortString GetText_TextField(TextField *text_box)
{
    // ShortString str[64] = {0};
    // strncpy(str, text_box->text, sizeof(str) - 1); // Copy text with safety check to prevent overflow

    // return str;
}

int GetTextWidth(char *text, char font_spacing, char scale)
{
    if (text == NULL)
        return 0;

    int char_count = strlen(text);
    if (char_count == 0)
        return 0;

    // In case there was no string termination
    // If there isn't, strlen will wonder outside the provided text buffer and count the 1st byte outside it as a char!
    if (text[char_count - 1] != '\0')
    {
        char_count--;
    }
    // 8 pixels for the char + 1 pixel for spacing = 9 total per char
    // Note: We subtract the very last spacing pixel for a perfect fit
    // (8 pixels per char + x pixel spacing) * scale
    int width = char_count * ((8 * scale) + (scale * font_spacing));
    // int width = char_count * ((8 + font_spacing) * scale); (8 * scale) + (scale * font.spacing);
    //  int width = (char_count * (8 * scale));// + ((char_count - 1) * scale);

    return width;
}

Vector2d GetTextCenterPos(const char *text, float fontSize, Vector2d origin)
{
    // 1. Calculate the center of the specific cell (c, r)
    // float centerX = origin.x + (c + 0.5f) * u.x + (r + 0.5f) * v.x;
    // float centerY = origin.y + (c + 0.5f) * u.y + (r + 0.5f) * v.y;

    // // 2. Measure the text dimensions
    // Vector2d textSize = MeasureTextEx(font, text, fontSize, 1.0f);

    // // 3. Subtract half dimensions to get the starting (top-left) point
    // Vector2d startPos;
    // startPos.x = centerX - (textSize.x / 2.0f);
    // startPos.y = centerY - (textSize.y / 2.0f);

    // return startPos;
}

// Custom text drawing function that uses our Bitmap_Font and supports scaling and color. Coordinate origin is the top-left corner of the text in pixels.
float DrawTextCustom(const char *text, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour)
{
    Vector2d char_coords = origin_coords;
    while (*text)
    {
        DrawChar(*text, char_coords, scale, font, colour);

        // Move to the next character slot
        // 8 pixels wide * scale + 1 pixel of "letter spacing"
        char_coords.x += (8 * scale) + (scale * font.spacing); // - (scale * scale) + 1; // Subtracting "scale" removes embedded white-space in each character
        text++;
    }
    return char_coords.x;
}

void DrawChar(char c, Vector2d origin_coords, int scale, Bitmap_Font font, ColourRgba colour)
{
    // Cast to unsigned to handle extended ASCII safely
    unsigned char u_c = (unsigned char)c;

    for (int row = 0; row < 8; row++)
    {
        unsigned char row_data = font.bitmap[u_c][row];

        for (int col = 0; col < 8; col++)
        {
            // We use a bitmask (0x80 is 10000000) and shift it right to check each bit in the byte.
            // The >> is the Bitwise Right Shift. It literally pushes the bits to the right by the number of places specified by col.
            if (row_data & (0x80 >> col))
            {
                // If the bit is 1, draw a "pixel" scaled up
                DrawRectangle(origin_coords.x + (col * scale), origin_coords.y + (row * scale), scale, scale, (Color){colour.r, colour.g, colour.b, colour.a});
            }
        }
    }
}

// void *GetTextFieldVertices(TextField text_box, Vector2d out_vertices[4])
// {
//     out_vertices[0] = text_box.origin;
//     out_vertices[1] = (Vector2d){text_box.origin.x + text_box.width, text_box.origin.y};
//     out_vertices[2] = (Vector2d){text_box.origin.x + text_box.width, text_box.origin.y + text_box.height};
//     out_vertices[3] = (Vector2d){text_box.origin.x, text_box.origin.y + text_box.height};
// }

// void EvalChildrenOriginsFromParentOffset(TextFieldsContainer *tfcont)
// {
//     // TextFieldsContainer origin and additional space + offsets that will affect the origin of its children
//     // There is no TextField padding (currently), so nothing funnels down to children, just the predetermined parent_offsets specified when the children were initialised
//     Vector2d tfcont_origin = tfcont->origin;
//     Collection tfield_coll = tfcont->text_fields.coll;

//     for (int i = 0; i < tfield_coll.count; i++)
//     {
//         TextField *tfield = ((TextField **)tfield_coll.items)[i];
//         Vector2d tfield_origin_total = VectorSum_2d(VectorSum_2d(tfield->parent_offset, tfcont->origin), tfcont->padding); // offset rel to parent (container) + parent padding
//         tfield->origin = tfield_origin_total;
//     }
// }

// void EvalTextFieldChildrenOrigins(TextField *text_field)
// {
//     // TextField origin and additional space + offsets that will affect the origin of its children
//     // There is no TextField padding (currently), so nothing funnels down to children, just the predetermined parent_offsets specified when the children were initialised
//     Vector2d tfield_origin = text_field->origin;
//     Vector2d tbox_parent_offset = text_field->text_box.parent_offset;
//     Vector2d tlabel_parent_offset = text_field->label.parent_offset;

//     text_field->text_box.origin = VectorSum_2d(tfield_origin, tbox_parent_offset);
//     text_field->label.origin = VectorSum_2d(tfield_origin, tlabel_parent_offset);
// }