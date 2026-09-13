/**********************************************************************************************
 *
 * ENTITY REGISTRY MODULE
 *
 * Provides centralised EntityId allocation and component attachment storage.
 *
 **********************************************************************************************/
#ifndef ENTITY_REGISTRY_H
#define ENTITY_REGISTRY_H

#include <stdbool.h>
#include "entities/entity_id.h"
#include "entities/entity_components.h"

typedef struct World2d World2d;

// Initialise the entity ID sequence and component storage.
void EntityRegistry_Init(void);

// Release all resources and component storage held by the registry.
void EntityRegistry_Shutdown(void);

// Allocate a globally unique EntityId.
EntityId EntityRegistry_AllocateId(void);

// Release an ID and advance its generation so stale handles cannot resolve.
void EntityRegistry_ReleaseId(EntityId entity_id);

// Return whether an ID currently refers to an allocated slot.
bool EntityRegistry_IsIdActive(EntityId entity_id);

// Inspect the next slot-based EntityId that would be allocated.
EntityId EntityRegistry_GetNextId(void);

// Record or update the world storage location for an active entity (-1 for world root, >= 0 for world->objects).
bool EntityRegistry_SetLocation(EntityId entity_id, EntityId world_id, int array_index);

// Clear an entity location while preserving the ID for a world transfer.
bool EntityRegistry_ClearLocation(EntityId entity_id);

// Resolve an active ID to its current Newtonoid storage.
Newtonoid2d *EntityRegistry_GetEntity(EntityId entity_id);

// Resolve the world currently owning an active ID.
World2d *EntityRegistry_GetEntityWorld(EntityId entity_id);

// Resolve the world ID currently owning an active entity ID.
EntityId EntityRegistry_GetEntityWorldId(EntityId entity_id);

// Read the storage location associated with an active ID.
bool EntityRegistry_GetLocation(EntityId entity_id, EntityId *out_world_id, int *out_array_index);

// Attach an optional component to an EntityId.
bool EntityRegistry_RegisterComponent(EntityId entity_id, const EntityComponent *component);

// Retrieve an optional component attached to an EntityId by component type.
EntityComponent *EntityRegistry_GetComponent(EntityId entity_id, EntityComponentType type);

// Remove all optional components associated with an EntityId.
void EntityRegistry_RemoveComponent(EntityId entity_id);

#endif
