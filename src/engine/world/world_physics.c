/**********************************************************************************************
 *
 * WORLD PHYSICS
 *
 **********************************************************************************************/

#include "world/world_internal.h"
#include "combat/projectile.h"
#include "entities/entity_registry.h"
#include "system/entities/portal_system.h"
#include "world/object_gizmos.h" // IsGizmoEnabled(GIZMO_COLLISIONS): gates collision-marker creation
#include "world/collision_pair_key.h"
#include "world/container_correction.h"
#include "world/contact_point.h"

// Apply a local basis to a vector using the project's standard basis-apply idiom
// (the same maths as Newtonoid_TransformVertices / Frame_TransformPoint_ToParent):
// result = u * v.x + v * v.y. Used to warp an entity's motion vectors through a
// cell's local warp_basis.
static inline Vector2d WarpApplyBasis(Basis2d basis, Vector2d v)
{
    return VectorSum_2d(VectorScale_2d(basis.u, v.x), VectorScale_2d(basis.v, v.y));
}

// Return true when the given basis is the identity basis, so inert cells can be
// skipped cheaply. Exact float comparison is valid because identity cells are
// written with the IDENTITY_BASIS_2D literal at the fresh-build site.
static inline bool WarpIsIdentity(Basis2d b)
{
    return b.u.x == 1.0f && b.u.y == 0.0f && b.v.x == 0.0f && b.v.y == 1.0f;
}

// Find the scalar projection interval of a polygon onto an axis. SAT reduces
// each 2D shape to this 1D interval using projection = vertex dot unit_axis.
static void FindMinMaxProjection(Vector2d *vertices, size_t count, Vector2d axis, float *out_min, float *out_max)
{
    *out_min = INFINITY;
    *out_max = -INFINITY;

    for (size_t i = 0; i < count; i++)
    {
        float proj = VectorDot_2d(vertices[i], axis);
        if (proj < *out_min)
            *out_min = proj;
        if (proj > *out_max)
            *out_max = proj;
    }
}

// Calculate interval overlap as min(max_a, max_b) - max(min_a, min_b).
// A non-positive result is a separating gap (or only a touching boundary).
static float CalculateOverlap(float a_min, float a_max, float b_min, float b_max)
{
    return (a_max < b_max ? a_max : b_max) - (a_min > b_min ? a_min : b_min);
}

// Find the vertex furthest along one side of an axis. The selected extreme is
// used as the contact point for debug markers.
Vector2d FindDeepestVertex(Vector2d *vertices, size_t count, Vector2d axis, bool find_minimum)
{
    Vector2d deepest = vertices[0];
    float extreme_proj = VectorDot_2d(vertices[0], axis);

    for (size_t i = 1; i < count; i++)
    {
        float proj = VectorDot_2d(vertices[i], axis);
        if ((find_minimum && proj < extreme_proj) || (!find_minimum && proj > extreme_proj))
        {
            extreme_proj = proj;
            deepest = vertices[i];
        }
    }

    return deepest;
}

static float CalculateHullCross(Vector2d point_a, Vector2d point_b, Vector2d point_c)
{
    Vector2d edge_ab = VectorDiff_2d(point_b, point_a);
    Vector2d edge_ac = VectorDiff_2d(point_c, point_a);
    return (edge_ab.x * edge_ac.y) - (edge_ab.y * edge_ac.x);
}

