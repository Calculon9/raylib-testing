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

#define MAX_TEXTBOX_CHARS (int)sizeof(String64)
#define MAX_LABEL_CHARS (int)sizeof(String64)

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define GET_UI_ELEMENT(la, idx) (*(UIElement **)LArray_Get(la, idx))
#define ZERO_BOX (UIBox) { .coords = {0, 0}, .dimensions = {0, 0} }
//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef void (*UIEventHandler)(UIElement *e);
// Per-element mouse-wheel handler. Mirrors UIEventHandler but carries the wheel delta
// so the handler can choose a direction (delta>0 forward, delta<0 backward). Dispatched
// by presence from ProcessUIInput, exactly like on_click/on_hover.
typedef void (*UIScrollHandler)(UIElement *e, float delta);

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
    UI_ELEMENT_BUTTON_ENUMERATE,
    UI_ELEMENT_BUTTON_SUBMIT,
    UI_ELEMENT_IMAGE,
    UI_ELEMENT_VIEW
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
    FLOW_NONE,
    FLOW_STACKED, // Stack elements vertically in sibling order, using child height plus gap.y. gap.x is ignored.
    FLOW_STACKED_WRAP, // Arrange elements vertically and wrap to the next column when content height is exceeded.
    FLOW_STACKED_SMART_WRAP, // Arrange elements vertically and wrap to the next column when content height is exceeded.
    FLOW_INLINE, // Arrange elements horizontally in sibling order, using child width plus gap.x.
    FLOW_INLINE_WRAP, // Arrange elements horizontally and wrap to the next row when content width is exceeded.
    FLOW_NORMAL, // Add a per-index gap offset on top of each child's authored/manual parent offset.
    FLOW_OVERLAYED
} FlowType;

// Per-axis sizing mode. Each axis of an element resolves independently via one of these.
typedef enum
{
    AXIS_FIXED,   // Authored literal from Size.dimensions on this axis (world units).
    AXIS_PERCENT, // Size.dimensions on this axis is a 0..1 fraction of the parent content extent.
    AXIS_FILL,    // Take all remaining parent content extent on this axis (dimensions ignored).
    AXIS_HUG      // Shrink to fit children + padding on this axis. If Size.dimensions on this
                  // axis > 0 it is a MAX cap; if 0/absent the hug is uncapped.
} AxisSizeMode;

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

typedef enum
{
    UI_TEXT_ALIGN_LEFT,
    UI_TEXT_ALIGN_CENTRE,
    UI_TEXT_ALIGN_RIGHT
} UITextHorizontalAlignment;

typedef enum
{
    UI_TEXT_VERTICAL_ALIGN_TOP,
    UI_TEXT_VERTICAL_ALIGN_CENTRE,
    UI_TEXT_VERTICAL_ALIGN_BOTTOM
} UITextVerticalAlignment;

typedef struct
{
    Vector2d offset;
    OffsetMode offset_mode;
} Offset;

// Element size: one authored dimension pair plus an independent mode per axis.
typedef struct
{
    Vector2d dimensions;   // Per-axis authored value; meaning depends on that axis's mode.
    AxisSizeMode width_mode;
    AxisSizeMode height_mode;
} Size;

// Convenience constructors. The hug-cap variant encodes the cap in dimensions.
#define UI_SIZE_FIXED(w, h)   ((Size){{(w), (h)}, AXIS_FIXED,   AXIS_FIXED})
#define UI_SIZE_PERCENT(w, h) ((Size){{(w), (h)}, AXIS_PERCENT, AXIS_PERCENT})
#define UI_SIZE_FILL          ((Size){{0.0f, 0.0f}, AXIS_FILL,  AXIS_FILL})
#define UI_SIZE_HUG           ((Size){{0.0f, 0.0f}, AXIS_HUG,   AXIS_HUG})   // old SIZE_CONTENT (uncapped both axes)
// Documents the capped-hug authoring form (old SIZE_CONTENT_MAX): dimensions is the per-axis cap.
#define UI_SIZE_HUG_CAPPED(w, h) ((Size){{(w), (h)}, AXIS_HUG,  AXIS_HUG})
// Mixed hug/fill. The suffix names the HUGGED axis; the other axis fills the parent.
#define UI_SIZE_HUG_WIDTH     ((Size){{0.0f, 0.0f}, AXIS_HUG,   AXIS_FILL})  // old SIZE_HUG_WIDTH
#define UI_SIZE_HUG_HEIGHT    ((Size){{0.0f, 0.0f}, AXIS_FILL,  AXIS_HUG})   // old SIZE_HUG_HEIGHT / SIZE_CONTENT_FILL

