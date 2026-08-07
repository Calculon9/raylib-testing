/**********************************************************************************************

 **********************************************************************************************/
#include <stdio.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "ui/ui.h"
#include "raylib.h"

static Vector2d GetContentArea(Vector2d dimensions, Vector2d padding)
{
    float content_w = fmaxf(0.0f, dimensions.x - (2.0f * padding.x));
    float content_h = fmaxf(0.0f, dimensions.y - (2.0f * padding.y));
    return (Vector2d){content_w, content_h};
}

static float ResolveOffsetToPercent(float offset_fixed, float content_extent)
{
    return content_extent > 0.0f ? (offset_fixed / content_extent) : 0.0f;
}

static float ResolveOffsetToFixed(float offset, float content_extent, OffsetMode mode)
{
    float safe_offset = fmaxf(0.0f, offset);
    if (mode == OFFSET_PERCENT)
    {
        return content_extent * safe_offset;
    }

    return safe_offset;
}

static Vector2d ResolveSpacingStep(Spacing spacing, Vector2d content_area_local, OffsetMode offset_mode)
{
    Vector2d result = spacing.spacing;

    if (spacing.spacing_mode == PERCENT && offset_mode == OFFSET_FIXED)
    {
        result.x *= content_area_local.x;
        result.y *= content_area_local.y;
    }
    else if (spacing.spacing_mode == NONE && offset_mode == OFFSET_PERCENT)
    {
        result.x = content_area_local.x > 0.0f ? result.x / content_area_local.x : 0.0f;
        result.y = content_area_local.y > 0.0f ? result.y / content_area_local.y : 0.0f;
    }

    return result; // If modes match, no conversion needed, just return the raw spacing
}

static Vector2d ResolveChildSizeFixed(const UIElement *child, Vector2d content_area_local, float consumed_fixed_y)
{
    if (!child)
    {
        return ZERO_VECTOR_2D;
    }

    if (child->size.size_mode == SIZE_PERCENT)
    {
        return (Vector2d){content_area_local.x * child->size.dimensions.x,
                          content_area_local.y * child->size.dimensions.y};
    }

    if (child->size.size_mode == SIZE_FILL)
    {
        return (Vector2d){content_area_local.x,
                          fmaxf(0.0f, content_area_local.y - consumed_fixed_y)};
    }

    return child->size.dimensions;
}

typedef struct StackedLayoutStats
{
    int fill_count;
    int child_count;
    float occupied_height;
} StackedLayoutStats;

static StackedLayoutStats CollectStackedLayoutStats(const UIElement *parent, float content_area_h)
{
    StackedLayoutStats stats = {0};
    if (!parent)
    {
        return stats;
    }

    for (UIElement *child = parent->first_child; child; child = child->next_sibling)
    {
        if (!child->is_enabled)
        {
            continue;
        }

        stats.child_count++;
        if (child->size.size_mode == SIZE_FILL)
        {
            stats.fill_count++;
        }
        else if (child->size.size_mode == SIZE_PERCENT)
        {
            stats.occupied_height += content_area_h * child->size.dimensions.y;
        }
        else
        {
            stats.occupied_height += child->size.dimensions.y;
        }
    }

    return stats;
}

static float ResolveStackedFillHeightPerChild(const UIElement *parent, float content_area_h, float spacing_y)
{
    StackedLayoutStats stats = CollectStackedLayoutStats(parent, content_area_h);
    if (stats.child_count <= 0 || stats.fill_count <= 0)
    {
        return 0.0f;
    }

    float total_spacing = spacing_y * (stats.child_count - 1);
    float remaining_space = fmaxf(0.0f, content_area_h - stats.occupied_height - total_spacing);
    return remaining_space / stats.fill_count;
}

static void DistributeChildrenNormal(UIElement *parent, Vector2d spacing_step_fixed, Vector2d spacing_step_percent)
{
    int child_count = 0;
    for (UIElement *child = parent->first_child; child; child = child->next_sibling)
    {
        Vector2d step = child->resolved_offset.offset_mode == OFFSET_PERCENT ? spacing_step_percent : spacing_step_fixed;
        Vector2d distributed = (Vector2d){step.x * child_count, step.y * child_count};

        child->resolved_offset.offset.x = child->authored_offset.offset.x + distributed.x;
        child->resolved_offset.offset.y = child->authored_offset.offset.y + distributed.y;
        child_count++;
    }
}