static size_t BuildConvexHull(Vector2d *vertices, size_t vertex_count,
                              Vector2d *out_hull)
{
    const float epsilon = 0.000001f;
    if (!vertices || !out_hull || vertex_count == 0 || vertex_count > MAX_SHAPE_VERTICES)
    {
        return 0;
    }

    Vector2d sorted_vertices[MAX_SHAPE_VERTICES];
    for (size_t vertex_index = 0; vertex_index < vertex_count; vertex_index++)
    {
        sorted_vertices[vertex_index] = vertices[vertex_index];
    }

    // Insertion sort is sufficient for the small, fixed maximum vertex count.
    for (size_t sort_index = 1; sort_index < vertex_count; sort_index++)
    {
        Vector2d candidate = sorted_vertices[sort_index];
        size_t insert_index = sort_index;
        while (insert_index > 0)
        {
            Vector2d previous = sorted_vertices[insert_index - 1];
            bool comes_before = candidate.x < previous.x ||
                                (candidate.x == previous.x && candidate.y < previous.y);
            if (!comes_before)
            {
                break;
            }

            sorted_vertices[insert_index] = previous;
            insert_index--;
        }
        sorted_vertices[insert_index] = candidate;
    }

    size_t unique_count = 0;
    for (size_t vertex_index = 0; vertex_index < vertex_count; vertex_index++)
    {
        if (unique_count == 0 ||
            fabsf(sorted_vertices[vertex_index].x - sorted_vertices[unique_count - 1].x) > epsilon ||
            fabsf(sorted_vertices[vertex_index].y - sorted_vertices[unique_count - 1].y) > epsilon)
        {
            sorted_vertices[unique_count++] = sorted_vertices[vertex_index];
        }
    }

    if (unique_count <= 2)
    {
        for (size_t vertex_index = 0; vertex_index < unique_count; vertex_index++)
        {
            out_hull[vertex_index] = sorted_vertices[vertex_index];
        }
        return unique_count;
    }

    size_t hull_count = 0;
    for (size_t vertex_index = 0; vertex_index < unique_count; vertex_index++)
    {
        while (hull_count >= 2 &&
               CalculateHullCross(out_hull[hull_count - 2], out_hull[hull_count - 1],
                                  sorted_vertices[vertex_index]) <= epsilon)
        {
            hull_count--;
        }
        out_hull[hull_count++] = sorted_vertices[vertex_index];
    }

    size_t lower_hull_count = hull_count;
    for (int vertex_index = (int)unique_count - 2; vertex_index >= 0; vertex_index--)
    {
        while (hull_count > lower_hull_count &&
               CalculateHullCross(out_hull[hull_count - 2], out_hull[hull_count - 1],
                                  sorted_vertices[vertex_index]) <= epsilon)
        {
            hull_count--;
        }
        out_hull[hull_count++] = sorted_vertices[vertex_index];
    }

    // The first point is repeated at the end while building the two hull halves.
    return hull_count > 1 ? hull_count - 1 : hull_count;
}

// Combine two material coefficients using the geometric mean:
//
//     e_collision = sqrt(e_a * e_b)
//
// This keeps the result symmetric and ensures a zero-restitution material
// removes the normal bounce even when it collides with a highly elastic
// material.
float CalcCollisionRestitution(const Newtonoid2d *a, const Newtonoid2d *b)
{
    float restitution_a = fmaxf(0.0f, fminf(a->restitution, 1.0f));
    float restitution_b = fmaxf(0.0f, fminf(b->restitution, 1.0f));
    float restitution_product = restitution_a * restitution_b;
    return sqrtf(fmaxf(restitution_product, 0.0f));
}

// Combine two material friction coefficients using the geometric mean:
//
//     mu_collision = sqrt(mu_a * mu_b)
//
// Friction is not limited to one because some material models use coefficients
// greater than one; only negative values are invalid.
float CalcCollisionFriction(const Newtonoid2d *a, const Newtonoid2d *b)
{
    float friction_a = fmaxf(a->friction, 0.0f);
    float friction_b = fmaxf(b->friction, 0.0f);
    float friction_product = friction_a * friction_b;
    return sqrtf(fmaxf(friction_product, 0.0f));
}

