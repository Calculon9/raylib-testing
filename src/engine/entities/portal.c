/**********************************************************************************************
 *
 * PORTAL ENTITY MODULE
 *
 **********************************************************************************************/
#include "entities/portal.h"
#include "entities/entity_internal.h"

// Create the physical portal and initialise its portal-specific state.
Newtonoid2d *PortalEntity_Create(const Newtonoid2dParams *params, PortalEntity *out_portal)
{
    if (!out_portal || !Entity_ValidateDimensions(params))
    {
        return NULL;
    }

    // Create the base entity.
    Surface2d surface = {0};
    surface.surface_vectors = CreateVertices_Portal((Vector2d){params->width, params->height});
    Newtonoid2d *entity = CreateNewtonoid2d_Allocated(params->mass, params->anchor_position, params->velocity,
        params->acceleration, surface);
    if (!entity)
    {
        return NULL;
    }

    entity->shape_type = SHAPE_ELLIPSE;
    entity->collision_mask = ENTITY_FLAG_NONE;
    entity->attribute_flags &= ~ENTITY_ATTR_FLAG_DAMAGEABLE;
    // Mark portal as position-locked and sensor-only: overlap dispatch runs
    // portal mechanics, while the explicit no-response flag skips impulses.
    entity->attribute_flags |= (ENTITY_ATTR_FLAG_POSITION_LOCKED | ENTITY_ATTR_FLAG_SENSOR | ENTITY_ATTR_FLAG_NO_CONTACT_RESPONSE);
    Newtonoid_ConfigureRestitution(entity, params->restitution);
    Newtonoid_ConfigureFriction(entity, params->friction);

    PortalDestination destination = {.portal_id = INVALID_ENTITY_ID};
    if (!PortalEntity_Initialise(out_portal, destination, ENTITY_FLAG_NEWTONOID | ENTITY_FLAG_PROJECTILE,30))
    {
        ClearLArray(&entity->surface.surface_vectors);
        Deallocate((void **)&entity, sizeof(Newtonoid2d));
        return NULL;
    }

    return entity;
}

// Initialise portal state that is kept outside the general Newtonoid structure.
bool PortalEntity_Initialise(PortalEntity *portal, PortalDestination destination,
                             EntityTypeFlags entrant_mask, int cooldown_frames)
{
    if (!portal || cooldown_frames < 0)
    {
        return false;
    }

    *portal = (PortalEntity){
        .destination = destination,
        .entrant_mask = entrant_mask,
        .cooldown_frames = cooldown_frames,
        .cooldown_entity_id = INVALID_ENTITY_ID,
        .cooldown_remaining = 0};
    return true;
}

// Check the identity and timing values required before a portal can be used.
bool PortalEntity_IsValid(const PortalEntity *portal)
{
    return portal && portal->cooldown_frames >= 0 && portal->cooldown_remaining >= 0;
}
