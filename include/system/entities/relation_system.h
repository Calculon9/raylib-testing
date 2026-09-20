/**********************************************************************************************
 *
 * RELATION SYSTEM MODULE
 *
 * Governs entity-to-entity relationships and paired behaviours. Relations enable cross-entity
 * mechanics such as portal linking, gear meshing, and attachment hierarchies.
 *
 **********************************************************************************************/
#ifndef RELATION_SYSTEM_H
#define RELATION_SYSTEM_H

#include <stdbool.h>
#include "entities/entity_id.h"
#include "entities/entity_components.h"

typedef struct World2d World2d;

// Create or update a relation between two entities. Returns false if either entity is invalid
// or if the source entity already has a relation of that type.
bool RelationSystem_Create(EntityId source_id, RelationType type, EntityId target_id);

// Retrieve an entity's relation by type, or NULL if not present or inactive.
RelationComponent *RelationSystem_GetRelation(EntityId entity_id);

// Deactivate or remove an entity's relation. Returns false if no relation was attached.
bool RelationSystem_RemoveRelation(EntityId entity_id);

// Activate or deactivate a relation without removing it. Returns false if no relation exists.
bool RelationSystem_SetActive(EntityId entity_id, bool is_active);

// Validate that a relation is still valid (both entities exist and are active). Returns false if broken.
bool RelationSystem_IsValid(EntityId entity_id);

// Update all relations in a world. Called once per frame during system updates.
// Systems that depend on relations should run after this to see updated state.
void RelationSystem_Update(World2d *world);

#endif
