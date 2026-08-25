/**********************************************************************************************
 *
 * WORLD LIFECYCLE AND SIMULATION
 *
 * World construction, destruction, and frame updates. Gameplay input lives in
 * src/system/world_system.c, while collision response lives in world_physics.c.
 *
 **********************************************************************************************/
#include "common/common.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "world/universe.h"
#include "physics/physics.h"
#include "system/job_system.h"

// Number of delayed world commands initially reserved for a new world.
static int initObjectCount = 4;

// Compose the world-local, universe-camera, and viewport transforms for rendering.
Matrix3x3 ResolveWorldToPixelMatrix(const World2d *world, const Camera2d *universe_camera)
{
    if (!world || !universe_camera)
    {
        return IDENTITY_MATRIX_3x3;
    }

    // Compose the viewport, camera, and supplied world-local transform.
    Matrix3x3 camera_to_pixel = MatrixMultiply_3x3_3x3(
        game_viewport.tunnel.source_to_dest_mtx,
        universe_camera->tunnel.source_to_dest_mtx);
    return MatrixMultiply_3x3_3x3(camera_to_pixel, world->tunnel.source_to_dest_mtx);
}

// Invert the world-to-pixel transform for screen-to-world input conversion.
Matrix3x3 ResolvePixelToWorldMatrix(const World2d *world, const Camera2d *universe_camera)
{
    return MatrixInvert_3x3(ResolveWorldToPixelMatrix(world, universe_camera));
}

// Initialise a world and allocate its entity, collision, spatial, and command storage.
bool CreateWorld(GridSpace2d space_obj, float gravity, struct Universe *universe, World2d *out_world)
{
    if (!out_world || !universe)
    {
        LOG_ERROR("Cannot create world without output storage and a universe owner.\n");
        return false;
    }

    out_world->grid_space = space_obj;
    out_world->gravity = gravity;
    out_world->universe = universe;
    out_world->flags = WORLD_FLAG_ACTIVE | WORLD_FLAG_VISIBLE | WORLD_FLAG_SELECTABLE |
                       WORLD_FLAG_PHYSICS_ENABLED | WORLD_FLAG_SPAWNS_ENABLED |
                       WORLD_FLAG_DRAGGABLE;
    out_world->grid_space.object.id = Universe_AllocateEntityId(universe);

    if (out_world->grid_space.object.id == INVALID_ENTITY_ID)
    {
        LOG_ERROR("Cannot create world: universe entity ID allocation failed.\n");
        return false;
    }
    int object_count = *GetNextWorldObjectCountPtr();
    if (object_count < 0)
    {
        object_count = 0;
    }
    out_world->objects = MakeLArray(object_count, sizeof(Newtonoid2d));
    out_world->collisions = MakeLArray(object_count, sizeof(Matrix2x2));
    out_world->temp_objects = MakeLArray(object_count, sizeof(Newtonoid2d));

    // Initialise per-world lookup maps and the delayed command queue.
    out_world->entity_space_map = MakeFlatMapInt(1 + (int)(space_obj.space.cells.count / 5));
    out_world->resolved_collisions = MakeFlatMapInt(1 + (int)(out_world->entity_space_map.count / 2));
    out_world->entity_world_index_registry = MakeFlatMapInt(1 + (int)(out_world->entity_space_map.count / 2));
    out_world->scheduled_world_cmds = MakeLArray(initObjectCount, sizeof(WorldCommand));
    InitJobSystem(256);
    return true;
}

// Release all allocations owned by a world and clear its state.
void DestroyWorld(World2d *world)
{
    if (!world)
    {
        return;
    }

    DestroyWorldEntityStorage(world);
    ClearLArray(&world->collisions);
    ClearLArray(&world->scheduled_world_cmds);
    ClearDArray(&world->grid_space.space.cells);
    ClearFlatMapInt(&world->entity_space_map);
    ClearFlatMapInt(&world->resolved_collisions);
    ClearFlatMapInt(&world->entity_world_index_registry);
    MemorySet(world, 0, sizeof(*world));
}

