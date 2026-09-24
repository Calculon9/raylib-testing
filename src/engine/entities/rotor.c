#include "entities/rotor.h"
#include "entities/entity_internal.h"

static const float rotor_angular_velocity = 2.0f;

// Validate the blade count required to generate a rotor surface.
static bool ValidateRotorParams(const Newtonoid2dParams *params)
{
    if (!Entity_ValidateDimensions(params))
    {
        return false;
    }

    if (params->vertice_count < 2 ||
        params->vertice_count > MAX_SHAPE_VERTICES / 6)
    {
        LOG_WARN("Invalid rotor blade count: blade_count = %d\n",
                 params->vertice_count);
        return false;
    }

    return true;
}

// Create a rotor entity and leave world registration to the caller.
Newtonoid2d *RotorEntity_Create(const Newtonoid2dParams *params)
{
    if (!ValidateRotorParams(params))
    {
        return NULL;
    }

    Surface2d surface = {0};
    surface.surface_vectors = CreateVertices_Rotor(
        params->vertice_count, params->width * 0.5f, params->height * 0.5f);
    Newtonoid2d *entity = CreateNewtonoid2d_Allocated(
        params->mass, params->anchor_position, params->velocity,
        params->acceleration, surface);
    if (!entity)
    {
        return NULL;
    }

    entity->shape_type = SHAPE_ROTOR;
    entity->angular_velocity = rotor_angular_velocity;
    entity->constraints |= ENTITY_CONSTRAINT_POSITION_LOCKED;
    Newtonoid_ConfigureRestitution(entity, params->restitution);
    Newtonoid_ConfigureFriction(entity, params->friction);
    return entity;
}
