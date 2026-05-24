/**********************************************************************************************
*
MEMORY MANAGEMENT MODULE
*
**********************************************************************************************/
#ifndef CMEMORY_H
#define CMEMORY_H
#include <stddef.h>

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
//#define NEW_ARRAY(count, type) AllocateArray(count, sizeof(type))
//#define NEW_COLLECTION(count, type) AllocateCollection(count, sizeof(type))
//#define FREE_ARRAY_SHALLOW(ptr, type, count) DeallocateArrayShallow((void **)&ptr, count, sizeof(type))
#define FREE_MEM_SHALLOW(ptr, type) DeallocateShallow((void **)&ptr, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

void *AllocateArray(size_t element_count, size_t element_bytes);
void DeallocateArrayShallow(void **array, size_t element_count, size_t element_bytes);
void ValidateAllocation(void *pMemory, size_t bytes);
size_t DeallocateShallow(void **ptr, size_t bytes);
void *AllocateCollection(size_t element_count, size_t element_bytes);
void *AllocateBytes(size_t bytes);
// void deallocate_deep(void **ptr, size_t bytes);
size_t CurrBytesAllocated();

// Returns the cumulative memory freed since the program started. Useful for tracking total deallocations over time.
size_t TotalBytesFreed();
size_t TotalBytesAllocated();

#endif // CMEMORY_H