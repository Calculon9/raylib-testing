#include "entities/entity_factory.h"
#include "entities/entity_registry.h"
#include "entities/gear.h"
#include "entities/portal.h"
#include "entities/rotor.h"
#include "entities/standard.h"
#include "world/world.h"

// Helper to clean up an allocated entity's surface vectors and struct allocation.
static void FreeAllocatedEntity(Newtonoid2d *entity)
{
    if (!entity)
    {
        return;
    }

    LArray *vectors = &entity->surface.surface_vectors;
    if (vectors->items && vectors->capacity > 0 && vectors->elem_bytes > 0)
    {
        size_t bytes = (size_t)vectors->capacity * vectors->elem_bytes;
        Deallocate(&vectors->items, bytes);
        vectors->items = NULL;
        vectors->count = 0;
        vectors->capacity = 0;
    }

    Deallocate((void **)&entity, sizeof(Newtonoid2d));
}

// Construct base physics geometry, allocate an ID, and attach preset components.
Newtonoid2d *EntityFactory_Create(const EntityCreateParams *params)
{
    if (!params)
    {
        return NULL;
    }

    // Allocate the EntityId upfront so components and base physics share the same ID.
    EntityId id = EntityRegistry_AllocateId();
    if (id == INVALID_ENTITY_ID)
    {
        return NULL;
    }

    Newtonoid2d *entity = NULL;
    bool component_attached = true;

    switch (params->preset)
    {
    case ENTITY_PRESET_STANDARD:
        entity = StandardEntity_Create(&params->physics);
        if (entity)
        {
            entity->id = id;
        }
        break;

    case ENTITY_PRESET_ROTOR:
        entity = RotorEntity_Create(&params->physics);
        if (entity)
        {
            entity->id = id;
            RotorComponent rotor;
            memset(&rotor, 0, sizeof(rotor));
            component_attached = EntityRegistry_AttachRotor(id, &rotor);
            EntityLifecycle_ApplyAttachedComponent(entity, ENTITY_COMPONENT_ROTOR, &rotor);
        }
        break;

    case ENTITY_PRESET_GEAR:
        entity = GearEntity_Create(&params->physics);
        if (entity)
        {
            entity->id = id;
            GearComponent gear;
            memset(&gear, 0, sizeof(gear));
            component_attached = EntityRegistry_AttachGear(id, &gear);
            EntityLifecycle_ApplyAttachedComponent(entity, ENTITY_COMPONENT_GEAR, &gear);
        }
        break;

    case ENTITY_PRESET_PORTAL:
    {
        PortalEntity portal = {0};
        entity = PortalEntity_Create(&params->physics, &portal);
        if (entity)
        {
            entity->id = id;
            component_attached = EntityRegistry_AttachPortal(id, &portal);
            EntityLifecycle_ApplyAttachedComponent(entity, ENTITY_COMPONENT_PORTAL, &portal);
        }
        break;
    }

    default:
        break;
    }

    // If physics construction or component attachment failed, roll back the entire creation.
    if (!entity || !component_attached)
    {
        FreeAllocatedEntity(entity);
        EntityRegistry_ReleaseId(id);
        return NULL;
    }

    return entity;
}

// Assemble an entity and insert it into the target world.
EntityId EntityFactory_Spawn(World2d *world, const EntityCreateParams *params, EntityId parent_id)
{
    if (!world || !params)
    {
        return INVALID_ENTITY_ID;
    }

    Newtonoid2d *entity = EntityFactory_Create(params);
    if (!entity)
    {
        return INVALID_ENTITY_ID;
    }

    EntityId id = entity->id;

    // AddObjectToWorld copies the entity into world->objects and binds location in EntityRegistry.
    EntityId registered_id = AddObjectToWorld(world, entity, parent_id);
    if (registered_id == INVALID_ENTITY_ID)
    {
        // Registration failed: release the ID and clean up heap entity.
        EntityRegistry_ReleaseId(id);
        FreeAllocatedEntity(entity);
        return INVALID_ENTITY_ID;
    }

    // The world's objects array now owns the surface vector items; free the temporary struct.
    Deallocate((void **)&entity, sizeof(Newtonoid2d));
    return id;
}
