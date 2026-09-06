#include "system/entity_creation.h"

// Validate the dimensions shared by ordinary and functional entity factories.
static bool ValidateEntityDimensions(const Newtonoid2dParams *params)
{
    if (!params || params->width <= 0.0f || params->height <= 0.0f)
    {
        if (params)
        {
            LOG_WARN("Invalid size: width = %f, height = %f\n",
                     params->width, params->height);
        }
        return false;
    }

    return true;
}

// Resolve ordinary shape defaults while keeping functional archetypes separate.
static bool ResolveOrdinaryShape(const Newtonoid2dParams *params,
                                 ShapeType *out_shape_type,
                                 int *out_vertex_count)
{
    if (!params || !out_shape_type || !out_vertex_count)
    {
        return false;
    }

    ShapeType shape_type = params->shape_type;
    int vertex_count = params->vertice_count;
    if (shape_type == SHAPE_AUTO)
    {
        if (vertex_count == 3)
        {
            shape_type = SHAPE_TRIANGLE;
        }
        else if (vertex_count == 4)
        {
            shape_type = SHAPE_SQUARE;
        }
        else
        {
            shape_type = SHAPE_POLYGON;
        }
    }

    switch (shape_type)
    {
    case SHAPE_TRIANGLE:
        vertex_count = 3;
        break;
    case SHAPE_SQUARE:
    case SHAPE_RECTANGLE:
        vertex_count = 4;
        break;
    case SHAPE_CIRCLE:
    case SHAPE_POLYGON:
        break;
    default:
        return false;
    }

    if (vertex_count < 3 || vertex_count > MAX_SHAPE_VERTICES)
    {
        LOG_WARN("Invalid ordinary vertex count: vertex_count = %d\n", vertex_count);
        return false;
    }

    *out_shape_type = shape_type;
    *out_vertex_count = vertex_count;
    return true;
}

// Validate the parameter that controls the geometry of each functional archetype.
static bool ValidateFunctionalArchetype(EntityArchetype archetype, int vertex_count)
{
    switch (archetype)
    {
    case ENTITY_ARCHETYPE_ROTOR:
        if (vertex_count < 2 || vertex_count > MAX_SHAPE_VERTICES / 6)
        {
            LOG_WARN("Invalid rotor blade count: blade_count = %d\n", vertex_count);
            return false;
        }
        return true;
    case ENTITY_ARCHETYPE_GEAR:
        if (vertex_count < 3 || vertex_count > MAX_SHAPE_VERTICES / 3)
        {
            LOG_WARN("Invalid gear tooth count: tooth_count = %d\n", vertex_count);
            return false;
        }
        return true;
    case ENTITY_ARCHETYPE_PORTAL:
        return true;
    default:
        return false;
    }
}

// Build a functional entity using its archetype as the factory authority.
static Newtonoid2d CreateFunctionalEntity(const Newtonoid2dParams *params)
{
    Vector2d dimensions = {params->width, params->height};
    switch (params->archetype)
    {
    case ENTITY_ARCHETYPE_ROTOR:
        return CreateNewtonoid2d_Rotor(
            params->vertice_count, dimensions, params->mass,
            params->anchor_position, params->velocity, params->acceleration);
    case ENTITY_ARCHETYPE_GEAR:
        return CreateNewtonoid2d_Gear(
            params->vertice_count, dimensions, params->mass,
            params->anchor_position, params->velocity, params->acceleration);
    case ENTITY_ARCHETYPE_PORTAL:
        return CreateNewtonoid2d_Portal(
            dimensions, params->mass, params->anchor_position,
            params->velocity, params->acceleration);
    default:
        return (Newtonoid2d){0};
    }
}

// Build an ordinary entity from its resolved geometry parameters.
static Newtonoid2d CreateOrdinaryEntity(const Newtonoid2dParams *params,
                                        ShapeType shape_type,
                                        int vertex_count)
{
    Surface2d surface = {0};
    if (shape_type == SHAPE_SQUARE || shape_type == SHAPE_RECTANGLE)
    {
        surface = CreateSurface_Rectangular(
            (Vector2d){params->width, params->height});
    }
    else
    {
        surface.surface_vectors = CreateVertices_Symmetric(
            vertex_count, params->width * 0.5f, params->height * 0.5f);
    }

    Newtonoid2d entity = CreateNewtonoid2d(
        params->mass, params->anchor_position, params->velocity,
        params->acceleration, surface);
    entity.shape_type = shape_type;
    return entity;
}

// Release failed geometry and return one heap-owned entity for world registration.
static Newtonoid2d *AllocateCreatedEntity(Newtonoid2d created,
                                           const Newtonoid2dParams *params)
{
    if (!created.surface.surface_vectors.items ||
        created.surface.surface_vectors.count < 3)
    {
        ClearLArray(&created.surface.surface_vectors);
        return NULL;
    }

    Newtonoid2d *entity = AllocateBytes(sizeof(*entity));
    if (!entity)
    {
        ClearLArray(&created.surface.surface_vectors);
        LOG_WARN("Failed to allocate new physical object. World entity pool full.\n");
        return NULL;
    }

    *entity = created;
    Newtonoid_ConfigureRestitution(entity, params->restitution);
    Newtonoid_ConfigureFriction(entity, params->friction);
    LOG_INFO("Successfully spawned Entity ID: %d [Type: %d] at Position (%.2f, %.2f)\n",
             entity->id, entity->shape_type,
             entity->anchor_position.x, entity->anchor_position.y);
    return entity;
}

// Convert editor and command-queue parameters into an allocated Newtonoid.
Newtonoid2d *CreateEntityFromParams(const Newtonoid2dParams *params)
{
    if (!ValidateEntityDimensions(params))
    {
        return NULL;
    }

    Newtonoid2d created = {0};
    if (params->archetype == ENTITY_ARCHETYPE_NONE)
    {
        ShapeType shape_type = SHAPE_AUTO;
        int vertex_count = 0;
        if (!ResolveOrdinaryShape(params, &shape_type, &vertex_count))
        {
            return NULL;
        }
        created = CreateOrdinaryEntity(params, shape_type, vertex_count);
    }
    else
    {
        if (!ValidateFunctionalArchetype(params->archetype,
                                          params->vertice_count))
        {
            return NULL;
        }
        created = CreateFunctionalEntity(params);
    }

    return AllocateCreatedEntity(created, params);
}
