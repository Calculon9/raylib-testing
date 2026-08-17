#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "collections/dynamic_array.h"
#include "collections/collection_internal.h"
#include "memory/cmemory.h"
#include "common/common.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
bool GrowDynamicArray(DArray *a);

// Create a new dynamic array with specified element size and count
DArray *AllocDArray(int elem_count, size_t elem_bytes)
{
    // Allocate memory for the DynamicArray struct itself
    DArray *a = AllocateBytes(sizeof(DArray));
    if (a == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for Dynamic Array!\n");
        return NULL;
    }
    a->elem_bytes = elem_bytes;
    a->capacity = elem_count;
    a->count = 0;
    a->enumeratorIndex = 0;
    a->enumerationCount = 0;
    a->items = Collection_AllocItemsBuffer(elem_count, elem_bytes, "Dynamic Array");
    if (a->items == NULL)
    {
        a->capacity = 0;
    }
    return a;
}

DArray MakeDArray(int elem_count, size_t elem_bytes)
{
    // Allocate memory for the DynamicArray struct itself
    DArray a = {0};
    a.elem_bytes = elem_bytes;
    a.capacity = elem_count;
    a.count = 0;
    a.enumeratorIndex = 0;
    a.enumerationCount = 0;
    a.items = Collection_AllocItemsBuffer(elem_count, elem_bytes, "Dynamic Array");

    // Simple safety check
    if (a.items == NULL)
    {
        a.capacity = 0; // Ensure nothing can be pushed
    }
    return a;
}

void *DArray_Get(DArray *a, int index)
{
    if (a == NULL) return NULL;
    if (index < 0 || index >= a->count)
    {
        fprintf(stderr, "Index %d out of bounds for Circular DArray of count %d!\n", index, a->count);
        return NULL;
    }

    // Map the logical index to the physical wrapped ring position
    int actual_physical_index = (a->front + index) % a->capacity;
    
    return (char *)a->items + (actual_physical_index * a->elem_bytes);
}

// Provide the address of the DynamicArray and the address of the item to push.
DArray *DArray_Push(DArray *a, void *item)
{
    if (a == NULL || item == NULL)
    {
        fprintf(stderr, "The provided DynamicArray or element is NULL.\n");
        return NULL;
    }

    if (a->count >= a->capacity) // Check for growth FIRST
    {
        if (!GrowDynamicArray(a))
        {
            fprintf(stderr, "Failed to grow array!\n");
            return a;
        }
    }

    void *target = (char *)a->items + (a->rear * a->elem_bytes);
    MemoryCopy(target, item, a->elem_bytes);

    // Upate state
    a->rear = (a->rear + 1) % a->capacity;
    a->count++;

    return a;
}

DArray *DArray_Pop(DArray *a, void *outItem)
{
    if (a == NULL) // || a->coll == NULL)
    {
        fprintf(stderr, "The provided collection is NULL. Cannot pop item.\n");
        return a; // Keep the return consistent with the signature
    }
    if (a->count <= 0)
    {
        fprintf(stderr, "Dynamic Array is empty! Cannot pop item.\n");
        return a;
    }

    // Calculate the address using the current FRONT index
    void *source = (char *)a->items + (a->front * a->elem_bytes);

    // Copy the ata out for the user
    if (outItem != NULL)
    {
        MemoryCopy(outItem, source, a->elem_bytes);
    }

    // Zero-out the popped slot so ghost data doesn't persist in memory
    MemorySet(source, 0, a->elem_bytes);

    // Upate the front index (Wrap around if it hits capacity)
    a->front = (a->front + 1) % a->capacity;
    a->enumeratorIndex = a->front; // Reset enumerator to the new front after a pop
    a->count--;

    return a;
}

