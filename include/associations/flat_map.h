/**********************************************************************************************
*
QUEUE MODULE
*
**********************************************************************************************/
#ifndef FLAT_MAP_H
#define FLAT_MAP_H
#include <stddef.h>

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
// #define NEW_ARRAY(count, type) NewArray(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Individual slot node
typedef struct FlatMapIntEntry
{
    int key;       // Native integer ID key
    int value;     // Associated integer value
    bool occupied; // Slot state tracker
} FlatMapIntEntry;

// Master Hash Map structure
typedef struct FlatMapInt
{
    FlatMapIntEntry *slots; // An array of pointers to HashEntries
    int capacity;           // Total number of slots/buckets
    int count;              // Number of active items currently stored
} FlatMapInt;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
FlatMapInt *AllocFlatMapInt(int capacity);
bool FlatMapInt_InsertOrUpdate(FlatMapInt *m, int key, int value);
FlatMapInt MakeFlatMapInt(int capacity);
bool FlatMapInt_GetValue(FlatMapInt *m, int key, int *out_value);
bool FlatMapInt_GetKey(FlatMapInt *m, int value, int *out_key);
void DisposeFlatMapInt(FlatMapInt *m);
void ClearFlatMapInt(FlatMapInt *m);
// void* Enumerate(DynamicArray *da);
// void* ResetEnumerator(DynamicArray *da);

#endif