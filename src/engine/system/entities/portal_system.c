/**********************************************************************************************
 *
 * PORTAL SYSTEM MODULE
 *
 **********************************************************************************************/
#include "system/entities/portal_system.h"
#include "entities/entity_registry.h"
#include "system/command_system.h"
#include "world/universe.h"

// Return whether this portal is currently suppressing re-entry by an entity.
static bool PortalSystem_IsOnCooldown(const PortalComponent *portal, EntityId entity_id)
{
    return portal && portal->cooldown_frames_remaining > 0 &&
           portal->cooldown_entity_id == entity_id;
}

// Apply one side of a portal pair's cooldown state to the entering entity.
static void PortalSystem_StartCooldown(PortalComponent *portal, EntityId entity_id)
{
    if (!portal)
    {
        return;
    }

    portal->cooldown_entity_id = entity_id;
    portal->cooldown_frames_remaining = portal->cooldown_frames;
}

// Link two registered portal entities so each endpoint resolves the other as destination.
bool PortalSystem_LinkPair(EntityId first_portal_id, EntityId second_portal_id)
{
    if (first_portal_id == INVALID_ENTITY_ID || second_portal_id == INVALID_ENTITY_ID || first_portal_id == second_portal_id)
    {
        return false;
    }

    PortalEntity first_portal = PortalEntity_GetView(first_portal_id);
    PortalEntity second_portal = PortalEntity_GetView(second_portal_id);
    if (!PortalEntity_IsValid(&first_portal) || !PortalEntity_IsValid(&second_portal))
    {
        return false;
    }

    PortalComponent *first_component = first_portal.portal_component;
    PortalComponent *second_component = second_portal.portal_component;

    if ((first_component->portal_destination_id != INVALID_ENTITY_ID &&
         first_component->portal_destination_id != second_portal_id) ||
        (second_component->portal_destination_id != INVALID_ENTITY_ID &&
         second_component->portal_destination_id != first_portal_id))
    {
        return false;
    }

    // Store the pair on the portal components and reset both cooldowns.
    first_component->portal_destination_id = second_portal_id;
    second_component->portal_destination_id = first_portal_id;
    first_component->cooldown_entity_id = INVALID_ENTITY_ID;
    second_component->cooldown_entity_id = INVALID_ENTITY_ID;
    first_component->cooldown_frames_remaining = 0;
    second_component->cooldown_frames_remaining = 0;
    return true;
}

// Check the portal and entity rules.
bool PortalSystem_IsEntityEligible(const PortalEntity *portal, EntityId portal_id, const Newtonoid2d *entity)
{
    if (!PortalEntity_IsValid(portal) || portal_id == INVALID_ENTITY_ID ||
        !entity || entity->id == INVALID_ENTITY_ID || entity->id == portal_id)
    {
        return false;
    }

    if (!(entity->status_flags & ENTITY_STATUS_FLAG_ALIVE))
    {
        return false;
    }

    if (EntityRegistry_GetPortal(entity->id) || PortalSystem_IsOnCooldown(portal->portal_component, entity->id))
    {
        return false;
    }

    return portal->portal_component->entrant_roles == ENTITY_ROLE_NONE ||
           (entity->roles & portal->portal_component->entrant_roles) != ENTITY_ROLE_NONE;
}

// Update cooldown state for all portal entities in a world. Called once per frame during system updates.
void PortalSystem_Update(World2d *world)
{
    if (!world)
    {
        return;
    }

    Newtonoid2d *objects = (Newtonoid2d *)world->objects.items;
    for (int object_index = 0; object_index < world->objects.count; object_index++)
    {
        Newtonoid2d *object = &objects[object_index];
        PortalComponent *portal = EntityRegistry_GetPortal(object->id);
        if (!portal || portal->cooldown_frames_remaining <= 0)
        {
            continue;
        }

        portal->cooldown_frames_remaining--;
        if (portal->cooldown_frames_remaining == 0)
        {
            portal->cooldown_entity_id = INVALID_ENTITY_ID;
        }
    }
}

// Validate a portal entry and defer the same-world or cross-world transfer until the command phase.
PortalTeleportResult PortalSystem_RequestTeleport(Universe *universe, EntityId portal_id, EntityId entity_id)
{
    PortalEntity portal = PortalEntity_GetView(portal_id);
    if (!universe || !PortalEntity_IsValid(&portal))
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    int source_world_index = -1;
    Newtonoid2d *entity = Universe_GetEntityByID(universe, entity_id, &source_world_index);
    if (!PortalSystem_IsEntityEligible(&portal, portal_id, entity))
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    int portal_world_index = -1;
    Newtonoid2d *portal_body = Universe_GetEntityByID(universe, portal_id, &portal_world_index);
    if (!portal_body || portal_world_index != source_world_index)
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    EntityId destination_portal_id = portal.portal_component->portal_destination_id;
    if (destination_portal_id == INVALID_ENTITY_ID || destination_portal_id == portal_id)
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    PortalEntity destination_portal = PortalEntity_GetView(destination_portal_id);
    int destination_world_index = -1;
    Newtonoid2d *destination_entity = Universe_GetEntityByID(
        universe, destination_portal_id, &destination_world_index);
    if (!destination_entity || !PortalEntity_IsValid(&destination_portal) || PortalSystem_IsOnCooldown(destination_portal.portal_component, entity_id))
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    World2d *destination_world = Universe_GetWorld(universe, destination_world_index);
    Vector2d destination_coordinates = destination_entity->anchor_position;
    if (!destination_world || GetIndexFromCoords(&destination_world->grid_space.space,
                                                 destination_coordinates) < 0)
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    if (!EnqueueMoveEntity(entity->id, source_world_index,
                           destination_world_index,
                           destination_entity->parent_id,
                           destination_coordinates,
                           entity->collision_role_mask))
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    PortalSystem_StartCooldown(portal.portal_component, entity_id);
    PortalSystem_StartCooldown(destination_portal.portal_component, entity_id);
    return PORTAL_TELEPORT_QUEUED;
}
