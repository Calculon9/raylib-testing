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

typedef struct EntityCreateResult
{
    Newtonoid2d *entity;
    EntityComponent component;
} EntityCreateResult;

// Create an entity and any entity-specific state required by its preset.
bool EntityFactory_Create(const EntityCreateParams *params, EntityCreateResult *out_result);

#endif
