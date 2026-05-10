/**********************************************************************************************
*
WORLD MODULE
*
**********************************************************************************************/
#ifndef UI_H
#define UI_H
#include "common/common.h"
#include "math/cvectors.h"
#include "math/coordinate_space.h"
#include "colour/colour.h"
#include "ui/cfont.h"
#include "system/systems.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define GET_UI_ELEMENT(la, idx) (*(UIElement **)LArray_Get(la, idx))
#define ZERO_BOX \
    (UIBox) { .coords = {0, 0}, .dimensions = {0, 0} }
//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef enum
{
    UI_ELEMENT_ROOT,
    UI_ELEMENT_CONTAINER,
    UI_ELEMENT_TEXTBOX,
    UI_ELEMENT_TEXTBOX_SAFE, // Will only save over previous text if ENTER is pressed
    UI_ELEMENT_TEXTFIELD,
    UI_ELEMENT_LABEL,
    UI_ELEMENT_BUTTON,
    UI_ELEMENT_IMAGE
} UIElementType;

typedef enum
{
    SIZE_FIXED,   // Use the literal pixel width
    SIZE_PERCENT, // Use a percentage of the parent's content area (0.0 to 1.0)
    SIZE_FILL,    // Take up all remaining space
} SizeMode;

typedef enum
{
    OFFSET_FIXED,   // Use the literal pixel width
    OFFSET_PERCENT, // Use a percentage of the parent's content area (0.0 to 1.0)
    ALIGNED_CENTRE
} OffsetMode;

typedef struct
{
    Vector2d offset;
    OffsetMode offset_mode;
} Offset;

typedef struct
{
    Vector2d dimensions;
    SizeMode size_mode;
} Size;

typedef struct
{
    CoordSpace2d coord_space;
} RootData;

typedef struct
{
    String64 text;
    Bitmap_Font font;
    int cursor_position;
} TextBoxData;

typedef struct
{
    String64 text;
    Bitmap_Font font;
} LabelData;

typedef struct
{
    TextBoxData textbox_data;
    LabelData label_data;
} TextFieldlData;

typedef struct
{
    String32 label;
    void (*on_click)(void);
} ButtonData;

typedef union
{
    RootData root;
    TextBoxData textbox;
    LabelData label;
    TextFieldlData textfield;
    ButtonData button;
    // UIImageData image;
} UIElementData;

typedef struct
{
    // int coords_x, coords_y, width, height;
    Vector2d coords;
    Vector2d dimensions;
} UIBox;

typedef struct UIElement UIElement;
typedef struct UIElement
{
    // LArray children;
    Offset parent_offset;
    //Vector2d parent_offset;
    Vector2d child_spacing;
    Vector2d padding;
    ColourRgba colour_border;
    ColourRgba colour_fill;
    Size size;
    UIElementType type;
    UIElementData data;
    UIBox cached_box;
    bool is_focused, is_dirty; // For interactive elements like TextBoxes and Buttons

    UIElement *parent;
    UIElement *first_child;
    UIElement *next_sibling;
} UIElement;

// typedef struct {
//     Texture *texture;
// } UIImageData;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
UIElement *CreateUIElement(UIElementType type, Size size, Offset parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill);
UIElement *CreateUIElementInTree(UIElementType type, Size size, UIElement *parent, Offset parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill);
void GetUIElementVertices(UIElement *e, Vector2d out_vertices[4]);
bool IsMouseOverElement(UIElement *el, Vector2d mouse_pos);
Vector2d ResolveElementPosition(UIElement *element, UIBox parent_box, Vector2d basis_scale);
UIBox ResolveElementBox(UIElement *element, UIBox parent_box, Vector2d basis_scale);
const char *GetElementTypeName(UIElementType type);
UIElement *GetLastChild(UIElement *e);
void AddElementToTree(UIElement *element, UIElement *parent);
void RemoveElementFromTree(UIElement *element);
UIElement *GetPreviousSibling(UIElement *element);
bool ElementHasSibling(UIElement *e);
UIElement *GetElementAt(UIElement *e, Vector2d pixel_coords);

// UIElement *CreateTextField(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d label_tbox_offset, Vector2d label_tbox_padding, char max_label_chars, char max_text_box_chars);
#endif