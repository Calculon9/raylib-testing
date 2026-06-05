#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "associations/flat_map.h"
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
unsigned long CalcIntHash(int key, int capacity);
unsigned long CalcStrHash(const char *str, int capacity);
bool GrowFlatMapInt(FlatMapInt *m);

unsigned long CalcStrHash(const char *str, int capacity)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }

    return hash % capacity; // Map the giant number cleanly to our bucket array size
}

unsigned long CalcIntHash(int key, int capacity)
{
    // Scramble the integer bits so sequential IDs distribute beautifully
    unsigned long hash = (unsigned long)key * 2654435761UL;
    return hash % capacity;
}

// Create a new flat hashmap (FlatMap) with specified capacity, returns a pointer to the new FlatMap (you must dispose BOTH the FlatMap struct AND the FlatMap's internal buffer holding the elements)
FlatMapInt *AllocFlatMapInt(int capacity)
{
    // Allocate memory for the HashMap struct itself
    FlatMapInt *m = AllocateBytes(sizeof(FlatMapInt));
    if (m == NULL)
        return NULL;

    m->capacity = capacity;
    m->count = 0;
    m->slots = AllocateBytes(capacity * sizeof(FlatMapIntEntry));

    if (m->slots == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for FlatMap internal slots!\n");
        m->capacity = 0;
        return m;
    }

    for (int i = 0; i < capacity; i++)
    {
        m->slots[i].occupied = false; // Init all slots to unoccupied state
    }

    return m;
}

// Creates a new flat hashmap (FlatMap) with specified capacity, returns the new FlatMap struct (you MUST dispose the FlatMap's internal buffer holding the elements)
FlatMapInt MakeFlatMapInt(int capacity)
{
    // Allocate memory for the HashMap struct itself
    FlatMapInt m = {0};
    m.capacity = capacity;
    m.count = 0;

    m.slots = AllocateBytes(capacity * sizeof(FlatMapIntEntry));

    if (m.slots == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for FlatMap internal slots!\n");
        m.capacity = 0;
        return m;
    }

    for (int i = 0; i < capacity; i++)
    {
        m.slots[i].occupied = false; // Init all slots to unoccupied state
    }

    return m;
}

bool FlatMapInt_Get(FlatMapInt *m, int key, int *out_value)
{
    // 1. Safety Guardrails
    if (m == NULL || out_value == NULL)
    {
        fprintf(stderr, "ERROR: Invalid NULL parameter passed to FlatMapInt_Get.\n");
        return false;
    }

    if (m->capacity == 0 || m->count == 0)
    {
        return false;
    }

    unsigned long index = CalcIntHash(key, m->capacity);
    unsigned long start_index = index;

    // Search until we hit an unoccupied slot
    while (m->slots[index].occupied)
    {
        // Straight primitive integer matching (Blazing fast!)
        if (m->slots[index].key == key)
        {
            *out_value = m->slots[index].value;
            return true;
        }

        index = (index + 1) % m->capacity;

        if (index == start_index)
        {
            break; // Searched full map array
        }
    }

    return false;
}

// Inserting or Updating an int only for now
bool FlatMapInt_InsertOrUpdate(FlatMapInt *m, int key, int value)
{
    if (m == NULL || m->capacity == 0)
        return false;

    // Automated Growth Trigger: Keep load factor under 75%
    if ((float)m->count / (float)m->capacity >= 0.75f)
    {
        if (!GrowFlatMapInt(m))
        {
            fprintf(stderr, "WARNING: FlatMap failed to grow. Insertion continuing under high load.\n");
        }
    }

    unsigned long i = CalcIntHash(key, m->capacity);
    unsigned long start_index = i;

    // Linear Probing: Look for our key, or the next available empty slot
    while (m->slots[i].occupied)
    {
        // UPDATE WORKFLOW
        if (m->slots[i].key == key)
        {
            m->slots[i].value = value;
            return true;
        }
        i = (i + 1) % m->capacity; // Collision happened, step forward 1 slot (wrap around at capacity)

        if (i == start_index)
            return false; // Guard map full loop
    }

    // INSERT WORKFLOW - EMPTY SLOT FOUND
    m->slots[i].key = key;
    m->slots[i].value = value;
    m->slots[i].occupied = true;
    m->count++;
    return true;
}

bool GrowFlatMapInt(FlatMapInt *m)
{
    int old_capacity = m->capacity;
    // Cast explicitly back to integer to keep compiler calculations happy
    int new_capacity = (int)(old_capacity * 1.6f);

    // Allocate the fresh expanded buffer block
    FlatMapIntEntry *new_slots = AllocateBytes(new_capacity * sizeof(FlatMapIntEntry));
    if (new_slots == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for growing Flat Map!\n");
        return false;
    }

    // Zero-out the new buffer occupancy states
    for (int i = 0; i < new_capacity; i++)
    {
        new_slots[i].occupied = false;
    }

    // FIX: REHASH EXISTING KEYS. Do not use memcpy!
    if (m->slots != NULL)
    {
        for (int i = 0; i < old_capacity; i++)
        {
            // Only migrate items that are actively holding data
            if (m->slots[i].occupied)
            {
                int current_key = m->slots[i].key;
                int current_value = m->slots[i].value;

                // Re-calculate hash position using the brand new layout size
                unsigned long new_index = CalcIntHash(current_key, new_capacity);

                // Probe forward if a collision happens in the expanded array space
                while (new_slots[new_index].occupied)
                {
                    new_index = (new_index + 1) % new_capacity;
                }

                // Pack tightly into its new home
                new_slots[new_index].key = current_key;
                new_slots[new_index].value = current_value;
                new_slots[new_index].occupied = true;
            }
        }

        // Safe to deallocate the old buffer block NOW after copying is complete
        size_t old_slots_bytes = old_capacity * sizeof(FlatMapIntEntry);
        Deallocate((void **)&m->slots, old_slots_bytes);
    }

    // Remap structural configs
    m->slots = new_slots;
    m->capacity = new_capacity;

    printf("Flat Map grew cleanly to capacity %d\n", m->capacity);
    return true;
}

void DisposeFlatMapInt(FlatMapInt *m)
{
    if (!m) return;

    // Clean internal structures via updated Clear tool
    ClearFlatMapInt(m);

    // Drop the master structural config wrapper configuration block
    Deallocate((void **)&m, sizeof(FlatMapInt));
}

void ClearFlatMapInt(FlatMapInt *m)
{
    if (!m) return;

    // We just drop the master data buffer block as one single fast transaction.
    if (m->slots != NULL)
    {
        size_t total_slots_bytes = m->capacity * sizeof(FlatMapIntEntry);
        Deallocate((void **)&m->slots, total_slots_bytes);
        m->slots = NULL;
    }
    
    m->count = 0;
    m->capacity = 0;
}
