#include "entities/gear.h"
#include "entities/entity_internal.h"

static const float gear_angular_velocity = 2.0f;

// Validate the tooth count required to generate a gear surface.
static bool ValidateGearParams(const Newtonoid2dParams *params)
{
    if (!Entity_ValidateDimensions(params))
    {
        return false;
    }

    if (params->vertice_count < 12 ||
        params->vertice_count > MAX_SHAPE_VERTICES / 3)
    {
        LOG_WARN("Invalid gear tooth count: tooth_count = %d\n",
                 params->vertice_count);
        return false;
    }

    return true;
}

// Create a gear entity and leave world registration to the caller.
Newtonoid2d *GearEntity_Create(const Newtonoid2dParams *params)
{
    if (!ValidateGearParams(params))
    {
        return NULL;
    }

    Surface2d surface = {0};
    surface.surface_vectors = CreateVertices_Gear(
        params->vertice_count, params->width * 0.5f, params->height * 0.5f);
    Newtonoid2d *entity = CreateNewtonoid2d_Allocated(
        params->mass, params->anchor_position, params->velocity,
        params->acceleration, surface);
    if (!entity)
    {
        return NULL;
    }

    entity->shape_type = SHAPE_GEAR;
    entity->angular_velocity = gear_angular_velocity;
    entity->constraints |= ENTITY_CONSTRAINT_POSITION_LOCKED;
    Newtonoid_ConfigureRestitution(entity, params->restitution);
    Newtonoid_ConfigureFriction(entity, params->friction);
    return entity;
}
