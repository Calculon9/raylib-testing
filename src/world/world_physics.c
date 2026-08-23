/**********************************************************************************************
*
* WORLD PHYSICS
*
**********************************************************************************************/

#include "world/world_internal.h"

// Helper: Find min and max projection of vertices onto an axis
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

// Helper: Calculate overlap between two projection ranges
static float CalculateOverlap(float a_min, float a_max, float b_min, float b_max)
{
    return (a_max < b_max ? a_max : b_max) - (a_min > b_min ? a_min : b_min);
}

// Helper: Find deepest penetrating vertex along an axis
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
    return VectorScale_2d(normal, penetration_depth);
}

static void ApplyPositionSeparation(Newtonoid2d *entity, Vector2d separation_vector, float move_fraction)
{
    if (!entity)
    {
        return;
    }

    entity->anchor_position = VectorSum_2d(entity->anchor_position, VectorScale_2d(separation_vector, move_fraction));
    entity->bounds_origin = VectorSum_2d(entity->bounds_origin, VectorScale_2d(separation_vector, move_fraction));
}

static float CalcElasticImpulseMagnitude(float normal_velocity_dot, float total_inv_mass)
{
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

            Vector2d authored_acceleration = obj->acceleration;
            obj->acceleration.y += world->gravity;
            CalcVectors(obj, frame_counter.delta_time);
            obj->acceleration = authored_acceleration;

            // Map the position after physics has advanced the object so hit-testing
            // and collision broad-phase use the same location that is rendered.
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

    // AABB broad-phase early exit (prevents unnecessary SAT computation)
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

    // Transform vertices to world space once (cache for multiple axis tests)
    Newtonoid_TransformVertices(a, a_world, MAX_SHAPE_VERTICES);
    Newtonoid_TransformVertices(b, b_world, MAX_SHAPE_VERTICES);

    float min_overlap_u = INFINITY;
    Vector2d final_u_axis = {0};
    int normal_owner = 0;

    // Test axes from shape A
    for (size_t i = 0; i < a_count; i++)
    {
        Vector2d p1_world = a_vertices[i];
        Vector2d p2_world = a_vertices[(i + 1) % a_vertices_arr.count];
        Vector2d u_axis_edge = (Vector2d){p2_world.x - p1_world.x, p2_world.y - p1_world.y};
        Vector2d u_axis_unit = VectorNormalize_2d(u_axis_edge);
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
            normal_owner = 1;
        }
    }

    // Test axes from shape B
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

    Vector2d separation_vector = VectorScale_2d(final_u_axis, min_overlap_u);
    Vector2d center_to_center = (Vector2d){b->anchor_position.x - a->anchor_position.x, b->anchor_position.y - a->anchor_position.y};
    if (VectorDot_2d(separation_vector, center_to_center) < 0.0f)
    {
        final_u_axis = (Vector2d){-final_u_axis.x, -final_u_axis.y};
        separation_vector = (Vector2d){-separation_vector.x, -separation_vector.y};
    }

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
    return AABB2d_Overlaps(AABB2d_FromOriginDimensions(a.bounds_origin, a.bounds_size),
                           AABB2d_FromOriginDimensions(b.bounds_origin, b.bounds_size));
}

void ResolveCollision(Newtonoid2d *a, Newtonoid2d *b)
{
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

    float a_move_fraction = a->inverse_mass / total_inv_mass;
    float b_move_fraction = b->inverse_mass / total_inv_mass;
    Vector2d separation_vector = CalcSeparationVector(normal, penetration_depth);

    ApplyPositionSeparation(a, separation_vector, -a_move_fraction);
    ApplyPositionSeparation(b, separation_vector, b_move_fraction);

    Vector2d a_b_vel_diff = VectorDiff_2d(a->velocity, b->velocity);
    float a_b_vel_dot = VectorDot_2d(a_b_vel_diff, normal);

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

        float entity_move_fraction = entity->inverse_mass / total_inv_mass;
        Vector2d separation_vector = CalcSeparationVector(inward_normal, penetration_depth);

        ApplyPositionSeparation(entity, separation_vector, entity_move_fraction);

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


