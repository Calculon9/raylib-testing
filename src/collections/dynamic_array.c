#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "collections/dynamic_array.h"
#include "collections/collection_internal.h"
#include "memory/cmemory.h"
#include "common/common.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------

// Forward declaration used by DArray_Push.
static bool GrowDynamicArray(DArray *a);

// Create a new dynamic array with specified element size and count
DArray *AllocDArray(int elem_count, size_t elem_bytes)
{
    DArray *a = AllocateBytes(sizeof(DArray));
    if (a == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for Dynamic Array!\n");
        return NULL;
    }

    *a = MakeDArray(elem_count, elem_bytes);
    return a;
}

DArray MakeDArray(int elem_count, size_t elem_bytes)
{
    DArray a = {0};
    a.elem_bytes = elem_bytes;
    a.capacity = elem_count;
    a.count = 0;
    a.enumeratorIndex = 0;
    a.enumerationCount = 0;
    a.items = Collection_AllocItemsBuffer(elem_count, elem_bytes, "Dynamic Array");

    if (a.items == NULL)
    {
        a.capacity = 0;
    }
    return a;
}

void *DArray_Get(DArray *a, int index)
{
    if (a == NULL) return NULL;
    if (index < 0 || index >= a->count)
    {
        fprintf(stderr, "Index %d out of bounds for Circular DArray of count %d!\n", index, a->count);
        return NULL;
    }

    int actual_physical_index = (a->front + index) % a->capacity;
    return (char *)a->items + (actual_physical_index * a->elem_bytes);
}

// Provide the address of the DynamicArray and the address of the item to push.
DArray *DArray_Push(DArray *a, void *item)
{
    if (a == NULL || item == NULL)
    {
        fprintf(stderr, "The provided DynamicArray or element is NULL.\n");
        return NULL;
    }

    if (a->count >= a->capacity)
    {
        if (!GrowDynamicArray(a))
        {
            fprintf(stderr, "Failed to grow array!\n");
            return a;
        }
    }

    void *target = (char *)a->items + (a->rear * a->elem_bytes);
    MemoryCopy(target, item, a->elem_bytes);

    a->rear = (a->rear + 1) % a->capacity;
    a->count++;

    return a;
}

DArray *DArray_Pop(DArray *a, void *outItem)
{
    if (a == NULL)
    {
        fprintf(stderr, "The provided collection is NULL. Cannot pop item.\n");
        return a;
    }
    if (a->count <= 0)
    {
        fprintf(stderr, "Dynamic Array is empty! Cannot pop item.\n");
        return a;
    }

    void *source = (char *)a->items + (a->front * a->elem_bytes);

    if (outItem != NULL)
    {
        MemoryCopy(outItem, source, a->elem_bytes);
    }

    MemorySet(source, 0, a->elem_bytes);

    a->front = (a->front + 1) % a->capacity;
    a->enumeratorIndex = a->front;
    a->count--;

    return a;
}

// Increase the capacity of the array by the shared growth factor.
static bool GrowDynamicArray(DArray *a)
{
    if (a == NULL)
        return false;

    if (a->elem_bytes < 1)
    {
        fprintf(stderr, "Invalid element size %zu in GrowDynamicArray!\n", a->elem_bytes);
        return false;
    }

    int new_capacity = Collection_CalcGrowthCapacity(a->capacity);

    void *new_items = AllocateBytes(new_capacity * a->elem_bytes);
    if (new_items == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for growing array to new capacity %d!\n", new_capacity);
        return false;
    }

    if (a->items != NULL)
    {
        int current_index = a->front;
        for (int i = 0; i < a->count; i++)
        {
            void *source = (char *)a->items + (current_index * a->elem_bytes);
            void *dest = (char *)new_items + (i * a->elem_bytes);

            MemoryCopy(dest, source, a->elem_bytes);

            current_index = (current_index + 1) % a->capacity;
        }

        size_t old_bytes = a->capacity * a->elem_bytes;
        Deallocate((void **)&a->items, old_bytes);
    }

    a->items = new_items;
    a->capacity = new_capacity;
    a->front = 0;
    a->rear = a->count;

    LOG_INFO("Dynamic Array grown to new capacity %d\n", new_capacity);
    return true;
}

// Dispose of the array and free its memory
void DisposeDArray(DArray *a)
{
    if (a == NULL)
        return;

    Collection_ClearItems(&a->items, &a->count, &a->capacity, a->elem_bytes);

    Deallocate((void **)&a, sizeof(DArray));
}

// Clears internal heap data buffers without freeing the header struct container itself
void ClearDArray(DArray *a)
{
    if (a == NULL)
        return;

    Collection_ClearItems(&a->items, &a->count, &a->capacity, a->elem_bytes);
}

void *Enumerate(DArray *a)
{
    if (a == NULL || a->count == 0)
        return NULL;

    if (a->enumerationCount == a->count)
    {
        ResetEnumerator(a);
        return NULL;
    }

    void *item = (char *)a->items + (a->enumeratorIndex * a->elem_bytes);

    a->enumeratorIndex = (a->enumeratorIndex + 1) % a->capacity;
    a->enumerationCount++;

    return item;
}

void ResetEnumerator(DArray *a)
{
    if (a != NULL)
    {
        a->enumeratorIndex = a->front;
        a->enumerationCount = 0;
    }
}
