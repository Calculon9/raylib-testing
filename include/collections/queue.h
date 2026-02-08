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
#define NEW_QUEUE(count, type) NewQueue(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct {
    void *items;       // Raw memory block
    size_t elemSize;   // Size of one element (e.g., sizeof(int))
    int capacity;      // Total space allocated
    int count;         // Number of items currently stored
    int front;         // Index of the front item (for circular buffer implementation)
    int rear;          // Index of the rear item (for circular buffer implementation)
} Queue;


//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Queue NewQueue(int elemCount, size_t elemSize);
Queue *push(Queue *q, void *item);
Queue *pop(Queue *q, void *outItem);
void dispose_queue(Queue *q);

//Vector3 vector3_sum_array (Vector3 *array, size_t count);
//Vector3* vector3_sum_array_dynamic(Vector3 *array, size_t count);

#endif