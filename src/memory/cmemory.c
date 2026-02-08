#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------

static size_t cumulative_bytes_allocated = 0;
static size_t cumulative_bytes_freed = 0;
static size_t bytes_allocated = 0;

// Validates if memory allocation was successful.
void validate_allocation(void *pMemory, size_t bytes)
{
    if (pMemory != NULL)
    {
        bytes_allocated += bytes;
    }
    else
    {
        fprintf(stderr, "Memory allocation failed!\n");
        exit(1);
    }
}

// Allocates memory for a generic array and updates bytes in use. Cast the returned generic ptr to required type by caller.
void *allocate_array(size_t element_count, size_t element_bytes)
{
    return allocate_collection(element_count, element_bytes);
}

// Allocates memory for a generic collection and updates bytes in use. Cast the returned generic ptr to required type by caller.
void *allocate_collection(size_t element_count, size_t element_bytes)
{
    // Multiply count by size to get total bytes
    size_t total_bytes = element_count * element_bytes;

    return allocate_bytes(total_bytes);
}

// Allocates memory for a single object and updates bytes in use. Cast the returned generic ptr to required type by caller.
void *allocate_bytes(size_t bytes)
{
    if (bytes == 0)
    {
        fprintf(stderr, "Cannot allocate zero bytes!\n");
        return NULL;
    }

    void *ptr = calloc(1, bytes);

    // Zero-initialize the allocated memory - not needed as calloc does this
    // memset(ptr, 0, total_size);

    validate_allocation(ptr, bytes);

    return ptr;
}

// Deallocates memory for anything and updates bytes in use.
void deallocate_shallow(void **ptr, size_t bytes)
{
    // If the pointer-to-pointer is NULL, or the pointer itself is already NULL,
    // do nothing. This prevents subtracting from bytes_in_use twice.
    if (ptr == NULL || *ptr == NULL)
    {
        return;
    }

    free(*ptr);
    *ptr = NULL; // Single de-reference to set the caller's pointer to NULL

    // Prevent underflow if 'bytes' passed is somehow larger than current count
    if (bytes_allocated >= bytes)
    {
        bytes_allocated -= bytes;
    }
}

// Deallocates memory for a SINGLE ALLOCATION generic array and updates bytes in use.
void deallocate_array_shallow(void **array, size_t element_count, size_t element_bytes)
{
    deallocate_shallow(array, element_count * element_bytes);
}

// Deallocates memory for anything and updates bytes in use. Use for deep deallocation, i.e., if the memory constitutes pointers.
// void deallocate_deep(void **ptr, size_t bytes)
// {
//     //Because every struct has a different "map" of internal pointers, we’ll have to write a specific destroy function for each complex struct type you create.
//     //free(*ptr);
//     //bytes_in_use -= bytes;
//     //*ptr = NULL;
// }

// Returns the total memory currently in use. This is the net of all allocations minus deallocations, giving a snapshot of current memory usage.
int curr_bytes_allocated()
{
    return bytes_allocated;
}

// Returns the cumulative memory freed since the program started. Useful for tracking total deallocations over time.
int total_bytes_freed()
{
    return cumulative_bytes_freed;
}

// Returns the cumulative memory allocated since the program started. Useful for tracking total allocations over time.
int total_bytes_allocated()
{
    return cumulative_bytes_allocated;
}