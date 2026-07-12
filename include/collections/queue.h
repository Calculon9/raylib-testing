/**********************************************************************************************
*
QUEUE MODULE
*
**********************************************************************************************/
#ifndef QUEUE_H
#define QUEUE_H
#include <stddef.h>

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define NEW_QUEUE(count, type) MakeQueue(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct Queue
{
    void *items;          // Raw memory block
    size_t elem_bytes;      // Size of one element (e.g., sizeof(int))
    int capacity;         // Total space allocated
    int count;            // Number of items currently stored
    int front;            // Index of the front item (for circular buffer implementation)
    int rear;             // Index of the rear item (for circular buffer implementation)
} Queue;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Queue *AllocQueue(int elem_count, size_t elem_bytes);
Queue MakeQueue(int elem_count, size_t elem_bytes);
Queue *Queue_Push(Queue *q, void *item);
Queue *Queue_Pop(Queue *q, void *outItem);
void DisposeQueue(Queue *q);

// Vector3 vector3_sum_array (Vector3 *array, size_t count);
// Vector3* vector3_sum_array_dynamic(Vector3 *array, size_t count);

#endif