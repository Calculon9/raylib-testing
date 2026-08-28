/**********************************************************************************************
*
* WORLD PHYSICS
*
**********************************************************************************************/

#include "world/world_internal.h"
#include "combat/projectile.h"

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
// used only to place the small debug collision marker, not to resolve the pair.
static Vector2d FindDeepestVertex(Vector2d *vertices, size_t count, Vector2d axis, bool find_minimum)
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

static Vector2d CalcSeparationVector(Vector2d normal, float penetration_depth)
{
    // The minimum translation vector is normal * penetration. It is the
    // shortest displacement that moves the overlapping boundaries apart.
    return VectorScale_2d(normal, penetration_depth);
}

static void ApplyPositionSeparation(Newtonoid2d *entity, Vector2d separation_vector, float move_fraction)
{
    if (!entity)
    {
        return;
    }

    // Each body receives a share proportional to inverse mass: lighter bodies
    // move more, while a zero-inverse-mass body remains fixed.
    entity->anchor_position = VectorSum_2d(entity->anchor_position, VectorScale_2d(separation_vector, move_fraction));
    entity->bounds_origin = VectorSum_2d(entity->bounds_origin, VectorScale_2d(separation_vector, move_fraction));
}

// static void ApplyPositionSeparation(Newtonoid2d *entity, Vector2d separation_vector, float move_fraction)
// {
//     if (!entity)
//     {
//         return;
//     }

//     // Each body receives a share proportional to inverse mass: lighter bodies
//     // move more, while a zero-inverse-mass body remains fixed.
//     entity->anchor_position = VectorSum_2d(entity->anchor_position, VectorScale_2d(separation_vector, move_fraction));
//     entity->bounds_origin = VectorSum_2d(entity->bounds_origin, VectorScale_2d(separation_vector, move_fraction));
// }

static float CalcElasticImpulseMagnitude(float normal_velocity_dot, float total_inv_mass)
{
    // Impulse magnitude for a contact is
    // j = -(1 + e) * (relative_velocity dot normal) / sum(inverse_mass).
    // e = 1 models a perfectly elastic impact, so the normal component reverses
    // without losing kinetic energy in this simplified response.
    float e = 1.0f;
    return -(1.0f + e) * normal_velocity_dot / total_inv_mass;
}

void PhysicsUpdateJob(void *context, int start, int end)
{
    if (!context)
        return;

    World2d *world = (World2d *)context;
    GridSpace2d *space_entity = &world->grid_space;
    Space2d *space = &space_entity->space;
    FlatMapInt *entity_space_map = &world->entity_space_map;

    LArray *object_arrays[] = {&world->objects, &world->temp_objects};
    int global_index = 0;
    for (size_t array_index = 0; array_index < 2; array_index++)
    {
        LArray *objects = object_arrays[array_index];
        Newtonoid2d *newtonoids = (Newtonoid2d *)objects->items;
        for (int index = 0; index < (int)objects->count; index++, global_index++)
        {
            if (global_index < start || global_index >= end)
            {
                continue;
            }

            Newtonoid2d *obj = &newtonoids[index];

            if (!(obj->status_flags & FLAG_STATUS_ALIVE) || (obj->entity_flags & FLAG_TYPE_EFFECT) || obj->parent_id != space_entity->object.id)
                continue;

            // Gravity is an environmental acceleration added for this step only;
            // restore authored acceleration afterwards so it is not accumulated
            // repeatedly into the entity's persistent state.
            Vector2d authored_acceleration = obj->acceleration;
            obj->acceleration.y += world->gravity;
            CalcVectors(obj, frame_counter.delta_time);
            obj->acceleration = authored_acceleration;

            // Map the post-integration position so rendering, hit-testing, and the
            // collision broad phase all observe the same location. Snapping the
            // transformed AABB to cells is conservative: it may produce extra
            // candidate pairs, but it cannot omit objects covered by the box.
            Vector2d snapped_aabb_verts[4] = {0};
            CalcSnappedAABB_Vertices(obj->surface.surface_vectors.items, obj->surface.surface_vectors.count, obj->anchor_position, space->frame.basis, snapped_aabb_verts);
            Matrix2x2 snapped_aabb_box = CalcAABBCoords_Tight(snapped_aabb_verts, 4, ZERO_VECTOR_2D);
            MapEntityToASpace(space, obj, snapped_aabb_box, entity_space_map);
            ResolveCollision_ContainerRect(obj, &space_entity->object);
            Newtonoid_SyncOrientationToVelocity(obj);
        }
    }
}

