/**********************************************************************************************
 *
 * SLOT MAP COLLECTION MODULE
 *
 * Generational slot map implementation providing stable handles and O(1) allocation/lookups.
 *
 **********************************************************************************************/
#include "collections/slot_map.h"
#include "collections/collection_internal.h"
#include "memory/cmemory.h"
#include "common/common.h"

// Grow internal slot metadata and payload storage in-place.
static bool GrowSlotMap(SlotMap *sm)
{
    if (!sm)
    {
        return false;
    }

    int new_capacity = Collection_CalcGrowthCapacity(sm->capacity);
    if (new_capacity > (int)SLOT_HANDLE_MAX_SLOTS)
    {
        new_capacity = (int)SLOT_HANDLE_MAX_SLOTS;
        if (new_capacity <= sm->capacity)
        {
            LOG_ERROR("SlotMap reached maximum capacity limit (%d)\n", sm->capacity);
            return false;
        }
    }

    size_t old_slots_bytes = (size_t)sm->capacity * sizeof(SlotEntry);
    size_t new_slots_bytes = (size_t)new_capacity * sizeof(SlotEntry);
    SlotEntry *new_slots = (SlotEntry *)ReallocateBytes(sm->slots, old_slots_bytes, new_slots_bytes);
    if (!new_slots)
    {
        LOG_ERROR("Failed to reallocate SlotMap slot metadata!\n");
        return false;
    }

    // Initialise newly created slots with default generation 1 and inactive state.
    for (int index = sm->capacity; index < new_capacity; index++)
    {
        new_slots[index].generation = 1u;
        new_slots[index].next_free = -1;
        new_slots[index].active = false;
    }
    sm->slots = new_slots;

    if (sm->elem_bytes > 0)
    {
        size_t old_items_bytes = (size_t)sm->capacity * sm->elem_bytes;
        size_t new_items_bytes = (size_t)new_capacity * sm->elem_bytes;
        void *new_items = ReallocateBytes(sm->items, old_items_bytes, new_items_bytes);
        if (!new_items)
        {
            LOG_ERROR("Failed to reallocate SlotMap items buffer!\n");
            return false;
        }

        if (new_items_bytes > old_items_bytes)
        {
            MemorySet((char *)new_items + old_items_bytes, 0, new_items_bytes - old_items_bytes);
        }
        sm->items = new_items;
    }

    sm->capacity = new_capacity;
    return true;
}

// Initialise a stack-allocated SlotMap with initial capacity and item byte size.
SlotMap MakeSlotMap(int initial_capacity, size_t elem_bytes)
{
    SlotMap sm = {0};
    int capacity = initial_capacity > 0 ? initial_capacity : 4;
    sm.elem_bytes = elem_bytes;
    sm.capacity = capacity;
    sm.count = 0;
    sm.high_water_mark = 0;
    sm.first_free = -1;

    sm.slots = (SlotEntry *)Collection_AllocItemsBuffer(capacity, sizeof(SlotEntry), "SlotMap slots");
    if (!sm.slots)
    {
        sm.capacity = 0;
        return sm;
    }

    for (int index = 0; index < capacity; index++)
    {
        sm.slots[index].generation = 1u;
        sm.slots[index].next_free = -1;
        sm.slots[index].active = false;
    }

    if (elem_bytes > 0)
    {
        sm.items = Collection_AllocItemsBuffer(capacity, elem_bytes, "SlotMap items");
        if (!sm.items)
        {
            Deallocate((void **)&sm.slots, (size_t)capacity * sizeof(SlotEntry));
            sm.capacity = 0;
            return sm;
        }
        MemorySet(sm.items, 0, (size_t)capacity * elem_bytes);
    }

    return sm;
}