void PhysicsUpdateJob(void *context, int start, int end)
{
    if (!context)
        return;

    World2d *world = (World2d *)context;
    GridSpace2d *space_entity = &world->grid_space;
    Space2d *space = &space_entity->space;
    CellOccupancy *occupancy = &world->cell_occupancy;

    // The integration step uses the same clamped delta as UpdateWorld (see MAX_PHYSICS_DELTA_TIME).
    float physics_dt = fminf(frame_counter.delta_time, MAX_PHYSICS_DELTA_TIME);

    LArray *objects = &world->objects;
    Newtonoid2d *newtonoids = (Newtonoid2d *)objects->items;
    for (int index = start; index < end && index < (int)objects->count; index++)
    {
        Newtonoid2d *obj = &newtonoids[index];

        if (!(obj->status_flags & ENTITY_STATUS_FLAG_ALIVE) || (obj->roles & ENTITY_ROLE_EFFECT) || obj->parent_id != space_entity->object.id)
            continue;

        // Gravity is an environmental acceleration added for this step only;
        // restore authored acceleration afterwards so it is not accumulated
        // repeatedly into the entity's persistent state.
        Vector2d authored_acceleration = obj->acceleration;
        if (!(obj->constraints & ENTITY_CONSTRAINT_POSITION_LOCKED))
        {
            obj->acceleration.y += world->gravity;
        }

        // Kinematic LEVEL-2 cell warp: a cell carrying a non-identity warp_basis
        // bends this entity's MOTION for this step only. Point-sample the cell by
        // the entity's centre (anchor_position); GetCellFromCoords returns NULL
        // out of bounds. Mirroring the gravity save/restore idiom above, we save
        // the authored velocity AND acceleration, replace them with their
        // basis-transformed versions so CalcVectors integrates the warped motion,
        // then restore both afterwards — warp is never accumulated into persistent
        // entity state. Re-sampling every frame means boundary transitions need no
        // per-entity state. NOTE: this is deliberately level-2 (motion only): the
        // collision / AABB / spatial-map code below stays world-space via
        // space->frame.basis and is intentionally left unwarped.
        // Only a warped entity needs its velocity saved/restored. The NON-warp path must
        // leave velocity to CalcVectors, which integrates v += a*dt — restoring velocity
        // unconditionally would discard that per-frame integration for EVERY entity (gravity
        // would read as constant drift, not acceleration). So scope the velocity save/restore
        // to the warp branch; acceleration is always restored (its gravity tweak above is the
        // per-step environmental add, never persisted — matching the original behaviour).
        Cell *warp_cell = GetCellFromCoords(space, obj->anchor_position);
        bool warped = (warp_cell && !WarpIsIdentity(warp_cell->warp_basis));
        Vector2d authored_velocity = obj->velocity;
        if (warped)
        {
            obj->velocity = WarpApplyBasis(warp_cell->warp_basis, obj->velocity);
            obj->acceleration = WarpApplyBasis(warp_cell->warp_basis, obj->acceleration);
        }

        CalcVectors(obj, physics_dt);

        // Restore the authored acceleration always (gravity/warp were per-step only). Restore
        // velocity ONLY when warped, so a non-warped entity keeps CalcVectors' v += a*dt.
        if (warped)
        {
            obj->velocity = authored_velocity;
        }
        obj->acceleration = authored_acceleration;

        // Map the post-integration position so rendering, hit-testing, and the
        // collision broad phase all observe the same location. Use transformed
        // vertices so rotation cannot make the grid footprint too small.
        Vector2d world_vertices[MAX_SHAPE_VERTICES] = {0};
        UpdateEntityBounds(obj, world_vertices);
        Vector2d snapped_aabb_verts[4] = {0};
        CalcSnappedAABB_Vertices(world_vertices, obj->surface.surface_vectors.count,
                                 ZERO_VECTOR_2D, snapped_aabb_verts);
        Matrix2x2 snapped_aabb_box = CalcAABBCoords_Tight(snapped_aabb_verts, 4, ZERO_VECTOR_2D);
        MapEntityToASpace(space, obj, snapped_aabb_box, occupancy);
        // Container walls are no longer resolved here: DetectContainerContacts caches them after the job and the
        // solver corrects them together with the pair contacts.
        Newtonoid_SyncOrientationToVelocity(obj);
    }
}