CollisionResult_SAT CheckForCollision_SAT(Newtonoid2d *a, Newtonoid2d *b)
{
    CollisionResult_SAT result = {0};
    result.is_colliding = false;

    // First use the cached axis-aligned boxes as a cheap broad phase. AABB
    // overlap is necessary but not sufficient, so only the non-overlap case
    // can be rejected here without testing the actual polygons.
    if (!AABB2d_Overlaps(AABB2d_FromOriginDimensions(a->bounds_origin, a->bounds_size),
                         AABB2d_FromOriginDimensions(b->bounds_origin, b->bounds_size)))
        return result;

    LArray a_vertices_arr = a->surface.surface_vectors;
    LArray b_vertices_arr = b->surface.surface_vectors;
    Vector2d *a_vertices = a_vertices_arr.items;
    Vector2d *b_vertices = b_vertices_arr.items;

    Vector2d a_world[MAX_SHAPE_VERTICES];
    Vector2d b_world[MAX_SHAPE_VERTICES];

    size_t a_count = a->surface.surface_vectors.count;
    size_t b_count = b->surface.surface_vectors.count;
    Vector2d *a_local = a->surface.surface_vectors.items;
    Vector2d *b_local = b->surface.surface_vectors.items;

    // Early exit for degenerate shapes
    if (a_count < 3 || b_count < 3)
        return result;

    // SAT applies to convex polygons with consistently ordered vertices. The
    // vertices are transformed once because every candidate axis reuses them.
    Newtonoid_TransformVertices(a, a_world, MAX_SHAPE_VERTICES);
    Newtonoid_TransformVertices(b, b_world, MAX_SHAPE_VERTICES);

    float min_overlap_u = INFINITY;
    Vector2d final_u_axis = {0};
    int normal_owner = 0;

    // The Separating Axis Theorem says two convex polygons are disjoint if at
    // least one axis has non-overlapping projections. For a polygon, it is
    // sufficient to test the perpendicular to each edge of both shapes.
    // Test candidate axes from shape A.
    for (size_t i = 0; i < a_count; i++)
    {
        Vector2d p1_world = a_vertices[i];
        Vector2d p2_world = a_vertices[(i + 1) % a_vertices_arr.count];
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
    for (size_t i = 0; i < b_vertices_arr.count; i++)
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

    // Pick the extreme vertex on the penetrating side for a useful contact
    // marker. This is a visualisation point; physical response is calculated
    // separately by ResolveCollision below.
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

// Process one candidate pair from the spatial broad phase and apply the
// projectile and physical responses after narrow-phase collision confirmation.
bool ProcessCollisionPair(World2d *world, EntityId obj_id_a, EntityId obj_id_b, int cell_i,
                          FlatMapInt *resolved_collisions, LArray *scheduled_world_cmds)
{
    // Validate entity IDs before looking up either object.
    if (obj_id_a < 1 || obj_id_b < 1)
    {
        LOG_ERROR("Could not find objects with IDs %d and %d in Cell (index = %d).\n", obj_id_a, obj_id_b, cell_i);
        return false;
    }

    // An entity can occupy several broad-phase cells, so the same pair can be
    // discovered repeatedly. Remember the pair after its first result to keep
    // one contact response per frame.
    unsigned long obj_pair_hash_key = CalcHashFromInts(obj_id_a, obj_id_b);
    short is_resolved = 0;
    if (FlatMapInt_GetValue(resolved_collisions, obj_pair_hash_key, (int *)&is_resolved))
        return false;

    Newtonoid2d *a = GetEntityByID(world, obj_id_a);
    Newtonoid2d *b = GetEntityByID(world, obj_id_b);
    if (!a || !b)
    {
        LOG_WARN("Collision pair references missing entities %d and %d in Cell (index = %d).\n",
                 obj_id_a, obj_id_b, cell_i);
        return false;
    }

    if (!(a->status_flags & FLAG_STATUS_ALIVE) || !(b->status_flags & FLAG_STATUS_ALIVE))
    {
        return false;
    }

    // Collision masks are a cheap compatibility filter. SAT remains necessary
    // because sharing a broad-phase cell does not prove surface intersection.
    if (!(a->collision_mask & b->entity_flags) || !(b->collision_mask & a->entity_flags))
        return false;

    CollisionResult_SAT collision_result = CheckForCollision_SAT(a, b);
    if (!collision_result.is_colliding)
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

    // Resolve every physical contact elastically, including projectiles that
    // are consumed after impact. Owner-overlap contacts intentionally skip both
    // damage and momentum transfer through the IGNORED result above.
    ResolveCollision(a, b);
    FlatMapInt_InsertOrUpdate(resolved_collisions, obj_pair_hash_key, 1);

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
    collision_obj.entity_flags = FLAG_TYPE_EFFECT;
    collision_obj.status_flags |= FLAG_LIFETIME_CLOCKED;
    StickEntity(world, &collision_obj, penetrating_entity);
    EntityId id = AddObjectToWorld(world, &collision_obj, penetrating_entity->id);

    if (id != INVALID_ENTITY_ID)
    {
        ScheduleEntityDeletion(scheduled_world_cmds, id, FLAG_STATUS_ALIVE, 120, 1, 1);
    }

    return true;
}

void ResolveCollision(Newtonoid2d *a, Newtonoid2d *b)
{
    // This response uses the cached AABBs, giving a stable axis-aligned contact
    // correction after SAT has confirmed that the actual polygons overlap.
    float total_inv_mass = a->inverse_mass + b->inverse_mass;
    if (total_inv_mass <= 0.0f)
        return;

    float a_half_w = a->bounds_size.x * 0.5f;
    float a_half_h = a->bounds_size.y * 0.5f;
    float b_half_w = b->bounds_size.x * 0.5f;
    float b_half_h = b->bounds_size.y * 0.5f;

    Vector2d distance_vec = VectorDiff_2d(b->anchor_position, a->anchor_position);

    float x_overlap = (a_half_w + b_half_w) - fabsf(distance_vec.x);
    float y_overlap = (a_half_h + b_half_h) - fabsf(distance_vec.y);

    if (x_overlap <= 0.0f || y_overlap <= 0.0f)
        return;

    Vector2d normal = {0.0f, 0.0f};
    float penetration_depth = 0.0f;

    // Resolve along the axis of least overlap. Moving along that axis is the
    // minimum translation needed to separate two overlapping AABBs.
    if (x_overlap < y_overlap)
    {
        penetration_depth = x_overlap;
        normal = (distance_vec.x > 0.0f) ? (Vector2d){1.0f, 0.0f} : (Vector2d){-1.0f, 0.0f};
    }
    else
    {
        penetration_depth = y_overlap;
        normal = (distance_vec.y > 0.0f) ? (Vector2d){0.0f, 1.0f} : (Vector2d){0.0f, -1.0f};
    }

    // Positional correction conserves the static body's position and shares the
    // correction between dynamic bodies according to inverse mass.
    float a_move_fraction = a->inverse_mass / total_inv_mass;
    float b_move_fraction = b->inverse_mass / total_inv_mass;
    Vector2d separation_vector = CalcSeparationVector(normal, penetration_depth);

    ApplyPositionSeparation(a, separation_vector, -a_move_fraction);
    ApplyPositionSeparation(b, separation_vector, b_move_fraction);

    // Relative normal speed determines whether the bodies are approaching. With
    // normal directed from A to B, a positive (v_a - v_b) dot normal means A is
    // moving into B and therefore needs an impulse.
    Vector2d a_b_vel_diff = VectorDiff_2d(a->velocity, b->velocity);
    float a_b_vel_dot = VectorDot_2d(a_b_vel_diff, normal);

    // Object may have angular momentum
    if(a->angular_velocity != 0 || b->angular_velocity != 0)
    {
        // Using the geometric centre as the pivot, calculate angular momentum
    }

    if (a_b_vel_dot > 0.0f)
    {
        float j = CalcElasticImpulseMagnitude(a_b_vel_dot, total_inv_mass);
        Vector2d impulse_vector = VectorScale_2d(normal, j);
        a->velocity = VectorSum_2d(a->velocity, VectorScale_2d(impulse_vector, a->inverse_mass));
        b->velocity = VectorSum_2d(b->velocity, VectorScale_2d(impulse_vector, -b->inverse_mass));
        Newtonoid_SyncOrientationToVelocity(a);
        Newtonoid_SyncOrientationToVelocity(b);
    }
}

void ResolveCollision_WithRotation(Newtonoid2d *a, Newtonoid2d *b)
{
    // This response uses the cached AABBs, giving a stable axis-aligned contact
    // correction after SAT has confirmed that the actual polygons overlap.
    float total_inv_mass = a->inverse_mass + b->inverse_mass;
    if (total_inv_mass <= 0.0f)
        return;

    float a_half_w = a->bounds_size.x * 0.5f;
    float a_half_h = a->bounds_size.y * 0.5f;
    float b_half_w = b->bounds_size.x * 0.5f;
    float b_half_h = b->bounds_size.y * 0.5f;

    Vector2d distance_vec = VectorDiff_2d(b->anchor_position, a->anchor_position);

    float x_overlap = (a_half_w + b_half_w) - fabsf(distance_vec.x);
    float y_overlap = (a_half_h + b_half_h) - fabsf(distance_vec.y);

    if (x_overlap <= 0.0f || y_overlap <= 0.0f)
        return;

    Vector2d normal = {0.0f, 0.0f};
    float penetration_depth = 0.0f;

    // Resolve along the axis of least overlap. Moving along that axis is the
    // minimum translation needed to separate two overlapping AABBs.
    if (x_overlap < y_overlap)
    {
        penetration_depth = x_overlap;
        normal = (distance_vec.x > 0.0f) ? (Vector2d){1.0f, 0.0f} : (Vector2d){-1.0f, 0.0f};
    }
    else
    {
        penetration_depth = y_overlap;
        normal = (distance_vec.y > 0.0f) ? (Vector2d){0.0f, 1.0f} : (Vector2d){0.0f, -1.0f};
    }

    // Positional correction conserves the static body's position and shares the
    // correction between dynamic bodies according to inverse mass.
    float a_move_fraction = a->inverse_mass / total_inv_mass;
    float b_move_fraction = b->inverse_mass / total_inv_mass;
    Vector2d separation_vector = CalcSeparationVector(normal, penetration_depth);

    ApplyPositionSeparation(a, separation_vector, -a_move_fraction);
    ApplyPositionSeparation(b, separation_vector, b_move_fraction);

    // Relative normal speed determines whether the bodies are approaching. With
    // normal directed from A to B, a positive (v_a - v_b) dot normal means A is
    // moving into B and therefore needs an impulse.
    Vector2d a_b_vel_diff = VectorDiff_2d(a->velocity, b->velocity);
    float a_b_vel_dot = VectorDot_2d(a_b_vel_diff, normal);

    // Object may have angular momentum
    if(a->angular_velocity != 0 || b->angular_velocity != 0)
    {
        // Using the geometric centre as the pivot, calculate angular momentum
    }

    if (a_b_vel_dot > 0.0f)
    {
        float j = CalcElasticImpulseMagnitude(a_b_vel_dot, total_inv_mass);
        Vector2d impulse_vector = VectorScale_2d(normal, j);
        a->velocity = VectorSum_2d(a->velocity, VectorScale_2d(impulse_vector, a->inverse_mass));
        b->velocity = VectorSum_2d(b->velocity, VectorScale_2d(impulse_vector, -b->inverse_mass));
        Newtonoid_SyncOrientationToVelocity(a);
        Newtonoid_SyncOrientationToVelocity(b);
    }
}

void ResolveCollision_ContainerRect(Newtonoid2d *entity, Newtonoid2d *container)
{
    if (!entity || !container || entity->parent_id != container->id)
        return;

    if (!(entity->collision_mask & container->entity_flags) ||
        !(container->collision_mask & entity->entity_flags))
        return;

    // The container's child coordinates are defined from (0,0) to its width and
    // height. Compare the child's AABB against those four walls independently.
    float c_min_x = 0.0f;
    float c_min_y = 0.0f;
    float c_max_x = container->bounds_size.x;
    float c_max_y = container->bounds_size.y;

    float e_min_x = entity->bounds_origin.x;
    float e_min_y = entity->bounds_origin.y;
    float e_max_x = entity->bounds_origin.x + entity->bounds_size.x;
    float e_max_y = entity->bounds_origin.y + entity->bounds_size.y;

    float penetration_depth = 0.0f;
    Vector2d inward_normal = {0.0f, 0.0f};

    // Select the wall penetration and its inward normal. The normal points back
    // into the legal container region, which makes the same impulse formula work
    // for all four walls.
    if (e_min_x < c_min_x)
    {
        penetration_depth = c_min_x - e_min_x;
        inward_normal = (Vector2d){1.0f, 0.0f};
    }
    else if (e_max_x > c_max_x)
    {
        penetration_depth = e_max_x - c_max_x;
        inward_normal = (Vector2d){-1.0f, 0.0f};
    }

    if (e_min_y < c_min_y)
    {
        penetration_depth = c_min_y - e_min_y;
        inward_normal = (Vector2d){0.0f, 1.0f};
    }
    else if (e_max_y > c_max_y)
    {
        penetration_depth = e_max_y - c_max_y;
        inward_normal = (Vector2d){0.0f, -1.0f};
    }

    if (penetration_depth > 0.0f)
    {
        float total_inv_mass = entity->inverse_mass + container->inverse_mass;
        if (total_inv_mass <= 0.0f)
            return;

        // Move only the child by its inverse-mass share; a static container does
        // not move because its inverse mass is zero.
        float entity_move_fraction = entity->inverse_mass / total_inv_mass;
        Vector2d separation_vector = CalcSeparationVector(inward_normal, penetration_depth);

        ApplyPositionSeparation(entity, separation_vector, entity_move_fraction);

        // A negative relative speed along the inward normal means the child is
        // travelling out through the wall, so reflect that component elastically.
        Vector2d c_e_vel_diff = VectorDiff_2d(entity->velocity, container->velocity);
        float c_e_vel_dot = VectorDot_2d(c_e_vel_diff, inward_normal);

        if (c_e_vel_dot < 0.0f)
        {
            float j = CalcElasticImpulseMagnitude(c_e_vel_dot, total_inv_mass);
            Vector2d impulse_vector = VectorScale_2d(inward_normal, j);
            entity->velocity = VectorSum_2d(entity->velocity, VectorScale_2d(impulse_vector, entity->inverse_mass));
            Newtonoid_SyncOrientationToVelocity(entity);
        }
    }
}



void MapEntityToASpace(Space2d *space, Newtonoid2d *object, Matrix2x2 snapped_aabb_box, FlatMapInt *O_entity_to_space_index_map)
{
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
            Cell *cell = GetCellFromCoords(space, cell_coords);
            if (cell == NULL)
            {
                //LOG_WARN("Cell (index %d) not found in MapEntityToASpace or its coordinates are out of bounds. Skipping this object-->cell mapping.\n", cell_i);
                continue;
            }

            if (cell->occupancy < MAX_CELL_OCCUPANCY)
            {
                cell->object_ids[cell->occupancy] = object->id;
                cell->occupancy++;

                if (O_entity_to_space_index_map != NULL)
                {
                    FlatMapInt_InsertOrUpdate(O_entity_to_space_index_map, cell_i, cell->occupancy);
                }
            }
            else
            {
                LOG_WARN("Cell index %d full. ID %d not tracked spatially.\n", cell_i, object->id);
            }
        }
    }
}

