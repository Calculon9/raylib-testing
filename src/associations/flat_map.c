#include <stdio.h>
#include <stdbool.h>
#include "associations/flat_map.h"
#include "memory/cmemory.h"
#include "common/common.h"

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
static unsigned long CalcIntHash(int key, int capacity);
static bool GrowFlatMapInt(FlatMapInt *m);

static unsigned long CalcIntHash(int key, int capacity)
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

    return m;
}

bool FlatMapInt_GetValue(FlatMapInt *m, int key, int *out_value)
{
    // Safety Guardrails
    if (m == NULL || out_value == NULL)
    {
        fprintf(stderr, "ERROR: Invalid NULL parameter passed to FlatMapInt_GetValue.\n");
        return false;
    }

    if (m->capacity == 0 || m->count == 0)
    {
        return false;
    }

    unsigned long index = CalcIntHash(key, m->capacity);
    unsigned long start_index = index;

    // Search until we hit an empty slot; deleted tombstones must keep probe chains intact.
    while (1)
    {
        FlatMapIntEntry *slot = &m->slots[index];

        if (slot->state == FLAT_MAP_SLOT_EMPTY)
        {
            return false;
        }

        if (slot->state == FLAT_MAP_SLOT_OCCUPIED && slot->key == key)
        {
            *out_value = slot->value;
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

bool FlatMapInt_DeactivateSlot(FlatMapInt *m, int key)
{
    // Safety Guardrails
    if (m == NULL)
    {
        fprintf(stderr, "ERROR: Invalid NULL parameter passed to FlatMapInt_GetValue.\n");
        return false;
    }

    if (m->capacity == 0 || m->count == 0)
    {
        return false;
    }

    unsigned long index = CalcIntHash(key, m->capacity);
    unsigned long start_index = index;

    // Search until we hit an empty slot; deleted tombstones cannot terminate probe walks.
    while (1)
    {
        FlatMapIntEntry *slot = &m->slots[index];

        if (slot->state == FLAT_MAP_SLOT_EMPTY)
        {
            return false;
        }

        if (slot->state == FLAT_MAP_SLOT_OCCUPIED && slot->key == key)
        {
            slot->state = FLAT_MAP_SLOT_DELETED;
            slot->key = 0;
            slot->value = 0;
            if (m->count > 0)
            {
                m->count--;
            }
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

    // Automated Growth Trigger: Keep load factor under 70% (reduced from 75% for Phase 3 optimization)
    // Lower threshold reduces collision chain length and rehash frequency with 2.0x growth
    if ((float)m->count / (float)m->capacity >= 0.70f)
    {
        if (!GrowFlatMapInt(m))
        {
            fprintf(stderr, "WARNING: FlatMap failed to grow. Insertion continuing under high load.\n");
        }
    }

    unsigned long i = CalcIntHash(key, m->capacity);
    unsigned long start_index = i;
    unsigned long first_deleted_index = (unsigned long)-1;

    // Linear Probing: Look for our key, or the next available empty slot
    while (1)
    {
        FlatMapIntEntry *slot = &m->slots[i];

        // UPDATE WORKFLOW
        if (slot->state == FLAT_MAP_SLOT_OCCUPIED && slot->key == key)
        {
            slot->value = value;
            return true;
        }

        if (slot->state == FLAT_MAP_SLOT_DELETED && first_deleted_index == (unsigned long)-1)
        {
            first_deleted_index = i;
        }

        if (slot->state == FLAT_MAP_SLOT_EMPTY)
        {
            unsigned long target = (first_deleted_index != (unsigned long)-1) ? first_deleted_index : i;
            m->slots[target].key = key;
            m->slots[target].value = value;
            m->slots[target].state = FLAT_MAP_SLOT_OCCUPIED;
            m->count++;
            return true;
        }

        i = (i + 1) % m->capacity; // Collision happened, step forward 1 slot (wrap around at capacity)

        if (i == start_index)
        {
            // Table may be saturated with tombstones; recycle first tombstone if one was found.
            if (first_deleted_index != (unsigned long)-1)
            {
                m->slots[first_deleted_index].key = key;
                m->slots[first_deleted_index].value = value;
                m->slots[first_deleted_index].state = FLAT_MAP_SLOT_OCCUPIED;
                m->count++;
                return true;
            }
            return false; // Guard map full loop
        }
    }
}

static bool GrowFlatMapInt(FlatMapInt *m)
{
    int old_capacity = m->capacity;
    // Increased growth factor from 1.6 to 2.0 for fewer rehashes (Phase 3 optimization)
    int new_capacity = old_capacity * 2;
    if (new_capacity < 8) new_capacity = 8;

    // Allocate the fresh expanded buffer block
    FlatMapIntEntry *new_slots = AllocateBytes(new_capacity * sizeof(FlatMapIntEntry));
    if (new_slots == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for growing Flat Map!\n");
        return false;
    }

    // REHASH: Migrate only occupied slots (skip tombstones for cleanup)
    int migrated_count = 0;
    if (m->slots != NULL)
    {
        for (int i = 0; i < old_capacity; i++)
        {
            // Only migrate items that are actively holding data (tombstone cleanup optimization)
            if (m->slots[i].state == FLAT_MAP_SLOT_OCCUPIED)
            {
                int current_key = m->slots[i].key;
                int current_value = m->slots[i].value;

                // Re-calculate hash position using the brand new layout size
                unsigned long new_index = CalcIntHash(current_key, new_capacity);

                // Probe forward if a collision happens in the expanded array space
                while (new_slots[new_index].state == FLAT_MAP_SLOT_OCCUPIED)
                {
                    new_index = (new_index + 1) % new_capacity;
                }

                // Pack tightly into its new home
                new_slots[new_index].key = current_key;
                new_slots[new_index].value = current_value;
                new_slots[new_index].state = FLAT_MAP_SLOT_OCCUPIED;
                migrated_count++;
            }
        }

        // Safe to deallocate the old buffer block NOW after copying is complete
        size_t old_slots_bytes = old_capacity * sizeof(FlatMapIntEntry);
        Deallocate((void **)&m->slots, old_slots_bytes);
    }

    // Remap structural configs
    m->slots = new_slots;
    m->capacity = new_capacity;
    m->count = migrated_count; // Update count to exclude tombstones

    LOG_INFO("Flat Map grown to new capacity %d (migrated %d entries, cleaned tombstones)\n", new_capacity, migrated_count);
    return true;
}

void DisposeFlatMapInt(FlatMapInt *m)
{
    if (!m)
        return;

    // Clean internal structures via updated Clear tool
    ClearFlatMapInt(m);

    // Drop the master structural config wrapper configuration block
    Deallocate((void **)&m, sizeof(FlatMapInt));
}

void ClearFlatMapInt(FlatMapInt *m)
{
    if (!m)
        return;

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

void ResetFlatMapInt(FlatMapInt *m)
{
    if (!m)
        return;

    // We just drop the master data buffer block as one single fast transaction.
    if (m->slots != NULL)
    {
        // Zero-out the new buffer occupancy states and values
        FlatMapIntEntry *slots = (FlatMapIntEntry *)m->slots;
        for (size_t i = 0; i < m->capacity; i++)
        {
            slots[i].key = 0;
            slots[i].value = 0;
            slots[i].state = FLAT_MAP_SLOT_EMPTY;
        }

        // size_t total_slots_bytes = m->capacity * sizeof(FlatMapIntEntry);
        // Deallocate((void **)&m->slots, total_slots_bytes);
        // m->slots = NULL;
    }

    m->count = 0;
    // m->capacity = 0;
}