// Advance one world through scheduled commands, physics integration, attachment
// updates, spatial remapping, and broad-phase collision-pair processing.
void UpdateWorld(World2d *world, float delta_time)
{
    LArray *objects = &world->objects;
    GridSpace2d *space_entity = &world->grid_space;
    Space2d *space = &space_entity->space;

    // Cache the per-frame maps and command storage used by the update passes.
    FlatMapInt *entity_space_map = &world->entity_space_map;
    FlatMapInt *resolved_collisions = &world->resolved_collisions;
    FlatMapInt *entity_world_index_registry = &world->entity_world_index_registry;
    LArray *scheduled_world_cmds = &world->scheduled_world_cmds;

    // Run delayed world events first so deletions continue even when no
    // inhabitants remain in the world.
    RunScheduledWorldCmds(scheduled_world_cmds, world);

    int obj_count = objects->count + world->temp_objects.count;
    if (obj_count < 1)
        return;

    LArray_Reset(&world->collisions);
    ResetFlatMapInt(entity_space_map);
    ResetFlatMapInt(resolved_collisions);

    ResetSpaceCells(space);
    Cell *cells = (Cell *)space->cells.items;

    if (!IsJobSystemInitialized())
    {
        InitJobSystem(256);
    }
    ClearJobs();
    SubmitJob(PhysicsUpdateJob, world, obj_count, 8);
    ExecuteJobs();
    ClearJobs();

    // Pass 1 is performed by PhysicsUpdateJob: integrate positions and
    // velocities, refresh AABBs, and insert each entity into the spatial grid.
    LArray *temp_objects = &world->temp_objects;

    // Pass 2 resolves attachment hierarchies after parent simulation and before
    // collision processing, then remaps each moved child in the spatial grid.
    Newtonoid2d *child;
    LArray_ForEach(temp_objects, Newtonoid2d *, child)
    {
        if (!(child->status_flags & FLAG_STATUS_ALIVE) || child->parent_id == space_entity->object.id)
            continue;

        // Look up where the parent currently lives in memory using the registry.
        int parent_packed_loc = 0;
        if (!FlatMapInt_GetValue(entity_world_index_registry, child->parent_id, &parent_packed_loc))
        {
            // The parent was likely deleted! Detach or kill the child so it doesn't crash
            child->parent_id = INVALID_ENTITY_ID;
            continue;
        }

        // Unpack the routing information to access the correct backing array.
        int parent_type = UNPACK_INT_HIGH(parent_packed_loc);
        int parent_idx = UNPACK_INT_LOW(parent_packed_loc);
        LArray *parent_array = (parent_type == ARCHETYPE_CLOCKED) ? temp_objects : objects;

        Newtonoid2d *parent = (Newtonoid2d *)LArray_Get(parent_array, parent_idx);
        if (!parent)
        {
            child->parent_id = INVALID_ENTITY_ID;
            continue;
        }

        Vector2d previous_snapped_aabb_verts[4] = {0};
        CalcSnappedAABB_Vertices(child->surface.surface_vectors.items,
                                 child->surface.surface_vectors.count,
                                 child->anchor_position,
                                 space->frame.basis,
                                 previous_snapped_aabb_verts);
        Matrix2x2 previous_snapped_aabb_box = CalcAABBCoords_Tight(
            previous_snapped_aabb_verts, 4, ZERO_VECTOR_2D);

        // Glue the child's world position to the parent's new position plus its offset.
        child->anchor_position = VectorSum_2d(parent->anchor_position, child->parent_offset);
        child->bounds_origin = VectorSum_2d(parent->bounds_origin, child->bounds_origin);
        RemapEntityInASpace(space, child, previous_snapped_aabb_box,
                            entity_space_map);
    }

    // Pass 3 resolves entity pairs sharing at least one broad-phase cell.
    if (obj_count < 2)
    {
        return;
    }

    // RESOLVE ENTITY-ENTITY COLLISIONS
    for (size_t i = 0; i < entity_space_map->capacity; i++)
    {
        if (entity_space_map->slots[i].key == 0 && entity_space_map->slots[i].value == 0)
            continue;

        int cell_i = entity_space_map->slots[i].key;
        int cell_occ = 0;
        FlatMapInt_GetValue(entity_space_map, cell_i, &cell_occ);

        if (cell_occ >= 2)
        {
            Cell *cell = &cells[cell_i];
            for (size_t m = 0; m < cell_occ; m++)
            {
                // Test every unique entity pairing in this cell. The physics
                // module deduplicates pairs found in multiple cells.
                for (size_t n = m + 1; n < cell_occ; n++)
                {
                    EntityId obj_id_a = cell->object_ids[m];
                    EntityId obj_id_b = cell->object_ids[n];
                    ProcessCollisionPair(world, obj_id_a, obj_id_b, cell_i,
                                         resolved_collisions, scheduled_world_cmds);
                }
            }
        }
    }
}

        // DEBUGGING - Rapid firing of polygonoids
        // keyDown = IsKeyDown(KEY_ONE);
        // if (keyDown & frame_counter.total_frames % 3 == 0)
        // {
        //     radius = GetRandomFloat(0.1, polygonoid_radius_default * 0.25);
        //     mass = radius * polygonoid_mass_default;
        //     velocity = (Vector2d){GetRandomFloat(polygonoid_velocity_default.x * -8, polygonoid_velocity_default.x * 8), GetRandomFloat(polygonoid_velocity_default.y * 0, polygonoid_velocity_default.y * 16)};
        //     Vector2d top_middle_world = (Vector2d){world.grid_space.object.bounds_size.x * 0.5, 0.3};
        //     // Vector2d top_middle_world_pixel = TransformCoordinates(camera_world.source_to_dest_mtx, top_middle_world);
        //     CreateAddNewtonoid(vertice_count, radius, SHAPE_MATH_POLY_HULL, mass, colour, top_middle_world, velocity, acceleration);
        //     UpdateWorld(&G_WorldState, frame_counter.delta_time);
        // }

        // // DEBUGGING - Collisions
        // if (IsKeyPressed(KEY_THREE))
        // {
        //     radius = 1.5;
        //     mass = radius * polygonoid_mass_default;
        //     velocity = (Vector2d){-3, 3};
        //     CreateAddNewtonoid(3, radius, SHAPE_MATH_EQUIDISTANT, mass, colour, click_world_coords, velocity, acceleration); // triangle
        //     UpdateWorld(&G_WorldState, frame_counter.delta_time);
        // }
        // if (IsKeyPressed(KEY_FOUR))
        // {
        //     radius = 1.5;
        //     mass = radius * polygonoid_mass_default;
        //     velocity = (Vector2d){-3, 3};
        //     CreateAddNewtonoid(4, radius, SHAPE_MATH_EQUIDISTANT, mass, colour, click_world_coords, velocity, acceleration); // rectangle
        //     UpdateWorld(&G_WorldState, frame_counter.delta_time);
        // }
        // Handle UP click: check if clicking different world or spawning in current world
        // if (IsKeyPressed(KEY_UP) && cursor_in_region)
        // {
        //     Vector2d click_pixel_coords = {mouse_x, mouse_y};
        //     Vector2d click_universe_coords = TransformCoordinates(G_Universe.camera.dest_to_source_mtx, click_pixel_coords);

        //     // Find which world (if any) was clicked
        //     int clicked_world_idx = -1;
        //     for (int i = 0; i < G_Universe.world_count; i++)
        //     {
        //         World2d *w = &G_Universe.worlds[i];
        //         Vector2d res = w->grid_space.space.resolution_ixj;

        //         // Un-map Universe Space to enter this specific World's Local Space.
        //         // This single matrix multiplication handles the world's custom rotation, scaling, and position.
        //         Vector2d local_coords = TransformCoordinates(w->camera.source_to_dest_mtx, click_universe_coords);

        //         // Re-anchor from center pivot to Top-Left for the boundary verification.
        //         // If w->world_to_uni_inverse treats the center of the world as (0,0):
        //         float half_width = res.x * 0.5f;
        //         float half_height = res.y * 0.5f;

        //         Vector2d local_grid_pos = {
        //             local_coords.x + half_width,
        //             local_coords.y + half_height};

        //         // Because the inverse matrix completely straightened out the rotation,
        //         // we can now safely use a standard flat bounds check!
        //         if (local_grid_pos.x >= 0.0f && local_grid_pos.y >= 0.0f &&
        //             local_grid_pos.x < res.x && local_grid_pos.y < res.y)
        //         {
        //             clicked_world_idx = i;
        //             break; // Found it!
        //         }
        //     }

        //     if (clicked_world_idx >= 0)
        //     {
        //         // A world was clicked
        //         if (clicked_world_idx == G_Universe.selected_world_index)
        //         {
        //             // Click was in the currently selected world; spawn object
        //             CreateAddNewtonoid(vertice_count, radius, SHAPE_MATH_POLY_HULL, mass, colour, click_world_coords, velocity, acceleration);
        //             finishScreen = 1;
        //         }
        //         else
        //         {
        //             // Different world was clicked; select it
        //             Universe_SelectWorld(&G_Universe, clicked_world_idx, game_viewport_local_origin);
        //         }
        //     }
        //     else
        //     {
        //         // No world was hit; deselect and reset camera offset so all worlds are visible
        //         G_Universe.selected_world_index = -1;
        //         //G_Universe.camera_offset = ZERO_VECTOR_2D;
        //     }
        // }

        // Vector2d ResolveEntityWorldCoords(Newtonoid2d *a, WorldState context)
