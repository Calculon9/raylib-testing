/**********************************************************************************************
 *
 * PORTAL MECHANIC MODULE
 *
 **********************************************************************************************/
#ifndef PORTAL_MECHANIC_H
#define PORTAL_MECHANIC_H

#include "entities/portal.h"

typedef struct Universe Universe;
typedef struct World2d World2d;

typedef enum PortalTeleportResult
{
    PORTAL_TELEPORT_REJECTED = 0,
    PORTAL_TELEPORT_QUEUED
} PortalTeleportResult;

// Link two registered portal components as a bidirectional destination pair.
bool Portal_LinkPair(EntityId first_portal_id, EntityId second_portal_id);

// Return whether an entity is an eligible entrant for the supplied portal.
bool Portal_IsEntityEligible(const PortalEntity *portal, EntityId portal_id,
                             const Newtonoid2d *entity);

// Decrement the runtime cooldown state for portals owned by a world.
void Portal_TickCooldowns(World2d *world);

// Queue a validated same-world or cross-world transfer for an entity entering the portal.
PortalTeleportResult Portal_RequestTeleport(Universe *universe,
                                            EntityId portal_id,
                                            EntityId entity_id);

#endif
