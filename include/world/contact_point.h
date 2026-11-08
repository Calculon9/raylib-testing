/**********************************************************************************************
 *
 * CONTACT POINT (pure maths)
 *
 * One contact point per pair or wall is cached by the solver, and with accumulated, warm-started
 * impulses a badly placed point becomes a PERSISTENT torque (a flat box resting on a floor would be
 * pushed up at one corner). FindDeepestVertex returns the first extreme vertex, which for a flat face
 * is an arbitrary corner. These helpers instead return the CENTRE of the contact face: the vertices
 * within a relative tolerance of the deepest projection form the face, and the point is the middle of
 * that face's tangential interval. A single point at the centre of the overlap is the lumped equivalent
 * of a two-point manifold for rocking: uniform support pressure gives zero net torque.
 *
 * The tolerance is RELATIVE to the penetration and the body size, never absolute, so the point does
 * not flip between face centre and corner at a tiny rotation. It is continuous in the pose except at
 * the face-to-vertex switch (a tilt of roughly tol / width), which is a real change of contact feature.
 *
 **********************************************************************************************/
#ifndef CONTACT_POINT_H
#define CONTACT_POINT_H

#include <math.h>
#include <stddef.h>
#include "math/cvectors.h"

// Face tolerance used by the detectors: a quarter of the penetration, but never below 2 % of the body extent.
static inline float ContactPoint_FaceTolerance(float depth, float body_extent)
{
    return fmaxf(0.25f * depth, 0.02f * body_extent);
}

// Deepest support face of a body into a wall. `inward` points from the wall into the legal region (the
// direction the body is pushed out). Vertices with (v . inward) <= min + tol form the face; their extent along
// the tangent t = (-inward.y, inward.x) is [*t_lo, *t_hi]. Returns the deepest point at the centre of that
// interval. Vertex order does not change the result.
static inline Vector2d FindDeepestSupportInterval(const Vector2d *verts, size_t count, Vector2d inward, float tol,
                                                  float *t_lo, float *t_hi)
{
    float deepest = INFINITY;
    for (size_t i = 0; i < count; i++)
    {
        float proj = verts[i].x * inward.x + verts[i].y * inward.y;
        if (proj < deepest)
            deepest = proj;
    }
    Vector2d t = {-inward.y, inward.x};
    float lo = INFINITY, hi = -INFINITY;
    for (size_t i = 0; i < count; i++)
    {
        float proj = verts[i].x * inward.x + verts[i].y * inward.y;
        if (proj <= deepest + tol)
        {
            float along = verts[i].x * t.x + verts[i].y * t.y;
            lo = fminf(lo, along);
            hi = fmaxf(hi, along);
        }
    }
    if (t_lo)
        *t_lo = lo;
    if (t_hi)
        *t_hi = hi;
    float mid = 0.5f * (lo + hi);
    return (Vector2d){inward.x * deepest + t.x * mid, inward.y * deepest + t.y * mid};
}

// Contact point of two overlapping bodies. `n` points from A to B. Each body's contact face is the set of its
// vertices within `tol` of its deepest projection on n (A: maximum, B: minimum); the point lies halfway between
// the two face planes, at the centre of the faces' common tangential interval (or of the gap between them for
// a corner touch).
static inline Vector2d ContactPoint_FaceCentre(const Vector2d *a_verts, size_t a_count, const Vector2d *b_verts,
                                               size_t b_count, Vector2d n, float tol)
{
    Vector2d t = {-n.y, n.x};
    float max_a = -INFINITY, min_b = INFINITY;
    for (size_t i = 0; i < a_count; i++)
        max_a = fmaxf(max_a, a_verts[i].x * n.x + a_verts[i].y * n.y);
    for (size_t i = 0; i < b_count; i++)
        min_b = fminf(min_b, b_verts[i].x * n.x + b_verts[i].y * n.y);

    float lo_a = INFINITY, hi_a = -INFINITY, lo_b = INFINITY, hi_b = -INFINITY;
    for (size_t i = 0; i < a_count; i++)
        if (a_verts[i].x * n.x + a_verts[i].y * n.y >= max_a - tol)
        {
            float along = a_verts[i].x * t.x + a_verts[i].y * t.y;
            lo_a = fminf(lo_a, along);
            hi_a = fmaxf(hi_a, along);
        }
    for (size_t i = 0; i < b_count; i++)
        if (b_verts[i].x * n.x + b_verts[i].y * n.y <= min_b + tol)
        {
            float along = b_verts[i].x * t.x + b_verts[i].y * t.y;
            lo_b = fminf(lo_b, along);
            hi_b = fmaxf(hi_b, along);
        }

    float lo = fmaxf(lo_a, lo_b);
    float hi = fminf(hi_a, hi_b);
    float t_mid = 0.5f * (lo + hi); // For disjoint faces (lo > hi) this is the midpoint of the gap.
    float n_mid = 0.5f * (max_a + min_b);
    return (Vector2d){n.x * n_mid + t.x * t_mid, n.y * n_mid + t.y * t_mid};
}

#endif // CONTACT_POINT_H