CollisionResult_SAT CheckForCollision_SAT(Newtonoid2d *a, Newtonoid2d *b)
{
    CollisionResult_SAT result = {0};
    result.is_colliding = false;

    if (!a || !b)
        return result;

    Vector2d a_world_vertices[MAX_SHAPE_VERTICES];
    Vector2d b_world_vertices[MAX_SHAPE_VERTICES];
    Vector2d a_hull[MAX_SHAPE_VERTICES];
    Vector2d b_hull[MAX_SHAPE_VERTICES];
    Vector2d *a_world = a_world_vertices;
    Vector2d *b_world = b_world_vertices;

    size_t a_count = a->surface.surface_vectors.count;
    size_t b_count = b->surface.surface_vectors.count;

    // Early exit for degenerate shapes
    if (a_count < 3 || b_count < 3)
        return result;

    // SAT applies to convex polygons with consistently ordered vertices. The
    // vertices are transformed once because every candidate axis reuses them.
    Newtonoid_TransformVertices(a, a_world_vertices, MAX_SHAPE_VERTICES);
    Newtonoid_TransformVertices(b, b_world_vertices, MAX_SHAPE_VERTICES);

    // Rotor outlines are concave, while this narrow phase requires convex
    // polygons. Use the outer hull for collision without changing the detailed
    // outline used by rendering.
    if (a->shape_type == SHAPE_ROTOR)
    {
        a_count = BuildConvexHull(a_world_vertices, a_count, a_hull);
        a_world = a_hull;
    }
    if (b->shape_type == SHAPE_ROTOR)
    {
        b_count = BuildConvexHull(b_world_vertices, b_count, b_hull);
        b_world = b_hull;
    }

    if (a_count < 3 || b_count < 3)
    {
        return result;
    }

    // Use current transformed bounds for the cheap rejection so manually
    // rotated objects cannot be rejected using stale unrotated AABBs. These axis-aligned boxes used as a cheap broad phase.
    Matrix2x2 a_world_bounds = CalcAABBCoords_Tight(a_world, (int)a_count, ZERO_VECTOR_2D);
    Matrix2x2 b_world_bounds = CalcAABBCoords_Tight(b_world, (int)b_count, ZERO_VECTOR_2D);
    if (!AABB2d_Overlaps(a_world_bounds, b_world_bounds))
        return result;

    float min_overlap_u = INFINITY;
    Vector2d final_u_axis = {0};
    int normal_owner = 0;

    // The Separating Axis Theorem says two convex polygons are disjoint if at
    // least one axis has non-overlapping projections. For a polygon, it is
    // sufficient to test the perpendicular to each edge of both shapes.
    // Test candidate axes from shape A.
    for (size_t i = 0; i < a_count; i++)
    {
        Vector2d p1_world = a_world[i];
        Vector2d p2_world = a_world[(i + 1) % a_count];
        Vector2d u_axis_edge = (Vector2d){p2_world.x - p1_world.x, p2_world.y - p1_world.y};
        Vector2d u_axis_unit = VectorNormalize_2d(u_axis_edge);
        Vector2d v_axis_unit = (Vector2d){-u_axis_unit.y, u_axis_unit.x};

        // Project both polygons onto the candidate normal and reject as soon
        // as a separating gap is found. Otherwise retain the smallest overlap;
        // it is the minimum translation depth used to describe the contact.
        float a_min_v, a_max_v, b_min_v, b_max_v;
        FindMinMaxProjection(a_world, a_count, v_axis_unit, &a_min_v, &a_max_v);
        FindMinMaxProjection(b_world, b_count, v_axis_unit, &b_min_v, &b_max_v);

        float dynamic_overlap = CalculateOverlap(a_min_v, a_max_v, b_min_v, b_max_v);
        if (dynamic_overlap <= 0.0f)
            return result;

        if (dynamic_overlap < min_overlap_u)
        {
            min_overlap_u = dynamic_overlap;
            final_u_axis = v_axis_unit;
            normal_owner = 1;
        }
    }

    // Shape B contributes the other half of SAT's candidate axes. Testing both
    // sets matters because either polygon may provide the separating edge.
    for (size_t i = 0; i < b_count; i++)
    {
        Vector2d p1_world = b_world[i];
        Vector2d p2_world = b_world[(i + 1) % b_count];
        Vector2d edge = (Vector2d){p2_world.x - p1_world.x, p2_world.y - p1_world.y};
        Vector2d u_axis_unit = VectorNormalize_2d(edge);
        Vector2d v_axis_unit = (Vector2d){-u_axis_unit.y, u_axis_unit.x};

        float a_min_v, a_max_v, b_min_v, b_max_v;
        FindMinMaxProjection(a_world, a_count, v_axis_unit, &a_min_v, &a_max_v);
        FindMinMaxProjection(b_world, b_count, v_axis_unit, &b_min_v, &b_max_v);

        float dynamic_overlap = CalculateOverlap(a_min_v, a_max_v, b_min_v, b_max_v);
        if (dynamic_overlap <= 0.0f)
            return result;

        if (dynamic_overlap < min_overlap_u)
        {
            min_overlap_u = dynamic_overlap;
            final_u_axis = v_axis_unit;
            normal_owner = 2;
        }
    }

    result.is_colliding = true;
    result.entity_a = a;
    result.entity_b = b;

    // The least-overlap axis is the minimum translation direction. Orient it
    // from A toward B so later response code has one consistent normal sign.
    Vector2d separation_vector = VectorScale_2d(final_u_axis, min_overlap_u);
    Vector2d center_to_center = (Vector2d){b->anchor_position.x - a->anchor_position.x, b->anchor_position.y - a->anchor_position.y};
    if (VectorDot_2d(separation_vector, center_to_center) < 0.0f)
    {
        final_u_axis = (Vector2d){-final_u_axis.x, -final_u_axis.y};
        separation_vector = (Vector2d){-separation_vector.x, -separation_vector.y};
    }

    result.collision_normal = final_u_axis;
    result.penetration_depth = min_overlap_u;

    // Single-point manifold for the impulse: the centre of the two bodies' common contact face (see
    // contact_point.h). FindDeepestVertex would return an arbitrary corner of a flat face, which under
    // accumulated, warm-started impulses becomes a persistent torque. The tolerance is relative to the
    // penetration and the smaller body's AABB half-diagonal.
    float a_extent = 0.5f * sqrtf((a_world_bounds.col2.x - a_world_bounds.col1.x) * (a_world_bounds.col2.x - a_world_bounds.col1.x) +
                                  (a_world_bounds.col2.y - a_world_bounds.col1.y) * (a_world_bounds.col2.y - a_world_bounds.col1.y));
    float b_extent = 0.5f * sqrtf((b_world_bounds.col2.x - b_world_bounds.col1.x) * (b_world_bounds.col2.x - b_world_bounds.col1.x) +
                                  (b_world_bounds.col2.y - b_world_bounds.col1.y) * (b_world_bounds.col2.y - b_world_bounds.col1.y));
    float face_tolerance = ContactPoint_FaceTolerance(min_overlap_u, fminf(a_extent, b_extent));
    result.contact_point = ContactPoint_FaceCentre(a_world, a_count, b_world, b_count, final_u_axis, face_tolerance);

    // Pick the extreme vertex on the penetrating side for a useful contact
    // marker. This is a visualisation point; the physical response is calculated
    // separately by the contact solver from result.contact_point.
    Vector2d deepest_vertex;
    if (normal_owner == 1)
    {
        result.penetrating_entity = b;
        deepest_vertex = FindDeepestVertex(b_world, b_count, final_u_axis, true);
    }
    else
    {
        result.penetrating_entity = a;
        deepest_vertex = FindDeepestVertex(a_world, a_count, final_u_axis, false);
    }

    result.collision_box.col1 = (Vector2d){deepest_vertex.x - 0.03f, deepest_vertex.y - 0.03f};
    result.collision_box.col2 = (Vector2d){deepest_vertex.x + 0.03f, deepest_vertex.y + 0.03f};
    return result;
}

