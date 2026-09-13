#ifndef ENTITY_COMPONENTS_H
#define ENTITY_COMPONENTS_H

#include "entities/portal.h"

// Optional data attached to a Newtonoid without changing the generic physics object.
typedef enum EntityComponentType
{
    ENTITY_COMPONENT_NONE = 0,
    ENTITY_COMPONENT_PORTAL = 1
} EntityComponentType;

typedef struct EntityComponent
{
    // The registry stores the owning EntityId separately as its lookup key.
    EntityComponentType type;
    union
    {
        PortalEntity portal;
    } data;
} EntityComponent;

#endif
