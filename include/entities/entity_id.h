#ifndef ENTITY_ID_H
#define ENTITY_ID_H

#include "collections/slot_map.h"

typedef int EntityId;

#define INVALID_ENTITY_ID ((EntityId)INVALID_SLOT_HANDLE)

#define ENTITY_ID_INDEX_BITS SLOT_HANDLE_INDEX_BITS
#define ENTITY_ID_GENERATION_BITS SLOT_HANDLE_GENERATION_BITS
#define ENTITY_ID_INDEX_MASK SLOT_HANDLE_INDEX_MASK
#define ENTITY_ID_GENERATION_MASK SLOT_HANDLE_GENERATION_MASK
#define ENTITY_ID_MAX_SLOTS SLOT_HANDLE_MAX_SLOTS

// Encode a slot index and generation into one positive, stale-handle-safe ID.
static inline EntityId EntityId_Create(uint32_t slot_index, uint32_t generation)
{
    return (EntityId)SlotHandle_Create(slot_index, generation);
}

// Return the slot index encoded in an ID, or the out-of-range sentinel for invalid IDs.
static inline uint32_t EntityId_GetSlotIndex(EntityId entity_id)
{
    return SlotHandle_GetIndex((SlotHandle)entity_id);
}

// Return the generation encoded in an ID, or zero for invalid IDs.
static inline uint32_t EntityId_GetGeneration(EntityId entity_id)
{
    return SlotHandle_GetGeneration((SlotHandle)entity_id);
}

#endif
