/**********************************************************************************************
*
QUEUE MODULE
*
**********************************************************************************************/
#ifndef COLLECTION_H
#define COLLECTION_H
#include <stddef.h>

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define NEW_COLLECTION(count, type) NewCollection(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct Collection {
    void *items;       // Raw memory block
    size_t elemSize;   // Size of one element (e.g., sizeof(int))
    int capacity;      // Total space allocated
    int count;         // Number of items currently stored
    int front;         // Index of the front item (for circular buffer implementation)
    int rear;          // Index of the rear item (for circular buffer implementation)
    int enumeratorIndex; // Index used for enumeration
    int enumerationCount; // Count of items enumerated so far (for safety check)
} Collection;

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
Collection *NewCollection(int elemCount, size_t elemSize);
void* Enumerate(Collection *c);
void* ResetEnumerator(Collection *c);
size_t GetElementCount(Collection *c);
void DisposeCollection(Collection *c);
bool GrowCollection(Collection *c);

#endif