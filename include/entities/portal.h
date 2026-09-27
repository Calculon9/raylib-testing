/**********************************************************************************************
 *
 * PORTAL ENTITY MODULE
 *
 **********************************************************************************************/
#ifndef PORTAL_ENTITY_H
#define PORTAL_ENTITY_H

#include "entities/entity_flags.h"
#include "physics/newtonoid.h"

typedef struct PortalComponent
{
    EntityId owner_id;
    EntityRoleFlags entrant_roles;
    EntityId portal_destination_id;
    int cooldown_frames;
    EntityId cooldown_entity_id;
    int cooldown_frames_remaining;
} PortalComponent;

// Configurable portal component values supplied by a prefab or caller.
typedef struct PortalComponentParams
{
    EntityRoleFlags entrant_roles;
    int cooldown_frames;
} PortalComponentParams;

// A non-owning view joining a portal's base body and registered component.
typedef struct PortalEntity
{
    Newtonoid2d *base;
    PortalComponent *portal_component;
} PortalEntity;

// Create the physical portal geometry; component initialization and attachment is handled by the factory.
Newtonoid2d *PortalEntity_Create(const Newtonoid2dParams *params);

// Initialise runtime component state from prefab-configurable values.
bool PortalComponent_Initialise(PortalComponent *component, const PortalComponentParams *params);

// Resolve a transient view from the registry; do not retain across storage changes.
PortalEntity PortalEntity_GetView(EntityId entity_id);

// Return whether portal component cooldown values are valid.
bool PortalComponent_IsValid(const PortalComponent *component);

// Return whether the portal state contains the minimum values required by mechanics.
bool PortalEntity_IsValid(const PortalEntity *portal);

#endif
