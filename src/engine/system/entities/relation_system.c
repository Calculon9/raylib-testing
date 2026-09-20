/**********************************************************************************************
 *
 * RELATION SYSTEM MODULE
 *
 **********************************************************************************************/
#include "system/entities/relation_system.h"
#include "entities/entity_registry.h"
#include "world/world.h"

// Create or update a relation between two entities. Returns false if either entity is invalid
// or if the source entity already has a relation of that type.
bool RelationSystem_Create(EntityId source_id, RelationType type, EntityId target_id)
{
    if (source_id == INVALID_ENTITY_ID || target_id == INVALID_ENTITY_ID ||
        type == RELATION_NONE || source_id == target_id)
    {
        return false;
    }

    if (!EntityRegistry_IsIdActive(source_id) || !EntityRegistry_IsIdActive(target_id))
    {
        return false;
    }

    // Prevent duplicate relation attachment.
    if (EntityRegistry_GetRelation(source_id) != NULL)
    {
        return false;
    }

    RelationComponent relation = {
        .type = type,
        .target_entity = target_id,
        .is_active = true
    };

    return EntityRegistry_AttachRelation(source_id, &relation);
}

// Retrieve an entity's relation by type, or NULL if not present or inactive.
RelationComponent *RelationSystem_GetRelation(EntityId entity_id)
{
    if (entity_id == INVALID_ENTITY_ID || !EntityRegistry_IsIdActive(entity_id))
    {
        return NULL;
    }

    RelationComponent *relation = EntityRegistry_GetRelation(entity_id);
    if (!relation || relation->type == RELATION_NONE || !relation->is_active)
    {
        return NULL;
    }

    return relation;
}

// Deactivate or remove an entity's relation. Returns false if no relation was attached.
bool RelationSystem_RemoveRelation(EntityId entity_id)
{
    if (entity_id == INVALID_ENTITY_ID || !EntityRegistry_IsIdActive(entity_id))
    {
        return false;
    }

    return EntityRegistry_RemoveRelation(entity_id);
}

// Activate or deactivate a relation without removing it. Returns false if no relation exists.
bool RelationSystem_SetActive(EntityId entity_id, bool is_active)
{
    if (entity_id == INVALID_ENTITY_ID || !EntityRegistry_IsIdActive(entity_id))
    {
        return false;
    }

    RelationComponent *relation = EntityRegistry_GetRelation(entity_id);
    if (!relation || relation->type == RELATION_NONE)
    {
        return false;
    }

    relation->is_active = is_active;
    return true;
}

// Validate that a relation is still valid (both entities exist and are active). Returns false if broken.
bool RelationSystem_IsValid(EntityId entity_id)
{
    if (entity_id == INVALID_ENTITY_ID || !EntityRegistry_IsIdActive(entity_id))
    {
        return false;
    }

    RelationComponent *relation = EntityRegistry_GetRelation(entity_id);
    if (!relation || relation->type == RELATION_NONE)
    {
        return false;
    }

    // Both entities must be active for the relation to be valid.
    return EntityRegistry_IsIdActive(relation->target_entity);
}

// Update all relations in a world. Called once per frame during system updates.
// Validates relations and invalidates those with missing targets.
void RelationSystem_Update(World2d *world)
{
    if (!world)
    {
        return;
    }

    Newtonoid2d *objects = (Newtonoid2d *)world->objects.items;
    for (int object_index = 0; object_index < world->objects.count; object_index++)
    {
        Newtonoid2d *object = &objects[object_index];
        RelationComponent *relation = EntityRegistry_GetRelation(object->id);
        if (!relation || relation->type == RELATION_NONE)
        {
            continue;
        }

        // Invalidate relation if target entity no longer exists.
        if (!EntityRegistry_IsIdActive(relation->target_entity))
        {
            relation->is_active = false;
        }
    }
}