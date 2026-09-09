/**********************************************************************************************
 *
 * CLICK RESOLVER MODULE
 *
 **********************************************************************************************/
#include "system/click_resolver.h"

#include "math/affine_space_ops.h"

// Recursively finds the deepest clickable space containing parent_point.
// Siblings are tested in order; the last hit overrides earlier ones so that the top-most
// visual element is selected. The returned local point is in the resolved space's frame.
const ClickableSpace *Clickable_ResolvePoint(const ClickableSpace *root, Vector2d parent_point,
                                             Vector2d *out_local)
{
    if (!root || !root->contains_local)
    {
        return NULL;
    }

    Vector2d local_point = Frame_TransformPoint_FromParent(parent_point, &root->frame);
    if (!root->contains_local(root, local_point))
    {
        return NULL;
    }

    // Root contains the point; see if any child contains it more specifically.
    const ClickableSpace *deepest = root;
    Vector2d deepest_local = local_point;

    for (const ClickableSpace *child = root->first_child; child != NULL; child = child->next_sibling)
    {
        Vector2d child_local = ZERO_VECTOR_2D;
        const ClickableSpace *child_hit = Clickable_ResolvePoint(child, local_point, &child_local);
        if (child_hit != NULL)
        {
            deepest = child_hit;
            deepest_local = child_local;
        }
    }

    if (out_local != NULL)
    {
        *out_local = deepest_local;
    }

    return deepest;
}