static void DistributeChildrenStacked(UIElement *parent, Vector2d content_area_local, Vector2d spacing_step_fixed)
{
    StackedLayoutStats stats = CollectStackedLayoutStats(parent, content_area_local.y);
    if (stats.child_count == 0)
        return;

    float fill_height_per_child = ResolveStackedFillHeightPerChild(parent, content_area_local.y, spacing_step_fixed.y);

    // PASS 2: Position elements using a single unified cursor
    float cursor_y = 0.0f;

    for (UIElement *child = parent->first_child; child; child = child->next_sibling)
    {
        if (!child->is_enabled)
        {
            continue;
        }

        // Preserve authored offset units per child when applying stacked cursor placement.
        if (child->resolved_offset.offset_mode == OFFSET_PERCENT)
        {
            float cursor_y_percent = ResolveOffsetToPercent(cursor_y, content_area_local.y);
            child->resolved_offset.offset = (Vector2d){
                child->authored_offset.offset.x,
                child->authored_offset.offset.y + cursor_y_percent};
        }
        else
        {
            child->resolved_offset.offset = (Vector2d){
                child->authored_offset.offset.x,
                child->authored_offset.offset.y + cursor_y};
        }

        // Determine height consumed by this child
        float child_h = 0.0f;
        if (child->size.size_mode == SIZE_FILL)
            child_h = fill_height_per_child;
        else if (child->size.size_mode == SIZE_PERCENT)
            child_h = content_area_local.y * child->size.dimensions.y;
        else
            child_h = child->size.dimensions.y;

        // Advance single unified cursor
        cursor_y += child_h + spacing_step_fixed.y;
    }
}

static void DistributeChildrenInline(UIElement *parent, Vector2d content_area_local, Vector2d spacing_step_fixed)
{
    float cursor_x = 0.0f;

    for (UIElement *child = parent->first_child; child; child = child->next_sibling)
    {
        if (child->resolved_offset.offset_mode == OFFSET_PERCENT)
        {
            float cursor_x_percent = ResolveOffsetToPercent(cursor_x, content_area_local.x);
            child->resolved_offset.offset = (Vector2d){
                child->authored_offset.offset.x + cursor_x_percent,
                child->authored_offset.offset.y};
        }
        else
        {
            child->resolved_offset.offset = (Vector2d){
                child->authored_offset.offset.x + cursor_x,
                child->authored_offset.offset.y};
        }

        float child_width = child->size.size_mode == SIZE_PERCENT
                                ? content_area_local.x * child->size.dimensions.x
                                : child->size.dimensions.x;
        cursor_x += child_width + spacing_step_fixed.x;
    }
}

static void DistributeChildrenInlineWrap(UIElement *parent, Vector2d content_area_local, Vector2d spacing_step_fixed)
{
    float cursor_x = 0.0f;
    float cursor_y = 0.0f;
    float row_height = 0.0f;

    for (UIElement *child = parent->first_child; child; child = child->next_sibling)
    {
        Vector2d child_size = ResolveChildSizeFixed(child, content_area_local, 0.0f);
        bool needs_wrap = cursor_x > 0.0f &&
                          cursor_x + spacing_step_fixed.x + child_size.x > content_area_local.x;

        if (needs_wrap)
        {
            cursor_x = 0.0f;
            cursor_y += row_height + spacing_step_fixed.y;
            row_height = 0.0f;
        }

        float child_x = cursor_x > 0.0f ? cursor_x + spacing_step_fixed.x : cursor_x;
        if (child->resolved_offset.offset_mode == OFFSET_PERCENT)
        {
            child->resolved_offset.offset = (Vector2d){
                child->authored_offset.offset.x + ResolveOffsetToPercent(child_x, content_area_local.x),
                child->authored_offset.offset.y + ResolveOffsetToPercent(cursor_y, content_area_local.y)};
        }
        else
        {
            child->resolved_offset.offset = (Vector2d){
                child->authored_offset.offset.x + child_x,
                child->authored_offset.offset.y + cursor_y};
        }

        cursor_x = child_x + child_size.x;
        row_height = fmaxf(row_height, child_size.y);
    }
}