static void RemoveEntityFromASpace(Space2d *space, EntityId entity_id, Matrix2x2 snapped_aabb_box, FlatMapInt *entity_space_map)
{
    float snapped_w = snapped_aabb_box.col2.x - snapped_aabb_box.col1.x;
    float snapped_h = snapped_aabb_box.col2.y - snapped_aabb_box.col1.y;

    for (size_t y = 0; y < snapped_h; y++)
    {
        for (size_t x = 0; x < snapped_w; x++)
        {
            Vector2d cell_coords = {snapped_aabb_box.col1.x + x, snapped_aabb_box.col1.y + y};
            int cell_i = GetIndexFromCoords(space, cell_coords);
            Cell *cell = GetCellFromCoords(space, cell_coords);
            if (!cell)
                continue;

            for (int index = 0; index < cell->occupancy; index++)
            {
                if (cell->object_ids[index] != entity_id)
                    continue;

                for (int shift = index; shift < cell->occupancy - 1; shift++)
                    cell->object_ids[shift] = cell->object_ids[shift + 1];

                cell->object_ids[cell->occupancy - 1] = 0;
                cell->occupancy--;
                index--;
            }

            if (entity_space_map)
            {
                if (cell->occupancy > 0)
                    FlatMapInt_InsertOrUpdate(entity_space_map, cell_i, cell->occupancy);
                else
                    FlatMapInt_DeactivateSlot(entity_space_map, cell_i);
            }
        }
    }
}

