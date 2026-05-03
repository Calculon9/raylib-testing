#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "collections/linear_array.h"
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
bool GrowLinearArray(LArray *da);

// Create a new queue with specified element size and count
LArray *NewLArray(int elem_count, size_t elem_bytes) 
{
    // Allocate memory for the DynamicArray struct itself
    LArray *la = AllocateBytes(sizeof(LArray));

    la->elem_bytes = elem_bytes;
    la->capacity = elem_count;
    la->count = 0;
    la->enumeratorIndex = 0;
    la->enumerationCount = 0;
    la->items = AllocateBytes(elem_bytes * elem_count);

    // Simple safety check
    if (la->items == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for Dynamic Array!\n");
        la->capacity = 0; // Ensure nothing can be pushed
        return la;
    }
    return la;
}

// Provide the address of the DynamicArray and the address of the item to push.
bool LArray_Push(LArray *la, void *item)
{
    if (la == NULL)
    {
        fprintf(stderr, "The provided Linear Array is NULL.\n");
        return false;
    }

    // 1. Check for growth FIRST
    // We use -> to access the REAL data, not a copy
    if (la->count >= la->capacity)
    {
        if (!GrowLinearArray(la))
        {
            fprintf(stderr, "Failed to grow array!\n");
            return false;
        }
    }

    // 2. Calculate the target address using the ACTUAL live data
    // Note: We use da->coll.items because GrowDynamicArray might have changed it!
    void *target = (char *)la->items + (la->count * la->elem_bytes);

    // 3. Copy the data
    memcpy(target, item, la->elem_bytes);

    // 4. Update the REAL state
    la->count++;

    return true;
}

void *LArray_Pop(LArray *la, void *out_item)
{
    if (la == NULL) // || da->coll == NULL)
    {
        fprintf(stderr, "The provided collection is NULL. Cannot pop item.\n");
    }
    if (la->count <= 0)
    {
        fprintf(stderr, "Linear Array is empty! Cannot pop item.\n");
    }

    // Calculate the address using the current FRONT index
    void *source = (char *)la->items + (la->front * la->elem_bytes);

    // Copy the data out for the user
    if (out_item != NULL)
    {
        memcpy(out_item, source, la->elem_bytes);
    }

    la->count--;
}

// Increase the capacity of the array by a specified factor (e.g., double the capacity)
bool GrowLinearArray(LArray *la)
{
    if (la == NULL)
        return false;

    if (la->elem_bytes < 1)
    {
        fprintf(stderr, "Invalid element size %zu in GrowLinearArray! Must be greater than 0.\n", la->elem_bytes);
        return false;
    }

    int new_capacity = (la->capacity == 0) ? 4 : la->capacity * 2;
    // Note: We use malloc here because we are manually re-ordering,
    // so we don't need realloc to copy the "old" scrambled order.
    void *new_items = AllocateBytes(new_capacity * la->elem_bytes);

    if (new_items == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for growing array to new capacity %d!\n", new_capacity);
        return false;
    }

    memcpy(new_items, la->items, new_capacity * la->elem_bytes);

    la->items = new_items;
    la->capacity = new_capacity;
    la->front = 0;       // Start is now at the beginning

    return true;
}

void *LArray_Get(LArray *la, int index)
{
    if (la == NULL)
    {
        fprintf(stderr, "The provided Linear Array is NULL. Cannot get item.\n");
        return NULL;
    }
    if (index < 0 || index >= la->count)
    {
        fprintf(stderr, "Index %d is out of bounds for Linear Array of count %d. Cannot get item.\n", index, la->count);
        return NULL;
    }

    // Calculate the address using the current FRONT index and the requested index
    void *source = (char *)la->items + ((la->front + index) * la->elem_bytes);

    return source;
}

bool LArray_RemoveAt(LArray *la, int index)
{
    if (la == NULL)
    {
        fprintf(stderr, "The provided Linear Array is NULL. Cannot remove item.\n");
        return false;
    }
    if (index < 0 || index >= la->count)
    {
        fprintf(stderr, "Index %d is out of bounds for Linear Array of count %d. Cannot remove item.\n", index, la->count);
        return false;
    }

    // Calculate the address of the item to remove
    void *source = (char *)la->items + (index * la->elem_bytes);

    // Shift items after the removed item forward to fill the gap
    memmove(source, (char *)source + la->elem_bytes, (la->count - index - 1) * la->elem_bytes);

    la->count--;
    return true;
}

// Dispose of the array and free its memory
void DisposeLinearArray(LArray *la)
{
    if (la == NULL)
        return;

    if (la->items != NULL)
    {
        DeallocateShallow(&la->items, la->capacity * la->elem_bytes);
        la->items = NULL;
    }
    la->count = 0;
    la->capacity = 0;

    DeallocateShallow((void **)&la, sizeof(LArray));
}



// Queue *pop(Queue *q, void *outItem)
// {
//     if (q == NULL || q->count <= 0)
//     {
//         fprintf(stderr, "Queue is empty! Cannot pop item.\n");
//         return q;
//     }
//     // Calculate the address of the front item
//     void *source = (char *)q->items; // Front item is always at the start of the block
//     memcpy(outItem, source, q->elemSize);

//     // Shift remaining items forward to fill the gap left by the popped item
//     //--void *memmove(void *dest, const void *src, size_t n)--
//     // dest: Where you want the data to go.
//     // src: Where the data is currently.
//     // n: How many bytes to move
//     //memmove(q->items, (char *)q->items + q->elemSize, (q->count - 1) * q->elemSize);

//     q->front = (q->front + 1) % q->capacity; // Update front index for circular buffer
//     q->count--;

//     return q;
// }

//
// void *Enumerate(DynamicArray *da)
// {
//     // Rear points to the next empty slot where data will be written, so we want to stop enumerating once we hit rear, not capacity
//     if (da == NULL) return NULL;
//     if (da->enumeratorIndex == da->rear)
//     {
//         ResetEnumerator(da); // Reset enumerator for the next time we want to enumerate
//         return NULL;
//     }

//     // Calculate the address of the current enumerator index
//     void *item = (char *)da->items + (da->enumeratorIndex * da->elemSize);

//     // Move the enumerator index to the next item for the next call
//     da->enumeratorIndex = (da->enumeratorIndex + 1) % da->capacity;

//     return item;
// }

// void *ResetEnumerator(DynamicArray *da) {
//     // Reset enumerator to the front
//     if (da != NULL) {
//         da->enumeratorIndex = da->front;
//     }
// }

// // Get the current number of elements in the array
// size_t GetElementCount(DynamicArray *da)
// {
//     if (da == NULL) return 0;
//     return da->count;
// }