bool CheckForCollision_AABB(Newtonoid2d a, Newtonoid2d b)
{
    // AABB testing is a fast conservative broad phase. It intentionally ignores
    // polygon rotation and shape detail, so callers must use SAT for confirmation.
    return AABB2d_Overlaps(AABB2d_FromOriginDimensions(a.bounds_origin, a.bounds_size),
                           AABB2d_FromOriginDimensions(b.bounds_origin, b.bounds_size));
}

// Return true only for live physical entities that may participate in the
// broad-phase map. Visual effects can remain attached and rendered without
// becoming cell occupants or collision-pair candidates.
bool EntityIsEligbleForSpatialMap(const Newtonoid2d *entity)
{
        return entity && (entity->status_flags & ENTITY_STATUS_FLAG_ALIVE) &&
            !(entity->roles & ENTITY_ROLE_EFFECT);
}

// Dispatches interaction behaviour between a sensor trigger volume and an entrant entity.
// SENSOR is deliberately independent from contact response: a sensor may still
// continue into normal projectile and rigid-body handling after this callback.
static void DispatchSensorTrigger(World2d *world, Newtonoid2d *sensor, Newtonoid2d *entrant,
                                  const CollisionResult_SAT *collision_result)
{
    (void)collision_result;
    if (!world || !sensor || !entrant)
    {
        return;
    }

    // Portal sensor: trigger teleportation to the paired destination portal.
    PortalComponent *portal_component = EntityRegistry_GetPortal(sensor->id);
    if (portal_component)
    {
        PortalSystem_RequestTeleport(world->universe, sensor->id, entrant->id);
        return;
    }

    // Additional sensor types (hazards, checkpoints, pickups) can be dispatched here by component type.
}