void RemapEntityInASpace(Space2d *space, Newtonoid2d *object, Matrix2x2 previous_snapped_aabb_box, FlatMapInt *entity_space_map)
{
    if (!space || !object)
        return;

    RemoveEntityFromASpace(space, object->id, previous_snapped_aabb_box, entity_space_map);

    Vector2d snapped_aabb_verts[4] = {0};
    CalcSnappedAABB_Vertices(object->surface.surface_vectors.items,
                             object->surface.surface_vectors.count,
                             object->anchor_position,
                             space->frame.basis,
                             snapped_aabb_verts);
    Matrix2x2 current_snapped_aabb_box = CalcAABBCoords_Tight(snapped_aabb_verts, 4, ZERO_VECTOR_2D);
    MapEntityToASpace(space, object, current_snapped_aabb_box, entity_space_map);
}

void RefreshWorldSpatialMap(World2d *world)
{
    if (!world)
        return;

    Space2d *space = &world->grid_space.space;
    if (!space->cells.items)
        return;

    ResetFlatMapInt(&world->entity_space_map);
    ResetSpaceCells(space);

    LArray *object_arrays[] = {&world->objects, &world->temp_objects};
    for (size_t array_index = 0; array_index < 2; array_index++)
    {
        LArray *object_array = object_arrays[array_index];
        Newtonoid2d *objects = (Newtonoid2d *)object_array->items;
        for (size_t index = 0; index < object_array->count; index++)
        {
            Newtonoid2d *object = &objects[index];
            if (!(object->status_flags & FLAG_STATUS_ALIVE) ||
                (object->entity_flags & FLAG_TYPE_EFFECT) ||
                object->parent_id != world->grid_space.object.id)
            {
                continue;
            }

            Vector2d snapped_aabb_verts[4] = {0};
            CalcSnappedAABB_Vertices(object->surface.surface_vectors.items,
                                     object->surface.surface_vectors.count,
                                     object->anchor_position,
                                     space->frame.basis,
                                     snapped_aabb_verts);
            Matrix2x2 snapped_aabb_box = CalcAABBCoords_Tight(snapped_aabb_verts, 4, ZERO_VECTOR_2D);
            MapEntityToASpace(space, object, snapped_aabb_box, &world->entity_space_map);
        }
    }
}

