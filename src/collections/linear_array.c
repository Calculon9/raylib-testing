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
//----------------------------------------------------------------------------------

// Create a new linear array with specified element size and count, returns a LArray pointer (you must dispose BOTH the array struct AND the internal buffer holding the elements)
LArray *AllocLArray(int elem_count, size_t elem_bytes)
{
    LArray *a = AllocateBytes(sizeof(LArray));
    if (a == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for Linear Array!\n");
        return NULL;
    }

    *a = MakeLArray(elem_count, elem_bytes);
    return a;
}

// Create a new linear array with specified element size and count, returns LArray directly (you MUST dispose the internal buffer holding the elements)
LArray MakeLArray(int elem_count, size_t elem_bytes)
{
    LArray a = {0};
    a.elem_bytes = elem_bytes;
    a.capacity = elem_count;
    a.count = 0;
    a.items = Collection_AllocItemsBuffer(elem_count, elem_bytes, "Linear Array");

    if (a.items == NULL)
    {
        a.capacity = 0;
    }
    return a;
}

// Provide the address of the LinearArray and the address of the item to push.
bool LArray_Push(LArray *a, void *item)
{
    if (a == NULL || item == NULL)
    {
        fprintf(stderr, "The provided Linear Array is NULL.\n");
        return false;
    }

    if (a->count >= a->capacity)
    {
        if (!Collection_GrowBuffer(&a->items, &a->capacity, a->elem_bytes))
        {
            fprintf(stderr, "Failed to grow array!\n");
            return false;
        }
    }

    void *target = (char *)a->items + (a->count * a->elem_bytes);
    MemoryCopy(target, item, a->elem_bytes);
    a->count++;

    return true;
}

void *LArray_Pop(LArray *a, void *out_item)
{
    if (a == NULL || a->count == 0)
        return NULL;

    void *source = (char *)a->items + ((a->count - 1) * a->elem_bytes);

    if (out_item != NULL)
    {
        MemoryCopy(out_item, source, a->elem_bytes);
    }

    a->count--;
    return source;
}

// Increase the capacity of the array by the shared growth factor.
bool GrowLinearArray(LArray *a)
{
    if (a == NULL)
        return false;
    if (a->elem_bytes < 1)
    {
        fprintf(stderr, "Invalid element size %zu in GrowLinearArray! Must be greater than 0.\n", a->elem_bytes);
        return false;
    }

    if (!Collection_GrowBuffer(&a->items, &a->capacity, a->elem_bytes))
    {
        return false;
    }

    LOG_INFO("Linear Array grown to new capacity %d\n", a->capacity);
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

    return (char *)a->items + (index * a->elem_bytes);
}

void *LArray_GetCircular(LArray *a, int *index_tracker)
{
    if (a == NULL || a->count == 0 || index_tracker == NULL) return NULL;

    void *item = (char *)a->items + ((*index_tracker) * a->elem_bytes);
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

    void *target = (char *)a->items + (index * a->elem_bytes);

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

    void *target = (char *)a->items + (index * a->elem_bytes);

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

    void *item = (char *)a->items + (a->enumerator_index * a->elem_bytes);
    a->enumerator_index = (a->enumerator_index + 1) % a->count;

    return item;
}

// Dispose of the array and free its memory
void DisposeLArray(LArray *a)
{
    if (a == NULL)
        return;

    Collection_ClearItems(&a->items, &a->count, &a->capacity, a->elem_bytes);

    Deallocate((void **)&a, sizeof(LArray));
}

// Clears internal heap data buffers without freeing the header struct container itself
void ClearLArray(LArray *a)
{
    if (a == NULL)
        return;

    Collection_ClearItems(&a->items, &a->count, &a->capacity, a->elem_bytes);
}

bool LArray_ResizeAndReset(LArray *a, int new_capacity)
{
    if (a == NULL)
    {
        fprintf(stderr, "The provided Linear Array is NULL. Cannot resize.\n");
        return false;
    }

    size_t new_bytes = (size_t)new_capacity * a->elem_bytes;
    size_t old_bytes = (size_t)a->capacity * a->elem_bytes;

    void *temp_items = ReallocateBytes(a->items, old_bytes, new_bytes);
    if (temp_items == NULL && new_bytes > 0)
    {
        fprintf(stderr, "Failed to reallocate memory to new capacity %d!\n", new_capacity);
        return false;
    }

    a->items = temp_items;

    if (new_bytes > 0)
    {
        MemorySet(a->items, 0, new_bytes);
    }

    a->capacity = new_capacity;
    a->count = 0;

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
        fprintf(stderr, "The provided Linear Array has a capacity of %d and element size of %zu. Cannot reset.\n", a->capacity, a->elem_bytes);
        return false;
    }

    MemorySet(a->items, 0, bytes);
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
