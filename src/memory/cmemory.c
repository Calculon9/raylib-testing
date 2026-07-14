#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "memory/cmemory.h"
#include <stdbool.h>
#include <stdint.h>

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------

static size_t cumulativeBytesAllocated = 0;
static size_t cumulativeBytesFreed = 0;
static size_t cumulativeBytesCopied = 0;
static size_t cumulativeBytesMoved = 0;
static size_t cumulativeBytesSet = 0;
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
    cumulativeBytesAllocated += bytes;

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

    // Prevent underflow if 'bytes' passed is somehow larger than current count
    if (bytesAllocated >= bytes)
    {
        bytesAllocated -= bytes;
    }

    cumulativeBytesFreed += bytes;

    return bytes;
}

void *ReallocateBytes(void *ptr, size_t old_bytes, size_t new_bytes)
{
    if (ptr == NULL)
    {
        if (new_bytes == 0)
        {
            return NULL;
        }
        return AllocateBytes(new_bytes);
    }

    if (new_bytes == 0)
    {
        free(ptr);
        if (bytesAllocated >= old_bytes)
        {
            bytesAllocated -= old_bytes;
        }
        cumulativeBytesFreed += old_bytes;
        return NULL;
    }

    void *new_ptr = realloc(ptr, new_bytes);
    if (new_ptr == NULL)
    {
        return NULL;
    }

    if (new_bytes > old_bytes)
    {
        size_t delta = new_bytes - old_bytes;
        bytesAllocated += delta;
        cumulativeBytesAllocated += delta;
    }
    else if (old_bytes > new_bytes)
    {
        size_t delta = old_bytes - new_bytes;
        if (bytesAllocated >= delta)
        {
            bytesAllocated -= delta;
        }
        cumulativeBytesFreed += delta;
    }

    return new_ptr;
}

void *MemoryCopy(void *dest, const void *src, size_t bytes)
{
    if (dest == NULL || src == NULL || bytes == 0)
    {
        return dest;
    }

    cumulativeBytesCopied += bytes;
    return memcpy(dest, src, bytes);
}

void *MemoryMove(void *dest, const void *src, size_t bytes)
{
    if (dest == NULL || src == NULL || bytes == 0)
    {
        return dest;
    }

    cumulativeBytesMoved += bytes;
    return memmove(dest, src, bytes);
}

void *MemorySet(void *dest, int value, size_t bytes)
{
    if (dest == NULL || bytes == 0)
    {
        return dest;
    }

    cumulativeBytesSet += bytes;
    return memset(dest, value, bytes);
}

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
}

// Returns the cumulative memory freed since the program started. Useful for tracking total deallocations over time.
size_t TotalBytesFreed()
{
    return cumulativeBytesFreed;
}

// Returns the cumulative memory allocated since the program started. Useful for tracking total allocations over time.
size_t TotalBytesAllocated()
{
    return cumulativeBytesAllocated;
}

size_t TotalBytesCopied()
{
    return cumulativeBytesCopied;
}

size_t TotalBytesMoved()
{
    return cumulativeBytesMoved;
}

size_t TotalBytesSet()
{
    return cumulativeBytesSet;
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
}

void PrintCurrentBytesAlloc()
{
    printf("TOTAL BYTES ALLOC: %0.2lf kbytes\n", (double)(bytesAllocated / 1024.0));
}

// ---------------- Pool allocator ----------------
struct Pool
{
    void *mem;
    void *free_list; // linked list of free slots (stores next pointer inside freed slot)
    size_t element_size;
    int capacity;
};

Pool *PoolCreate(size_t element_size, int capacity)
{
    if (element_size == 0 || capacity <= 0)
        return NULL;

    Pool *p = (Pool *)AllocateBytes(sizeof(Pool));
    if (!p)
        return NULL;

    size_t total = element_size * (size_t)capacity;
    void *mem = AllocateBytes(total);
    if (!mem)
    {
        Deallocate((void **)&p, sizeof(Pool));
        return NULL;
    }

    p->mem = mem;
    p->element_size = element_size;
    p->capacity = capacity;
    p->free_list = NULL;

    // initialize free list
    for (int i = 0; i < capacity; i++)
    {
        void *slot = (char *)mem + (i * element_size);
        // store pointer to next at start of slot
        void **next_ptr = (void **)slot;
        *next_ptr = p->free_list;
        p->free_list = slot;
    }

    return p;
}

void *PoolAlloc(Pool *p)
{
    if (!p || !p->free_list)
        return NULL;
    void *slot = p->free_list;
    void **next_ptr = (void **)slot;
    p->free_list = *next_ptr;
    // zero init
    MemorySet(slot, 0, p->element_size);
    return slot;
}

void PoolFree(Pool *p, void *element)
{
    if (!p || !element)
        return;
    // push back onto free list
    void **next_ptr = (void **)element;
    *next_ptr = p->free_list;
    p->free_list = element;
}

bool PoolOwns(Pool *p, void *element)
{
    if (!p || !element)
        return false;
    uintptr_t base = (uintptr_t)p->mem;
    uintptr_t e = (uintptr_t)element;
    uintptr_t top = base + p->element_size * (uintptr_t)p->capacity;
    return (e >= base && e < top);
}
