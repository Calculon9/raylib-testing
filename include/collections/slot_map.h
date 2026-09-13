/**********************************************************************************************
 *
 * SLOT MAP COLLECTION MODULE
 *
 * Provides a generic generational slot map container. Stored items are indexed by stable,
 * versioned handles (SlotHandle) that detect stale references upon slot recycling.
 *
 **********************************************************************************************/
#ifndef SLOT_MAP_H
#define SLOT_MAP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

// SlotHandle is a stable, versioned handle used to index into the slot map.
// They are bit-packed into a single 32-bit value.
// The lower bits store the slot index, and the upper bits store the generation.
typedef uint32_t SlotHandle;

#define INVALID_SLOT_HANDLE 0u

#define SLOT_HANDLE_INDEX_BITS 15u
#define SLOT_HANDLE_GENERATION_BITS 16u
#define SLOT_HANDLE_INDEX_MASK ((1u << SLOT_HANDLE_INDEX_BITS) - 1u)
#define SLOT_HANDLE_GENERATION_MASK ((1u << SLOT_HANDLE_GENERATION_BITS) - 1u)
#define SLOT_HANDLE_MAX_SLOTS SLOT_HANDLE_INDEX_MASK

// Encode a slot index and generation into one positive, stale-handle-safe handle.
static inline SlotHandle SlotHandle_Create(uint32_t slot_index, uint32_t generation)
{
    uint32_t encoded_index = slot_index + 1u;
    if (encoded_index > SLOT_HANDLE_INDEX_MASK || generation == 0u || generation > SLOT_HANDLE_GENERATION_MASK)
    {
        return INVALID_SLOT_HANDLE;
    }

    return (SlotHandle)((generation << SLOT_HANDLE_INDEX_BITS) | encoded_index);
}

// Return the slot index encoded in a handle, or SLOT_HANDLE_MAX_SLOTS for invalid handles.
static inline uint32_t SlotHandle_GetIndex(SlotHandle handle)
{
    if (handle == INVALID_SLOT_HANDLE)
    {
        return SLOT_HANDLE_MAX_SLOTS;
    }

    uint32_t encoded_index = handle & SLOT_HANDLE_INDEX_MASK;
    return encoded_index == 0u ? SLOT_HANDLE_MAX_SLOTS : encoded_index - 1u;
}

// Return the generation encoded in a handle, or zero for invalid handles.
static inline uint32_t SlotHandle_GetGeneration(SlotHandle handle)
{
    if (handle == INVALID_SLOT_HANDLE)
    {
        return 0u;
    }

    return (handle >> SLOT_HANDLE_INDEX_BITS) & SLOT_HANDLE_GENERATION_MASK;
}

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Metadata associated with a single slot in the map.
typedef struct SlotEntry
{
    uint32_t generation; // Version number incremented on slot reuse.
    int next_free;       // Index of the next free slot (-1 if none).
    bool active;         // Whether this slot currently holds an active item.
} SlotEntry;

// Generic generational slot map container.
typedef struct SlotMap
{
    SlotEntry *slots;   // Dynamic array of slot metadata entries.
    void *items;        // Contiguous buffer holding stored item payloads.
    size_t elem_bytes;  // Size of one stored element in bytes (0 for handle-only tracking).
    int capacity;       // Maximum slots currently allocated in the buffer.
    int count;          // Total number of currently active slots.
    int high_water_mark;// Highest slot index reached during sequential allocation.
    int first_free;     // Index of the first available slot in the free list (-1 if none).
} SlotMap;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Initialise a stack-value SlotMap with the specified initial capacity and element size.
// OWNERSHIP: Caller owns struct; call ClearSlotMap() before scope exit to free buffers.
SlotMap MakeSlotMap(int initial_capacity, size_t elem_bytes);

// Allocate a heap-allocated SlotMap pointer with the specified capacity and element size.
// OWNERSHIP: Caller owns returned pointer; must call DisposeSlotMap() to free both buffer and struct.
SlotMap *AllocSlotMap(int initial_capacity, size_t elem_bytes);

// Dispose of a heap-allocated SlotMap, releasing internal buffers and freeing the container.
void DisposeSlotMap(SlotMap *sm);

// Release internal buffers and reset the SlotMap container fields to zero.
void ClearSlotMap(SlotMap *sm);

// Reset all slots to inactive, incrementing generations to invalidate all existing handles.
void ResetSlotMap(SlotMap *sm);

// Insert an item into the slot map, copying elem_bytes from item, and return its handle.
SlotHandle SlotMap_Insert(SlotMap *sm, const void *item);

// Allocate a slot and zero-initialise its item memory, returning its handle.
SlotHandle SlotMap_Allocate(SlotMap *sm);

// Invalidate a slot handle, increment its generation, and recycle the slot for reuse.
bool SlotMap_Remove(SlotMap *sm, SlotHandle handle);

// Retrieve a pointer to the item stored in the slot if the handle is valid and active.
void *SlotMap_Get(const SlotMap *sm, SlotHandle handle);

// Return whether a given handle refers to an active, valid slot.
bool SlotMap_Contains(const SlotMap *sm, SlotHandle handle);

// Inspect the next SlotHandle that would be returned by an allocation without advancing state.
SlotHandle SlotMap_GetNextHandle(const SlotMap *sm);

// Inspect slot state by linear index. Populates out_handle and out_item if active and returns true.
bool SlotMap_GetSlotInfo(const SlotMap *sm, int slot_index, SlotHandle *out_handle, void **out_item);

#endif // SLOT_MAP_H
