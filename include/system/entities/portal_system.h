/**********************************************************************************************
 *
 * PORTAL SYSTEM MODULE
 *
 * Governs portal entity behaviour, linking, and teleportation mechanics. Portals are
 * entities with sensor collision that can teleport entering entities to linked destinations.
 *
 **********************************************************************************************/
#ifndef PORTAL_SYSTEM_H
#define PORTAL_SYSTEM_H

#include <stdbool.h>
#include "entities/entity_id.h"
#include "entities/portal.h"

typedef struct World2d World2d;
typedef struct Universe Universe;

// Portal teleportation result codes for RequestTeleport operations.
typedef enum PortalTeleportResult
{
    PORTAL_TELEPORT_REJECTED = 0,
    PORTAL_TELEPORT_QUEUED = 1
} PortalTeleportResult;

// Link two registered portal entities so each endpoint resolves the other as destination.
bool PortalSystem_LinkPair(EntityId first_portal_id, EntityId second_portal_id);

// Check the portal and entity rules that are independent of world ownership.
bool PortalSystem_IsEntityEligible(const PortalEntity *portal, EntityId portal_id, const Newtonoid2d *entity);

// Update cooldown state for all portal entities in a world. Called once per frame during system updates.
void PortalSystem_Update(World2d *world);

// Validate a portal entry and defer the same-world or cross-world transfer until the command phase.
PortalTeleportResult PortalSystem_RequestTeleport(Universe *universe, EntityId portal_id, EntityId entity_id);

#endif
