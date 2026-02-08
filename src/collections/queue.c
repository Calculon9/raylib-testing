#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "collections/queue.h"
#include "memory/cmemory.h"

// Create a new queue with specified element size and count
Queue NewQueue(int elemCount, size_t elemSize)
{
    Queue q;
    q.elemSize = elemSize;
    q.capacity = elemCount;
    q.count = 0;
    q.front = 0;
    q.rear = 0;
    q.items = allocate_collection(elemCount, elemSize);

    // Simple safety check
    if (q.items == NULL) {
        fprintf(stderr, "Failed to allocate memory for Queue!\n");
        q.capacity = 0; // Ensure nothing can be pushed
    }
    return q;
}

Queue *push(Queue *q, void *item)
{
    if (q == NULL || q->count >= q->capacity)
    {
        fprintf(stderr, "Queue is full! Cannot push new item.\n");
        return q; // Keep the return consistent with the signature
    }

    // 1. Calculate the address using the CURRENT rear (e.g., 0)
    // We cast it to a char * because the size of a char is guaranteed to be exactly 1 byte.
    // Otherwise pointer arithmetic would be scaled by the size of the type pointed to, which is not what we want here since we're treating it as a raw byte array.
    void *target = (char *)q->items + (q->rear * q->elemSize);
    
    // 2. Put the data there
    memcpy(target, item, q->elemSize);

    // 3. Now move the rear for the NEXT push
    // This correctly wraps around to 0 only AFTER the last slot is filled
    q->rear = (q->rear + 1) % q->capacity;
    
    q->count++;

    return q;
}

Queue *pop(Queue *q, void *outItem)
{
    if (q == NULL || q->count <= 0)
    {
        fprintf(stderr, "Queue is empty! Cannot pop item.\n");
        return q;
    }

    // Calculate the address using the current FRONT index
    void *source = (char *)q->items + (q->front * q->elemSize);
    
    // Copy the data out for the user
    if (outItem != NULL) {
        memcpy(outItem, source, q->elemSize);
    }

    // Update the front index (Wrap around if it hits capacity)
    q->front = (q->front + 1) % q->capacity;
    
    q->count--;

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
void dispose_queue(Queue *q)
{
    if (q->items != NULL)
    {
        deallocate_shallow(&q->items, q->capacity * q->elemSize);
        q->items = NULL;
    }
    q->count = 0;
    q->capacity = 0;
}