// Allocate a heap-allocated SlotMap.
SlotMap *AllocSlotMap(int initial_capacity, size_t elem_bytes)
{
    SlotMap *sm = (SlotMap *)AllocateBytes(sizeof(SlotMap));
    if (!sm)
    {
        LOG_ERROR("Failed to allocate heap memory for SlotMap!\n");
        return NULL;
    }

    *sm = MakeSlotMap(initial_capacity, elem_bytes);
    return sm;
}

// Dispose of a heap-allocated SlotMap and its internal buffers.
void DisposeSlotMap(SlotMap *sm)
{
    if (!sm)
    {
        return;
    }

    ClearSlotMap(sm);
    Deallocate((void **)&sm, sizeof(SlotMap));
}

// Release all buffer allocations and reset the SlotMap container.
void ClearSlotMap(SlotMap *sm)
{
    if (!sm)
    {
        return;
    }

    if (sm->slots)
    {
        Deallocate((void **)&sm->slots, (size_t)sm->capacity * sizeof(SlotEntry));
    }

    if (sm->items && sm->elem_bytes > 0)
    {
        Deallocate((void **)&sm->items, (size_t)sm->capacity * sm->elem_bytes);
    }

    sm->capacity = 0;
    sm->count = 0;
    sm->high_water_mark = 0;
    sm->first_free = -1;
    sm->elem_bytes = 0;
}

// Reset all slots to inactive and advance their generations, invalidating all live handles.
void ResetSlotMap(SlotMap *sm)
{
    if (!sm || sm->capacity == 0)
    {
        return;
    }

    for (int index = 0; index < sm->high_water_mark; index++)
    {
        if (sm->slots[index].active)
        {
            sm->slots[index].active = false;
            sm->slots[index].generation = (sm->slots[index].generation + 1u) & SLOT_HANDLE_GENERATION_MASK;
            if (sm->slots[index].generation == 0u)
            {
                sm->slots[index].generation = 1u;
            }
        }
        sm->slots[index].next_free = -1;
    }

    if (sm->items && sm->elem_bytes > 0)
    {
        MemorySet(sm->items, 0, (size_t)sm->capacity * sm->elem_bytes);
    }

    sm->count = 0;
    sm->high_water_mark = 0;
    sm->first_free = -1;
}

// Allocate a slot and zero-initialise its payload memory, returning a stable handle.
SlotHandle SlotMap_Allocate(SlotMap *sm)
{
    if (!sm)
    {
        return INVALID_SLOT_HANDLE;
    }

    int slot_index = -1;
    if (sm->first_free >= 0)
    {
        // Reuse slot from the free list.
        slot_index = sm->first_free;
        sm->first_free = sm->slots[slot_index].next_free;
        sm->slots[slot_index].next_free = -1;
    }
    else
    {
        // Allocate next sequential slot from the high water mark.
        if (sm->high_water_mark >= sm->capacity)
        {
            if (!GrowSlotMap(sm))
            {
                return INVALID_SLOT_HANDLE;
            }
        }

        slot_index = sm->high_water_mark++;
    }

    SlotEntry *slot = &sm->slots[slot_index];
    slot->active = true;
    sm->count++;

    if (sm->items && sm->elem_bytes > 0)
    {
        MemorySet((char *)sm->items + ((size_t)slot_index * sm->elem_bytes), 0, sm->elem_bytes);
    }

    return SlotHandle_Create((uint32_t)slot_index, slot->generation);
}

// Insert an item into the slot map by copying elem_bytes, returning its stable handle.
SlotHandle SlotMap_Insert(SlotMap *sm, const void *item)
{
    SlotHandle handle = SlotMap_Allocate(sm);
    if (handle == INVALID_SLOT_HANDLE)
    {
        return INVALID_SLOT_HANDLE;
    }

    if (item && sm->items && sm->elem_bytes > 0)
    {
        uint32_t slot_index = SlotHandle_GetIndex(handle);
        MemoryCopy((char *)sm->items + ((size_t)slot_index * sm->elem_bytes), item, sm->elem_bytes);
    }

    return handle;
}