typedef struct
{
    Vector2d gap;
    NumberForm gap_mode;
    FlowType flow;
} Layout;

#define UI_SPACING_NONE ((Layout){{0.0f, 0.0f}, NONE, FLOW_NONE})

typedef struct
{
    UISpace2d space;
} RootData;

typedef struct
{
    String64 text;
    DataType data_type;
    void *data_bind;   // retained: legacy path still uses it
    Binder *binder;    // retained: legacy path still uses it
    Binding *binding;  // single bidirectional carrier (source=read, sink=write); coexists with legacy data_bind/binder
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
    String64 text;         // authored base text (from the XML "text" attribute / constructor arg)
    String64 display_text; // text actually drawn; equals `text` unless a binding derives it (e.g. "<text>: ON")
    Bitmap_Font font;
    void *data_bind;   // retained: button-driven view-selector click reads this (see §8)
    Binding *binding; // optional heap binding: command-sink (XML button actions) or query-source display; NULL otherwise
} ButtonData;

typedef union
{
    RootData root;
    TextBoxData textbox;
    LabelData label;
    ButtonData button;
    // UIImageData image;
} UIElementData;

typedef struct
{
    // int coords_x, coords_y, width, height;
    Vector2d coords;
    Vector2d dimensions;
} UIBox;

// Pixel-space clipping rectangle defined by minimum and maximum extents.
typedef struct UIClipRect
{
    Vector2d min;
    Vector2d max;
} UIClipRect;

#define UI_UNCONSTRAINED_CLIP ((UIClipRect){{-1000000.0f, -1000000.0f}, {1000000.0f, 1000000.0f}})

typedef struct UIElement UIElement;
typedef struct UIElement
{
    // LArray children;
    Offset resolved_offset;
    Offset authored_offset;
    Layout child_spacing;
    Vector2d padding;
    Vector2d scroll_offset; // Scroll offset in local units applied to child placement
    ColourRgba colour_border;
    ColourRgba colour_fill;
    Size size;
    UITextHorizontalAlignment text_horizontal_alignment;
    UITextVerticalAlignment text_vertical_alignment;
    UIElementType type;
    UIElementData data;
    UIBox local_box;
    UIBox screen_box;
    Vector2d measured_content_size;
    float cached_stacked_fill_height; // Set once per layout pass by the stacked parent; read by this element's own box resolution.
    bool is_focused, is_dirty, is_draggable, is_enabled; // For interactive elements like TextBoxes and Buttons
    bool is_scrollable_x; // Bypasses horizontal size clamping when scrolling
    bool is_scrollable_y; // Bypasses vertical size clamping when scrolling
    bool allow_overflow;  // When true, this element may extend beyond its parent's content box:
                          // its box dimensions are NOT clamped to the parent's remaining space
                          // (both axes). Used for fan-out submenus, tooltips, dropdowns. Scroll
                          // clipping (is_scrollable_*) still applies. Defaults to false.

    // Interaction handlers live on the BASE element so ANY element can be clickable
    // or hoverable; dispatch is by handler-presence, not element type (see ui_input.c).
    UIEventHandler on_click;            // fired on click for any element with a non-NULL handler
    UIEventHandler on_hover;            // fired while hovered for any enabled element with a non-NULL handler
    UIEventHandler on_draw;             // when non-NULL, the renderer calls it to draw custom per-element
                                        // content on top of the element's box; presence-dispatched, reads
                                        // e->screen_box for geometry. No new type or data union change.
    UIScrollHandler on_scroll;          // fired on mouse wheel while this element (or its scroll
                                        // target) is hovered; NULL => fall through to scroll panels
    void *interaction_user_data;        // unified opaque per-element state pointer for click/hover handlers (buttons + former hover items)
    void *interaction_data_bind;        // base data_bind, used by the hover-view-selector LABEL only (see §8)

    UIElement *parent;
    UIElement *first_child;
    UIElement *next_sibling;

    // id= from markup, empty when absent; used by UILoader_FindById for a generic,
    // domain-free lookup. Byte-identical to MAX_UI_ELEMENT_ID (declared via the
    // String64 size so ui.h need not include ui_loader.h, which would be circular).
    char id[(int)sizeof(String64)];
} UIElement;

