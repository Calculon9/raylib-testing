#include "system/ecs.h"
#include "collections/dynamic_array.h"
#include "memory/cmemory.h"
#include <string.h>
#include <stdio.h>

#define MAX_ENTITIES 2048
#define MAX_COMPONENT_TYPES 16


typedef struct
{
    uint32_t id;
    uint32_t version;
    bool alive;
} EntityRecord;

typedef struct ComponentStorage
{
    ComponentID comp_id;
    DArray *data;        // Array of void* pointers to component data
    size_t element_size; // Size of each component instance
} ComponentStorage;

typedef struct ECS
{
    EntityRecord *entities;
    int entity_count;
    int entity_capacity;
    uint32_t next_entity_id;
    uint32_t next_version;

    ComponentStorage components[MAX_COMPONENT_TYPES];
    int component_count;
} ECS;

ECS *ECS_Create(void)
{
    ECS *ecs = (ECS *)AllocateBytes(sizeof(ECS));
    if (!ecs)
        return NULL;

    ecs->entity_capacity = MAX_ENTITIES;
    ecs->entities = (EntityRecord *)AllocateBytes(sizeof(EntityRecord) * ecs->entity_capacity);
    if (!ecs->entities)
    {
        Deallocate((void **)&ecs, sizeof(ECS));
        return NULL;
    }

    MemorySet(ecs->entities, 0, sizeof(EntityRecord) * (size_t)ecs->entity_capacity);
    ecs->entity_count = 0;
    ecs->next_entity_id = 1; // Reserve 0 as invalid
    ecs->next_version = 0;
    ecs->component_count = 0;

    return ecs;
}

void ECS_Destroy(ECS *ecs)
{
    if (!ecs)
        return;

    // Clean up component storage
    for (int i = 0; i < ecs->component_count; i++)
    {
        if (ecs->components[i].data)
        {
            // Each component data slot is a void* allocated via AllocateBytes.
            for (int j = 0; j < ecs->components[i].data->count; j++)
            {
                void **slot = (void **)DArray_Get(ecs->components[i].data, j);
                if (slot && *slot)
                {
                    Deallocate((void **)slot, ecs->components[i].element_size);
                }
            }
            DisposeDArray(ecs->components[i].data);
        }
    }

    Deallocate((void **)&ecs->entities, sizeof(EntityRecord) * ecs->entity_capacity);
    Deallocate((void **)&ecs, sizeof(ECS));
}

Entity ECS_CreateEntity(ECS *ecs)
{
    if (!ecs || ecs->entity_count >= ecs->entity_capacity)
        return 0; // Invalid entity

    EntityRecord *record = &ecs->entities[ecs->entity_count];
    record->id = ecs->next_entity_id++;
    record->version = ecs->next_version++;
    record->alive = true;

    ecs->entity_count++;

    Entity e = (Entity)record->id;
    return e;
}

void ECS_DestroyEntity(ECS *ecs, Entity e)
{
    if (!ecs || e == 0)
        return;

    // Find and mark entity as dead
    for (int i = 0; i < ecs->entity_count; i++)
    {
        if (ecs->entities[i].id == e)
        {
            ecs->entities[i].alive = false;
            // Remove all components from dead entity
            for (int c = 0; c < ecs->component_count; c++)
            {
                ECS_RemoveComponent(ecs, e, ecs->components[c].comp_id);
            }
            return;
        }
    }
}

bool ECS_IsEntityAlive(ECS *ecs, Entity e)
{
    if (!ecs || e == 0)
        return false;
    for (int i = 0; i < ecs->entity_count; i++)
    {
        if (ecs->entities[i].id == e)
            return ecs->entities[i].alive;
    }
    return false;
}

void ECS_AddComponent(ECS *ecs, Entity e, ComponentID comp_id, const void *component_data, size_t component_size)
{
    if (!ecs || e == 0 || comp_id == COMPONENT_INVALID || !component_data)
        return;

    if (!ECS_IsEntityAlive(ecs, e))
        return;

    // Find or create component storage
    ComponentStorage *storage = NULL;
    for (int i = 0; i < ecs->component_count; i++)
    {
        if (ecs->components[i].comp_id == comp_id)
        {
            storage = &ecs->components[i];
            break;
        }
    }

    if (!storage)
    {
        if (ecs->component_count >= MAX_COMPONENT_TYPES)
            return;
        storage = &ecs->components[ecs->component_count++];
        storage->comp_id = comp_id;
        storage->element_size = component_size;
        storage->data = AllocDArray(ecs->entity_capacity, sizeof(void *));
        if (!storage->data)
            return;
    }

    // Allocate and store component data
    void *comp_instance = AllocateBytes(component_size);
    if (!comp_instance)
        return;
    MemoryCopy(comp_instance, component_data, component_size);

    // Ensure entity index slot exists in component array
    while (storage->data->count <= e)
    {
        void *null_ptr = NULL;
        DArray_Push(storage->data, &null_ptr);
    }

    // Store pointer to component
    void **slot = (void **)DArray_Get(storage->data, e);
    if (slot)
        *slot = comp_instance;
}

void ECS_RemoveComponent(ECS *ecs, Entity e, ComponentID comp_id)
{
    if (!ecs || e == 0 || comp_id == COMPONENT_INVALID)
        return;

    for (int i = 0; i < ecs->component_count; i++)
    {
        if (ecs->components[i].comp_id == comp_id && ecs->components[i].data)
        {
            if (e < ecs->components[i].data->count)
            {
                void **slot = (void **)DArray_Get(ecs->components[i].data, e);
                if (slot && *slot)
                {
                    Deallocate((void **)slot, ecs->components[i].element_size);
                }
            }
            return;
        }
    }
}

void *ECS_GetComponent(ECS *ecs, Entity e, ComponentID comp_id)
{
    if (!ecs || e == 0 || comp_id == COMPONENT_INVALID)
        return NULL;

    for (int i = 0; i < ecs->component_count; i++)
    {
        if (ecs->components[i].comp_id == comp_id && ecs->components[i].data)
        {
            if (e < ecs->components[i].data->count)
            {
                void **slot = (void **)DArray_Get(ecs->components[i].data, e);
                if (slot)
                    return *slot;
            }
            return NULL;
        }
    }
    return NULL;
}

void ECS_IterateEntities(ECS *ecs, const ComponentID *component_ids, int component_count, SystemIteratorFn fn, void *user_ctx)
{
    if (!ecs || !component_ids || component_count <= 0 || !fn || component_count > MAX_COMPONENT_TYPES)
        return;

    void *component_ptrs[MAX_COMPONENT_TYPES] = {0};

    // For each entity
    for (int e_idx = 0; e_idx < ecs->entity_count; e_idx++)
    {
        EntityRecord *rec = &ecs->entities[e_idx];
        if (!rec->alive)
            continue;

        Entity e = (Entity)rec->id;

        // Check if entity has all required components
        bool has_all = true;
        for (int c = 0; c < component_count; c++)
        {
            void *comp = ECS_GetComponent(ecs, e, component_ids[c]);
            if (!comp)
            {
                has_all = false;
                break;
            }
            component_ptrs[c] = comp;
        }

        if (has_all)
        {
            fn(user_ctx, e, component_ptrs);
        }
    }

}

int ECS_GetEntityCount(ECS *ecs)
{
    if (!ecs)
        return 0;
    int count = 0;
    for (int i = 0; i < ecs->entity_count; i++)
    {
        if (ecs->entities[i].alive)
            count++;
    }
    return count;
}