// {
//    // Base Case 1: If this object has no parent, its local coordinates are its world coordinates
//    if (a->parent_id == 0)
//    {
//       return a->anchor_position;
//    }

//    int parent_index = 0;
//    FlatMapInt_GetValue(context.entity_world_index_registry, a->parent_id, &parent_index);

//    // Base Case 2: Parent ID exists but can't be found/resolved in registry
//    if (parent_index <= 0)
//    {
//       LOG_WARN("Parent object with ID %d could not be found in the entiy-world index registry in ResolveEntityWorldCoords.", a->parent_id);
//       return a->anchor_position;
//    }

//    Newtonoid2d *parent = (Newtonoid2d *)LArray_Get(context.world_objects, parent_index);
//    if (parent == NULL)
//    {
//       return a->anchor_position;
//    }

//    // Recursion --> Go find the parent's absolute world coordinates first
//    Vector2d parent_world_coords = ResolveEntityWorldCoords(parent, context);

//    // Unwinding: Add this object's local offset to the parent's world position
//    return VectorSum_2d(parent_world_coords, a->anchor_position);
// }

//**********************************************************************************************
// {
//    ScheduledAction action = CreateScheduledAction(update_action_to_run, 1);

//     // Execute the function that was passed in!
//     update_action_to_run(event_data);

