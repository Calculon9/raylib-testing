/**********************************************************************************************
 *
 * PORTAL ENTITY MODULE
 *
 **********************************************************************************************/
#ifndef PORTAL_ENTITY_H
#define PORTAL_ENTITY_H

#include "physics/newtonoid.h"

typedef struct PortalDestination
{
    EntityId portal_id;
} PortalDestination;

typedef struct PortalEntity
{
    PortalDestination destination;
    EntityTypeFlags entrant_mask;
    int cooldown_frames;
    EntityId cooldown_entity_id; // The entity currently on cooldown for this portal.
    int cooldown_remaining;
} PortalEntity;

// Create the physical portal and initialise its portal-specific state.
Newtonoid2d *PortalEntity_Create(const Newtonoid2dParams *params, PortalEntity *out_portal);

// Initialise portal-specific state without assigning an entity identity.
bool PortalEntity_Initialise(PortalEntity *portal, PortalDestination destination,
                             EntityTypeFlags entrant_mask, int cooldown_frames);

// Return whether the portal state contains the minimum values required by mechanics.
bool PortalEntity_IsValid(const PortalEntity *portal);

#endif
