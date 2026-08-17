#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "collections/linear_array.h"
#include "collections/collection_internal.h"
#include "memory/cmemory.h"
#include "common/common.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
bool GrowLinearArray(LArray *da);

// Create a new linear array with specified element size and count, returns a LArray pointer (you must dispose BOTH the array struct AND the internal buffer holding the elements)
LArray *AllocLArray(int elem_count, size_t elem_bytes)
{
    // Allocate memory for the DynamicArray struct itself
    LArray *a = AllocateBytes(sizeof(LArray));

    if (a == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for Linear Array!\n");
        return NULL;
    }

    a->elem_bytes = elem_bytes;
    a->capacity = elem_count;
    a->count = 0;
    // a->enumeratorIndex = 0;
    // a->enumerationCount = 0;
    a->items = Collection_AllocItemsBuffer(elem_count, elem_bytes, "Linear Array");

    if (a->items == NULL)
    {
        a->capacity = 0;
    }
    return a;
}

// Create a new linear array with specified element size and count, returns LArray directly (you MUST dispose the internal buffer holding the elements)
LArray MakeLArray(int elem_count, size_t elem_bytes)
{
    LArray a = {0};
    a.elem_bytes = elem_bytes;
    a.capacity = elem_count;
    a.count = 0;
    // a.enumeratorIndex = 0;
    // a.enumerationCount = 0;
    a.items = Collection_AllocItemsBuffer(elem_count, elem_bytes, "Linear Array");

    // Simple safety check
    if (a.items == NULL)
    {
        a.capacity = 0; // Ensure nothing can be pushed
    }
    return a;
}

// Provide the address of the DynamicArray and the address of the item to push.
bool LArray_Push(LArray *a, void *item)
{
    if (a == NULL || item == NULL)
    {
        fprintf(stderr, "The provided Linear Array is NULL.\n");
        return false;
    }

    // Check for growth FIRST
    if (a->count >= a->capacity)
    {
        if (!GrowLinearArray(a))
        {
            fprintf(stderr, "Failed to grow array!\n");
            return false;
        }
    }

    // Calcuate the target address using the ACTUAL live data
    void *target = (char *)a->items + (a->count * a->elem_bytes);
    MemoryCopy(target, item, a->elem_bytes);
    a->count++;

    return true;
}

void *LArray_Pop(LArray *a, void *out_item)
{
    if (a == NULL || a->count == 0)
        return NULL;

    // Calcuate the address using the current FRONT index
    void *source = (char *)a->items + ((a->count - 1) * a->elem_bytes);

    // Copy the data out for the user
    if (out_item != NULL)
    {
        MemoryCopy(out_item, source, a->elem_bytes);
    }

    a->count--;
    return source;
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
    int new_capacity = Collection_CalcGrowthCapacity(a->capacity);
    size_t old_bytes = (size_t)a->capacity * a->elem_bytes;
    size_t new_bytes = (size_t)new_capacity * a->elem_bytes;

    void *new_items = ReallocateBytes(a->items, old_bytes, new_bytes);
    if (new_items == NULL)
    {
        fprintf(stderr, "Failed to grow array to new capacity %d!\n", new_capacity);
        return false;
    }

    if (new_capacity > a->capacity)
    {
        MemorySet((char *)new_items + old_bytes, 0, new_bytes - old_bytes);
    }

    a->items = new_items;
    a->capacity = new_capacity;

    return true;
    LOG_INFO("Linear Array grown to new capacity %d\n", new_capacity);
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
    void *source = (char *)a->items + (index * a->elem_bytes);

    return source;
}

void *LArray_GetCircular(LArray *a, int *index_tracker)
{
    if (a == NULL || a->count == 0 || index_tracker == NULL) return NULL;

    // Get the current item using the tracker passed in
    void *item = (char *)a->items + ((*index_tracker) * a->elem_bytes);

    // Advance and wrap the external tracker safely
    *index_tracker = (*index_tracker + 1) % a->count;

    return item;
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
    void *target = (char *)a->items + (index * a->elem_bytes);

    // If it's not the last element, compact the array forward to fill the void gap
    if (index < a->count - 1)
    {
        void *next_element = (char *)target + a->elem_bytes;
        size_t bytes_to_shift = (a->count - index - 1) * a->elem_bytes;
        MemoryMove(target, next_element, bytes_to_shift);
    }

    a->count--;
    return true;
}

