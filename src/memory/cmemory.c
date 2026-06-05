#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------

static size_t cumulativeBytesAllocated = 0;
static size_t cumulativeBytesFreed = 0;
static size_t bytesAllocated = 0;
 

// Validates if memory allocation was successful.
void ValidateAllocation(void *pMemory, size_t bytes)
{
    if (pMemory != NULL)
    {
        bytesAllocated += bytes;
    }
    else
    {
        fprintf(stderr, "Memory allocation failed!\n");
        exit(1);
    }
}

// Allocates memory for a generic array and updates bytes in use. Cast the returned generic ptr to required type by caller.
void *AllocateArray(size_t element_count, size_t element_bytes)
{
    return AllocateCollection(element_count, element_bytes);
}

// Allocates memory for a generic collection and updates bytes in use. Cast the returned generic ptr to required type by caller.
void *AllocateCollection(size_t element_count, size_t element_bytes)
{
    // Multiply count by size to get total bytes
    size_t total_bytes = element_count * element_bytes;

    return AllocateBytes(total_bytes);
}

// Allocates memory for a single object and updates bytes in use. Cast the returned generic ptr to required type by caller.
void *AllocateBytes(size_t bytes)
{
    if (bytes == 0)
    {
        fprintf(stderr, "Cannot allocate zero bytes!\n");
        return NULL;
    }

    // If a signed integer calculation went deeply negative before casting to size_t, 
    // it will show up as a staggeringly huge value (like 18 exabytes).
    if (bytes > 0x7FFFFFFFFFFFFFFFUL) 
    {
        fprintf(stderr, "FATAL ERROR: Ridiculous allocation size detected (%zu bytes). Potential integer overflow!\n", bytes);
        return NULL;
    }

    void *ptr = calloc(1, bytes);

    ValidateAllocation(ptr, bytes);

    //printf("Allocated %zu bytes. Total currently allocated: %zu bytes.\n", bytes, bytesAllocated); // zu is the format specifier for size_t

    return ptr;
}

// Deallocates memory for anything and updates bytes in use.
size_t Deallocate(void **ptr, size_t bytes)
{
    // If the pointer-to-pointer is NULL, or the pointer itself is already NULL,
    // do nothing. This prevents subtracting from bytes_in_use twice.
    if (ptr == NULL || *ptr == NULL)
    {
        return 0;
    }

    free(*ptr);
    *ptr = NULL; // Single de-reference to set the caller's pointer to NULL

    //printf("Deallocated %zu bytes. Total currently allocated: %zu bytes.\n", bytes, bytesAllocated);

    // Prevent underflow if 'bytes' passed is somehow larger than current count
    if (bytesAllocated >= bytes)
    {
        bytesAllocated -= bytes;
    }

    return bytes;
}

// Deallocates memory for a SINGLE ALLOCATION generic array and updates bytes in use.
// void DeallocateArrayShallow(void **array, size_t element_count, size_t element_bytes)
// {
//     DeallocateShallow(array, element_count * element_bytes);
// }

// Deallocates memory for anything and updates bytes in use. Use for deep deallocation, i.e., if the memory constitutes pointers.
// void deallocate_deep(void **ptr, size_t bytes)
// {
//     //Because every struct has a different "map" of internal pointers, we’ll have to write a specific destroy function for each complex struct type you create.
//     //free(*ptr);
//     //bytes_in_use -= bytes;
//     //*ptr = NULL;
// }

// Returns the total memory currently in use. This is the net of all allocations minus deallocations, giving a snapshot of current memory usage.
size_t CurrBytesAllocated()
{
    if (bytesAllocated > 0)
    {
        return bytesAllocated;
    }
    else
    {
        return 0; // Ensure we never return a negative value
    }
    return bytesAllocated;
}

// Returns the cumulative memory freed since the program started. Useful for tracking total deallocations over time.
size_t TotalBytesFreed()
{
    if (cumulativeBytesFreed > 0)
    {
        return cumulativeBytesFreed;
    }
    else
    {
        return 0; // Ensure we never return a negative value
    }
    return cumulativeBytesFreed;
}

// Returns the cumulative memory allocated since the program started. Useful for tracking total allocations over time.
size_t TotalBytesAllocated()
{
    if (cumulativeBytesAllocated > 0)
    {
        return cumulativeBytesAllocated;
    }
    else
    {
        return 0; // Ensure we never return a negative value
    }
    return cumulativeBytesAllocated;
}

// Returns the consumed memory in bytes out of the total allocated bytes. Useful for tracking how much of the allocated memory is actually in use.
size_t CurrBytesConsumed()
{
    if (cumulativeBytesAllocated > 0)
    {
        return cumulativeBytesAllocated;
    }
    else
    {
        return 0; // Ensure we never return a negative value
    }
    return cumulativeBytesAllocated;
}