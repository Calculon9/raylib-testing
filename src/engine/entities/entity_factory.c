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

// Construct the physics body, allocate an ID, and attach each configured component.
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

    // Create physics body with appropriate geometry (portal, rotor, gear, or standard).
    Newtonoid2d *entity = NULL;
    if (params->component_flags & CREATION_COMPONENT_PORTAL)
    {
        entity = PortalEntity_Create(&params->physics);
    }
    else if (params->component_flags & CREATION_COMPONENT_ROTOR)
    {
        entity = RotorEntity_Create(&params->physics);
    }
    else if (params->component_flags & CREATION_COMPONENT_GEAR)
    {
        entity = GearEntity_Create(&params->physics);
    }
    else
    {
        entity = StandardEntity_Create(&params->physics);
    }

    if (!entity)
    {
        EntityRegistry_ReleaseId(id);
        return NULL;
    }

    entity->id = id;
    bool component_attached = true;

    // Attach each component and apply its entity-level lifecycle effects.    if (component_attached && (params->component_flags & CREATION_COMPONENT_PORTAL))
    {
        PortalComponent portal = {0};
        component_attached = PortalComponent_Initialise(&portal, &params->portal_params) && EntityRegistry_AttachPortal(id, &portal);
        if (component_attached)
        {
            EntityLifecycle_ApplyAttachedComponent(entity, ENTITY_COMPONENT_PORTAL, &portal);
        }
    }
    if (component_attached && (params->component_flags & CREATION_COMPONENT_ROTOR))
    {
        RotorComponent rotor = {0};
        component_attached = EntityRegistry_AttachRotor(id, &rotor);
        if (component_attached)
        {
            EntityLifecycle_ApplyAttachedComponent(entity, ENTITY_COMPONENT_ROTOR, &rotor);
        }
    }

    if (component_attached && (params->component_flags & CREATION_COMPONENT_GEAR))
    {
        GearComponent gear = {0};
        component_attached = EntityRegistry_AttachGear(id, &gear);
        if (component_attached)
        {
            EntityLifecycle_ApplyAttachedComponent(entity, ENTITY_COMPONENT_GEAR, &gear);
        }
    }

    if (component_attached && (params->component_flags & CREATION_COMPONENT_HEALTH))
    {
        HealthComponent health = {0};
        component_attached = HealthComponent_Initialise(&health, &params->health_params) &&
                             EntityRegistry_AttachHealth(id, &health);
        if (component_attached)
        {
            EntityLifecycle_ApplyAttachedComponent(entity, ENTITY_COMPONENT_HEALTH, &health);
        }
    }

    if (!component_attached)
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