// Traverse a UI element's children in sibling order.
#define ForEachChild(parent, child) \
    for (UIElement *(child) = (parent) ? (parent)->first_child : NULL; \
         (child) != NULL; \
         (child) = (child)->next_sibling)

// Returns true if the element is non-null and enabled.
static inline bool UIElement_IsEnabled(const UIElement *e)
{
    return e != NULL && e->is_enabled;
}

// Returns true if the element is non-null, enabled, and has at least one child.
static inline bool UIElement_HasEnabledChildren(const UIElement *e)
{
    return e != NULL && e->is_enabled && e->first_child != NULL;
}

typedef struct View
{
    UIElement *container;
    ViewType type;
    float scroll_x;       // Current horizontal scroll offset in local coordinate units
    float max_scroll_x;   // Maximum horizontal scroll offset
    float content_width;  // Total measured width of children inside this view
    float scroll_y;       // Current vertical scroll offset in local coordinate units
    float max_scroll_y;   // Maximum vertical scroll offset (content_height - visible_height)
    float content_height; // Total measured height of children inside this view
    bool is_scrollable_x; // Whether horizontal scrolling is active on this view
    bool is_scrollable_y; // Whether vertical scrolling is active on this view
} View;

typedef struct Pool Pool;

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
bool UI_AABB_Intersects(UIBox a, UIBox b);

// Clip rectangle helpers for software clipping
UIClipRect UIClipRect_FromBox(UIBox box);
UIClipRect UIClipRect_Intersect(UIClipRect a, UIClipRect b);
bool UIClipRect_IsEmpty(UIClipRect rect);
bool UIClipRect_ContainsBox(UIClipRect clip, UIBox box);
bool UIClipRect_IntersectsBox(UIClipRect clip, UIBox box);
UIBox UIBox_Clip(UIBox box, UIClipRect clip);

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
UIElement *GetElementAtClipped(UIElement *e, Vector2d pixel_coords, UIClipRect clip);
void DisposeUIElement(UIElement *e);
void EnableElement(UIElement *element);
void DisableElement(UIElement *element);
void DisableParent(UIElement *child);
void SetEnabledState(UIElement *element, bool is_enabled);
void SetParentEnabledState(UIElement *child, bool is_enabled);
Pool *GetUIElementPool(void);
void SetUIElementPool(Pool *pool);
void SetUIElementTextHorizontalAlignment(UIElement *element, UITextHorizontalAlignment alignment);
void SetUIElementTextVerticalAlignment(UIElement *element, UITextVerticalAlignment alignment);
bool IsTextbox(UIElement *e);
bool IsEditableTextbox(const UIElement *e);
bool IsBtn(UIElement *e);
void ToggleElementEnabled(UIElement *element);

// Refresh a single element's display from its attached binding's source (data -> UI).
// No-op unless the element is a button with a binding that has a readable source. For a query
// source it composes the drawn text as "<authored text>: ON/OFF" into display_text, leaving the
// authored `text` base intact. Pure pull: reads current truth, so it is safe to call every
// frame and never goes stale. A textbox carrying a readable binding source refreshes via this
// pull too (skipped while focused so in-progress typing is never overwritten); textboxes without
// a binding stay on the RefreshTextboxFields path.
void UIElement_RefreshBinding(UIElement *e);
// Scroll a horizontally scrollable element by local coordinate units.
void ScrollUIElementX(UIElement *element, float delta);
// Scroll a vertically scrollable element by local coordinate units.
void ScrollUIElementY(UIElement *element, float delta);
// UIElement *CreateTextField(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d label_tbox_offset, Vector2d label_tbox_padding, char max_label_chars, char max_text_box_chars);
#endif