static void DistributeChildrenStackedWrap(UIElement *parent, Vector2d content_area_local,
                                          Vector2d spacing_step_fixed)
{
    float cursor_x = 0.0f;
    float cursor_y = 0.0f;
    float column_width = 0.0f;

    for (UIElement *child = parent->first_child; child; child = child->next_sibling)
    {
        if (!child->is_enabled)
        {
            continue;
        }

        Vector2d child_size = ResolveChildSizeFixed(child, content_area_local, 0.0f);
        bool needs_wrap = cursor_y > 0.0f &&
                          cursor_y + spacing_step_fixed.y + child_size.y > content_area_local.y;

        if (needs_wrap)
        {
            cursor_y = 0.0f;
            cursor_x += column_width + spacing_step_fixed.x;
            column_width = 0.0f;
        }

        float child_y = cursor_y > 0.0f ? cursor_y + spacing_step_fixed.y : cursor_y;
        if (child->resolved_offset.offset_mode == OFFSET_PERCENT)
        {
            child->resolved_offset.offset = (Vector2d){
                child->authored_offset.offset.x +
                    ResolveOffsetToPercent(cursor_x, content_area_local.x),
                child->authored_offset.offset.y +
                    ResolveOffsetToPercent(child_y, content_area_local.y)};
        }
        else
        {
            child->resolved_offset.offset = (Vector2d){
                child->authored_offset.offset.x + cursor_x,
                child->authored_offset.offset.y + child_y};
        }

        cursor_y = child_y + child_size.y;
        column_width = fmaxf(column_width, child_size.x);
    }
}

static void DistributeChildrenWithContentArea(UIElement *e, Vector2d content_area_local)
{
    if (!e || !e->first_child)
    {
        return;
    }

    Spacing s = e->child_spacing;
    if (s.spacing_type == SPACING_NONE)
    {
        return;
    }

    Vector2d spacing_step_fixed = ResolveSpacingStep(s, content_area_local, OFFSET_FIXED);
    Vector2d spacing_step_percent = ResolveSpacingStep(s, content_area_local, OFFSET_PERCENT);

    if (s.spacing_type == SPACING_NORMAL)
    {
        DistributeChildrenNormal(e, spacing_step_fixed, spacing_step_percent);
    }
    else if (s.spacing_type == SPACING_STACKED)
    {
        DistributeChildrenStacked(e, content_area_local, spacing_step_fixed);
    }
    else if (s.spacing_type == SPACING_INLINE)
    {
        DistributeChildrenInline(e, content_area_local, spacing_step_fixed);
    }
    else if (s.spacing_type == SPACING_INLINE_WRAP)
    {
        DistributeChildrenInlineWrap(e, content_area_local, spacing_step_fixed);
    }
    else if (s.spacing_type == SPACING_STACKED_WRAP)
    {
        DistributeChildrenStackedWrap(e, content_area_local, spacing_step_fixed);
    }
}

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

// Pool for UIElement instances (uses memory pool API)
#include "memory/cmemory.h"
static Pool *ui_element_pool = NULL;

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------

UIElement *CreateUIElement(UIElementType type, Size size, Offset parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill)
{
    // Fast-path: allocate from a simple static pool to reduce heap churn for UI elements
    // Increased from 512 to 2048 for Phase 2 optimization (reduced heap allocation overhead)
    if (!ui_element_pool)
        ui_element_pool = PoolCreate(sizeof(UIElement), 2048);

    UIElement *e = NULL;
    if (ui_element_pool)
        e = (UIElement *)PoolAlloc(ui_element_pool);

    if (!e)
        e = AllocateBytes(sizeof(UIElement));

    e->colour_fill = colour_fill;
    e->colour_border = colour_border;
    e->size = size;
    e->text_horizontal_alignment = UI_TEXT_ALIGN_LEFT;
    e->text_vertical_alignment = UI_TEXT_VERTICAL_ALIGN_CENTRE;
    e->padding = padding;
    e->resolved_offset = parent_offset;
    e->authored_offset = parent_offset;
    e->type = type;
    e->is_enabled = true;
    // MUST explicitly set these to NULL
    e->first_child = NULL;
    e->next_sibling = NULL;
    e->parent = NULL;
    // Default values for interactive elements
    e->is_focused = false;
    e->is_dirty = true;
    e->is_draggable = false;
    e->child_spacing = (Spacing){{0, 0}, PERCENT, SPACING_NONE};

    return e;
}