void PrintVerticeCoords(LArray *vertices_arr, Vector2d offset)
{
    if (vertices_arr == NULL || vertices_arr->count == 0)
    {
        LOG_INFO("Vertices x_coords: []\nVertices y_coords: []\n");
        return;
    }

    Vector2d *vertices = (Vector2d *)vertices_arr->items;
    char x_buffer[2048] = "x_coords = [";
    char y_buffer[2048] = "y_coords = [";
    size_t x_offset = strlen(x_buffer);
    size_t y_offset = strlen(y_buffer);

    for (size_t i = 0; i <= vertices_arr->count; i++)
    {
        bool is_last = (i == vertices_arr->count);
        const char *delimiter = is_last ? "]" : ", ";

        size_t j = vertices_arr->count - (i % vertices_arr->count) - 1;
        int x_written = snprintf(x_buffer + x_offset, sizeof(x_buffer) - x_offset, "%.2f%s", vertices[j].x + offset.x, delimiter);
        if (x_written > 0 && x_offset + x_written < sizeof(x_buffer))
        {
            x_offset += x_written;
        }

        int y_written = snprintf(y_buffer + y_offset, sizeof(y_buffer) - y_offset, "%.2f%s", vertices[j].y + offset.y, delimiter);
        if (y_written > 0 && y_offset + y_written < sizeof(y_buffer))
        {
            y_offset += y_written;
        }
    }

    LOG_INFO("%s\n", x_buffer);
    LOG_INFO("%s\n", y_buffer);
}