// DETECT half of one candidate pair from the spatial broad phase: gates, SAT, sensor and projectile
// callbacks (each fires once per pair per frame), then ONE cached contact for the solver. No impulse or
// position change happens here; ContactSolver_* applies the response after every pair has been detected.
bool DetectCollisionPair(World2d *world, EntityId obj_id_a, EntityId obj_id_b, int cell_i,
                          FlatMapInt *resolved_collisions, LArray *scheduled_world_cmds)
{
    // Validate entity IDs before looking up either object.
    if (obj_id_a < 1 || obj_id_b < 1)
    {
        LOG_ERROR("Could not find objects with IDs %d and %d in Cell (index = %d).\n", obj_id_a, obj_id_b, cell_i);
        return false;
    }

    // Resolve both ids through the generation-checked lookup first, so the pair key below is only
    // ever built from live entities (a stale id could otherwise alias a recycled slot).
    Newtonoid2d *a = GetEntityByID(world, obj_id_a);
    Newtonoid2d *b = GetEntityByID(world, obj_id_b);
    if (!a || !b)
    {
        LOG_WARN("Collision pair references missing entities %d and %d in Cell (index = %d).\n",
                 obj_id_a, obj_id_b, cell_i);
        return false;
    }

    // An entity can occupy several broad-phase cells, so the same pair can be
    // discovered repeatedly. Remember the pair after its first result to keep
    // one contact response per frame. The key is symmetric and collision-free
    // (see collision_pair_key.h), so one pair can never mask another.
    int obj_pair_hash_key = CollisionPairKey(obj_id_a, obj_id_b);
    // Must be an int: FlatMapInt_GetValue writes a full int through the pointer. A short here
    // overran the 2-byte stack slot on every duplicate-pair hit (undefined behaviour).
    int is_resolved = 0;
    if (FlatMapInt_GetValue(resolved_collisions, obj_pair_hash_key, &is_resolved))
        return false;

    // Treat this as a defensive boundary in case a stale or future indexing
    // path supplies an effect or dead entity to the pair resolver.
    if (!EntityIsEligbleForSpatialMap(a) || !EntityIsEligbleForSpatialMap(b))
    {
        return false;
    }

    // Sensors must still be checked while sleeping because overlap can trigger
    // gameplay behaviour. Ordinary sleeping pairs have no active motion that
    // needs a response, so mark them resolved without running SAT or impulses.
    bool a_is_sensor = (a->capabilities & ENTITY_CAPABILITY_SENSOR) != 0;
    bool b_is_sensor = (b->capabilities & ENTITY_CAPABILITY_SENSOR) != 0;
    if (!a_is_sensor && !b_is_sensor && (a->status_flags & ENTITY_STATUS_FLAG_SLEEPING) && (b->status_flags & ENTITY_STATUS_FLAG_SLEEPING))
    {
        FlatMapInt_InsertOrUpdate(resolved_collisions, obj_pair_hash_key, 1);
        return true;
    }

    CollisionResult_SAT collision_result = CheckForCollision_SAT(a, b);
    if (!collision_result.is_colliding)
        return false;

    // Mark the pair seen as soon as SAT confirms it, so a pair rejected later by the role-mask test below is
    // processed (SAT, sensor callback) at most once per frame instead of once per shared cell.
    FlatMapInt_InsertOrUpdate(resolved_collisions, obj_pair_hash_key, 1);

    // Sensors generate gameplay interaction events independently of physical
    // response. A pair with two sensors does not select an entrant, but it may
    // still continue into solid handling if neither side suppresses response.
    if (a_is_sensor || b_is_sensor)
    {
        if (a_is_sensor != b_is_sensor)
        {
            Newtonoid2d *sensor = a_is_sensor ? a : b;
            Newtonoid2d *entrant = a_is_sensor ? b : a;
            DispatchSensorTrigger(world, sensor, entrant, &collision_result);
        }

        // Sensor interaction and physical contact are separate capabilities.
        // Only an explicit no-contact flag prevents impulses and projectile
        // collision handling from running after the sensor callback.
        if ((a->constraints & ENTITY_CONSTRAINT_NO_CONTACT_RESPONSE) ||
            (b->constraints & ENTITY_CONSTRAINT_NO_CONTACT_RESPONSE))
        {
            FlatMapInt_InsertOrUpdate(resolved_collisions, obj_pair_hash_key, 1);
            return true;
        }
    }

    // Collision masks are a cheap compatibility filter. SAT has already
    // confirmed the polygons overlap, removing broad-phase false positives.
    if (!(a->collision_role_mask & b->roles) || !(b->collision_role_mask & a->roles))
        return false;

    ProjectileCollisionResult projectile_result = Projectile_HandleCollision(world, a, b);
    if (projectile_result == PROJECTILE_COLLISION_IGNORED)
    {
        FlatMapInt_InsertOrUpdate(resolved_collisions, obj_pair_hash_key, 1);
        return true;
    }

    // Record the contact before applying response so debug rendering can show
    // where the SAT support vertex reported the impact.
    LArray_Push(&world->collisions, &collision_result.collision_box);
    LOG_INFO("COLLISION detected between Object ID %d and Object ID %d Coord Box Range: [%0.2f,%0.2f] [%0.2f,%0.2f] \n",
             obj_id_a, obj_id_b, collision_result.collision_box.col1.x, collision_result.collision_box.col1.y,
             collision_result.collision_box.col2.x, collision_result.collision_box.col2.y);

    // Cache every physical contact for the solver (normal restitution and tangential friction are applied
    // there), including projectiles that are consumed after impact. Owner-overlap contacts intentionally
    // skip both damage and momentum transfer through the IGNORED result above.
    EmitPairContact(world, a, b, &collision_result);
    FlatMapInt_InsertOrUpdate(resolved_collisions, obj_pair_hash_key, 1);

    // Collision markers can be switched off from the toolbar (GIZMO_COLLISIONS, default ON). When
    // off, no marker entity is created at all (no surface allocation, no entity-count clutter);
    // markers already alive simply expire on their scheduled deletion. The contact has already
    // been cached for the solver above, so physics is identical either way.
    if (!IsGizmoEnabled(GIZMO_COLLISIONS))
    {
        return true;
    }

    // Create a short-lived effect at the reported support vertex for debugging.
    Newtonoid2d *penetrating_entity = collision_result.penetrating_entity;
    Matrix2x2 collision_box = collision_result.collision_box;
    Vector2d collision_center = CalcGeometricCentre_FromBox(collision_box);
    Vector2d dimensions = {collision_box.col2.x - collision_box.col1.x, collision_box.col2.y - collision_box.col1.y};
    Vector2d collision_vertices_arr[4];
    CalcBoxVertices(dimensions, ZERO_VECTOR_2D, collision_vertices_arr);
    Surface2d collision_surface = {0};
    collision_surface.surface_vectors = MakeLArray(4, sizeof(Vector2d));
    MemoryCopy(collision_surface.surface_vectors.items, collision_vertices_arr, sizeof(collision_vertices_arr));
    collision_surface.surface_vectors.count = 4;
    Newtonoid2d collision_obj = CreateNewtonoid2d(0.00001f, collision_center, penetrating_entity->velocity,
                                                  penetrating_entity->acceleration, collision_surface);
    collision_obj.roles = ENTITY_ROLE_EFFECT;
    collision_obj.status_flags |= ENTITY_STATUS_FLAG_CLOCKED;
    StickEntity(world, &collision_obj, penetrating_entity);
    EntityId id = AddObjectToWorld(world, &collision_obj, penetrating_entity->id);

    if (id != INVALID_ENTITY_ID)
    {
        ScheduleEntityDeletion(scheduled_world_cmds, id, ENTITY_STATUS_FLAG_ALIVE, 120, 1, 1);
    }

    return true;
}