UIElement *CreateBtnUIElementInTree(UIElementType type, Size size, UIElement *parent, Offset parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill)
{
    UIElement *btn = CreateUIElementInTree(type, size, parent, parent_offset, padding, colour_border, colour_fill);

    if (!btn)
        return NULL;

    btn->data.button.on_click = NULL;
    btn->data.button.slave = NULL;
    btn->data.button.data_bind = NULL;
    btn->data.button.user_data = NULL;
    btn->data.button.font = FONT_BASIC;
    btn->data.button.label.string[0] = '\0';

    return btn;
}

UIElement *CreateUIElementInTree(UIElementType type, Size size, UIElement *parent, Offset parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill)
{
    UIElement *e = CreateUIElement(type, size, parent_offset, padding, colour_border, colour_fill);

    AddElementToTree(e, parent);

    return e;
}

void AddElementToTree(UIElement *e, UIElement *parent)
{
    if (!e)
    {
        return;
    }
    e->parent = parent;
    e->next_sibling = NULL;

    // Make the provided child the youngest sibling, get the next_sibling starting at the first_child
    if (parent->first_child == NULL)
    {
        // This is the only child
        parent->first_child = e;
    }
    else
    {
        UIElement *last_sibling = GetLastChild(parent);
        if (last_sibling)
        {
            last_sibling->next_sibling = e;
        }
        else
        {
            parent->first_child = e;
            printf("ERROR adding [%s] as child to [%s]: Parent has child but last_sibling of new element not found. Likely a bug in GetLastChild.\n", GetElementTypeName(e->type), GetElementTypeName(parent->type));
        }
    }
}

void RemoveElementFromTree(UIElement *e)
{
    if (!e)
    {
        return;
    }
    UIElement *p = e->parent;
    UIElement *prev_sibling = GetPreviousSibling(p);

    // Remove element from siblings list by pointing prev_sibling to the removed element's next_sibling
    if (prev_sibling)
    {
        prev_sibling->next_sibling = e->next_sibling;
    }
    else
    {
        // The element we want to remove must've been the first_child, need to update this
        // if (next_sibling)
        {
            p->first_child = e->next_sibling;
        }
    }
    // Unlink the element from the tree completely
    e->parent = NULL;
    e->next_sibling = NULL;
    // Dispose of the element
}

UIElement *GetLastChild(UIElement *e)
{
    if (!e || !e->first_child)
    {
        return NULL;
    }

    UIElement *next_child = e->first_child;
    while (next_child->next_sibling)
    {
        next_child = next_child->next_sibling;
    }

    return next_child;
}

UIElement *GetPreviousSibling(UIElement *e)
{
    if (!e)
    {
        return NULL;
    }
    UIElement *p = e->parent;
    if (!p)
    {
        return NULL;
    }

    UIElement *prev_child = p->first_child;
    UIElement *next_child = prev_child;
    while (next_child && next_child != e)
    {
        prev_child = next_child;
        next_child = next_child->next_sibling;
    }

    if (next_child != e)
    {
        return NULL;
    }

    return prev_child;
}

bool ElementHasSibling(UIElement *e)
{
    if (!e || !e->parent)
        return false;
    return (e->next_sibling != NULL) || (e->parent->first_child != e);
}

// This needs to be called BEFORE the child's box is resolved, so that the child can use the parent's child_spacing to determine its position
// Purpose: Distribute the children of a parent container according to its configured spacing rules. This function modifies the parent_offset of each child based on the parent's child_spacing settings.
void UI_LayoutSubtree(UIElement *e, UIBox parent_box)
{
    if (!e)
        return;

    // ResolveElementBox looks UP at parent_box to figure out e's own dimensions.
    // UI_DistributeChildren looks DOWN at its children using e's own resolved content area to position them.

    // Resolve THIS element's box based on parent context
    e->local_box = ResolveElementBox(e, parent_box);

    // Now that e's box is resolved we can distribute immediate children inside its resovled content area
    if (e->first_child)
    {
        Vector2d content_area = GetContentArea(e->local_box.dimensions, e->padding);
        DistributeChildrenWithContentArea(e, content_area);
    }

    // 3. Recurse down into children
    for (UIElement *child = e->first_child; child; child = child->next_sibling)
    {
        UI_LayoutSubtree(child, e->local_box);
    }
}

