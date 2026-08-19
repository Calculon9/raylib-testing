/**********************************************************************************************
 *
 * CLICK RESOLVER MODULE
 *
 **********************************************************************************************/
#ifndef CLICK_RESOLVER_H
#define CLICK_RESOLVER_H

#include <stdbool.h>
#include "math/cvectors.h"

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// A clickable region in a hierarchical coordinate space.
// Children are tested in sibling order, so the last matching sibling wins (top-most).
typedef struct ClickableSpace
{
    Frame2d frame;                              // Transforms parent-space points into local space.
    bool (*contains_local)(const struct ClickableSpace *space, Vector2d local_point);
    void *ctx;                                  // Optional context passed to contains_local.
    int user_data;                              // Optional integer payload (e.g. array index).
    struct ClickableSpace *first_child;         // First child in a singly-linked sibling list.
    struct ClickableSpace *next_sibling;        // Next sibling at the same hierarchy level.
} ClickableSpace;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Walks the clickable hierarchy and returns the deepest space that contains parent_point.
// If out_local is non-NULL, it receives the resolved point in the returned space's local frame.
// Returns NULL if root does not contain the point.
const ClickableSpace *Clickable_ResolvePoint(const ClickableSpace *root, Vector2d parent_point,
                                             Vector2d *out_local);

#endif // CLICK_RESOLVER_H