// Register an entity in every in-grid cell touched by its snapped AABB, in the sparse occupancy store.
// Absent (carved) in-grid cells still register (wall collision and picking depend on it); out-of-grid
// cells (index < 0) never register. A full cell logs a warning and the entity is simply not tracked there.
void MapEntityToASpace(Space2d *space, Newtonoid2d *object, Matrix2x2 snapped_aabb_box, CellOccupancy *occupancy)
{
    // Keep this function as the final admission boundary for cell occupancy so
    // every caller follows the same physical-entity rule.
    if (!space || !occupancy || !EntityIsEligbleForSpatialMap(object))
        return;

    // The spatial grid is a broad-phase index: insert an entity into every cell
    // touched by its snapped AABB, then let SAT remove false-positive pairs.
    float snapped_w = (snapped_aabb_box.col2.x - snapped_aabb_box.col1.x);
    float snapped_h = (snapped_aabb_box.col2.y - snapped_aabb_box.col1.y);
    Vector2d snapped_t_left = snapped_aabb_box.col1;
    Vector2d snapped_b_right = snapped_aabb_box.col2;

    for (size_t y = 0; y < snapped_h; y++)
    {
        for (size_t x = 0; x < snapped_w; x++)
        {
            Vector2d cell_coords = (Vector2d){snapped_t_left.x + x, snapped_t_left.y + y};
            int cell_i = GetIndexFromCoords(space, cell_coords);
            if (cell_i < 0)
            {
                // Out-of-grid cells are never registered (index -1).
                continue;
            }

            // No cell flag is consulted: absent in-grid cells register occupants too.
            OccupancyAddResult result = CellOccupancy_Add(occupancy, cell_i, object->id);
            if (result == CELL_OCC_FULL)
            {
                // One line per second at most: a dense pile refuses hundreds of inserts per frame and each stderr
                // write is costly (see Log_LimiterAllow in log.c).
                LOG_WARN_LIMIT(1, 1.0f, "Cell index %d full. ID %d not tracked spatially.", cell_i, object->id);
            }
            else if (result == CELL_OCC_OUT_OF_RANGE)
            {
                // Layout invariant broken (store not rebuilt after a resize); skip rather than mis-register.
                LOG_ERROR("Cell index %d outside the occupancy store range. ID %d not tracked spatially.\n", cell_i, object->id);
            }
            else if (result == CELL_OCC_ALLOC_FAILED && CellOccupancy_TakeAllocFailureReport(occupancy))
            {
                LOG_ERROR("Cell occupancy store allocation failed. Some entities are not tracked spatially this tick.\n");
            }
        }
    }
}

