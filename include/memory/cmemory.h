/**********************************************************************************************
*
MEMORY MANAGEMENT MODULE
*
**********************************************************************************************/
#ifndef CMEMORY_H
#define CMEMORY_H
#include <stddef.h>
#include <stdbool.h>

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

// OWNERSHIP: Caller owns returned pointer, must call Deallocate()
void *AllocateArray(size_t element_count, size_t element_bytes);

//void DeallocateArrayShallow(void **array, size_t element_count, size_t element_bytes);
void ValidateAllocation(void *pMemory, size_t bytes);

// OWNERSHIP: Transfers ownership to allocator, NULLs the pointer
size_t Deallocate(void **ptr, size_t bytes);

// OWNERSHIP: Caller owns returned pointer, must call Deallocate()
void *AllocateCollection(size_t element_count, size_t element_bytes);

// OWNERSHIP: Caller owns returned pointer, must call Deallocate()
void *AllocateBytes(size_t bytes);

// OWNERSHIP: Caller owns returned pointer, must call Deallocate()
void *ReallocateBytes(void *ptr, size_t old_bytes, size_t new_bytes);

void *MemoryCopy(void *dest, const void *src, size_t bytes);
void *MemoryMove(void *dest, const void *src, size_t bytes);
void *MemorySet(void *dest, int value, size_t bytes);
// void deallocate_deep(void **ptr, size_t bytes);
size_t CurrBytesAllocated();

// Returns the cumulative memory freed since the program started. Useful for tracking total deallocations over time.
size_t TotalBytesFreed();
size_t TotalBytesAllocated();
size_t TotalBytesCopied();
size_t TotalBytesMoved();
size_t TotalBytesSet();
void PrintCurrentBytesAlloc();

// Simple fixed-size pool allocator
typedef struct Pool Pool;
Pool *PoolCreate(size_t element_size, int capacity);
void PoolDestroy(Pool *p);
void *PoolAlloc(Pool *p);
void PoolFree(Pool *p, void *element);
bool PoolOwns(Pool *p, void *element);

#endif // CMEMORY_H
