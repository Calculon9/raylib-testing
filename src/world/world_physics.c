/**********************************************************************************************
*
* WORLD PHYSICS
*
**********************************************************************************************/

#include "world/world_internal.h"

void PhysicsUpdateJob(void *context, int start, int end)
{
    if (!context)
        return;

    WorldState *world_context = (WorldState *)context;
    GridSpace2d *space_entity = &world_context->world->grid_space;
    Space2d *space = &space_entity->space;
    LArray *objects = &world_context->world->objects;
    FlatMapInt *entity_space_map = &world_context->world->entity_space_map;
    Newtonoid2d *newtonoids = (Newtonoid2d *)objects->items;

    for (int index = start; index < end && index < (int)objects->count; ++index)
    {
        Newtonoid2d *obj = &newtonoids[index];

        if (!(obj->flags & FLAG_STATUS_ALIVE) || (obj->entity_layer & FLAG_TYPE_EFFECT) || obj->parent_id != space_entity->object.id)
            continue;

        // Use the current center position to derive the snapped grid footprint for this frame.
        Vector2d snapped_aabb_verts[4] = {0};
        CalcSnappedAABB_Vertices(obj->surface.surface_vectors.items, obj->surface.surface_vectors.count, obj->coords_center, space->system.basis, snapped_aabb_verts);
        Matrix2x2 snapped_aabb_box = CalcAABBCoords_Tight(snapped_aabb_verts, 4, ZERO_VECTOR_2D);

        CalcVectors(obj, frame_counter.delta_time);
        MapEntityToASpace(space, obj, snapped_aabb_box, entity_space_map);
        ResolveCollision_ContainerRect(obj, &space_entity->object);
    }
}