// }
// AxisIntersectionRange2d GetAxisCollisionRange(float min_a, float max_a, float min_b, float max_b, Vector2d unit_axis)
// {
//    // Even though u and v axis live in world space, the scalar results (minA, maxA) do not retain their independent X and Y identities.
//    // Once you perform the dot product operation, we have flattened a 2D coordinate into a single 1D number line
//    // You cannot reverse-engineer that single number back into a unique 2D point because you have discarded the perpendicular information.
//    // We need to convert the 1D scalar overlap region into the global X and Y world space axis from the u_unit_axis and v_unit_axis
//    // To get back to 2D world space coordinates, we aren't changing coordinate systems or multiplying by an inverse basis matrix.
//    // We are simply taking that 1D scalar boundary (d_start, d_end) line segment and un-flattening it back along the global vector direction: e.g., P_start = d_start * u_axis.
//    AxisIntersectionRange2d global_range = {0};

//    // Find the 1D scalar overlap region on the separating axis (u_axis or v_axis from SAT)
//    // The overlap starts at the maximum of the two minimums
//    float overlap_start = (min_a > min_b) ? min_a : min_b;
//    // The overlap ends at the minimum of the two maximums
//    float overlap_end = (max_b < max_b) ? max_b : max_b;

//    // If start >= end, there is no actual overlap happening on this axis
//    if (overlap_start >= overlap_end)
//    {
//       return global_range;
//    }

//    // Transform the 1D scalars back into 2D World Coordinate Space
//    // Since the axis vector is already in world space, we just scale it
//    global_range.start.x = overlap_start * unit_axis.x;
//    global_range.start.y = overlap_start * unit_axis.y;

//    global_range.end.x = overlap_end * unit_axis.x;
//    global_range.end.y = overlap_end * unit_axis.y;

//    return global_range;
// }
// Matrix2x2 CalcProjectedPoints_SAT(LArray vertices_arr, Vector2d vertice_offset, Vector2d u_unit_axis, Vector2d v_unit_axis) // Newtonoid2d *a, Newtonoid2d *b)
// {
//    // GET MAX AND MIN OF ALL VERTICES BY PROJECTING ONTO THE ABOVE AXIS (instead of the default x,y axis)
//    float min_x = INFINITY;
//    float max_x = -INFINITY;
//    float min_y = INFINITY;
//    float max_y = -INFINITY;

//    Vector2d *vertices = (Vector2d *)vertices_arr.items;
//    for (size_t j = 0; j < vertices_arr.count; j++)
//    {
//       Vector2d vertice = VectorSum_2d(vertices[j], vertice_offset); // Get vertice's world coordinates by adding the object's center coordinates to the vertice's local coordinates
//       // U AXIS
//       //  Angle between vertice_line and v_axis_unit is calculated by getting the difference in their angles in radians, then using that angle to get the magnitude of the vertice_line multiplied by the cosine of that angle to get the length of the vertice_line that is projected onto the v_axis_unit. This is equivalent to the dot product of the vertice_line and v_axis_unit.
//       Polar2d vertice_polar = PolarForm_2d(vertice);
//       float angle_diff = vertice_polar.radians - u_unit_axis.radians;
//       float vertice_proj_u_axis = vertice_polar.magnitude * cosf(angle_diff);

