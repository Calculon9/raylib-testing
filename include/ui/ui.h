/**********************************************************************************************
*
WORLD MODULE
*
**********************************************************************************************/
#ifndef UI_H
#define UI_H
#include "common/common.h"
#include "math/cvectors.h"
#include "colour/colour.h"
#include "ui/cfont.h"
#include "system/systems.h"


//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define GET_UI_ELEMENT(la, idx) (*(UIElement **)LArray_Get(la, idx))
//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef enum
{
    UI_ELEMENT_PANEL,
    UI_ELEMENT_TEXTBOX,
    UI_ELEMENT_TEXTFIELD,
    UI_ELEMENT_LABEL,
    UI_ELEMENT_BUTTON,
    UI_ELEMENT_IMAGE
} UIElementType;

typedef struct {
    String64 text;
} TextBoxData;

typedef struct {
    String64 text;
} LabelData;

typedef struct {
    TextBoxData textbox_data;
    LabelData label_data;
} TextFieldlData;

typedef struct {
    String32 label;
    void (*on_click)(void);
} ButtonData;

typedef union
{
    TextBoxData textbox;
    LabelData label;
    TextFieldlData textfield;
    ButtonData button;
    //UIImageData image;
} UIElementData;

typedef struct UIElement UIElement;
typedef struct UIElement
{
    LArray children;
    UIElement *parent;
    Vector2d parent_offset;
    Vector2d child_spacing;
    Vector2d padding;
    Vector2d origin;
    ColourRgba colour_border;
    ColourRgba colour_fill;
    float width, height;
    UIElementType type;
    UIElementData data;
    bool is_focused; // For interactive elements like TextBoxes and Buttons
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
UIElement *CreateUIElement(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill);
UIElement *CreateUIElementUnderParent(UIElement *parent, float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill);
bool DisposeUIElement(UIElement *e);
void GetUIElementVertices(UIElement *e, Vector2d out_vertices[4]);
//UIElement *CreateTextField(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d label_tbox_offset, Vector2d label_tbox_padding, char max_label_chars, char max_text_box_chars);
#endif