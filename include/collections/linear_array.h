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

// OWNERSHIP: Caller owns returned pointer (heap-allocated struct + buffer)
// Must call DisposeLArray() to free both struct and buffer
LArray *AllocLArray(int elem_count, size_t elem_bytes);

// OWNERSHIP: Caller owns returned struct (stack-allocated struct, heap buffer)
// Must call DisposeLArray(&arr) to free internal buffer before scope exit
LArray MakeLArray(int elem_count, size_t elem_bytes);

bool LArray_Push(LArray *la, void *item);
void *LArray_Pop(LArray *la, void *out_item);
void *LArray_Get(LArray *la, int index);
bool LArray_RemoveAt(LArray *la, int index);
bool LArray_SwapPopAt(LArray *a, int index);

// OWNERSHIP: Frees internal buffer (la->items)
// For AllocLArray: also frees struct pointer
// For MakeLArray: only frees buffer, caller's struct remains on stack
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

//----------------------------------------------------------------------------------
// Iterator Macros
//----------------------------------------------------------------------------------

// Iterate over all elements in an LArray with automatic bounds checking and null safety.
// Usage: LArray_ForEach(array, ItemType*, item_ptr) { /* use item_ptr */ }
#define LArray_ForEach(arr, item_type, item_name) \
    for (int _i##item_name = 0; \
         (arr) && _i##item_name < (arr)->count && ((item_name) = (item_type)LArray_Get((arr), _i##item_name)); \
         _i##item_name++)

// Iterate over unique pairs in an LArray (i, j) where j > i.
// Usage: LArray_ForEachPair(array, i, j) { ItemType* a = LArray_Get(array, i); ItemType* b = LArray_Get(array, j); }
#define LArray_ForEachPair(arr, i_name, j_name) \
    for (int i_name = 0; (arr) && i_name < (arr)->count; i_name++) \
        for (int j_name = i_name + 1; j_name < (arr)->count; j_name++)

// Check if array is valid and non-empty.
// Usage: if (LArray_IsValid(array)) { /* proceed */ }
#define LArray_IsValid(arr) ((arr) && (arr)->count > 0)

// Get element count safely (returns 0 if array is null).
// Usage: int count = LArray_Count(array);
#define LArray_Count(arr) ((arr) ? (arr)->count : 0)

#endif
