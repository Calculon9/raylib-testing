#ifndef ENTITY_FACTORY_H
#define ENTITY_FACTORY_H

#include "entities/entity_components.h"
#include "entities/portal.h"
#include "entities/rotor.h"
#include "entities/gear.h"
#include "physics/newtonoid.h"

// Component creation flags for EntityCreateParams.
// Bitmask values indicating which components to attach during entity creation.
typedef enum EntityCreationComponentFlags
{
    CREATION_COMPONENT_NONE = 0,
    CREATION_COMPONENT_PORTAL = (1U << 0),
    CREATION_COMPONENT_ROTOR = (1U << 1),
    CREATION_COMPONENT_GEAR = (1U << 2),
    CREATION_COMPONENT_HEALTH = (1U << 3),
} EntityCreationComponentFlags;

// Entity creation parameters with optional component configuration.
// Component parameters are embedded; use component_flags to indicate which should be attached.
typedef struct EntityCreateParams
{
    Newtonoid2dParams physics;
    PortalComponentParams portal_params;
    RotorComponentParams rotor_params;
    GearComponentParams gear_params;
    HealthComponentParams health_params;
    // Bitmask indicating which components to attach (EntityCreationComponentFlags).
    unsigned int component_flags;
} EntityCreateParams;

// Forward declaration of World2d.
typedef struct World2d World2d;

// Create an entity with allocated EntityId and attach any preset components.
// Returns an allocated Newtonoid2d with its ID assigned, or NULL on failure.
// The caller is responsible for either spawning it into a world or releasing it.
Newtonoid2d *EntityFactory_Create(const EntityCreateParams *params);

// Create an entity with attached components and register it in the specified world.
// Returns the allocated EntityId on success, or INVALID_ENTITY_ID on failure.
EntityId EntityFactory_Spawn(World2d *world, const EntityCreateParams *params, EntityId parent_id);

#endif
