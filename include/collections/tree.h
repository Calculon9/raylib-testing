/**********************************************************************************************
*
QUEUE MODULE
*
**********************************************************************************************/
#ifndef TREE_H
#define TREE_H
#include <stddef.h>

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
// #define NEW_ARRAY(count, type) NewArray(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Simple linear array
typedef struct Tree
{
    size_t elem_bytes;    // Size of one element (e.g., sizeof(int))
    void *root;           // Raw memory block
    int count;            // Number of items currently stored
    int enumeratorIndex;  // Index used for enumeration
    int enumerationCount; // Count of items enumerated so far (for safety check)
} Tree;

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

// void* Enumerate(DynamicArray *da);
// void* ResetEnumerator(DynamicArray *da);
// size_t GetElementCount(DynamicArray *da);
// void DisposeArray(DynamicArray *da);

// Vector3 vector3_sum_array (Vector3 *array, size_t count);
// Vector3* vector3_sum_array_dynamic(Vector3 *array, size_t count);

#endif