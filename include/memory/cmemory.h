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
#define NEW_ARRAY(count, type) allocate_array(count, sizeof(type))
#define NEW_COLLECTION(count, type) allocate_collection(count, sizeof(type))
#define FREE_ARRAY_SHALLOW(ptr, type, count) deallocate_array_shallow((void **)&ptr, count, sizeof(type))
#define FREE_MEM_SHALLOW(ptr, type) deallocate((void **)&ptr, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

void *allocate_array(size_t element_count, size_t element_bytes);
void deallocate_array_shallow(void **array, size_t element_count, size_t element_bytes);
void validate_allocation(void *pMemory, size_t bytes);
void deallocate_shallow(void **ptr, size_t bytes);
void *allocate_collection(size_t element_count, size_t element_bytes);
void *allocate_bytes(size_t bytes);
// void deallocate_deep(void **ptr, size_t bytes);
int curr_bytes_allocated();

// Returns the cumulative memory freed since the program started. Useful for tracking total deallocations over time.
int total_bytes_freed();
int total_bytes_allocated();

#endif // CMEMORY_H