CollisionResult_SAT CheckForCollision_SAT(Newtonoid2d *a, Newtonoid2d *b)
{
    CollisionResult_SAT result = {0};
    result.is_colliding = false;

    float a_min_x = a->coords_origin.x;
    float a_max_x = a->coords_origin.x + a->boxed_dimensions.x;
    float a_min_y = a->coords_origin.y;
    float a_max_y = a->coords_origin.y + a->boxed_dimensions.y;

    float b_min_x = b->coords_origin.x;
    float b_max_x = b->coords_origin.x + b->boxed_dimensions.x;
    float b_min_y = b->coords_origin.y;
    float b_max_y = b->coords_origin.y + b->boxed_dimensions.y;

    if (a_max_x < b_min_x || a_min_x > b_max_x)
        return result;
    if (a_max_y < b_min_y || a_min_y > b_max_y)
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

    for (size_t i = 0; i < a_count; i++)
    {
        a_world[i].x = (a_local[i].x * a->local_axis_x.x) + (a_local[i].y * a->local_axis_y.x) + a->coords_center.x;
        a_world[i].y = (a_local[i].x * a->local_axis_x.y) + (a_local[i].y * a->local_axis_y.y) + a->coords_center.y;
    }
    for (size_t i = 0; i < b_count; i++)
    {
        b_world[i].x = (b_local[i].x * b->local_axis_x.x) + (b_local[i].y * b->local_axis_y.x) + b->coords_center.x;
        b_world[i].y = (b_local[i].x * b->local_axis_x.y) + (b_local[i].y * b->local_axis_y.y) + b->coords_center.y;
    }

    float min_overlap_u = INFINITY;
    Vector2d final_u_axis = {0};
    int normal_owner = 0;

    for (size_t i = 0; i < a_count; i++)
    {
        Vector2d p1_world = a_vertices[i];
        Vector2d p2_world = a_vertices[(i + 1) % a_vertices_arr.count];
        Vector2d u_axis_edge = (Vector2d){p2_world.x - p1_world.x, p2_world.y - p1_world.y};
        float u_len = VectorMagnitude_2d(u_axis_edge);
        Vector2d u_axis_unit = (u_len > 0.0f) ? VectorScale_2d(u_axis_edge, 1.0f / u_len) : (Vector2d){1.0f, 0.0f};
        Vector2d v_axis_unit = (Vector2d){-u_axis_unit.y, u_axis_unit.x};

        float a_min_v = INFINITY, a_max_v = -INFINITY;
        for (size_t v = 0; v < a_count; v++)
        {
            float proj_v = VectorDot_2d(a_world[v], v_axis_unit);
            if (proj_v < a_min_v)
                a_min_v = proj_v;
            if (proj_v > a_max_v)
                a_max_v = proj_v;
        }

        float b_min_v = INFINITY, b_max_v = -INFINITY;
        for (size_t v = 0; v < b_count; v++)
        {
            float proj_v = VectorDot_2d(b_world[v], v_axis_unit);
            if (proj_v < b_min_v)
                b_min_v = proj_v;
            if (proj_v > b_max_v)
                b_max_v = proj_v;
        }

        float dynamic_overlap = (a_max_v < b_max_v ? a_max_v : b_max_v) - (a_min_v > b_min_v ? a_min_v : b_min_v);
        if (dynamic_overlap <= 0.0f)
            return result;

        if (dynamic_overlap < min_overlap_u)
        {
            min_overlap_u = dynamic_overlap;
            final_u_axis = v_axis_unit;
            normal_owner = 1;
        }
    }

    for (size_t i = 0; i < b_vertices_arr.count; i++)
    {
        Vector2d p1_world = b_world[i];
        Vector2d p2_world = b_world[(i + 1) % b_count];
        Vector2d edge = (Vector2d){p2_world.x - p1_world.x, p2_world.y - p1_world.y};
        float len = VectorMagnitude_2d(edge);
        Vector2d u_axis_unit = (len > 0.0f) ? VectorScale_2d(edge, 1.0f / len) : (Vector2d){1.0f, 0.0f};
        Vector2d v_axis_unit = (Vector2d){-u_axis_unit.y, u_axis_unit.x};

        float a_min_v = INFINITY, a_max_v = -INFINITY;
        for (size_t v = 0; v < a_count; v++)
        {
            float proj_v = VectorDot_2d(a_world[v], v_axis_unit);
            if (proj_v < a_min_v)
                a_min_v = proj_v;
            if (proj_v > a_max_v)
                a_max_v = proj_v;
        }

        float b_min_v = INFINITY, b_max_v = -INFINITY;
        for (size_t v = 0; v < b_count; v++)
        {
            float proj_v = VectorDot_2d(b_world[v], v_axis_unit);
            if (proj_v < b_min_v)
                b_min_v = proj_v;
            if (proj_v > b_max_v)
                b_max_v = proj_v;
        }

        float dynamic_overlap = (a_max_v < b_max_v ? a_max_v : b_max_v) - (a_min_v > b_min_v ? a_min_v : b_min_v);
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
    Vector2d center_to_center = (Vector2d){b->coords_center.x - a->coords_center.x, b->coords_center.y - a->coords_center.y};
    if (VectorDot_2d(separation_vector, center_to_center) < 0.0f)
    {
        final_u_axis = (Vector2d){-final_u_axis.x, -final_u_axis.y};
        separation_vector = (Vector2d){-separation_vector.x, -separation_vector.y};
    }

    Vector2d deepest_vertex = {0};
    if (normal_owner == 1)
    {
        result.penetrating_entity = b;
        float min_proj = INFINITY;
        for (size_t v = 0; v < b_count; v++)
        {
            float proj_v = VectorDot_2d(b_world[v], final_u_axis);
            if (proj_v < min_proj)
            {
                min_proj = proj_v;
                deepest_vertex = b_world[v];
            }
        }
    }
    else
    {
        result.penetrating_entity = a;
        float max_proj = -INFINITY;
        for (size_t v = 0; v < a_count; v++)
        {
            float proj_v = VectorDot_2d(a_world[v], final_u_axis);
            if (proj_v > max_proj)
            {
                max_proj = proj_v;
                deepest_vertex = a_world[v];
            }
        }
    }

    result.collision_box.col1 = (Vector2d){deepest_vertex.x - 0.03f, deepest_vertex.y - 0.03f};
    result.collision_box.col2 = (Vector2d){deepest_vertex.x + 0.03f, deepest_vertex.y + 0.03f};
    return result;
}

bool CheckForCollision_AABB(Newtonoid2d a, Newtonoid2d b)
{
    float a_min_x = a.coords_origin.x;
    float a_max_x = a.coords_origin.x + a.boxed_dimensions.x;
    float a_min_y = a.coords_origin.y;
    float a_max_y = a.coords_origin.y + a.boxed_dimensions.y;

    float b_min_x = b.coords_origin.x;
    float b_max_x = b.coords_origin.x + b.boxed_dimensions.x;
    float b_min_y = b.coords_origin.y;
    float b_max_y = b.coords_origin.y + b.boxed_dimensions.y;

    if (a_max_x < b_min_x || a_min_x > b_max_x)
        return false;
    if (a_max_y < b_min_y || a_min_y > b_max_y)
        return false;

    return true;
}

void ResolveCollision(Newtonoid2d *a, Newtonoid2d *b)
{
    float total_inv_mass = a->inverse_mass + b->inverse_mass;
    if (total_inv_mass <= 0.0f)
        return;

    float a_half_w = a->boxed_dimensions.x * 0.5f;
    float a_half_h = a->boxed_dimensions.y * 0.5f;
    float b_half_w = b->boxed_dimensions.x * 0.5f;
    float b_half_h = b->boxed_dimensions.y * 0.5f;

    Vector2d distance_vec = VectorSum_2d(b->coords_center, VectorScale_2d(a->coords_center, -1.0f));

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
    Vector2d separation_vector = VectorScale_2d(normal, penetration_depth);

    a->coords_center = VectorSum_2d(a->coords_center, VectorScale_2d(separation_vector, -a_move_fraction));
    a->coords_origin = VectorSum_2d(a->coords_origin, VectorScale_2d(separation_vector, -a_move_fraction));

    b->coords_center = VectorSum_2d(b->coords_center, VectorScale_2d(separation_vector, b_move_fraction));
    b->coords_origin = VectorSum_2d(b->coords_origin, VectorScale_2d(separation_vector, b_move_fraction));

    Vector2d a_b_vel_diff = VectorSum_2d(a->velocity, VectorScale_2d(b->velocity, -1));
    float a_b_vel_dot = VectorDot_2d(a_b_vel_diff, normal);

    if (a_b_vel_dot > 0.0f)
    {
        float e = 1.0f;
        float j = -(1.0f + e) * a_b_vel_dot / total_inv_mass;
        Vector2d impulse_vector = VectorScale_2d(normal, j);
        a->velocity = VectorSum_2d(a->velocity, VectorScale_2d(impulse_vector, a->inverse_mass));
        b->velocity = VectorSum_2d(b->velocity, VectorScale_2d(impulse_vector, -b->inverse_mass));
    }
}

void ResolveCollision_ContainerRect(Newtonoid2d *entity, Newtonoid2d *container)
{
    if (entity->parent_id != container->id)
        return;

    float c_min_x = 0.0f;
    float c_min_y = 0.0f;
    float c_max_x = container->boxed_dimensions.x;
    float c_max_y = container->boxed_dimensions.y;

    float e_min_x = entity->coords_origin.x;
    float e_min_y = entity->coords_origin.y;
    float e_max_x = entity->coords_origin.x + entity->boxed_dimensions.x;
    float e_max_y = entity->coords_origin.y + entity->boxed_dimensions.y;

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
        Vector2d separation_vector = VectorScale_2d(inward_normal, penetration_depth);

        entity->coords_center = VectorSum_2d(entity->coords_center, VectorScale_2d(separation_vector, entity_move_fraction));
        entity->coords_origin = VectorSum_2d(entity->coords_origin, VectorScale_2d(separation_vector, entity_move_fraction));

        Vector2d c_e_vel_diff = VectorSum_2d(entity->velocity, VectorScale_2d(container->velocity, -1.0f));
        float c_e_vel_dot = VectorDot_2d(c_e_vel_diff, inward_normal);

        if (c_e_vel_dot < 0.0f)
        {
            float e = 1.0f;
            float j = -(1.0f + e) * c_e_vel_dot / total_inv_mass;
            Vector2d impulse_vector = VectorScale_2d(inward_normal, j);
            entity->velocity = VectorSum_2d(entity->velocity, VectorScale_2d(impulse_vector, entity->inverse_mass));
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
            bool out_of_bounds = Frame_ContainsPoint_Local(cell_coords, &space->system);
            if (cell == NULL || out_of_bounds == false)
            {
                //LOG_WARN("Cell (index %d) not found in MapEntityToASpace or its coordinates are out of bounds. Skipping this object-->cell mapping.\n", cell_i);
                continue;
            }

            if (cell->occupancy < MAX_CELL_OCCUPANCY)
            {
                cell->object_ids[cell->occupancy] = object->id;
                cell->occupancy++;

                if (O_entity_to_space_index_map != NULL && cell->occupancy >= 1)
                {
                    int cell_occu = 0;
                    FlatMapInt_InsertOrUpdate(O_entity_to_space_index_map, cell_i, cell->occupancy);
                    FlatMapInt_GetValue(O_entity_to_space_index_map, cell_i, &cell_occu);
                }
            }
            else
            {
                LOG_WARN("Cell index %d full. ID %d not tracked spatially.\n", cell_i, object->id);
            }
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


