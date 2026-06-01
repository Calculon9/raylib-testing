#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "collections/linear_array.h"
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
bool GrowLinearArray(LArray *da);

// Create a new linear array with specified element size and count, returns a LArray pointer (you must dispose BOTH the array struct AND the internal buffer holding the elements)
LArray *AllocLArray(int elem_count, size_t elem_bytes) 
{
    // Allocate memory for the DynamicArray struct itself
    LArray *a = AllocateBytes(sizeof(LArray));

    a->elem_bytes = elem_bytes;
    a->capacity = elem_count;
    a->count = 0;
    a->enumeratorIndex = 0;
    a->enumerationCount = 0;
    a->items = AllocateBytes(elem_bytes * elem_count);

    // Simple safety check
    if (a->items == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for Linear Array!\n");
        a->capacity = 0; // Ensure nothing can be pushed
        return a;
    }
    return a;
}

// Create a new linear array with specified element size and count, returns LArray directly (you must dispose the internal buffer holding the elements)
LArray MakeLArray(int elem_count, size_t elem_bytes) 
{
    // Allocate memory for the DynamicArray struct itself
    LArray a = {0};
    a.elem_bytes = elem_bytes;
    a.capacity = elem_count;
    a.count = 0;
    a.enumeratorIndex = 0;
    a.enumerationCount = 0;
    a.items = AllocateBytes(elem_bytes * elem_count);

    // Simple safety check
    if (a.items == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for Linear Array!\n");
        a.capacity = 0; // Ensure nothing can be pushed
        return a;
    }
    return a;
}

// Provide the address of the DynamicArray and the address of the item to push.
bool LArray_Push(LArray *a, void *item)
{
    if (a == NULL)
    {
        fprintf(stderr, "The provided Linear Array is NULL.\n");
        return false;
    }

    // 1. Check for growth FIRST
    // We use -> to access the REAL data, not a copy
    if (a->count >= a->capacity)
    {
        if (!GrowLinearArray(a))
        {
            fprintf(stderr, "Failed to grow array!\n");
            return false;
        }
    }

    // 2. Calcuate the target address using the ACTUAL live data
    // Note: We use da->coll.items because GrowDynamicArray might have changed it!
    void *target = (char *)a->items + (a->count * a->elem_bytes);

    // 3. Copy the data
    memcpy(target, item, a->elem_bytes);

    // 4. Update the REAL state
    a->count++;

    return true;
}

void *LArray_Pop(LArray *a, void *out_item)
{
    if (a == NULL) // || da->coll == NULL)
    {
        fprintf(stderr, "The provided collection is NULL. Cannot pop item.\n");
    }
    if (a->count <= 0)
    {
        fprintf(stderr, "Linear Array is empty! Cannot pop item.\n");
    }

    // Calcuate the address using the current FRONT index
    void *source = (char *)a->items + (a->front * a->elem_bytes);

    // Copy the data out for the user
    if (out_item != NULL)
    {
        memcpy(out_item, source, a->elem_bytes);
    }

    a->count--;
}

// Increase the capacity of the array by a specified factor (e.g., double the capacity)
bool GrowLinearArray(LArray *a)
{
    if (a == NULL)
        return false;

    if (a->elem_bytes < 1)
    {
        fprintf(stderr, "Invalid element size %zu in GrowLinearArray! Must be greater than 0.\n", a->elem_bytes);
        return false;
    }

    int new_capacity = (a->capacity == 0) ? 4 : a->capacity * 2;
    // Note: We use malloc here because we are manually re-ordering,
    // so we don't need realloc to copy the "old" scrambled order.
    void *new_items = AllocateBytes(new_capacity * a->elem_bytes);

    if (new_items == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for growing array to new capacity %d!\n", new_capacity);
        return false;
    }

    memcpy(new_items, a->items, new_capacity * a->elem_bytes);

    a->items = new_items;
    a->capacity = new_capacity;
    a->front = 0;       // Start is now at the beginning

    return true;
}

void *LArray_Get(LArray *a, int index)
{
    if (a == NULL)
    {
        fprintf(stderr, "The provided Linear Array is NULL. Cannot get item.\n");
        return NULL;
    }
    if (index < 0 || index >= a->count)
    {
        fprintf(stderr, "Index %d is out of bounds for Linear Array of count %d. Cannot get item.\n", index, a->count);
        return NULL;
    }

    // Calcuate the address using the current FRONT index and the requested index
    void *source = (char *)a->items + ((a->front + index) * a->elem_bytes);

    return source;
}

bool LArray_RemoveAt(LArray *a, int index)
{
    if (a == NULL)
    {
        fprintf(stderr, "The provided Linear Array is NULL. Cannot remove item.\n");
        return false;
    }
    if (index < 0 || index >= a->count)
    {
        fprintf(stderr, "Index %d is out of bounds for Linear Array of count %d. Cannot remove item.\n", index, a->count);
        return false;
    }

    // Calcuate the address of the item to remove
    void *source = (char *)a->items + (index * a->elem_bytes);

    // Shift items after the removed item forward to fill the gap
    memmove(source, (char *)source + a->elem_bytes, (a->count - index - 1) * a->elem_bytes);

    a->count--;
    return true;
}

// Dispose of the array and free its memory
void DisposeLArray(LArray *a)
{
    if (a == NULL)
        return;

    if (a->items != NULL)
    {
        DeallocateShallow(&a->items, a->capacity * a->elem_bytes);
        a->items = NULL;
    }
    a->count = 0;
    a->capacity = 0;

    DeallocateShallow((void **)&a, sizeof(LArray));
}

// Clears internal heap data buffers without freeing the header struct container itself
void ClearLArray(LArray *a)
{
    if (a == NULL) return;
    
    if (a->items != NULL)
    {
        DeallocateShallow(&a->items, a->capacity * a->elem_bytes);
        a->items = NULL;
    }
    a->count = 0;
    a->capacity = 0;
}

// Queue *pop(Queue *q, void *outItem)
// {
//     if (q == NULL || q->count <= 0)
//     {
//         fprintf(stderr, "Queue is empty! Cannot pop item.\n");
//         return q;
//     }
//     // Calcuate the address of the front item
//     void *source = (char *)q->items; // Front item is always at the start of the block
//     memcpy(outItem, source, q->elemSize);

//     // Shift remaining items forward to fill the gap left by the popped item
//     //--void *memmove(void *dest, const void *src, size_t n)--
//     // dest: Where you want the data to go.
//     // src: Where the data is currently.
//     // n: How many bytes to move
//     //memmove(q->items, (char *)q->items + q->elemSize, (q->count - 1) * q->elemSize);

//     q->front = (q->front + 1) % q->capacity; // Update front index for circuar buffer
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

//     // Calcuate the address of the current enumerator index
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