/**********************************************************************************************
*
QUEUE MODULE
*
**********************************************************************************************/
#ifndef FLAT_MAP_H
#define FLAT_MAP_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

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
    uint8_t state; // Slot state tracker (empty/occupied/deleted)
} FlatMapIntEntry;

typedef enum FlatMapSlotState
{
    FLAT_MAP_SLOT_EMPTY = 0,
    FLAT_MAP_SLOT_OCCUPIED = 1,
    FLAT_MAP_SLOT_DELETED = 2
} FlatMapSlotState;

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
bool FlatMapInt_DeactivateSlot(FlatMapInt *m, int key);
void DisposeFlatMapInt(FlatMapInt *m);
void ClearFlatMapInt(FlatMapInt *m);
void ResetFlatMapInt(FlatMapInt *m);
// void* Enumerate(DynamicArray *da);
// void* ResetEnumerator(DynamicArray *da);

#endif