// Release a slot, invalidate the handle by incrementing generation, and push to the free list.
bool SlotMap_Remove(SlotMap *sm, SlotHandle handle)
{
    if (!sm || handle == INVALID_SLOT_HANDLE)
    {
        return false;
    }

    uint32_t slot_index = SlotHandle_GetIndex(handle);
    uint32_t generation = SlotHandle_GetGeneration(handle);
    if (slot_index >= (uint32_t)sm->high_water_mark || !sm->slots[slot_index].active ||
        sm->slots[slot_index].generation != generation)
    {
        return false;
    }

    SlotEntry *slot = &sm->slots[slot_index];
    slot->active = false;
    slot->generation = (slot->generation + 1u) & SLOT_HANDLE_GENERATION_MASK;
    if (slot->generation == 0u)
    {
        slot->generation = 1u;
    }

    slot->next_free = sm->first_free;
    sm->first_free = (int)slot_index;
    sm->count--;

    if (sm->items && sm->elem_bytes > 0)
    {
        MemorySet((char *)sm->items + ((size_t)slot_index * sm->elem_bytes), 0, sm->elem_bytes);
    }

    return true;
}

// Retrieve a pointer to an active slot payload if the handle is valid and current.
void *SlotMap_Get(const SlotMap *sm, SlotHandle handle)
{
    if (!sm || handle == INVALID_SLOT_HANDLE || sm->elem_bytes == 0 || !sm->items)
    {
        return NULL;
    }

    uint32_t slot_index = SlotHandle_GetIndex(handle);
    uint32_t generation = SlotHandle_GetGeneration(handle);
    if (slot_index >= (uint32_t)sm->high_water_mark || !sm->slots[slot_index].active ||
        sm->slots[slot_index].generation != generation)
    {
        return NULL;
    }

    return (char *)sm->items + ((size_t)slot_index * sm->elem_bytes);
}

// Return whether a handle references an active slot with a matching generation.
bool SlotMap_Contains(const SlotMap *sm, SlotHandle handle)
{
    if (!sm || handle == INVALID_SLOT_HANDLE)
    {
        return false;
    }

    uint32_t slot_index = SlotHandle_GetIndex(handle);
    uint32_t generation = SlotHandle_GetGeneration(handle);
    if (slot_index >= (uint32_t)sm->high_water_mark || !sm->slots[slot_index].active ||
        sm->slots[slot_index].generation != generation)
    {
        return false;
    }

    return true;
}

// Inspect the next handle that would be issued by SlotMap_Allocate without modifying state.
SlotHandle SlotMap_GetNextHandle(const SlotMap *sm)
{
    if (!sm)
    {
        return INVALID_SLOT_HANDLE;
    }

    if (sm->first_free >= 0)
    {
        int slot_index = sm->first_free;
        return SlotHandle_Create((uint32_t)slot_index, sm->slots[slot_index].generation);
    }

    int slot_index = sm->high_water_mark;
    if (slot_index >= (int)SLOT_HANDLE_MAX_SLOTS)
    {
        return INVALID_SLOT_HANDLE;
    }

    uint32_t generation = (slot_index < sm->capacity) ? sm->slots[slot_index].generation : 1u;
    return SlotHandle_Create((uint32_t)slot_index, generation);
}

// Retrieve handle and item pointer for an active slot by linear index.
bool SlotMap_GetSlotInfo(const SlotMap *sm, int slot_index, SlotHandle *out_handle, void **out_item)
{
    if (!sm || slot_index < 0 || slot_index >= sm->high_water_mark || !sm->slots[slot_index].active)
    {
        return false;
    }

    if (out_handle)
    {
        *out_handle = SlotHandle_Create((uint32_t)slot_index, sm->slots[slot_index].generation);
    }

    if (out_item)
    {
        *out_item = (sm->items && sm->elem_bytes > 0)
            ? (char *)sm->items + ((size_t)slot_index * sm->elem_bytes)
            : NULL;
    }

    return true;
}
