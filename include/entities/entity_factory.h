#ifndef ENTITY_FACTORY_H
#define ENTITY_FACTORY_H

#include "entities/entity_components.h"
#include "physics/newtonoid.h"

typedef enum EntityPreset
{
    ENTITY_PRESET_STANDARD,
    ENTITY_PRESET_ROTOR,
    ENTITY_PRESET_GEAR,
    ENTITY_PRESET_PORTAL,
} EntityPreset;

typedef struct EntityCreateParams
{
    Newtonoid2dParams physics;
    EntityPreset preset;
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