// looks DOWN at its children using e's own resolved content area to position them.
// ResolveElementBox looks UP at parent_box to figure out e's own dimensions.
void UI_DistributeChildren(UIElement *e)
{
    if (!e || !e->first_child)
        return;

    Vector2d content_area = GetContentArea(e->local_box.dimensions, e->padding);
    DistributeChildrenWithContentArea(e, content_area);
}

// Call this to kick of the recursive distribution of children for a given parent element and its resolved box. This function will traverse the entire subtree of the parent element, applying the appropriate spacing rules to each child based on the parent's configuration.
//  Correct top-down recursion

UIElement *GetElementAt(UIElement *e, Vector2d pixel_coords)
{
    if (!e)
        return NULL;

    // Disabled elements and their subtrees are not interactive.
    if (!e->is_enabled)
    {
        return NULL;
    }

    // 1. If the mouse isn't even over THIS element, it can't be over its children
    if (!IsMouseOverElement(e, pixel_coords))
    {
        return NULL;
    }

    // 2. Check children in REVERSE order (last sibling is usually drawn on top)
    // For simplicity here, we'll go first-to-last, but the top-most child wins
    UIElement *found = NULL;
    UIElement *child = e->first_child;
    while (child)
    {
        UIElement *clicked = GetElementAt(child, pixel_coords);
        if (clicked)
        {
            found = clicked; // Keep track of the most recent (top-most) match
        }
        child = child->next_sibling;
    }

    // 3. If a child was clicked, return that. Otherwise, it's this element.
    return (found) ? found : e;
}

void DisableElement(UIElement *element)
{
    element->is_enabled = false;
}

void EnableElement(UIElement *element)
{
    element->is_enabled = true;
}

void ToggleElementEnabled(UIElement *element)
{
    element->is_enabled = !element->is_enabled;
}

void SetUIElementTextHorizontalAlignment(UIElement *element, UITextHorizontalAlignment alignment)
{
    if (!element)
    {
        return;
    }

    element->text_horizontal_alignment = alignment;
}

void SetUIElementTextVerticalAlignment(UIElement *element, UITextVerticalAlignment alignment)
{
    if (!element)
    {
        return;
    }

    element->text_vertical_alignment = alignment;
}

bool IsTextbox(UIElement *e)
{
    if (!e)
        return false;
    return (e->type == UI_ELEMENT_TEXTBOX_O ||
            e->type == UI_ELEMENT_TEXTBOX_SAFE_IO ||
            e->type == UI_ELEMENT_TEXTBOX_IO);
}

bool IsBtn(UIElement *e)
{
    if (!e)
        return false;
    return (e->type == UI_ELEMENT_BUTTON_SIMPLE ||
            e->type == UI_ELEMENT_BUTTON_SWITCH ||
            e->type == UI_ELEMENT_BUTTON_ENUMERATE ||
            e->type == UI_ELEMENT_BUTTON_SUBMIT);
}

// Disposes Element - assumes it has already been removed from its UI Tree if in one
void DisposeUIElement(UIElement *e)
{
    if (!e)
        return;

    // 1. Recursively destroy the first child (and all its siblings)
    // This goes "deep" before it stays "wide"
    if (e->first_child)
    {
        DisposeUIElement(e->first_child);
    }

    // 2. Recursively destroy the next sibling
    // This ensures every element in the horizontal list is freed
    if (e->next_sibling)
    {
        DisposeUIElement(e->next_sibling);
    }

    // 3. Cleanup element-specific resources (e.g., attached binders)
    if (IsTextbox(e) && e->data.textbox.binder)
    {
        Binder_Destroy(e->data.textbox.binder);
        e->data.textbox.binder = NULL;
    }

    // 4. Finally, free the current element
    // Now that children and siblings are gone, it's safe to delete this one
    // If the element points into our static pool, return it to the free list; otherwise free the heap allocation.
    if (ui_element_pool && PoolOwns(ui_element_pool, e))
    {
        // reset minimal state and return to pool
        e->first_child = NULL;
        e->next_sibling = NULL;
        e->parent = NULL;
        e->is_focused = false;
        e->is_dirty = false;
        e->is_draggable = false;
        e->is_enabled = false;
        PoolFree(ui_element_pool, e);
    }
    else
    {
        Deallocate((void **)&e, sizeof(UIElement));
    }
}