//       if (vertice_proj_u_axis > max_x)
//       {
//          max_x = vertice_proj_u_axis;
//       }
//       else if (vertice_proj_u_axis < min_x)
//       {
//          min_x = vertice_proj_u_axis;
//       }
//       printf("Vertice %zu projects to x = %0.2f on u_axis of line (%zu -> %zu). Radians = %0.2f\n", j, vertice_proj_u_axis, j, j + 1, angle_diff);

//       // V AXIS
//       // Angle between vertice_line and v_axis_unit is calculated by getting the difference in their angles in radians, then using that angle to get the magnitude of the vertice_line multiplied by the cosine of that angle to get the length of the vertice_line that is projected onto the v_axis_unit. This is equivalent to the dot product of the vertice_line and v_axis_unit.
//       angle_diff = vertice_polar.radians - v_unit_axis.radians;
//       float vertice_proj_v_axis = vertice_polar.magnitude * cosf(angle_diff);

//       if (vertice_proj_v_axis > max_y)
//       {
//          max_y = vertice_proj_v_axis;
//       }
//       else if (vertice_proj_v_axis < min_y)
//       {
//          min_y = vertice_proj_v_axis;
//       }

//       printf("Vertice %zu projects to y = %.2f on v_axis of line (%zu -> %zu). Radians = %0.2f\n", j, vertice_proj_v_axis, j, j + 1, angle_diff);
//    }
//    return (Matrix2x2){{min_x, max_x}, {min_y, max_y}};
// }

// void ResolvePenetration(Newtonoid2d *a, Newtonoid2d *b)
// {
//    // COMMON DATA FOR PENETRATION & VELOCITY RESOLUTIONS
//    Vector2d a_b_vel_diff = VectorSum_2d(a->velocity, VectorScale_2d(b->velocity, -1));              // relative velocity, or velocity felt by a
//    Vector2d a_b_pos_diff = VectorSum_2d(a->anchor_position, VectorScale_2d(b->anchor_position, -1.0f)); // distance vector between centers (From B to A)
//    float a_b_pos_diff_mag = VectorMagnitude_2d(a_b_pos_diff);                                       // actual distance when just touching
//    float a_b_min_diff_mag = a->radius + b->radius;                                                  // target distance when just touching

//    // RESOLVE PENETRATION
//    if (a_b_pos_diff_mag < a_b_min_diff_mag)
//    {
//       // Calc scalar penetration depth
//       float penetration_depth = a_b_min_diff_mag - a_b_pos_diff_mag;

//       // Calc the collision normal unit vector
//       Vector2d normal = {0.0f, 0.0f};
//       if (a_b_pos_diff_mag > 0.0f)
//       {
//          normal = VectorScale_2d(a_b_pos_diff, 1.0f / a_b_pos_diff_mag);
//       }
//       else
//       {
//          // Edge case: Objects are perfectly stacked on top of each other. Pick an arbitrary up normal to push them apart.
//          normal = (Vector2d){0.0f, -1.0f};
//       }
//       // Distribute the overlapping vector between A and B to shift their positions (weighted inversely to their mass)
//       float total_inv_mass = a->inverse_mass + b->inverse_mass;
//       if (total_inv_mass <= 0.0f)
//          return; // both are static immovable objects, skip push out

//       // Calc Resolution Shifting Multipliers
//       float a_move_fraction = a->inverse_mass / total_inv_mass;
//       float b_move_fraction = b->inverse_mass / total_inv_mass;

//       // Total resolution vector required
//       Vector2d separation_vector = VectorScale_2d(normal, penetration_depth);

//       // A moves forward along the normal vector direction, B moves backward along the normal vector direction
//       a->anchor_position = VectorSum_2d(a->anchor_position, VectorScale_2d(separation_vector, a_move_fraction));
//       b->anchor_position = VectorSum_2d(b->anchor_position, VectorScale_2d(separation_vector, -b_move_fraction));
//    }
// }

