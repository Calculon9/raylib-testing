/**********************************************************************************************
*
QUEUE MODULE
*
**********************************************************************************************/
#ifndef LINEAR_ARRAY_H
#define LINEAR_ARRAY_H
#include <stddef.h>

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
//#define NEW_ARRAY(count, type) NewArray(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Simple linear array
typedef struct LArray
{
    size_t elem_bytes;   // Size of one element (e.g., sizeof(int))
    void *items;       // Raw memory block
    int capacity;      // Total space allocated
    int count;         // Number of items currently stored
    int enumerator_index; // Index used for enumeration
    //int enumerationCount; // Count of items enumerated so far (for safety check)
} LArray;

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
LArray *AllocLArray(int elem_count, size_t elem_bytes);
LArray MakeLArray(int elem_count, size_t elem_bytes);
bool LArray_Push(LArray *la, void *item);
void *LArray_Pop(LArray *la, void *out_item);
void *LArray_Get(LArray *la, int index);
bool LArray_RemoveAt(LArray *la, int index);
bool LArray_SwapPopAt(LArray *a, int index);
void *LArray_CircularEnumerate(LArray *a);
void *LArray_GetCircular(LArray *a, int *index_tracker);
void DisposeLArray(LArray *la);
void ClearLArray(LArray *la);
bool LArray_Reset(LArray *a);
bool LArray_ResizeAndReset(LArray *a, int new_capacity);
// void* Enumerate(DynamicArray *da);
// void* ResetEnumerator(DynamicArray *da);
// size_t GetElementCount(DynamicArray *da);
// void DisposeArray(DynamicArray *da);

// Vector3 vector3_sum_array (Vector3 *array, size_t count);
// Vector3* vector3_sum_array_dynamic(Vector3 *array, size_t count);

#endif