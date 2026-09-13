/**********************************************************************************************
 *
 * PORTAL MECHANIC MODULE
 *
 **********************************************************************************************/
#include "mechanics/portal.h"
#include "entities/entity_registry.h"
#include "system/command_queue.h"
#include "world/universe.h"

// Return whether this portal is currently suppressing re-entry by an entity.
static bool Portal_IsOnCooldown(const PortalEntity *portal, EntityId entity_id)
{
    return portal && portal->cooldown_remaining > 0 &&
           portal->cooldown_entity_id == entity_id;
}

// Apply one side of a portal pair's cooldown state to the entering entity.
static void Portal_StartCooldown(PortalEntity *portal, EntityId entity_id)
{
    if (!portal)
    {
        return;
    }

    portal->cooldown_entity_id = entity_id;
    portal->cooldown_remaining = portal->cooldown_frames;
}

// Link two registered portal components so each endpoint resolves the other by ID.
bool Portal_LinkPair(EntityId first_portal_id, EntityId second_portal_id)
{
    if (first_portal_id == INVALID_ENTITY_ID || second_portal_id == INVALID_ENTITY_ID || first_portal_id == second_portal_id)
    {
        return false;
    }

    EntityComponent *first_component = EntityRegistry_GetComponent(first_portal_id, ENTITY_COMPONENT_PORTAL);
    EntityComponent *second_component = EntityRegistry_GetComponent(second_portal_id, ENTITY_COMPONENT_PORTAL);
    if (!first_component || !second_component)
    {
        return false;
    }

    PortalEntity *first_portal = &first_component->data.portal;
    PortalEntity *second_portal = &second_component->data.portal;
    if (!PortalEntity_IsValid(first_portal) ||
        !PortalEntity_IsValid(second_portal))
    {
        return false;
    }

    first_portal->destination.portal_id = second_portal_id;
    second_portal->destination.portal_id = first_portal_id;
    first_portal->cooldown_entity_id = INVALID_ENTITY_ID;
    second_portal->cooldown_entity_id = INVALID_ENTITY_ID;
    first_portal->cooldown_remaining = 0;
    second_portal->cooldown_remaining = 0;
    return true;
}

// Check the portal and entity rules that are independent of world ownership.
bool Portal_IsEntityEligible(const PortalEntity *portal, EntityId portal_id, const Newtonoid2d *entity)
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

    if (EntityRegistry_GetComponent(entity->id, ENTITY_COMPONENT_PORTAL) || Portal_IsOnCooldown(portal, entity->id))
    {
        return false;
    }

    return portal->entrant_mask == ENTITY_FLAG_NONE ||
           (entity->entity_flags & portal->entrant_mask) != ENTITY_FLAG_NONE;
}

// Decrement the runtime cooldown state for portals owned by a world.
void Portal_TickCooldowns(World2d *world)
{
    if (!world)
    {
        return;
    }

    Newtonoid2d *objects = (Newtonoid2d *)world->objects.items;
    for (int object_index = 0; object_index < world->objects.count; object_index++)
    {
        Newtonoid2d *object = &objects[object_index];
        EntityComponent *component = EntityRegistry_GetComponent(object->id, ENTITY_COMPONENT_PORTAL);
        PortalEntity *portal = component ? &component->data.portal : NULL;
        if (!portal || portal->cooldown_remaining <= 0)
        {
            continue;
        }

        portal->cooldown_remaining--;
        if (portal->cooldown_remaining == 0)
        {
            portal->cooldown_entity_id = INVALID_ENTITY_ID;
        }
    }
}

// Validate a portal entry and defer the same-world or cross-world transfer until the command phase.
PortalTeleportResult Portal_RequestTeleport(Universe *universe, EntityId portal_id, EntityId entity_id)
{
    EntityComponent *component = EntityRegistry_GetComponent(portal_id, ENTITY_COMPONENT_PORTAL);
    PortalEntity *portal = component ? &component->data.portal : NULL;
    if (!universe || !PortalEntity_IsValid(portal))
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    int source_world_index = -1;
    Newtonoid2d *entity = Universe_GetEntityByID(universe, entity_id, &source_world_index);
    if (!Portal_IsEntityEligible(portal, portal_id, entity))
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    int portal_world_index = -1;
    Newtonoid2d *portal_entity = Universe_GetEntityByID(universe, portal_id, &portal_world_index);
    if (!portal_entity || portal_world_index != source_world_index)
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    EntityId destination_portal_id = portal->destination.portal_id;
    if (destination_portal_id == INVALID_ENTITY_ID || destination_portal_id == portal_id)
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    EntityComponent *destination_component = EntityRegistry_GetComponent(
        destination_portal_id, ENTITY_COMPONENT_PORTAL);
    PortalEntity *destination_portal = destination_component ? &destination_component->data.portal : NULL;
    int destination_world_index = -1;
    Newtonoid2d *destination_entity = Universe_GetEntityByID(
        universe, destination_portal_id, &destination_world_index);
    if (!destination_entity || !PortalEntity_IsValid(destination_portal) || Portal_IsOnCooldown(destination_portal, entity_id))
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
                           entity->collision_mask))
    {
        return PORTAL_TELEPORT_REJECTED;
    }

    Portal_StartCooldown(portal, entity_id);
    Portal_StartCooldown(destination_portal, entity_id);
    return PORTAL_TELEPORT_QUEUED;
}
