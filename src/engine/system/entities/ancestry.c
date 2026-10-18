/**********************************************************************************************
 *
 * ANCESTRY MODULE
 *
 **********************************************************************************************/
#include "system/entities/ancestry.h"
#include "entities/entity_registry.h"
#include "physics/newtonoid.h"

// Absolute safety cap on any upward walk, independent of the caller's max_generations. The
// frame tree is shallow (entity -> parents -> world -> root), so this comfortably exceeds any
// legitimate depth while guaranteeing termination even if parent_id links were ever malformed
// into a cycle.
#define ANCESTRY_MAX_WALK 256

// Classify a node by id: invalid if inactive, world if it carries the world tag, else entity.
AncestryNodeKind Ancestry_GetKind(EntityId id)
{
    if (id == INVALID_ENTITY_ID || !EntityRegistry_IsIdActive(id))
    {
        return ANCESTRY_KIND_INVALID;
    }

    // Worlds and plain entities both resolve to a Newtonoid2d body, so the WorldComponent
    // presence tag is the discriminator. Check it before assuming "entity".
    if (EntityRegistry_HasComponent(id, ENTITY_COMPONENT_WORLD))
    {
        return ANCESTRY_KIND_WORLD;
    }

    return ANCESTRY_KIND_ENTITY;
}

// Resolve the immediate parent via the node's body parent_id. Returns INVALID at the root or
// when the id does not resolve to a live body.
EntityId Ancestry_GetParent(EntityId id)
{
    if (id == INVALID_ENTITY_ID)
    {
        return INVALID_ENTITY_ID;
    }

    Newtonoid2d *body = EntityRegistry_GetEntity(id);
    if (!body)
    {
        return INVALID_ENTITY_ID;
    }

    return body->parent_id;
}

// Walk upward from `start`, recording ids start-first until the root, the capacity, the hop cap
// or max_generations is reached. Returns the count written.
int Ancestry_WalkUp(EntityId start, int max_generations, EntityId *out_ids, int out_capacity)
{
    // Nothing to write into, or an invalid start: produce an empty chain.
    if (out_capacity <= 0 || !out_ids || start == INVALID_ENTITY_ID ||
        !EntityRegistry_IsIdActive(start))
    {
        return 0;
    }

    // Record the starting node, then follow parent hops. Each iteration appends one parent,
    // bounded by max_generations (caller intent), out_capacity (buffer), and ANCESTRY_MAX_WALK
    // (cycle guard). A negative max_generations is treated as zero.
    int count = 0;
    out_ids[count++] = start;

    int hop_limit = max_generations < 0 ? 0 : max_generations;
    EntityId current = start;
    for (int hop = 0; hop < hop_limit && hop < ANCESTRY_MAX_WALK && count < out_capacity; hop++)
    {
        EntityId parent = Ancestry_GetParent(current);
        if (parent == INVALID_ENTITY_ID || !EntityRegistry_IsIdActive(parent))
        {
            break; // Reached the root (or a severed link): the chain ends here.
        }

        out_ids[count++] = parent;
        current = parent;
    }

    return count;
}

// Count parent hops from `id` to the root. Root depth is 0; an unresolvable id is -1.
int Ancestry_GetDepth(EntityId id)
{
    if (id == INVALID_ENTITY_ID || !EntityRegistry_IsIdActive(id))
    {
        return -1;
    }

    int depth = 0;
    EntityId current = id;
    for (int hop = 0; hop < ANCESTRY_MAX_WALK; hop++)
    {
        EntityId parent = Ancestry_GetParent(current);
        if (parent == INVALID_ENTITY_ID || !EntityRegistry_IsIdActive(parent))
        {
            break; // current is the root.
        }

        depth++;
        current = parent;
    }

    return depth;
}
