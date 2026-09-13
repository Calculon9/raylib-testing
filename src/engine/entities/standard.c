#include "entities/standard.h"
#include "entities/entity_internal.h"

// Resolve ordinary shape defaults and validate the resulting vertex count.
static bool ResolveStandardShape(const Newtonoid2dParams *params, ShapeType *out_shape_type,
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

// Create an ordinary entity and leave world registration to the caller.
Newtonoid2d *StandardEntity_Create(const Newtonoid2dParams *params)
{
    if (!Entity_ValidateDimensions(params))
    {
        return NULL;
    }

    ShapeType shape_type = SHAPE_AUTO;
    int vertex_count = 0;
    if (!ResolveStandardShape(params, &shape_type, &vertex_count))
    {
        return NULL;
    }

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

    Newtonoid2d *entity = CreateNewtonoid2d_Allocated(
        params->mass, params->anchor_position, params->velocity,
        params->acceleration, surface);
    if (!entity)
    {
        return NULL;
    }

    entity->shape_type = shape_type;
    Newtonoid_ConfigureRestitution(entity, params->restitution);
    Newtonoid_ConfigureFriction(entity, params->friction);
    return entity;
}