// Increase the capacity of the array by a specified factor (e.g., double the capacity)
bool GrowDynamicArray(DArray *a)
{
    if (a == NULL)
        return false;

    if (a->elem_bytes < 1)
    {
        fprintf(stderr, "Invalid element size %zu in GrowDynamicArray!\n", a->elem_bytes);
        return false;
    }

    // Explicitly guarantee capacity expands significantly
    int new_capacity = Collection_CalcGrowthCapacity(a->capacity);

    // Allocate the fresh buffer block safely
    void *new_items = AllocateBytes(new_capacity * a->elem_bytes);
    if (new_items == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for growing array to new capacity %d!\n", new_capacity);
        return false;
    }

    // SAFELY UNWRAP CIRCULAR DATA:
    // Instead of doing error-prone slice math with memcpy, march through the active count elements
    if (a->items != NULL)
    {
        int current_index = a->front;
        for (int i = 0; i < a->count; i++)
        {
            void *source = (char *)a->items + (current_index * a->elem_bytes);
            void *dest = (char *)new_items + (i * a->elem_bytes);

            MemoryCopy(dest, source, a->elem_bytes);

            // Advance the source pointer index along the old ring layout
            current_index = (current_index + 1) % a->capacity;
        }

        // Fix: Use your custom tracking Deallocate tool to match AllocateBytes!
        size_t old_bytes = a->capacity * a->elem_bytes;
        Deallocate((void **)&a->items, old_bytes);
    }

    // Re-anchor state cleanly
    a->items = new_items;
    a->capacity = new_capacity;
    a->front = 0;
    a->rear = a->count;

    LOG_INFO("Dynamic Array grown to new capacity %d\n", new_capacity);
    return true;
}

// Dispose of the array and free its memory
void DisposeDArray(DArray *a)
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

    Deallocate((void **)&a, sizeof(DArray));
}

// Clears internal heap ata buffers without freeing the header struct container itself
void ClearDArray(DArray *a)
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

void *Enumerate(DArray *a)
{
    // Rear points to the next empty slot where ata will be written, so we want to stop enumerating once we hit rear, not capacity
    if (a == NULL || a->count == 0)
        return NULL;

    // Reset if we've enumerated all items (safety check using enumerationCount to prevent infinite loops in case of bugs)
    if (a->enumerationCount == a->count)
    {
        ResetEnumerator(a); // Reset enumerator for the next time we want to enumerate
        return NULL;
    }

    // Calculate the address of the current enumerator index
    void *item = (char *)a->items + (a->enumeratorIndex * a->elem_bytes);

    // Move the enumerator index to the next item for the next call and increment the enumeration count
    a->enumeratorIndex = (a->enumeratorIndex + 1) % a->capacity;
    a->enumerationCount++;

    return item;
}

void ResetEnumerator(DArray *a)
{
    // Reset enumerator to the front
    if (a != NULL)
    {
        a->enumeratorIndex = a->front;
        a->enumerationCount = 0; // Reset the progress tracker!
    }
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
//     memcpy(outItem, source, q->elem_bytes);

//     // Shift remaining items forward to fill the gap left by the popped item
//     //--void *memmove(void *dest, const void *src, size_t n)--
//     // dest: Where you want the ata to go.
//     // src: Where the ata is currently.
//     // n: How many bytes to move
//     //memmove(q->items, (char *)q->items + q->elem_bytes, (q->count - 1) * q->elem_bytes);

//     q->front = (q->front + 1) % q->capacity; // Upate front index for circular buffer
//     q->count--;

//     return q;
// }

//
// void *Enumerate(DynamicArray *a)
// {
//     // Rear points to the next empty slot where ata will be written, so we want to stop enumerating once we hit rear, not capacity
//     if (a == NULL) return NULL;
//     if (a->enumeratorIndex == a->rear)
//     {
//         ResetEnumerator(a); // Reset enumerator for the next time we want to enumerate
//         return NULL;
//     }

//     // Calculate the address of the current enumerator index
//     void *item = (char *)a->items + (a->enumeratorIndex * a->elem_bytes);

//     // Move the enumerator index to the next item for the next call
//     a->enumeratorIndex = (a->enumeratorIndex + 1) % a->capacity;

//     return item;
// }

// void *ResetEnumerator(DynamicArray *a) {
//     // Reset enumerator to the front
//     if (a != NULL) {
//         a->enumeratorIndex = a->front;
//     }
// }

// // Get the current number of elements in the array
// size_t GetElementCount(DynamicArray *a)
// {
//     if (a == NULL) return 0;
//     return a->count;
// }