void GetUIElementVertices(UIElement *e, Vector2d out_vertices[4])
{
    out_vertices[0] = e->screen_box.coords;
    out_vertices[1] = (Vector2d){e->screen_box.coords.x + e->screen_box.dimensions.x, e->screen_box.coords.y};
    out_vertices[2] = (Vector2d){e->screen_box.coords.x + e->screen_box.dimensions.x, e->screen_box.coords.y + e->screen_box.dimensions.y};
    out_vertices[3] = (Vector2d){e->screen_box.coords.x, e->screen_box.coords.y + e->screen_box.dimensions.y};
}

bool IsMouseOverElement(UIElement *e, Vector2d mouse_pos)
{
    return (mouse_pos.x >= e->screen_box.coords.x &&
            mouse_pos.x <= e->screen_box.coords.x + e->screen_box.dimensions.x &&
            mouse_pos.y >= e->screen_box.coords.y &&
            mouse_pos.y <= e->screen_box.coords.y + e->screen_box.dimensions.y);
}

// looks UP at parent_box to figure out e's own dimensions.
UIBox ResolveElementBox(UIElement *element, UIBox parent_box)
{
    if (!element)
        return ZERO_BOX;

    UIBox box;
    box.coords = parent_box.coords; // Start at parent origin
    float p_mid_x = parent_box.dimensions.x / 2;
    float p_mid_y = parent_box.dimensions.y / 2;

    // Calculate the available content area inside the parent
    float content_area_w = parent_box.dimensions.x;
    float content_area_h = parent_box.dimensions.y;

    // Apply any padding to correct the available area
    if (element->parent)
    {
        // Account for padding
        float pad_x = element->parent->padding.x;
        float pad_y = element->parent->padding.y;

        box.coords.x += pad_x;
        box.coords.y += pad_y;

        content_area_w -= (pad_x * 2.0f);
        content_area_h -= (pad_y * 2.0f);
    }

    // Resolve the local Offset (Relative to the content area)
    float adj_offset_x = ResolveOffsetToFixed(element->resolved_offset.offset.x, content_area_w, element->resolved_offset.offset_mode);
    float adj_offset_y = ResolveOffsetToFixed(element->resolved_offset.offset.y, content_area_h, element->resolved_offset.offset_mode);

    // Apply the offset to the final coordinates
    box.coords.x += adj_offset_x;
    box.coords.y += adj_offset_y;

    // Resolve Dimensions
    if (element->size.size_mode == SIZE_PERCENT)
    {
        box.dimensions.x = element->size.dimensions.x * content_area_w;
        box.dimensions.y = element->size.dimensions.y * content_area_h;
    }
    else
    {
        box.dimensions.x = element->size.dimensions.x; // * basis_tfrm.x;
        box.dimensions.y = element->size.dimensions.y; // * basis_tfrm.y;
    }

    // Resolve Size with Right/Bottom clamping
    // The "Space Left" is the content area minus how far we shifted in
    float remaining_w = fmaxf(0.0f, content_area_w - adj_offset_x);
    float remaining_h = fmaxf(0.0f, content_area_h - adj_offset_y);

    if (element->size.size_mode == SIZE_FILL)
    {
        box.dimensions.x = remaining_w;
        box.dimensions.y = remaining_h;

        if (element->parent && element->parent->child_spacing.spacing_type == SPACING_STACKED)
        {
            Vector2d spacing_step = ResolveSpacingStep(
                element->parent->child_spacing,
                (Vector2d){content_area_w, content_area_h}, OFFSET_FIXED);

            float fill_height_per_child = ResolveStackedFillHeightPerChild(element->parent, content_area_h, spacing_step.y);
            if (fill_height_per_child > 0.0f)
            {
                box.dimensions.y = fill_height_per_child;
            }
        }
    }
    else
    {
        box.dimensions.x = fminf(box.dimensions.x, remaining_w);
        box.dimensions.y = fminf(box.dimensions.y, remaining_h);
    }

    return box;
}