// void ResolveVelocity(Newtonoid2d *a, Newtonoid2d *b)
// {
//    // COMMON DATA FOR PENETRATION & VELOCITY RESOLUTIONS
//    Vector2d a_b_vel_diff = VectorSum_2d(a->velocity, VectorScale_2d(b->velocity, -1));              // relative velocity, or velocity felt by a
//    Vector2d a_b_pos_diff = VectorSum_2d(a->anchor_position, VectorScale_2d(b->anchor_position, -1.0f)); // distance vector between centers (From B to A)
//    float a_b_pos_diff_mag = VectorMagnitude_2d(a_b_pos_diff);                                       // actual distance when just touching
//    float a_b_min_diff_mag = a->radius + b->radius;                                                  // target distance when just touching

//    // RESOLVE VELOCITY
//    float total_inverse_mass = a->inverse_mass + b->inverse_mass;
//    if (total_inverse_mass <= 0.0f) // We have 2 immovable/static objects so just continue to the next collision
//    {
//       printf("WARNING: Both objects in this collision have infinite mass (0 inverse mass). No collision response applied.\n");
//       return;
//    }
//    // Apply momentum conservation to determine velocities of a and b
//    float a_mom_1 = VectorMagnitude_2d(a->velocity) / a->inverse_mass;
//    float b_mom_1 = VectorMagnitude_2d(b->velocity) / b->inverse_mass;
//    // For simplicity, we'll assume the normal is in the direction that starts at A's origin and points to B's origin
//    Vector2d a_b_pos_normal = VectorScale_2d(a_b_pos_diff, 1 / VectorMagnitude_2d(a_b_pos_diff)); // this is the unit-direction
//    float a_b_vel_dot = VectorDot_2d(a_b_vel_diff, a_b_pos_normal);                               // Velocity along the Normal (The Dot Product)

//    // Get the Impulse: $$j = \frac{-(1 + e)(\mathbf{v}_{rel} \cdot \mathbf{n})}{\frac{1}{m_a} + \frac{1}{m_b}}$$
//    // Calculate Impulse Scalar
//    float e = 1; // Coefficient of Restitution
//    float j = -(1 + e) * a_b_vel_dot / total_inverse_mass;

//    // Apply the Impulse
//    // Turn that scalar back into a vector and update the velocities
//    Vector2d impulse_vector = VectorScale_2d(a_b_pos_normal, j);

//    // Apply the impulse vector to A and B to get velocities
//    Vector2d a_vel_change = VectorScale_2d(impulse_vector, a->inverse_mass);
//    Vector2d b_vel_change = VectorScale_2d(impulse_vector, b->inverse_mass);

//    a->velocity = VectorSum_2d(a->velocity, a_vel_change);
//    b->velocity = VectorSum_2d(b->velocity, VectorScale_2d(b_vel_change, -1));
// }

// Need assign an area of effect (footprint), i.e. Snapped AABB, for the object based on its radius and update the occupancy of all cells that fall within that area
// otherwise we won't detect collisions until the objects are already overlapping significantly, which can cause tunneling issues where fast moving objects pass through each other without detecting a collision.
// Surface2d snapped_aabb = CalculateSnappedAABB(space->basis, object->surface, object->anchor_position);

// void UpdateWorld(World2d *world, float delta_time)
// {
//    LArray *objects = &world->objects; //.items;
//    int count = objects->count;
//    // 1. Update Object state first
//    // 1.1 Update Grid cells while we're here
//    if (count < 1)
//       return;

//    // Zero out the occupancy and object_ids of all cells in the grid before we update them based on the new positions of the objects
//    Cell *cells = world->grid_space.space.cells.coll.items;
//    int cell_count = world->grid_space.space.cells.coll.count;
//    for (size_t i = 0; i < cell_count; i++)
//    {
//       Cell *target_cell = &cells[i];
//       target_cell->occupancy = 0;
//       memset(&cells[i].object_ids, 0, sizeof(cells[i].object_ids));
//    }

//    Polygonoid *polygonoids = objects->items;
//    for (size_t i = 0; i < count; i++)
//    {
//       Newtonoid2d *obj = &polygonoids[i].newtonian_properties;

//       // Ensure the object isn't outside the bounds of the world before we try to get the cell it's in, otherwise we could get an out of bounds error when we try to access the cell's object_ids array. We can just skip updating the cell for this object if it's out of bounds, but we should still update its vectors based on its acceleration and velocity so that it can move back into the bounds of the world.
//       if (obj->bounds_origin.x < 0 || obj->bounds_origin.x >= world->grid_space.space.resolution_ixj.x ||
//           obj->bounds_origin.y < 0 || obj->bounds_origin.y >= world->grid_space.space.resolution_ixj.y)
//       {
//          printf("WARNING: Object ID %d is out of bounds at coordinates (%.1f, %.1f). Skipping cell update.\n", polygonoids[i].id, obj->bounds_origin.x, obj->bounds_origin.y);