// Remove every listing of entity_id from the in-grid cells covered by a previously snapped AABB.
// Out-of-grid cells (index < 0) are skipped; an emptied block simply stays in the pool with count 0.
static void RemoveEntityFromASpace(Space2d *space, EntityId entity_id, Matrix2x2 snapped_aabb_box, CellOccupancy *occupancy)
{
    if (!occupancy)
        return;

    float snapped_w = snapped_aabb_box.col2.x - snapped_aabb_box.col1.x;
    float snapped_h = snapped_aabb_box.col2.y - snapped_aabb_box.col1.y;

    for (size_t y = 0; y < snapped_h; y++)
    {
        for (size_t x = 0; x < snapped_w; x++)
        {
            Vector2d cell_coords = {snapped_aabb_box.col1.x + x, snapped_aabb_box.col1.y + y};
            int cell_i = GetIndexFromCoords(space, cell_coords);
            if (cell_i < 0)
                continue;

            CellOccupancy_Remove(occupancy, cell_i, entity_id);
        }
    }
}

// Move an entity's occupancy from its previous snapped box to its current one.
void RemapEntityInASpace(Space2d *space, Newtonoid2d *object, Matrix2x2 previous_snapped_aabb_box, CellOccupancy *occupancy)
{
    if (!space || !object)
        return;

    RemoveEntityFromASpace(space, object->id, previous_snapped_aabb_box, occupancy);

    // Remove stale membership if necessary, but never reinsert an entity that
    // is dead or exists only as a visual effect.
    if (!EntityIsEligbleForSpatialMap(object))
        return;

    Vector2d snapped_aabb_verts[4] = {0};
    CalcSnappedAABB_Vertices(object->surface.surface_vectors.items,
                             object->surface.surface_vectors.count,
                             object->anchor_position,
                             snapped_aabb_verts);
    Matrix2x2 current_snapped_aabb_box = CalcAABBCoords_Tight(snapped_aabb_verts, 4, ZERO_VECTOR_2D);
    MapEntityToASpace(space, object, current_snapped_aabb_box, occupancy);
}

void RefreshWorldSpatialMap(World2d *world)
{
    if (!world)
        return;

    Space2d *space = &world->grid_space.space;
    if (!space->cells.items)
        return;

    // Rewind the sparse store (O(occupied cells)) and stamp the current cell count, then re-map.
    CellOccupancy_Reset(&world->cell_occupancy, (int)space->cells.count);

    LArray *object_array = &world->objects;
    Newtonoid2d *objects = (Newtonoid2d *)object_array->items;
    for (size_t index = 0; index < object_array->count; index++)
    {
        Newtonoid2d *object = &objects[index];
        if (!EntityIsEligbleForSpatialMap(object) ||
            object->parent_id != world->grid_space.object.id)
        {
            continue;
        }

        Vector2d world_vertices[MAX_SHAPE_VERTICES] = {0};
        UpdateEntityBounds(object, world_vertices);
        Vector2d snapped_aabb_verts[4] = {0};
        CalcSnappedAABB_Vertices(world_vertices,
                                 object->surface.surface_vectors.count,
                                 ZERO_VECTOR_2D,
                                 snapped_aabb_verts);
        Matrix2x2 snapped_aabb_box = CalcAABBCoords_Tight(snapped_aabb_verts, 4, ZERO_VECTOR_2D);
        MapEntityToASpace(space, object, snapped_aabb_box, &world->cell_occupancy);
    }
}

