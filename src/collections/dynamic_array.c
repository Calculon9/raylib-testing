#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "collections/dynamic_array.h"
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
bool GrowDynamicArray(DynamicArray *da);


// Create a new queue with specified element size and count
DynamicArray *NewDynamicArray(int elemCount, size_t elemSize)
{
    // Allocate memory for the DynamicArray struct itself
    DynamicArray *da = AllocateBytes(sizeof(DynamicArray));

    // Allocate memory for the internal Collection struct 
    da->coll = *NewCollection(elemCount, elemSize);
    
    // Simple safety check
    if (da == NULL)// || da->coll == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for Dynamic Array!\n");
        //da->coll = NULL; // Ensure nothing can done on the collection if allocation failed
    }
    return da;
}

// Provide the address of the DynamicArray and the address of the item to push.
DynamicArray *Array_Push(DynamicArray *da, void *item)
{   
    if (da == NULL)
    {
        fprintf(stderr, "The provided DynamicArray is NULL.\n");
        return NULL; 
    }

    // 1. Check for growth FIRST
    // We use -> to access the REAL data, not a copy
    if (da->coll.count >= da->coll.capacity)
    {
        if (!GrowDynamicArray(da))
        {
            fprintf(stderr, "Failed to grow array!\n");
            return da;
        }
    }

    // 2. Calculate the target address using the ACTUAL live data
    // Note: We use da->coll.items because GrowDynamicArray might have changed it!
    void *target = (char *)da->coll.items + (da->coll.rear * da->coll.elemSize);

    // 3. Copy the data
    memcpy(target, item, da->coll.elemSize);

    // 4. Update the REAL state
    da->coll.rear = (da->coll.rear + 1) % da->coll.capacity;
    da->coll.count++;

    return da;
}

DynamicArray *Array_Pop(DynamicArray *da, void *outItem)
{
    if (da == NULL)// || da->coll == NULL)
    {
        fprintf(stderr, "The provided collection is NULL. Cannot pop item.\n");
        return da; // Keep the return consistent with the signature
    }
    if (da->coll.count <= 0)
    {
        fprintf(stderr, "Dynamic Array is empty! Cannot pop item.\n");
        return da;
    }

    // Calculate the address using the current FRONT index
    void *source = (char *)da->coll.items + (da->coll.front * da->coll.elemSize);

    // Copy the data out for the user
    if (outItem != NULL)
    {
        memcpy(outItem, source, da->coll.elemSize);
    }

    // Update the front index (Wrap around if it hits capacity)
    da->coll.front = (da->coll.front + 1) % da->coll.capacity;
    da->coll.enumeratorIndex = da->coll.front; // Reset enumerator to the new front after a pop
    da->coll.count--;

    return da;
}

// Increase the capacity of the array by a specified factor (e.g., double the capacity)
bool GrowDynamicArray(DynamicArray *da) 
{
    return GrowCollection(&da->coll);
    // // 1. Calculate new capacity (Double it)
    // int newCapacity = (da->coll->capacity == 0) ? 4 : da->coll->capacity * 2;
    
    // // 2. Use a TEMPORARY pointer for safety
    // void *newItems = realloc(da->coll->items, newCapacity * da->coll->elemSize);

    // if (newItems == NULL) 
    // {
    //     fprintf(stderr, "Critical: Failed to grow array!\n");
    //     return false; // Old data is still safe in da->items
    // }

    // // 3. Success! Update the metadata
    // printf("Array capacity increased from %d to %d.\n", da->coll->capacity, newCapacity);
    // da->coll->items = newItems;
    // da->coll->capacity = newCapacity;
    // return true;
}

// Dispose of the array and free its memory
void DisposeArray(DynamicArray *da)
{
    if(da == NULL) return;
    DisposeCollection(&da->coll);
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