bool LArray_SwapPopAt(LArray *a, int index)
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
    void *target = (char *)a->items + (index * a->elem_bytes);

    // If it's not the last element, move the last element to where the removed element was
    if (index < a->count - 1)
    {
        void *last_element = (char *)a->items + (a->count - 1) * a->elem_bytes;
        MemoryMove(target, last_element, a->elem_bytes);
    }

    a->count--;
    return true;
}

void *LArray_CircularEnumerate(LArray *a)
{
    if (a == NULL || a->count == 0)
    {
        return NULL;
    }

    // Calculate the address of the CURRENT item first
    void *item = (char *)a->items + (a->enumerator_index * a->elem_bytes);

    // Advance the index, wrapping smoothly back to 0 if we hit the end
    // Formula: (current_index + 1) % total_count
    a->enumerator_index = (a->enumerator_index + 1) % a->count;

    // eturn the item (this will never be NULL if the array has items)
    return item;
}

// Dispose of the array and free its memory
void DisposeLArray(LArray *a)
{
    if (a == NULL)
        return;

    if (a->items != NULL)
    {
        Deallocate(&a->items, a->capacity * a->elem_bytes);
        a->items = NULL;
    }
    a->count = 0;
    a->capacity = 0;

    Deallocate((void **)&a, sizeof(LArray));
}

// Clears internal heap data buffers without freeing the header struct container itself
void ClearLArray(LArray *a)
{
    if (a == NULL)
        return;

    if (a->items != NULL)
    {
        Deallocate(&a->items, a->capacity * a->elem_bytes);
        a->items = NULL;
    }
    a->count = 0;
    a->capacity = 0;
}

bool LArray_ResizeAndReset(LArray *a, int new_capacity)
{
    if (a == NULL)
    {
        fprintf(stderr, "The provided Linear Array is NULL. Cannot resize.\n");
        return false;
    }

    size_t new_bytes = (size_t)new_capacity * a->elem_bytes;

    // Reallocate safely using a temporary pointer
    size_t old_bytes = (size_t)a->capacity * a->elem_bytes;
    void *temp_items = ReallocateBytes(a->items, old_bytes, new_bytes);
    if (temp_items == NULL && new_bytes > 0)
    {
        fprintf(stderr, "Failed to reallocate memory to new capacity %d!\n", new_capacity);
        return false;
    }

    // Assign the newly allocated/resized block
    a->items = temp_items;

    // Reset the resized storage without passing a null pointer to memset.
    if (new_bytes > 0)
    {
        MemorySet(a->items, 0, new_bytes);
    }
    // if (new_capacity > a->capacity)
    // {
    //     // Get a pointer to where the old data ends and the new memory begins
    //     unsigned char *new_memory_start = (unsigned char *)a->items + old_bytes;
    //     size_t extra_bytes = new_bytes - old_bytes;

    //     // Zero out just the brand-new segment
    //     memset(new_memory_start, 0, extra_bytes);
    // }

    // Update the capacity tracking
    a->capacity = new_capacity;
    a->count = 0;
    // If we SHRUNK the array, clamp the item count so it doesn't overflow bounds
    // if (a->count > new_capacity)
    // {
    //     a->count = new_capacity;
    // }

    return true;
}

bool LArray_Reset(LArray *a)
{
    if (a == NULL || a->items == NULL)
    {
        fprintf(stderr, "The provided Linear Array is NULL. Cannot reset.\n");
        return false;
    }

    size_t bytes = (size_t)a->capacity * a->elem_bytes;

    if (bytes == 0)
    {
        fprintf(stderr, "The provided Linear Array has a capacity of %d and element size of %d. Cannot reset.\n", a->capacity, a->elem_bytes);
        return false;
    }

    MemorySet(a->items, 0, bytes);

    // Update the capacity tracking
    a->count = 0;

    return true;
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
