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

static Vector2d ResolveSpacingStep(Spacing spacing, Vector2d content_area_local, OffsetMode offset_mode)
{
    if (spacing.spacing_mode == PERCENT)
    {
        return (offset_mode == OFFSET_PERCENT)
                   ? spacing.spacing
                   : (Vector2d){content_area_local.x * spacing.spacing.x,
                                content_area_local.y * spacing.spacing.y};
    }

    return (offset_mode == OFFSET_PERCENT)
               ? (Vector2d){content_area_local.x > 0.0f ? spacing.spacing.x / content_area_local.x : 0.0f,
                            content_area_local.y > 0.0f ? spacing.spacing.y / content_area_local.y : 0.0f}
               : spacing.spacing;
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

static void DistributeChildrenNormal(UIElement *parent, Vector2d spacing_step_fixed, Vector2d spacing_step_percent)
{
    int child_count = 0;
    for (UIElement *child = parent->first_child; child; child = child->next_sibling)
    {
        Vector2d step = child->parent_offset.offset_mode == OFFSET_PERCENT ? spacing_step_percent : spacing_step_fixed;
        Vector2d distributed = (Vector2d){step.x * child_count, step.y * child_count};

        child->parent_offset.offset.x = child->manual_parent_offset.x + distributed.x;
        child->parent_offset.offset.y = child->manual_parent_offset.y + distributed.y;
        child_count++;
    }
}

static void DistributeChildrenStacked(UIElement *parent, Vector2d content_area_local, Vector2d spacing_step_fixed, Vector2d spacing_step_percent)
{
    float cursor_fixed_y = 0.0f;
    float cursor_percent_y = 0.0f;

    for (UIElement *child = parent->first_child; child; child = child->next_sibling)
    {
        Vector2d child_size_fixed = ResolveChildSizeFixed(child, content_area_local, cursor_fixed_y);
        float child_size_percent_y = content_area_local.y > 0.0f ? child_size_fixed.y / content_area_local.y : 0.0f;

        if (child->parent_offset.offset_mode == OFFSET_PERCENT)
        {
            child->parent_offset.offset = (Vector2d){child->manual_parent_offset.x, child->manual_parent_offset.y + cursor_percent_y};
        }
        else
        {
            child->parent_offset.offset = (Vector2d){child->manual_parent_offset.x, child->manual_parent_offset.y + cursor_fixed_y};
        }

        cursor_fixed_y += child_size_fixed.y + spacing_step_fixed.y;
        cursor_percent_y += child_size_percent_y + spacing_step_percent.y;
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
        DistributeChildrenStacked(e, content_area_local, spacing_step_fixed, spacing_step_percent);
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

// // Takes in pixel coord point and determines if they are in the target region
// bool IsFocused(Vector2d pixel_coords, Vector2d *vertices, int vertex_count)
// {
//     // Vector2d *vertices = polygon->vertices.coll.items;
//     return IsPointInPolygon(pixel_coords, vertices, vertex_count);
// }

UIElement *CreateUIElement(UIElementType type, Size size, Offset parent_offset, Vector2d padding, ColourRgba colour_border, ColourRgba colour_fill)
{
    // Fast-path: allocate from a simple static pool to reduce heap churn for UI elements
    if (!ui_element_pool)
    {
        ui_element_pool = PoolCreate(sizeof(UIElement), 512);
    }

    UIElement *e = NULL;
    if (ui_element_pool)
    {
        e = (UIElement *)PoolAlloc(ui_element_pool);
    }
    if (!e)
    {
        e = AllocateBytes(sizeof(UIElement));
    }
    // e->origin = origin_coords;
    e->colour_fill = colour_fill;
    e->colour_border = colour_border;
    e->size = size;
    e->padding = padding;
    e->parent_offset = parent_offset;
    e->manual_parent_offset = parent_offset.offset;
    e->has_manual_parent_offset = true;
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

    btn->is_enabled = true;
    btn->is_draggable = false;
    btn->is_focused = false;
    btn->is_dirty = true;

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
    // e->children = *NewLArray(2, sizeof(UIElement *)); // Initialise the children array for this element

    // Get the parent Element and add this as a child
    // bool success = LArray_Push(&parent->children, &e);

    // printf("Error: Failed to add UIElement to parent's children array.\n");
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
    UIElement *parent = e->parent;
    UIElement *p = e->parent;

    UIElement *next_sibling = e->next_sibling;
    UIElement *prev_sibling = GetPreviousSibling(parent);

    // Remove element from siblings list by pointing prev_sibling to the removed element's next_sibling
    if (prev_sibling)
    {
        prev_sibling->next_sibling = next_sibling;
    }
    else
    {
        // The element we want to remove must've been the first_child, need to update this
        if (next_sibling)
        {
            p->first_child = next_sibling;
        }
    }

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
    if (!e)
    {
        return NULL;
    }
    if (e->next_sibling != NULL)
        return true;

    UIElement *p = e->parent;
    if (!p)
    {
        return false;
    }
    if (p->first_child != NULL && p->first_child != e)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// This needs to be called BEFORE the child's box is resolved, so that the child can use the parent's child_spacing to determine its position
// Purpose: Distribute the children of a parent container according to its configured spacing rules. This function modifies the parent_offset of each child based on the parent's child_spacing settings.
void DistributeChildren(UIElement *e)
{
    if (!e || !e->first_child)
    {
        return;
    }

    Vector2d content_area_local = GetContentArea(e->local_box.dimensions, e->padding);
    DistributeChildrenWithContentArea(e, content_area_local);
}

void DistributeChildrenRecursive(UIElement *e)
{
    if (!e)
    {
        return;
    }

    // Apply spacing at this level when configured.
    DistributeChildren(e);

    // Always recurse so nested containers can still distribute their own children.
    for (UIElement *child = e->first_child; child; child = child->next_sibling)
    {
        DistributeChildrenRecursive(child);
    }
}

void DistributeChildrenResolved(UIElement *e, UIBox parent_box)
{
    if (!e || !e->first_child)
    {
        return;
    }

    Vector2d content_area_local = GetContentArea(parent_box.dimensions, e->padding);
    DistributeChildrenWithContentArea(e, content_area_local);
}

//Call this to kick of the recursive distribution of children for a given parent element and its resolved box. This function will traverse the entire subtree of the parent element, applying the appropriate spacing rules to each child based on the parent's configuration.
void DistributeChildrenRecursiveResolved(UIElement *e, UIBox parent_box)
{
    if (!e)
    {
        return;
    }

    DistributeChildrenResolved(e, parent_box);
    e->local_box = ResolveElementBox(e, parent_box);

    for (UIElement *child = e->first_child; child; child = child->next_sibling)
    {
        //child->local_box = ResolveElementBox(child, e->local_box);
        //UIBox child_box = ResolveElementBox(child, e->local_box);
        DistributeChildrenRecursiveResolved(child, e->local_box);
    }
}

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

bool IsTextbox(UIElement *e)
{
    if (!e)
        return false;

    if (e->type == UI_ELEMENT_TEXTBOX_O || e->type == UI_ELEMENT_TEXTBOX_SAFE_IO || e->type == UI_ELEMENT_TEXTBOX_IO)
    {
        return true;
    }
    return false;
}

bool IsBtn(UIElement *e)
{
    if (!e)
        return false;

    if (e->type == UI_ELEMENT_BUTTON_SIMPLE || e->type == UI_ELEMENT_BUTTON_SWITCH || e->type == UI_ELEMENT_BUTTON_ENUMERATE || e->type == UI_ELEMENT_BUTTON_SUBMIT)
    {
        return true;
    }
    return false;
}

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

// bool DisposeUIElement(UIElement *e)
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

void GetUIElementVertices(UIElement *e, Vector2d out_vertices[4])
{
    out_vertices[0] = e->screen_box.coords;
    out_vertices[1] = (Vector2d){e->screen_box.coords.x + e->screen_box.dimensions.x, e->screen_box.coords.y};
    out_vertices[2] = (Vector2d){e->screen_box.coords.x + e->screen_box.dimensions.x, e->screen_box.coords.y + e->screen_box.dimensions.y};
    out_vertices[3] = (Vector2d){e->screen_box.coords.x, e->screen_box.coords.y + e->screen_box.dimensions.y};
    // out_vertices[0] = e->origin;
    // out_vertices[1] = (Vector2d){e->origin.x + e->width, e->origin.y};
    // out_vertices[2] = (Vector2d){e->origin.x + e->width, e->origin.y + e->height};
    // out_vertices[3] = (Vector2d){e->origin.x, e->origin.y + e->height};
}

bool IsMouseOverElement(UIElement *e, Vector2d mouse_pos)
{
    return (mouse_pos.x >= e->screen_box.coords.x &&
            mouse_pos.x <= e->screen_box.coords.x + e->screen_box.dimensions.x &&
            mouse_pos.y >= e->screen_box.coords.y &&
            mouse_pos.y <= e->screen_box.coords.y + e->screen_box.dimensions.y);
}

// Calculates the final screen-space pixel coordinates for an element
// Vector2d ResolveElementPosition(UIElement *element, UIBox parent_box, Vector2d basis_scale)
// {
//     if (!element)
//         return ZERO_VECTOR_2D;

//     // 1. Start with the Parent's top-left anchor (already in pixels)
//     Vector2d resolved = parent_box.coords;

//     // 2. Add Parent's Padding (if the parent exists)
//     // Note: We scale the padding by the basis so it shrinks/grows with zoom
//     if (element->parent)
//     {
//         resolved.x += element->parent->padding.x * basis_scale.x;
//         resolved.y += element->parent->padding.y * basis_scale.y;
//     }

//     // 3. Add the Child's specific Local Offset
//     resolved.x += element->parent_offset.x * basis_scale.x;
//     resolved.y += element->parent_offset.y * basis_scale.y;

//     resolved.x = floorf(resolved.x);
//     resolved.y = floorf(resolved.y);

//     return resolved;
// }

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
    float safe_offset_x = fmaxf(0.0f, element->parent_offset.offset.x);
    float safe_offset_y = fmaxf(0.0f, element->parent_offset.offset.y);
    float adj_offset_x, adj_offset_y;

    // if (element->parent_offset.offset_mode == ALIGNED_CENTRE)
    // {
    //     // ALIGNED_CENTRE will overide the set Offset value for the element
    //     // Determine the dimensions, then use them to calculate the offset

    // }
    if (element->parent_offset.offset_mode == OFFSET_PERCENT)
    {
        adj_offset_x = content_area_w * safe_offset_x;
        adj_offset_y = content_area_h * safe_offset_y;
    }
    else
    {
        adj_offset_x = safe_offset_x; // * basis_tfrm.x;
        adj_offset_y = safe_offset_y; // * basis_tfrm.y;
    }

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

    box.dimensions.x = fminf(box.dimensions.x, remaining_w);
    box.dimensions.y = fminf(box.dimensions.y, remaining_h);

    // Pixel Snapping
    // box.coords.x = floorf(box.coords.x);
    // box.coords.y = floorf(box.coords.y);
    // box.dimensions.x = floorf(box.dimensions.x);
    // box.dimensions.y = floorf(box.dimensions.y);

    return box;
}

// WE NEED TO BE PASSING IN THE PARENT'S BOX BECAUSE WE NEED TO KNOW THE ADJUSTED SPACE WE HAVE TO RENDER IN (I.E., THE PARENT'S DIMENSIONS MINUS PADDING) TO BE ABLE TO CLAMP THE CHILD ELEMENT'S SIZE AND PREVENT IT FROM LEAKING OUT OF THE PARENT
// The basis scale needs to be able to convert to pixels, so we can apply the parent's padding and the child's offset in pixels
// UIBox ResolveElementBox(UIElement *element, UIBox parent_box, Vector2d basis_tfrm)
// {
//     if (!element)
//         return ZERO_BOX;

//     UIBox box;
//     box.coords = parent_box.coords; // Start at parent origin
//     float p_mid_x = parent_box.dimensions.x / 2;
//     float p_mid_y = parent_box.dimensions.y / 2;

//     // Calculate the available content area inside the parent
//     float content_area_w = parent_box.dimensions.x;
//     float content_area_h = parent_box.dimensions.y;

//     // Apply any padding to correct the available area
//     if (element->parent)
//     {
//         // Account for padding
//         float pad_x = element->parent->padding.x;
//         float pad_y = element->parent->padding.y;

//         box.coords.x += pad_x;
//         box.coords.y += pad_y;

//         content_area_w -= (pad_x * 2.0f);
//         content_area_h -= (pad_y * 2.0f);
//     }

//     // Resolve the local Offset (Relative to the content area)
//     float safe_offset_x = fmaxf(0.0f, element->parent_offset.offset.x);
//     float safe_offset_y = fmaxf(0.0f, element->parent_offset.offset.y);
//     float adj_offset_x, adj_offset_y;

//     // if (element->parent_offset.offset_mode == ALIGNED_CENTRE)
//     // {
//     //     // ALIGNED_CENTRE will overide the set Offset value for the element
//     //     // Determine the dimensions, then use them to calculate the offset

//     // }
//     if (element->parent_offset.offset_mode == OFFSET_PERCENT)
//     {
//         adj_offset_x = content_area_w * safe_offset_x;
//         adj_offset_y = content_area_h * safe_offset_y;
//     }
//     else
//     {
//         adj_offset_x = safe_offset_x * basis_tfrm.x;
//         adj_offset_y = safe_offset_y * basis_tfrm.y;
//     }

//     // Apply the offset to the final coordinates
//     box.coords.x += adj_offset_x;
//     box.coords.y += adj_offset_y;

//     // Resolve Dimensions
//     if (element->size.size_mode == SIZE_PERCENT)
//     {
//         box.dimensions.x = element->size.dimensions.x * content_area_w;
//         box.dimensions.y = element->size.dimensions.y * content_area_h;
//     }
//     else
//     {
//         box.dimensions.x = element->size.dimensions.x * basis_tfrm.x;
//         box.dimensions.y = element->size.dimensions.y * basis_tfrm.y;
//     }

//     // Resolve Size with Right/Bottom clamping
//     // The "Space Left" is the content area minus how far we shifted in
//     float remaining_w = fmaxf(0.0f, content_area_w - adj_offset_x);
//     float remaining_h = fmaxf(0.0f, content_area_h - adj_offset_y);

//     box.dimensions.x = fminf(box.dimensions.x, remaining_w);
//     box.dimensions.y = fminf(box.dimensions.y, remaining_h);

//     // Pixel Snapping
//     box.coords.x = floorf(box.coords.x);
//     box.coords.y = floorf(box.coords.y);
//     box.dimensions.x = floorf(box.dimensions.x);
//     box.dimensions.y = floorf(box.dimensions.y);

//     return box;
// }

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
