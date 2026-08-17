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

#endif // COLLECTION_INTERNAL_H
