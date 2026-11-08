/**********************************************************************************************
 *
 * CONTAINER CORRECTION (pure maths)
 *
 * Positional correction of an entity against the world's outer rectangle (the immovable
 * container). Each axis is resolved independently at FULL depth, then the centre is clamped
 * inside the rectangle. Impulses are applied by the caller; this only computes the displacement.
 *
 **********************************************************************************************/
#ifndef CONTAINER_CORRECTION_H
#define CONTAINER_CORRECTION_H

#include "math/cvectors.h"

typedef struct ContainerCorrection
{
    float depth_x;         // Penetration into the left/right wall (0 = none).
    float depth_y;         // Penetration into the top/bottom wall (0 = none).
    Vector2d normal_x;     // Inward normal of the penetrated x wall (valid when depth_x > 0).
    Vector2d normal_y;     // Inward normal of the penetrated y wall (valid when depth_y > 0).
    Vector2d shift;        // Total displacement to apply: full-depth push-out plus centre clamp.
} ContainerCorrection;

// Compute the correction for an AABB (bounds_origin/bounds_size) whose centre is `centre`, against
// the rectangle [grid_min, grid_max]. A zero shift means the entity is already legal.
static inline ContainerCorrection ContainerCorrection_Compute(Vector2d bounds_origin, Vector2d bounds_size,
                                                              Vector2d centre, Vector2d grid_min,
                                                              Vector2d grid_max)
{
    ContainerCorrection c = {0};
    float e_max_x = bounds_origin.x + bounds_size.x;
    float e_max_y = bounds_origin.y + bounds_size.y;

    // Each axis picks at most one wall (left before right, top before bottom, as before).
    if (bounds_origin.x < grid_min.x)
    {
        c.depth_x = grid_min.x - bounds_origin.x;
        c.normal_x = (Vector2d){1.0f, 0.0f};
    }
    else if (e_max_x > grid_max.x)
    {
        c.depth_x = e_max_x - grid_max.x;
        c.normal_x = (Vector2d){-1.0f, 0.0f};
    }

    if (bounds_origin.y < grid_min.y)
    {
        c.depth_y = grid_min.y - bounds_origin.y;
        c.normal_y = (Vector2d){0.0f, 1.0f};
    }
    else if (e_max_y > grid_max.y)
    {
        c.depth_y = e_max_y - grid_max.y;
        c.normal_y = (Vector2d){0.0f, -1.0f};
    }

    // Full-depth push-out on each penetrated axis (the container is immovable).
    c.shift.x = c.normal_x.x * c.depth_x;
    c.shift.y = c.normal_y.y * c.depth_y;

    // Safety clamp: the centre must end inside the rectangle (covers oversized entities).
    float cx = centre.x + c.shift.x;
    float cy = centre.y + c.shift.y;
    if (cx < grid_min.x)
        c.shift.x += grid_min.x - cx;
    else if (cx > grid_max.x)
        c.shift.x += grid_max.x - cx;
    if (cy < grid_min.y)
        c.shift.y += grid_min.y - cy;
    else if (cy > grid_max.y)
        c.shift.y += grid_max.y - cy;

    return c;
}

#endif // CONTAINER_CORRECTION_H
