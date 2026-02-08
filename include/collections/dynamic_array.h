/**********************************************************************************************
*
QUEUE MODULE
*
**********************************************************************************************/
#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H
#include <stddef.h>

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define NEW_DYNAMIC_ARRAY(count, type) NewDynamicArray(count, sizeof(type))

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
    int enumeratorIndex; // Index used for enumeration
} DynamicArray;

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
DynamicArray *NewDynamicArray(int elemCount, size_t elemSize);
DynamicArray *pushArray(DynamicArray *da, void *item);
DynamicArray *popArray(DynamicArray *da, void *outItem);
void* Enumerate(DynamicArray *da);
void* ResetEnumerator(DynamicArray *da);
void dispose_array(DynamicArray *da);

//Vector3 vector3_sum_array (Vector3 *array, size_t count);
//Vector3* vector3_sum_array_dynamic(Vector3 *array, size_t count);

#endif