/**********************************************************************************************
 *
 * ANCESTRY MODULE
 *
 * A read-only query layer over the EntityId frame hierarchy. Every frame-bearing node (a plain
 * entity, a world, the universe root world) is an EntityId that resolves - via the entity
 * registry - to a Newtonoid2d body carrying a parent_id. Ancestry follows that parent_id trail
 * upward: child entity -> parent entities -> owning world -> universe root world -> INVALID.
 *
 * This is NOT a new data structure: it is traversal + classification over the existing
 * registry/world data. Nodes are identified by EntityId throughout (pointers into the packed
 * entity storage are not stable and are never retained across hops).
 *
 **********************************************************************************************/
#ifndef ANCESTRY_H
#define ANCESTRY_H

#include "entities/entity_id.h"

// Classification of a frame node, resolved from the registry. Worlds and plain entities both
// resolve to a Newtonoid2d body, so they are told apart by the WorldComponent presence tag.
typedef enum AncestryNodeKind
{
    ANCESTRY_KIND_INVALID = 0, // id is not active / does not resolve.
    ANCESTRY_KIND_ENTITY,      // a plain entity node.
    ANCESTRY_KIND_WORLD        // a world node (carries ENTITY_COMPONENT_WORLD).
} AncestryNodeKind;

// Classify a node by id. Returns ANCESTRY_KIND_INVALID for an inactive/unresolvable id,
// ANCESTRY_KIND_WORLD when the id carries the world tag, otherwise ANCESTRY_KIND_ENTITY.
AncestryNodeKind Ancestry_GetKind(EntityId id);

// Return the immediate parent of a node via its body's parent_id, or INVALID_ENTITY_ID when the
// node is the tree root (the universe root world) or the id does not resolve.
EntityId Ancestry_GetParent(EntityId id);

// Walk from `start` up to the tree root, writing the chain (start first, ending at the last
// reachable ancestor) into out_ids. The walk stops at the root (parent == INVALID), when
// out_capacity is reached, or after max_generations hops - whichever comes first. The hop cap
// also guards against a malformed cycle. Returns the number of ids written.
//
//   start           : the node to walk up from.
//   max_generations : maximum number of PARENT hops to follow (>= 0). 0 yields just `start`.
//   out_ids         : caller buffer receiving the chain; may be NULL only if out_capacity == 0.
//   out_capacity    : capacity of out_ids in elements.
int Ancestry_WalkUp(EntityId start, int max_generations, EntityId *out_ids, int out_capacity);

// Return the number of parent hops from `id` up to the root (the root itself has depth 0), or
// -1 for an unresolvable id. Bounded internally by a sane cap so a malformed cycle cannot hang.
int Ancestry_GetDepth(EntityId id);

#endif