bool UI_AABB_Intersects(UIBox a, UIBox b) 
{
    return (a.coords.x < b.coords.x + b.dimensions.x &&
            a.coords.x + a.dimensions.x > b.coords.x &&
            a.coords.y < b.coords.y + b.dimensions.y &&
            a.coords.y + a.dimensions.y > b.coords.y);
}

const char *GetElementTypeName(UIElementType type)
{
    switch (type)
    {
    case UI_ELEMENT_ROOT:
        return "ROOT";
    case UI_ELEMENT_TEXTFIELD:
        return "TEXTFIELD";
    case UI_ELEMENT_CONTAINER:
        return "CONTAINER";
    case UI_ELEMENT_LABEL:
        return "LABEL";
    case UI_ELEMENT_TEXTBOX_O:
        return "TEXTBOX_O";
    case UI_ELEMENT_TEXTBOX_IO:
        return "TEXTBOX_IO";
    case UI_ELEMENT_TEXTBOX_SAFE_IO:
        return "TEXTBOX_SAFE_IO";
    case UI_ELEMENT_BUTTON_SWITCH:
        return "BUTTON_SWITCH";
    case UI_ELEMENT_BUTTON_SIMPLE:
        return "BUTTON_SIMPLE";
    case UI_ELEMENT_BUTTON_ENUMERATE:
        return "BUTTON_ENUMERATE";
    case UI_ELEMENT_BUTTON_SUBMIT:
        return "BUTTON_SUBMIT";
    case UI_ELEMENT_IMAGE:
        return "IMAGE";
    default:
        return "UNKNOWN_TYPE";
    }
}

// // This needs to be called after ResolveElementDimensions because the resolved dimensions are required to determine mid-points which are used here
// Offset ResolveElementOffset(UIElement *element, Vector2d available_area, Vector2d basis_scale)
// {
//     // 1. Calculate the available content area inside the parent
//     Offset p_offset = {0};
//     float offset_x = fmaxf(0.0f, element->parent_offset.offset.x);
//     float offset_y = fmaxf(0.0f, element->parent_offset.offset.y);
//     float pixel_offset_x, pixel_offset_y;

//     float p_mid_x = available_area.x / 2;
//     float p_mid_y = available_area.y / 2;

//     if (element->parent_offset.offset_mode == ALIGNED_CENTRE)
//     {
//         // This means the mid-point of the element needs to equal mid-point of parent
//         pixel_offset_x;
//     }
//     else if (element->parent_offset.offset_mode == OFFSET_PERCENT)
//     {
//         pixel_offset_x = available_area * offset_x;
//         pixel_offset_y = content_area_h * offset_y;
//     }
//     else if (element->parent_offset.offset_mode == OFFSET_FIXED)
//     {
//         if (element->parent)
//         {
//             // Account for padding
//             float pad_x = element->parent->padding.x * basis_scale.x;
//             float pad_y = element->parent->padding.y * basis_scale.y;

//             p_offset.offset.x += pad_x;
//             p_offset.offset.y += pad_y;

//             content_area_w -= (pad_x * 2.0f);
//             content_area_h -= (pad_y * 2.0f);
//         }
//     }
// }

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

// bool DisposeUITree(UIElement *e)
// {
//     // Remove from parent's children array
//     if (e->parent != NULL)
//     {
//         LArray *siblings = &e->parent->children;
//         for (size_t i = 0; i < siblings->count; i++)
//         {
//             if (((UIElement **)siblings->items)[i] == e)
//             {
//                 LArray_RemoveAt(siblings, i);
//                 break;
//             }
//         }
//     }

//     // Free the element's own resources
//     // Note: If the element has its own children, you may want to recursively destroy them here as well. We will.
//     for (size_t i = 0; i < e->children.count; i++)
//     {
//         DisposeUIElement(((UIElement **)e->children.items)[i]);
//     }
//     free(e);
//     return true;
// }