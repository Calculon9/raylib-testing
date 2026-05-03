/**********************************************************************************************

 **********************************************************************************************/
#include <stdio.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "ui/ui.h"
#include "raylib.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
// void DrawChar(char c, float x, float y, int scale, Bitmap_Font font, ColourRgba colour);

// // Takes in pixel coord point and determines if they are in the target region
// bool IsFocused(Vector2d pixel_coords, Vector2d *vertices, int vertex_count)
// {
//     // Vector2d *vertices = polygon->vertices.coll.items;
//     return IsPointInPolygon(pixel_coords, vertices, vertex_count);
// }

UIElement *CreateUIElement(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill)
{
    UIElement *e = AllocateBytes(sizeof(UIElement));
    // e->origin = origin_coords;
    e->colour_fill = colour_fill;
    e->colour_border = colour_border;
    e->width = width;
    e->height = height;
    e->padding = padding;

    return e;
}

UIElement *CreateUIElementUnderParent(UIElement *parent, float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill)
{
    UIElement *e = AllocateBytes(sizeof(UIElement));
    e->parent = parent;
    e->parent_offset = parent_offset;
    // e->origin = origin_coords;
    e->colour_fill = colour_fill;
    e->colour_border = colour_border;
    e->width = width;
    e->height = height;
    e->padding = padding;

    // Get the parent Element and add this as a child
    bool success = LArray_Push(&parent->children, &e);
    // e->children = *NewLArray(4, sizeof(UIElement));

    if (success)
    {
        return e;
    }
    else
    {
        printf("Error: Failed to add UIElement to parent's children array.\n");
        return e;
    }
}

bool DisposeUIElement(UIElement *e)
{
    // Remove from parent's children array
    if (e->parent != NULL)
    {
        LArray *siblings = &e->parent->children;
        for (size_t i = 0; i < siblings->count; i++)
        {
            if (((UIElement **)siblings->items)[i] == e)
            {
                LArray_RemoveAt(siblings, i);
                break;
            }
        }
    }

    // Free the element's own resources
    // Note: If the element has its own children, you may want to recursively destroy them here as well. We will.
    for (size_t i = 0; i < e->children.count; i++)
    {
        DisposeUIElement(((UIElement **)e->children.items)[i]);
    }
    free(e);
    return true;
}

void GetUIElementVertices(UIElement *e, Vector2d out_vertices[4])
{
    out_vertices[0] = e->origin;
    out_vertices[1] = (Vector2d){e->origin.x + e->width, e->origin.y};
    out_vertices[2] = (Vector2d){e->origin.x + e->width, e->origin.y + e->height};
    out_vertices[3] = (Vector2d){e->origin.x, e->origin.y + e->height};
}

// UIElement *CreateTextField(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d label_tbox_offset, Vector2d label_tbox_padding, char max_label_chars, char max_text_box_chars)
// {
//     UIElement *tf = AllocateBytes(sizeof(UIElement));
//     tf->origin = origin_coords;
//     tf->width = width;
//     tf->height = height;
//     tf->parent_offset = parent_offset;
//     tf->children = *NewLArray(2, sizeof(UIElement));
//     // tf.parent = parent;

//     // Determine if Label is above or inline
//     // If the vertical offset is small, we assume they sit side-by-side
//     bool label_is_inline = (label_tbox_offset.y < height / 4);

//     UIElement tb = {0};
//     UIElement tl = {0};
//     // Set backlink for the label to the TextField we are currently building
//     // Note: We'll set tf.label = tl at the end, copying the data.
//     tl.parent = tf;
//     tb.parent = tf; // TextBox bubbles to the Container

//     //tb.max_len = max_text_box_chars;

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
//     ((UIElement *)(tf->children.items))[0] = tl;
//     ((UIElement *)(tf->children.items))[1] = tb;

//     return tf;
// }

// TextBox *CreateTextBox(float width, float height, Vector2d origin_coords, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill)
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

