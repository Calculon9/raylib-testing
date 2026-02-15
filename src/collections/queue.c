#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "collections/queue.h"
#include "memory/cmemory.h"

// Create a new queue with specified element size and count
Queue *NewQueue(int elemCount, size_t elemSize)
{
    // Allocate memory for the Queue struct itself
    Queue *q = AllocateBytes(sizeof(Queue));

    // Allocate memory for the internal Collection struct 
    q->coll = NewCollection(elemCount, elemSize);

    // Simple safety check
    if (q == NULL || q->coll == NULL) 
    {
        fprintf(stderr, "Failed to allocate memory for Queue!\n");
        q->coll = NULL; // Ensure nothing can done on the collection if allocation failed
    }
    return q;
}

Queue *Queue_Push(Queue *q, void *item)
{
    if (q == NULL || q->coll == NULL || q->coll->count >= q->coll->capacity)
    {
        fprintf(stderr, "Queue is full! Cannot push new item.\n");
        return q; // Keep the return consistent with the signature
    }

    // 1. Calculate the address using the CURRENT rear (e.g., 0)
    // We cast it to a char * because the size of a char is guaranteed to be exactly 1 byte.
    // Otherwise pointer arithmetic would be scaled by the size of the type pointed to, which is not what we want here since we're treating it as a raw byte array.
    void *target = (char *)q->coll->items + (q->coll->rear * q->coll->elemSize);
    
    // 2. Put the data there
    memcpy(target, item, q->coll->elemSize);

    // 3. Now move the rear for the NEXT push
    // This correctly wraps around to 0 only AFTER the last slot is filled
    q->coll->rear = (q->coll->rear + 1) % q->coll->capacity;
    
    q->coll->count++;

    return q;
}

Queue *Queue_Pop(Queue *q, void *outItem)
{
    if (q == NULL || q->coll == NULL || q->coll->count <= 0)
    {
        fprintf(stderr, "Queue is empty! Cannot pop item.\n");
        return q;
    }

    // Calculate the address using the current FRONT index
    void *source = (char *)q->coll->items + (q->coll->front * q->coll->elemSize);
    
    // Copy the data out for the user
    if (outItem != NULL) {
        memcpy(outItem, source, q->coll->elemSize);
    }

    // Update the front index (Wrap around if it hits capacity)
    q->coll->front = (q->coll->front + 1) % q->coll->capacity;
    
    q->coll->count--;

    return q;
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

// Dispose of the queue and free its memory
void DisposeQueue(Queue *q)
{
    if(q == NULL) return;
    DisposeCollection(q->coll);
}