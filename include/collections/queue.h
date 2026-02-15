/**********************************************************************************************
*
QUEUE MODULE
*
**********************************************************************************************/
#ifndef QUEUE_H
#define QUEUE_H
#include <stddef.h>
#include "collections/collection.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define NEW_QUEUE(count, type) NewQueue(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct Queue{
    Collection *coll;  // Collection struct to hold the actual data and metadata
} Queue;


//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Queue *NewQueue(int elemCount, size_t elemSize);
Queue *Queue_Push(Queue *q, void *item);
Queue *Queue_Pop(Queue *q, void *outItem);
void DisposeQueue(Queue *q);

//Vector3 vector3_sum_array (Vector3 *array, size_t count);
//Vector3* vector3_sum_array_dynamic(Vector3 *array, size_t count);

#endif