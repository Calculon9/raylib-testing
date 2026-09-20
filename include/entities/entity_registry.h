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

// Entity description: snapshot of an entity's component state for UI inspection.
typedef struct
{
    EntityId id;
    // Components indexed by EntityComponentType (0-4). NULL if not attached.
    void *components[ENTITY_COMPONENT_RELATION + 1];
} EntityDescription;

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

// Capture the current component state of an entity into a description struct.
EntityDescription EntityRegistry_Describe(const Newtonoid2d *object);

// ============================================================================
// Generic Component Dispatchers
// ============================================================================

// Attach component data of the specified EntityComponentType to an EntityId.
// Returns false if parameters are invalid, or if a component of this type is already attached.
bool EntityRegistry_AttachComponent(EntityId entity_id, EntityComponentType type, const void *component_data);

// Retrieve a pointer to the component data attached to an EntityId, or NULL if not attached.
void *EntityRegistry_GetComponent(EntityId entity_id, EntityComponentType type);

// Remove the component of the specified EntityComponentType from an EntityId.
// Returns false if no component of this type was attached.
bool EntityRegistry_RemoveComponent(EntityId entity_id, EntityComponentType type);

// Check whether an EntityId currently has a component of the specified EntityComponentType attached.
bool EntityRegistry_HasComponent(EntityId entity_id, EntityComponentType type);

// ============================================================================
// Component Lifecycle Hooks
// ============================================================================

// Synchronise an entity's physical state (flags, velocity, sensor rules) when a component is attached.
void EntityLifecycle_ApplyAttachedComponent(Newtonoid2d *entity, EntityComponentType type, const void *component_data);

// Synchronise an entity's physical state (reverting flags, velocity, sensor rules) when a component is detached.
void EntityLifecycle_ApplyDetachedComponent(Newtonoid2d *entity, EntityComponentType type);

// ============================================================================
// Component Accessors - Rotor Component
// ============================================================================

// Attach a RotorComponent to an EntityId. Returns false if entity already has a component.
bool EntityRegistry_AttachRotor(EntityId entity_id, const RotorComponent *rotor);

// Retrieve the RotorComponent attached to an EntityId, or NULL if not attached.
RotorComponent *EntityRegistry_GetRotor(EntityId entity_id);

// Remove the RotorComponent from an EntityId. Returns false if no rotor was attached.
bool EntityRegistry_RemoveRotor(EntityId entity_id);

// ============================================================================
// Component Accessors - Gear Component
// ============================================================================

// Attach a GearComponent to an EntityId. Returns false if entity already has a component.
bool EntityRegistry_AttachGear(EntityId entity_id, const GearComponent *gear);

// Retrieve the GearComponent attached to an EntityId, or NULL if not attached.
GearComponent *EntityRegistry_GetGear(EntityId entity_id);

// Remove the GearComponent from an EntityId. Returns false if no gear was attached.
bool EntityRegistry_RemoveGear(EntityId entity_id);

// ============================================================================
// Component Accessors - Portal Component
// ============================================================================

// Attach a PortalEntity to an EntityId. Returns false if entity already has a component.
bool EntityRegistry_AttachPortal(EntityId entity_id, const PortalEntity *portal);

// Retrieve the PortalEntity attached to an EntityId, or NULL if not attached.
PortalEntity *EntityRegistry_GetPortal(EntityId entity_id);

// Remove the PortalEntity from an EntityId. Returns false if no portal was attached.
bool EntityRegistry_RemovePortal(EntityId entity_id);

// ============================================================================
// Component Accessors - Relation Component
// ============================================================================

// Attach a RelationComponent to an EntityId. Returns false if entity already has a component.
bool EntityRegistry_AttachRelation(EntityId entity_id, const RelationComponent *relation);

// Retrieve the RelationComponent attached to an EntityId, or NULL if not attached.
RelationComponent *EntityRegistry_GetRelation(EntityId entity_id);

// Remove the RelationComponent from an EntityId. Returns false if no relation was attached.
bool EntityRegistry_RemoveRelation(EntityId entity_id);

#endif
