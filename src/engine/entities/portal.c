/**********************************************************************************************
 *
 * PORTAL ENTITY MODULE
 *
 **********************************************************************************************/
#include "entities/portal.h"
#include "entities/entity_internal.h"
#include "entities/entity_registry.h"

// Create the physical portal geometry; component initialisation and attachment is handled by the factory.
Newtonoid2d *PortalEntity_Create(const Newtonoid2dParams *params)
{
    if (!Entity_ValidateDimensions(params))
    {
        return NULL;
    }

    // Create the base entity with portal-specific geometry.
    Surface2d surface = {0};
    surface.surface_vectors = CreateVertices_Portal((Vector2d){params->width, params->height});
    Newtonoid2d *entity = CreateNewtonoid2d_Allocated(params->mass, params->anchor_position, params->velocity,
        params->acceleration, surface);
    if (!entity)
    {
        return NULL;
    }

    entity->shape_type = SHAPE_ELLIPSE;
    Newtonoid_ConfigureRestitution(entity, params->restitution);
    Newtonoid_ConfigureFriction(entity, params->friction);

    return entity;
}

// Initialise portal component state.
bool PortalComponent_Initialise(PortalComponent *component, const PortalComponentParams *params)
{
    if (!component || !params || params->cooldown_frames < 0)
    {
        return false;
    }

    *component = (PortalComponent){
        .owner_id = INVALID_ENTITY_ID,
        .entrant_roles = params->entrant_roles,
        .portal_destination_id = INVALID_ENTITY_ID,
        .cooldown_frames = params->cooldown_frames,
        .cooldown_entity_id = INVALID_ENTITY_ID,
        .cooldown_frames_remaining = 0};
    return true;
}

// Resolve a transient portal view from the base-entity and component registries.
PortalEntity PortalEntity_GetView(EntityId entity_id)
{
    Newtonoid2d *base = EntityRegistry_GetEntity(entity_id);
    PortalComponent *portal_component = EntityRegistry_GetPortal(entity_id);
    if (!base || !portal_component)
    {
        return (PortalEntity){0};
    }

    return (PortalEntity){
        .base = base,
        .portal_component = portal_component};
}

// Check the component's configured and mutable cooldown values.
bool PortalComponent_IsValid(const PortalComponent *component)
{
    return component && component->cooldown_frames >= 0 &&
           component->cooldown_frames_remaining >= 0;
}

// Check both references in a resolved portal view and its component state.
bool PortalEntity_IsValid(const PortalEntity *portal)
{
    return portal && portal->base &&
           PortalComponent_IsValid(portal->portal_component);
}