// TextFieldsContainer *CreateTextFieldContainer(float width, float height, Vector2d origin_coords, Vector2d padding, Vector2d field_spacing, ColourRgba colour_border, ColourRgba colour_fill)
// {
//     TextFieldsContainer *tc = AllocateBytes(sizeof(TextFieldsContainer));
//     tc->origin = origin_coords;
//     tc->colour_fill = colour_fill;
//     tc->colour_border = colour_border;
//     tc->width = width;
//     tc->height = height;
//     tc->padding = padding;
//     tc->field_spacing = field_spacing;

//     tc->text_fields = *NEW_DYNAMIC_ARRAY(4, sizeof(TextField)); // Start with capacity for 4 text fields, will grow as needed
//     return tc;
// }

// ShortString GetText_TextField(TextField *text_box)
// {
//     // ShortString str[64] = {0};
//     // strncpy(str, text_box->text, sizeof(str) - 1); // Copy text with safety check to prevent overflow

//     // return str;
// }

// int MeasureTextWidth(const char *text, char font_spacing, char scale)
// {
//     if (text == NULL)
//         return 0;

//     int char_count = strlen(text);
//     if (char_count == 0)
//         return 0;

//     // 8 pixels for the char + 1 pixel for spacing = 9 total per char
//     // Note: We subtract the very last spacing pixel for a perfect fit
//     // (8 pixels per char + x pixel spacing) * scale
//     int width = char_count * ((8 * scale) + (scale * font_spacing));
//     // int width = char_count * ((8 + font_spacing) * scale); (8 * scale) + (scale * font.spacing);
//     //  int width = (char_count * (8 * scale));// + ((char_count - 1) * scale);

//     return width;
// }

// Vector2d GetTextCenterPos(const char *text, float fontSize, Vector2d origin)
// {
//     // 1. Calculate the center of the specific cell (c, r)
//     // float centerX = origin.x + (c + 0.5f) * u.x + (r + 0.5f) * v.x;
//     // float centerY = origin.y + (c + 0.5f) * u.y + (r + 0.5f) * v.y;

//     // // 2. Measure the text dimensions
//     // Vector2d textSize = MeasureTextEx(font, text, fontSize, 1.0f);

//     // // 3. Subtract half dimensions to get the starting (top-left) point
//     // Vector2d startPos;
//     // startPos.x = centerX - (textSize.x / 2.0f);
//     // startPos.y = centerY - (textSize.y / 2.0f);

//     // return startPos;
// }

// // Custom text drawing function that uses our Bitmap_Font and supports scaling and color. Coordinate origin is the top-left corner of the text in pixels.
// float DrawTextCustom(const char *text, float origin_x, float origin_y, char scale, Bitmap_Font font, ColourRgba colour)
// {
//     float current_x = origin_x;
//     while (*text)
//     {
//         DrawChar(*text, current_x, origin_y, scale, font, colour);

//         // Move to the next character slot
//         // 8 pixels wide * scale + 1 pixel of "letter spacing"
//         current_x += (8 * scale) + (scale * font.spacing); // - (scale * scale) + 1; // Subtracting "scale" removes embedded white-space in each character
//         text++;
//     }
//     return current_x;
// }

// void DrawChar(char c, float origin_x, float origin_y, int scale, Bitmap_Font font, ColourRgba colour)
// {
//     // Cast to unsigned to handle extended ASCII safely
//     unsigned char u_c = (unsigned char)c;

//     for (int row = 0; row < 8; row++)
//     {
//         unsigned char row_data = font.bitmap[u_c][row];

//         for (int col = 0; col < 8; col++)
//         {
//             // We use a bitmask (0x80 is 10000000) and shift it right to check each bit in the byte.
//             // The >> is the Bitwise Right Shift. It literally pushes the bits to the right by the number of places specified by col.
//             if (row_data & (0x80 >> col))
//             {
//                 // If the bit is 1, draw a "pixel" scaled up
//                 DrawRectangle(origin_x + (col * scale), origin_y + (row * scale), scale, scale, (Color){colour.r, colour.g, colour.b, colour.a});
//             }
//         }
//     }
// }

// void GetTextFieldVertices(TextField text_box, Vector2d out_vertices[4])
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