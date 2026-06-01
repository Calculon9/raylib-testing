#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "collections/dynamic_array.h"
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
bool GrowDynamicArray(DArray *a);


// Create a new dynamic array with specified element size and count
DArray *AllocDArray(int elem_count, size_t elem_bytes)
{
    // Allocate memory for the DynamicArray struct itself
    DArray *a = AllocateBytes(sizeof(DArray));
    a->elem_bytes = elem_bytes;
    a->capacity = elem_count;
    a->count = 0;
    a->enumeratorIndex = 0;
    a->enumerationCount = 0;
    a->items = AllocateBytes(elem_bytes * elem_count);
    
    // Simple safety check
    if (a == NULL)// || a->coll == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for Dynamic Array!\n");
        a->items = NULL; // Ensure nothing can done on the collection if allocation failed
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
DArray *DArray_Push(DArray *a, void *item)
{   
    if (a == NULL)
    {
        fprintf(stderr, "The provided DynamicArray is NULL.\n");
        return NULL; 
    }

    // 1. Check for growth FIRST
    // We use -> to access the REAL ata, not a copy
    if (a->count >= a->capacity)
    {
        if (!GrowDynamicArray(a))
        {
            fprintf(stderr, "Failed to grow array!\n");
            return a;
        }
    }

    // 2. Calculate the target address using the ACTUAL live ata
    // Note: We use a->coll.items because GrowDynamicArray might have changed it!
    void *target = (char *)a->items + (a->rear * a->elem_bytes);

    // 3. Copy the ata
    memcpy(target, item, a->elem_bytes);

    // 4. Upate the REAL state
    a->rear = (a->rear + 1) % a->capacity;
    a->count++;

    return a;
}

DArray *DArray_Pop(DArray *a, void *outItem)
{
    if (a == NULL)// || a->coll == NULL)
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
        memcpy(outItem, source, a->elem_bytes);
    }

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
        fprintf(stderr, "Invalid element size %zu in GrowCollection! Must be greater than 0.\n", a->elem_bytes);
        return false;
    }

    int newCapacity = (a->capacity == 0) ? 4 : a->capacity * 2;
    // Note: We use malloc here because we are manually re-ordering,
    // so we don't need realloc to copy the "old" scrambled order.
    void *newItems = AllocateCollection(newCapacity, a->elem_bytes);

    if (newItems == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for growing collection to new capacity %d!\n", newCapacity);
        return false;
    }

    if (a->count > 0)
    {
        // 1. Copy from 'front' to the physical end of the old buffer
        int firstPartCount = a->capacity - a->front;
        memcpy(newItems, (char *)a->items + (a->front * a->elem_bytes), firstPartCount * a->elem_bytes);

        // 2. Copy the wrapped part (from 0 to rear) to immediately after the first part
        if (a->front > 0) // Only need this if we actually wrapped
        {
            memcpy((char *)newItems + (firstPartCount * a->elem_bytes), a->items, a->rear * a->elem_bytes);
        }
    }

    free(a->items); // Get rid of the old, scrambled buffer

    a->items = newItems;
    a->capacity = newCapacity;
    a->front = 0;       // Start is now at the beginning
    a->rear = a->count; // Next empty slot is at the end of the ata

    return true;
}

// Dispose of the array and free its memory
void DisposeDArray(DArray *a)
{
    if(a == NULL) return;
    if (a->items != NULL)
    {
        DeallocateShallow(&a->items, a->capacity * a->elem_bytes);
        a->items = NULL;
    }
    a->count = 0;
    a->capacity = 0;

    DeallocateShallow((void **)&a, sizeof(DArray));
}

// Clears internal heap ata buffers without freeing the header struct container itself
void ClearDArray(DArray *a)
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