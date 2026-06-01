/**********************************************************************************************
*
QUEUE MODULE
*
**********************************************************************************************/
#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H
#include <stddef.h>
//#include "collections/collection.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define NEW_DYNAMIC_ARRAY(count, type) NewDArray(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// typedef struct DynamicArray
// {
//     Collection coll; // Collection struct to hold the actual data and metadata
// } DynamicArray;

typedef struct DArray
{
    void *items;          // Raw memory block
    size_t elem_bytes;      // Size of one element (e.g., sizeof(int))
    int capacity;         // Total space allocated
    int count;            // Number of items currently stored
    int front;            // Index of the front item (for circular buffer implementation)
    int rear;             // Index of the rear item (for circular buffer implementation)
    int enumeratorIndex;  // Index used for enumeration
    int enumerationCount; // Count of items enumerated so far (for safety check)
} DArray;

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
DArray *AllocDArray(int elem_count, size_t elem_bytes);
DArray MakeDArray(int elem_count, size_t elem_bytes);
DArray *DArray_Push(DArray *da, void *item);
DArray *DArray_Pop(DArray *da, void *outItem);
void *Enumerate(DArray *da);
void ResetEnumerator(DArray *da);
// size_t GetElementCount(DynamicArray *da);
void DisposeDArray(DArray *da);
void ClearDArray(DArray *da);

// Vector3 vector3_sum_array (Vector3 *array, size_t count);
// Vector3* vector3_sum_array_dynamic(Vector3 *array, size_t count);

#endif