//          // Need to calculate a collision response to push the object back into the bounds of the world here, otherwise it will just keep moving out of bounds and we won't be able to track it anymore. For simplicity, let's just reverse the velocity of the object when it hits the boundary of the world, which will create a bouncing effect. We can also apply a damping factor to the velocity to simulate energy loss during the collision, which will prevent the object from bouncing indefinitely.
//          if (obj->bounds_origin.x < 0 || obj->bounds_origin.x >= world->grid_space.space.resolution_ixj.x)
//          {
//             obj->velocity.x = -obj->velocity.x; // Reverse and dampen the x velocity
//             // obj->bounds_origin.x = obj->bounds_origin.x < 0 ? 0 : world->grid_space.space.resolution_ixj.x - 1; // Move the object back within bounds
//          }
//          if (obj->bounds_origin.y < 0 || obj->bounds_origin.y >= world->grid_space.space.resolution_ixj.y)
//          {
//             obj->velocity.y = -obj->velocity.y; // Reverse and dampen the y velocity
//             // obj->bounds_origin.y = obj->bounds_origin.y < 0 ? 0 : world->grid_space.space.resolution_ixj.y - 1; // Move the object back within bounds
//          }
//          //Recalc inverse_mass in case mass was changed
//          obj->inverseMass = 1.0/obj->mass;
//          CalculateVectors(obj, delta_time); // Still update the object's vectors based on its acceleration and velocity so that it can move back into the bounds of the world
//          continue;
//       }

//       // Add the object's ID to the cell's object_ids array if there is space
//       Cell *target_cell = GetCellFromCoords(&world->grid_space.space, polygonoids[i].newtonian_properties.bounds_origin);

//       if (target_cell != NULL && target_cell->occupancy < MAX_CELL_OCCUPANCY)
//       {
//          target_cell->object_ids[target_cell->occupancy] = polygonoids[i].id;
//          target_cell->occupancy++;
//       }
//       else
//       {
//          printf("WARNING: Cell (%d,%d) full. ID %d not tracked spatially.\n", target_cell->coords.x, target_cell->coords.y, polygonoids[i].id);
//          return;
//       }

//       // Update the object's vectors based on its current acceleration, velocity, and position, and the elapsed time since the last update
//       CalculateVectors(obj, delta_time);
//    }

//    // 2. Check for collisions
//    if (count < 2)
//       return;

//    //for(int i = 0; i <)
//    for (size_t i = 0; i < count; i++)
//    {
//       for (size_t j = i + 1; j < count; j++) // Optimized j loop
//       {
//          Polygonoid *a = &polygonoids[i];
//          Polygonoid *b = &polygonoids[j];

//          bool colliding = CheckForCollision(a->newtonian_properties, b->newtonian_properties);

//          if (colliding)
//          {
//             // Apply momentum conservation to determine velocities of a and b
//             float a_mom_1 = VectorMagnitude_2d(a->newtonian_properties.velocity) / a->newtonian_properties.inverseMass;
//             float b_mom_1 = VectorMagnitude_2d(b->newtonian_properties.velocity) / b->newtonian_properties.inverseMass;
//             Vector2d a_b_vel = VectorSum_2d(a->newtonian_properties.velocity, VectorScale_2d(b->newtonian_properties.velocity, -1));

//             // Get the collision normal - just use A as the reference object
//             // For simplicity, we'll assume the normal is in the direction that starts at A's origin and points to B's origin
//             Vector2d a_b_pos = VectorSum_2d(a->newtonian_properties.bounds_origin, VectorScale_2d(b->newtonian_properties.bounds_origin, -1));
//             Vector2d a_b_pos_normal = VectorScale_2d(a_b_pos, 1 / VectorMagnitude_2d(a_b_pos));

//             // Velocity along the Normal (The Dot Product)
//             float a_b_vel_dot = VectorDot_2d(a_b_vel, a_b_pos_normal);

