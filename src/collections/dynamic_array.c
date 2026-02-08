#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "collections/dynamic_array.h"
#include "memory/cmemory.h"

// Create a new queue with specified element size and count
DynamicArray *NewDynamicArray(int elemCount, size_t elemSize)
{
    DynamicArray *da = allocate_bytes(sizeof(DynamicArray));
    da->elemSize = elemSize;
    da->capacity = elemCount;
    da->count = 0;
    da->front = 0; // Front points to the next item to be popped
    da->rear = 0;  // Rear points to the next empty slot where data will be written
    da->enumeratorIndex = 0;
    da->items = allocate_collection(elemCount, elemSize);

    // Simple safety check
    if (da->items == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for Dynamic Array!\n");
        da->capacity = 0; // Ensure nothing can be pushed
    }
    return da;
}

DynamicArray *pushArray(DynamicArray *da, void *item)
{
    if (da == NULL || da->count >= da->capacity)
    {
        fprintf(stderr, "Array is full! Cannot push new item.\n");
        return da; // Keep the return consistent with the signature
    }

    // 1. Calculate the address using the CURRENT rear (e.g., 0)
    // We cast it to a char * because the size of a char is guaranteed to be exactly 1 byte.
    // Otherwise pointer arithmetic would be scaled by the size of the type pointed to, which is not what we want here since we're treating it as a raw byte array.
    void *target = (char *)da->items + (da->rear * da->elemSize);

    // 2. Put the data there
    memcpy(target, item, da->elemSize);

    // 3. Now move the rear for the NEXT push
    // This correctly wraps around to 0 only AFTER the last slot is filled
    da->rear = (da->rear + 1) % da->capacity;

    da->count++;

    return da;
}

DynamicArray *popArray(DynamicArray *da, void *outItem)
{
    if (da == NULL || da->count <= 0)
    {
        fprintf(stderr, "Dynamic Array is empty! Cannot pop item.\n");
        return da;
    }

    // Calculate the address using the current FRONT index
    void *source = (char *)da->items + (da->front * da->elemSize);

    // Copy the data out for the user
    if (outItem != NULL)
    {
        memcpy(outItem, source, da->elemSize);
    }

    // Update the front index (Wrap around if it hits capacity)
    da->front = (da->front + 1) % da->capacity;

    da->enumeratorIndex = da->front; // Reset enumerator to the new front after a pop

    da->count--;

    return da;
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
void *Enumerate(DynamicArray *da)
{
    // Rear points to the next empty slot where data will be written, so we want to stop enumerating once we hit rear, not capacity
    if (da == NULL) return NULL;
    if (da->enumeratorIndex == da->rear)
    {
        ResetEnumerator(da); // Reset enumerator for the next time we want to enumerate
        return NULL;
    }

    // Calculate the address of the current enumerator index
    void *item = (char *)da->items + (da->enumeratorIndex * da->elemSize);

    // Move the enumerator index to the next item for the next call
    da->enumeratorIndex = (da->enumeratorIndex + 1) % da->capacity;

    return item;
}

void* ResetEnumerator(DynamicArray *da) {
    // Reset enumerator to the front
    if (da != NULL) {
        da->enumeratorIndex = da->front;
    } 
}

// Dispose of the array and free its memory
void dispose_array(DynamicArray *da)
{
    if (da->items != NULL)
    {
        deallocate_shallow(&da->items, da->capacity * da->elemSize);
        da->items = NULL;
    }
    da->count = 0;
    da->capacity = 0;
}