/**********************************************************************************************
*
QUEUE MODULE
*
**********************************************************************************************/
#ifndef ARRAY_H
#define ARRAY_H
#include <stddef.h>
#include "collections/collection.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define NEW_ARRAY(count, type) NewArray(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// typedef struct DynamicArray {
//     Collection coll;  // Collection struct to hold the actual data and metadata
// } DynamicArray;

// typedef struct {
//     void *items;       // Raw memory block
//     size_t elemSize;   // Size of one element (e.g., sizeof(int))
//     int capacity;      // Total space allocated
//     int count;         // Number of items currently stored
//     int front;         // Index of the front item (for circular buffer implementation)
//     int rear;          // Index of the rear item (for circular buffer implementation)
// } Enumerator;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
void *NewArray(int elemCount, size_t elemSize);
void *Array_Push(void *array, void *item);
void *Array_Pop(void *array, void *out_item);
//void* Enumerate(DynamicArray *da);
//void* ResetEnumerator(DynamicArray *da);
//size_t GetElementCount(DynamicArray *da);
//void DisposeArray(DynamicArray *da);

//Vector3 vector3_sum_array (Vector3 *array, size_t count);
//Vector3* vector3_sum_array_dynamic(Vector3 *array, size_t count);

#endif