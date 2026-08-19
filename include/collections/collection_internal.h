/**********************************************************************************************
*
* COLLECTION INTERNAL HELPERS
*
* Shared alloc/growth internals for DArray (circular) and LArray (linear). Not part of the
* public collections API - only included by dynamic_array.c and linear_array.c.
*
**********************************************************************************************/
#ifndef COLLECTION_INTERNAL_H
#define COLLECTION_INTERNAL_H

#include <stddef.h>
#include <stdio.h>
#include "memory/cmemory.h"

// Shared growth formula so DArray/LArray capacities expand identically.
static inline int Collection_CalcGrowthCapacity(int capacity)
{
    return (capacity == 0) ? 4 : (int)(capacity * 1.4f) + 1;
}

// Allocates the raw items buffer for a new array; on failure prints a message and returns NULL.
static inline void *Collection_AllocItemsBuffer(int elem_count, size_t elem_bytes, const char *owner_label)
{
    void *items = AllocateBytes(elem_bytes * (size_t)elem_count);
    if (items == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for %s!\n", owner_label);
    }
    return items;
}

// Releases shared item storage and resets the common collection bookkeeping.
static inline void Collection_ClearItems(void **items, int *count, int *capacity, size_t elem_bytes)
{
    if (!items || !count || !capacity)
    {
        return;
    }

    if (*items != NULL)
    {
        Deallocate(items, (size_t)*capacity * elem_bytes);
    }

    *count = 0;
    *capacity = 0;
}

// Grows an items buffer in-place using ReallocateBytes and zero-fills the new tail.
// Returns true on success. On failure the original buffer is left untouched.
static inline bool Collection_GrowBuffer(void **items, int *capacity, size_t elem_bytes)
{
    if (!items || !capacity)
    {
        return false;
    }

    int new_capacity = Collection_CalcGrowthCapacity(*capacity);
    size_t old_bytes = (size_t)(*capacity) * elem_bytes;
    size_t new_bytes = (size_t)new_capacity * elem_bytes;

    void *new_items = ReallocateBytes(*items, old_bytes, new_bytes);
    if (new_items == NULL)
    {
        fprintf(stderr, "Failed to grow collection buffer to new capacity %d!\n", new_capacity);
        return false;
    }

    if (new_capacity > *capacity)
    {
        MemorySet((char *)new_items + old_bytes, 0, new_bytes - old_bytes);
    }

    *items = new_items;
    *capacity = new_capacity;
    return true;
}

#endif // COLLECTION_INTERNAL_H
