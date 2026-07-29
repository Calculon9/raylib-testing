/**********************************************************************************************
*
WORLD MODULE
*
**********************************************************************************************/
#ifndef UI_H
#define UI_H
#include "common/common.h"
#include "math/coordinate_space.h"
#include "math/geometry.h"
#include "ui/cfont.h"
#include "system/systems.h"
#include "ui/binding.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define GET_UI_ELEMENT(la, idx) (*(UIElement **)LArray_Get(la, idx))
#define ZERO_BOX (UIBox) { .coords = {0, 0}, .dimensions = {0, 0} }
//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef void (*UIEventHandler)(UIElement *e);

typedef enum
{
    UI_ELEMENT_NONE,
    UI_ELEMENT_ROOT,
    UI_ELEMENT_CONTAINER,
    UI_ELEMENT_TEXTBOX_O,       // Will save over previous text with whatever is typed into it
    UI_ELEMENT_TEXTBOX_IO,      // Will only save over previous text if ENTER is pressed
    UI_ELEMENT_TEXTBOX_SAFE_IO, // Will only save over previous text if ENTER is pressed
    UI_ELEMENT_TEXTFIELD,
    UI_ELEMENT_LABEL,
    UI_ELEMENT_BUTTON_SIMPLE,
    UI_ELEMENT_BUTTON_SWITCH,
    UI_ELEMENT_BUTTON_ENUMERATE,
    UI_ELEMENT_BUTTON_SUBMIT,
    UI_ELEMENT_IMAGE
} UIElementType;

// Actions that can be attached to buttons via `user_data` (pointer to int)
typedef enum
{
    BUTTON_ACTION_NONE = 0,
    BUTTON_ACTION_CREATE_ENTITY = 1,
    BUTTON_ACTION_DELETE_ENTITY = 2,
    BUTTON_ACTION_CREATE_WORLD = 3,
    BUTTON_ACTION_SELECT_WORLD_PREV = 4,
    BUTTON_ACTION_SELECT_WORLD_NEXT = 5,
} ButtonAction;
typedef enum
{
    SPACING_NONE,
    SPACING_STACKED, // Stack elements vertically in sibling order, using child height plus spacing.y. spacing.x is ignored.
    SPACING_NORMAL, // Add a per-index spacing offset on top of each child's authored/manual parent offset.
    SPACING_OVERLAYED
} SpacingType;

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

typedef enum
{
    LEFT,
    RIGHT,
    MIDDLE
} MouseBtn;

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
    Vector2d spacing;
    NumberForm spacing_mode;
    SpacingType spacing_type;
} Spacing;

typedef struct
{
    Space2d space;
} RootData;

typedef struct
{
    String64 text;
    DataType data_type;
    void *data_bind;
    Binder *binder;
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
    String64 label;
    Bitmap_Font font;
    UIEventHandler on_click;
    UIElement *slave;
    void *data_bind;
    void *user_data; // 8-byte magic pointer for ANY custom state
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
    Vector2d manual_parent_offset;
    bool has_manual_parent_offset;
    Spacing child_spacing;
    Vector2d padding;
    ColourRgba colour_border;
    ColourRgba colour_fill;
    Size size;
    UIElementType type;
    UIElementData data;
    UIBox local_box;
    UIBox screen_box;
    bool is_focused, is_dirty, is_draggable, is_enabled; // For interactive elements like TextBoxes and Buttons

    UIElement *parent;
    UIElement *first_child;
    UIElement *next_sibling;
} UIElement;

typedef struct View
{
    UIElement *container;
    ViewType type;
} View;

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
UIElement *CreateBtnUIElementInTree(UIElementType type, Size size, UIElement *parent, Offset parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill);
void GetUIElementVertices(UIElement *e, Vector2d out_vertices[4]);
bool IsMouseOverElement(UIElement *el, Vector2d mouse_pos);
bool UI_AABB_Intersects(UIBox a, UIBox b) ;
void UI_LayoutSubtree(UIElement *e, UIBox parent_box);
void UI_DistributeChildren(UIElement *e);
UIBox ResolveElementBox(UIElement *element, UIBox parent_box);
const char *GetElementTypeName(UIElementType type);
UIElement *GetLastChild(UIElement *e);
void AddElementToTree(UIElement *element, UIElement *parent);
void RemoveElementFromTree(UIElement *element);
UIElement *GetPreviousSibling(UIElement *element);
bool ElementHasSibling(UIElement *e);
UIElement *GetElementAt(UIElement *e, Vector2d pixel_coords);
void EnableElement(UIElement *element);
void DisableElement(UIElement *element);
bool IsTextbox(UIElement *e);
bool IsBtn(UIElement *e);
void ToggleElementEnabled(UIElement *element);
// UIElement *CreateTextField(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d label_tbox_offset, Vector2d label_tbox_padding, char max_label_chars, char max_text_box_chars);
#endif