//             // Get the Impulse: $$j = \frac{-(1 + e)(\mathbf{v}_{rel} \cdot \mathbf{n})}{\frac{1}{m_a} + \frac{1}{m_b}}$$
//             // Calculate Impulse Scalar
//             float e = 1; // Coefficient of Restitution
//             float j = -(1 + e) * a_b_vel_dot / (a->newtonian_properties.inverseMass + b->newtonian_properties.inverseMass);

//             // Apply the Impulse
//             // Turn that scalar back into a vector and update the velocities
//             Vector2d impulse_vector = VectorScale_2d(a_b_pos_normal, j);

//             // Apply the impulse vector to A and B to get velocities
//             Vector2d a_vel_change = VectorScale_2d(impulse_vector, a->newtonian_properties.inverseMass);
//             Vector2d b_vel_change = VectorScale_2d(impulse_vector, b->newtonian_properties.inverseMass);

//             a->newtonian_properties.velocity = VectorSum_2d(a->newtonian_properties.velocity, a_vel_change);
//             b->newtonian_properties.velocity = VectorSum_2d(b->newtonian_properties.velocity, VectorScale_2d(b_vel_change, -1));
//          }

//          frame_counter.total_frames % 300 == 0 ? printf("COLLISION CHECK for A(%.0f,%.0f) B(%.0f,%.0f) = %s\n",
//                 a->newtonian_properties.bounds_origin.x,
//                 a->newtonian_properties.bounds_origin.y,
//                 b->newtonian_properties.bounds_origin.x,
//                 b->newtonian_properties.bounds_origin.y,
//                 colliding ? "TRUE" : "FALSE") : (void)0;

//       }
//    }
//    // UpdateObjectVectors(objs, delta_time);
//    // UpdateWorldState(objs, &world->grid_space.space, delta_time);
// }

// void UpdateWorldState(Collection *polygonoids, Space2d *space, float delta_time)
//{
//  // 1. Update Object state first
//  // 1.1 Update Grid cells while we're here
//  if (polygonoids->count < 1)
//     return;

// // Zero out the occupancy and object_ids of all cells in the grid before we update them based on the new positions of the objects
// Cell *cells = space->cells.coll.items;
// for (size_t i = 0; i < space->cells.coll.count; i++)
// {
//    cells[i].occupancy = 0;
//    memset(cells[i].object_ids, 0, sizeof(cells[i].object_ids));
// }

// Polygonoid *pts = (Polygonoid *)polygonoids->items;
// for (size_t i = 0; i < polygonoids->count; i++)
// {
//    // NO COPYING. Point directly to the source in the heap.
//    CalculateVectors(&pts[i].newtonian_properties, delta_time);
//    // Add the object's ID to the cell's object_ids array if there is space

//    Cell *target_cell = GetCellFromCoords(space, pts[i].newtonian_properties.bounds_origin);

//    if (target_cell != NULL && target_cell->occupancy < MAX_CELL_OCCUPANCY)
//    {
//       object->id = world->next_object_id++;
//       target_cell->object_ids[target_cell->occupancy] = object->id;
//       target_cell->occupancy++;
//    }
//    else
//    {
//       printf("WARNING: Cell %d full. ID %d not tracked spatially.\n", cell_index, object->id);
//       return;
//    }
// }

// // 2. Check for collisions
// if (polygonoids->count < 2)
//    return;

// for (size_t i = 0; i < polygonoids->count; i++)
// {
//    for (size_t j = i + 1; j < polygonoids->count; j++) // Optimized j loop
//    {
//       Polygonoid *a = &pts[i];
//       Polygonoid *b = &pts[j];

//       bool colliding = CheckForCollision(a->newtonian_properties, b->newtonian_properties);

//       printf("COLLISION CHECK for A(%.0f,%.0f) B(%.0f,%.0f) = %s\n",
//              a->newtonian_properties.bounds_origin.x,
//              a->newtonian_properties.bounds_origin.y,
//              b->newtonian_properties.bounds_origin.x,
//              b->newtonian_properties.bounds_origin.y,
//              colliding ? "TRUE" : "FALSE");
//    }
// }
//}

// void UpdateObjectsAndGrid(Collection *polygonoids, float delta_time)
// {
//    Polygonoid *pts = (Polygonoid *)polygonoids->items;

//    if (polygonoids->count < 1)
//       return;

//    for (size_t i = 0; i < polygonoids->count; i++)
//    {
//       // NO COPYING. Point directly to the source in the heap.
//       CalculateVectors(&pts[i].newtonian_properties, delta_time);
//    }
// }

// World CalculateFieldLines(Field field);
// World InitialiseFieldCells(Field field);