#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "collections/queue.h"
#include "memory/cmemory.h"

// Create a new queue with specified element size and count
Queue *AllocQueue(int elem_count, size_t elem_bytes)
{
    // Allocate memory for the Queue struct itself
    Queue *q = AllocateBytes(sizeof(Queue));
    q->elem_bytes = elem_bytes;
    q->capacity = elem_count;
    q->count = 0;
    q->enumeratorIndex = 0;
    q->enumerationCount = 0;
    q->items = AllocateBytes(elem_bytes * elem_count);

    // Simple safety check
    if (q == NULL || q->items == NULL) 
    {
        fprintf(stderr, "Failed to allocate memory for Queue!\n");
        q->items = NULL; // Ensure nothing can done on the collection if allocation failed
    }
    return q;
}

Queue MakeQueue(int elem_count, size_t elem_bytes) 
{
    // Allocate memory for the DynamicArray struct itself
    Queue a = {0};
    a.elem_bytes = elem_bytes;
    a.capacity = elem_count;
    a.count = 0;
    a.enumeratorIndex = 0;
    a.enumerationCount = 0;
    a.items = AllocateBytes(elem_bytes * elem_count);

    // Simple safety check
    if (a.items == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for Queue!\n");
        a.capacity = 0; // Ensure nothing can be pushed
        return a;
    }
    return a;
}

Queue *Queue_Push(Queue *q, void *item)
{
    if (q == NULL || q->items == NULL || q->count >= q->capacity)
    {
        fprintf(stderr, "Queue is full! Cannot push new item.\n");
        return q; // Keep the return consistent with the signature
    }

    // 1. Calculate the address using the CURRENT rear (e.g., 0)
    // We cast it to a char * because the size of a char is guaranteed to be exactly 1 byte.
    // Otherwise pointer arithmetic would be scaled by the size of the type pointed to, which is not what we want here since we're treating it as a raw byte array.
    void *target = (char *)q->items + (q->rear * q->elem_bytes);
    
    // 2. Put the data there
    memcpy(target, item, q->elem_bytes);

    // 3. Now move the rear for the NEXT push
    // This correctly wraps around to 0 only AFTER the last slot is filled
    q->rear = (q->rear + 1) % q->capacity;
    
    q->count++;

    return q;
}

Queue *Queue_Pop(Queue *q, void *outItem)
{
    if (q == NULL || q->items == NULL || q->count <= 0)
    {
        fprintf(stderr, "Queue is empty! Cannot pop item.\n");
        return q;
    }

    // Calculate the address using the current FRONT index
    void *source = (char *)q->items + (q->front * q->elem_bytes);
    
    // Copy the data out for the user
    if (outItem != NULL) {
        memcpy(outItem, source, q->elem_bytes);
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
//     memcpy(outItem, source, q->elem_bytes);

//     // Shift remaining items forward to fill the gap left by the popped item
//     //--void *memmove(void *dest, const void *src, size_t n)--
//     // dest: Where you want the data to go.
//     // src: Where the data is currently.
//     // n: How many bytes to move
//     //memmove(q->items, (char *)q->items + q->elem_bytes, (q->count - 1) * q->elem_bytes);

//     q->front = (q->front + 1) % q->capacity; // Update front index for circular buffer
//     q->count--;

//     return q;
// }

// Dispose of the queue and free its memory
void DisposeQueue(Queue *q)
{
    if(q == NULL) return;

    if (q->items != NULL)
    {
        DeallocateShallow(&q->items, q->capacity * q->elem_bytes);
        q->items = NULL;
    }
    q->count = 0;
    q->capacity = 0;

    DeallocateShallow((void **)&q, sizeof(Queue));
}