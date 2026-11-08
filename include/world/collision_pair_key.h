/**********************************************************************************************
 *
 * COLLISION PAIR KEY (pure)
 *
 * Builds the per-frame "pair already resolved" key used by ProcessCollisionPair.
 *
 * The key is SYMMETRIC (key(a,b) == key(b,a)) and COLLISION-FREE:
 *  - An EntityId is (generation << 15) | (slot_index + 1). A slot holds at most one live entity
 *    at a time, so within one world and one pair pass two live entities never share a slot index.
 *  - slot_index + 1 is in [1, 32767] (15 bits), so (low << 15) | high is in [1, 2^30 - 1]:
 *    it fits a positive int (FlatMapInt's key type) with no overflow, and distinct unordered
 *    index pairs give distinct keys.
 *  - Callers must only form a key from ids that passed the generation-checked entity lookup, so a
 *    stale id can never alias a live entity that recycled its slot.
 * Returns 0 (never a valid key) if either id is invalid.
 *
 **********************************************************************************************/
#ifndef COLLISION_PAIR_KEY_H
#define COLLISION_PAIR_KEY_H

#include "entities/entity_id.h"

// Symmetric, collision-free key for the unordered pair of live entities (a, b).
static inline int CollisionPairKey(EntityId a, EntityId b)
{
    uint32_t index_a = (uint32_t)EntityId_GetSlotIndex(a) + 1u; // 1..32767 for valid ids
    uint32_t index_b = (uint32_t)EntityId_GetSlotIndex(b) + 1u;
    if (index_a > ENTITY_ID_INDEX_MASK || index_b > ENTITY_ID_INDEX_MASK)
        return 0; // Invalid id (the sentinel index wraps past the mask).

    uint32_t low = index_a < index_b ? index_a : index_b;
    uint32_t high = index_a < index_b ? index_b : index_a;
    return (int)((low << ENTITY_ID_INDEX_BITS) | high);
}

#endif // COLLISION_PAIR_KEY_H
