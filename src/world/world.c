/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/
#include "common/common.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "events/events.h"
#include "physics/physics.h"
#include "system/job_system.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

static int initObjectCount = 4;

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------

void CreateWorld(GridSpace2d space_obj, Camera2d world_camera, float gravity, World2d *out_world)
{
   // World2d world = {0};
   out_world->grid_space = space_obj;
   out_world->camera = world_camera;
   out_world->gravity = gravity;
   out_world->next_object_id = 1; // Initialize the next available ID for NewtonObjects
   out_world->objects = MakeLArray(initObjectCount, sizeof(Newtonoid2d));
   out_world->collisions = MakeLArray(initObjectCount, sizeof(Matrix2x2));
   out_world->temp_objects = MakeLArray(initObjectCount, sizeof(Newtonoid2d));

   // INIT WORLD INTERNAL STATE (PER-WORLD)
   out_world->entity_space_map = MakeFlatMapInt(1 + (int)(space_obj.space.cells.count / 5));
   out_world->resolved_collisions = MakeFlatMapInt(1 + (int)(out_world->entity_space_map.count / 2));
   out_world->entity_world_index_registry = MakeFlatMapInt(1 + (int)(out_world->entity_space_map.count / 2));
   out_world->scheduled_world_cmds = MakeLArray(initObjectCount, sizeof(WorldCommand));
   G_WorldState.entity_world_index_registry = &out_world->entity_world_index_registry;
   G_WorldState.collisions = &out_world->collisions;
   G_WorldState.world = out_world;
   InitJobSystem(256);
   // return world;
}

int AddObjectToWorld(World2d *world, Newtonoid2d *object, int parent_id)
{
   // Object placement is center-based; grid occupancy still snaps that center into a cell index.
   Vector2d local_coords = object->coords_center;
   object->parent_id = parent_id;
   if (local_coords.x < 0 || local_coords.y < 0 || local_coords.x >= world->grid_space.space.columns || local_coords.y >= world->grid_space.space.rows)
   {
      LOG_WARN("Desired spawn point (%0.2f,%0.2f) out of bounds. Cannot add entity to the world.\n", local_coords.x, local_coords.y);
      return -1; // Click is outside the structural world viewport boundaries! Avoid resolving cell.
   }

   // Register the object first so the world/entity maps stay in sync with its center position.
   int assigned_id = RegisterEntity(&G_WorldState, object);

   // Solid objects are collision-enabled, need to be tracked spacially
   int cell_index = -1;
   if (!(object->entity_layer & FLAG_TYPE_EFFECT))
   {
      cell_index = ((int)local_coords.y * world->grid_space.space.columns) + (int)local_coords.x;
      // Add the object's ID to the cell's object_ids array if there is space, and update the object's footprint based on its surface and the coordinate space's basis vectors.
      // We also need to update the occupancy of the cell and ensure that we don't exceed the maximum
      Cell *cells = world->grid_space.space.cells.items;
      Cell *target_cell = &cells[cell_index];
      if (target_cell->occupancy < MAX_CELL_OCCUPANCY)
      {
         target_cell->object_ids[target_cell->occupancy] = object->id;
         target_cell->occupancy++;
      }
      else
      {
         LOG_WARN("Cell %d full. ID %d not tracked spatially.\n", cell_index, object->id);
         return assigned_id;
      }
   }
   // object->id = world->next_object_id++;
   // LArray_Push(&world->objects, object);
   LOG_INFO("CREATED OBJECT (ID %d): Cell %d : Center(%.1f, %.1f)\n", object->id, cell_index, local_coords.x, local_coords.y);
   return assigned_id;
}

void UpdateWorld(WorldState *context, float delta_time)
{
   // PrintCurrentBytesAlloc();
   LArray *objects = &context->world->objects;
   int obj_count = objects->count;
   if (obj_count < 1)
      return;
   GridSpace2d *space_entity = &context->world->grid_space;
   Space2d *space = &space_entity->space;

   // RESET TRACKING-STATE - Zero out
   FlatMapInt *entity_space_map = &context->world->entity_space_map;
   FlatMapInt *resolved_collisions = &context->world->resolved_collisions;
   FlatMapInt *entity_world_index_registry = &context->world->entity_world_index_registry;
   LArray *scheduled_world_cmds = &context->world->scheduled_world_cmds;

   LArray_Reset(context->collisions);
   ResetFlatMapInt(entity_space_map);
   ResetFlatMapInt(resolved_collisions);

   // Zero out the occupancy and object_ids of all cells in the grid before we update them based on the new positions of the objects
   Cell *cells = space_entity->space.cells.items;
   int cell_count = space->cells.count;
   for (size_t i = 0; i < cell_count; i++)
   {
      Cell *target_cell = &cells[i];
      target_cell->occupancy = 0;
      MemorySet(&cells[i].object_ids, 0, sizeof(cells[i].object_ids));
   }

   // RUN SCHEDULED WORLD EVENTS
   RunScheduledWorldCmds(scheduled_world_cmds, context);

   if (!IsJobSystemInitialized())
   {
      InitJobSystem(256);
   }
   ClearJobs();
   SubmitJob(PhysicsUpdateJob, context, obj_count, 8);
   ExecuteJobs();
   ClearJobs();

   Newtonoid2d *newtonoids = (Newtonoid2d *)objects->items;
   // PASS 1: Simulating Independent Physics
   // Update object positions based on their velocity and acceleration, then update the cells they occupy in the coordinate space grid as well as the entity_space_map which tracks how many objects occupy each cell (for collision checking later)
   // The physics work is now split into jobs for better task separation and future parallelism.

   // NOTE: The loop body is executed via PhysicsUpdateJob. No inline loop here anymore.

   LArray *temp_objects = &context->world->temp_objects;
   // PASS 2: Resolving Attachment Hierarchies
   for (size_t i = 0; i < temp_objects->count; i++)
   {
      // Newtonoid2d *child = &newtonoids[i];
      Newtonoid2d *child = (Newtonoid2d *)LArray_Get(temp_objects, i);

      if (!(child->flags & FLAG_STATUS_ALIVE) || child->parent_id == space_entity->object.id)
         continue;

      // Look up where the parent currently lives in memory using the registry
      int parent_packed_loc = 0;
      if (!FlatMapInt_GetValue(entity_world_index_registry, child->parent_id, &parent_packed_loc))
      {
         // The parent was likely deleted! Detach or kill the child so it doesn't crash
         child->parent_id = -1;
         continue;
      }

      // Unpack the routing info to pull the parent out of the correct array
      int parent_type = UNPACK_INT_HIGH(parent_packed_loc);
      int parent_idx = UNPACK_INT_LOW(parent_packed_loc);
      LArray *parent_array = (parent_type == ARCHETYPE_CLOCKED) ? temp_objects : objects;

      Newtonoid2d *parent = (Newtonoid2d *)LArray_Get(parent_array, parent_idx);

      // Glue the child's world position to the parent's new position + offset
      child->coords_center = VectorSum_2d(parent->coords_center, child->local_offset);
      child->coords_origin = VectorSum_2d(parent->coords_origin, child->coords_origin);
   }
   // Check for collisions
   if (obj_count < 2) // early return
   {
      // ClearFlatMapInt(&entity_space_map);
      // PrintCurrentBytesAlloc();
      return;
   }

   World2d *world = context->world;

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
            // Check against all possible object pairings out of all object occupants in this cell
            for (size_t n = m + 1; n < cell_occ; n++)
            {
               int obj_id_a = cell->object_ids[m];
               int obj_id_b = cell->object_ids[n];

               if (obj_id_a >= world->next_object_id || obj_id_b >= world->next_object_id || obj_id_a < 1 || obj_id_b < 1)
               {
                  LOG_ERROR("Could not find objects with IDs %d and %d in Cell (index = %d).\n", obj_id_a, obj_id_b, cell_i);
                  continue;
               }
               // Check whether this object-pair has been resolved already
               unsigned long obj_pair_hash_key = CalcHashFromInts(obj_id_a, obj_id_b);
               short is_resolved = 0;
               if (FlatMapInt_GetValue(resolved_collisions, obj_pair_hash_key, (int *)&is_resolved))
                  continue;
               if (obj_id_a < 1 || obj_id_b < 1)
                  continue; // Early safety rejection
               Newtonoid2d *a = &newtonoids[obj_id_a - 1];
               Newtonoid2d *b = &newtonoids[obj_id_b - 1];

               // Check collision flags for collision compatiibility
               if (!(a->collision_mask & b->entity_layer) || !(b->collision_mask & a->entity_layer))
                  continue;

               CollisionResult_SAT collision_result = CheckForCollision_SAT(a, b);
               if (collision_result.is_colliding == true)
               {
                  LArray_Push(&world->collisions, &collision_result.collision_box);
                  LOG_INFO("COLLISION detected between Object ID %d and Object ID %d Coord Box Range: [%0.2f,%0.2f] [%0.2f,%0.2f] \n", obj_id_a, obj_id_b, collision_result.collision_box.col1.x, collision_result.collision_box.col1.y, collision_result.collision_box.col2.x, collision_result.collision_box.col2.y);
                  ResolveCollision(a, b);
                  // Add object pair's key to hash map of resolved collisions
                  FlatMapInt_InsertOrUpdate(resolved_collisions, obj_pair_hash_key, 1); // a value of 1 means resolved

                  // For debugging, create a temporary object at the collision center with the dimensions of the collision box to visualize the collision area and the entity that is penetrating the other
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
                  Newtonoid2d collision_obj = CreateNewtonoid2d(0.00001f, collision_center, penetrating_entity->velocity, penetrating_entity->acceleration, collision_surface);
                  collision_obj.entity_layer = FLAG_TYPE_EFFECT;
                  collision_obj.flags |= FLAG_LIFETIME_CLOCKED;
                  StickEntity(&G_WorldState, &collision_obj, penetrating_entity);                        // ISSUE IS HERE OR WHEN GETTING COLLISION BOX VERTICES ABOVE
                  int id = AddObjectToWorld(G_WorldState.world, &collision_obj, penetrating_entity->id); // looks like a duplicate collision object is being created? continue this debug session and see what array it's in

                  // Create a scheduled update to flag the collision object for removal
                  ScheduleEntityDeletion(scheduled_world_cmds, id, FLAG_STATUS_ALIVE, 120, 1, 1);
               }
            }
         }
      }
   }
}


// Vector2d ResolveEntityWorldCoords(Newtonoid2d *a, WorldState context)
// {
//    // Base Case 1: If this object has no parent, its local coordinates are its world coordinates
//    if (a->parent_id == 0)
//    {
//       return a->coords_center;
//    }

//    int parent_index = 0;
//    FlatMapInt_GetValue(context.entity_world_index_registry, a->parent_id, &parent_index);

//    // Base Case 2: Parent ID exists but can't be found/resolved in registry
//    if (parent_index <= 0)
//    {
//       LOG_WARN("Parent object with ID %d could not be found in the entiy-world index registry in ResolveEntityWorldCoords.", a->parent_id);
//       return a->coords_center;
//    }

//    Newtonoid2d *parent = (Newtonoid2d *)LArray_Get(context.world_objects, parent_index);
//    if (parent == NULL)
//    {
//       return a->coords_center;
//    }

//    // Recursion --> Go find the parent's absolute world coordinates first
//    Vector2d parent_world_coords = ResolveEntityWorldCoords(parent, context);

//    // Unwinding: Add this object's local offset to the parent's world position
//    return VectorSum_2d(parent_world_coords, a->coords_center);
// }

// void AddScheduledWorldUpdate(int event_data, Action update_action_to_run)
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
//    Vector2d a_b_pos_diff = VectorSum_2d(a->coords_center, VectorScale_2d(b->coords_center, -1.0f)); // distance vector between centers (From B to A)
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
//       a->coords_center = VectorSum_2d(a->coords_center, VectorScale_2d(separation_vector, a_move_fraction));
//       b->coords_center = VectorSum_2d(b->coords_center, VectorScale_2d(separation_vector, -b_move_fraction));
//    }
// }

// void ResolveVelocity(Newtonoid2d *a, Newtonoid2d *b)
// {
//    // COMMON DATA FOR PENETRATION & VELOCITY RESOLUTIONS
//    Vector2d a_b_vel_diff = VectorSum_2d(a->velocity, VectorScale_2d(b->velocity, -1));              // relative velocity, or velocity felt by a
//    Vector2d a_b_pos_diff = VectorSum_2d(a->coords_center, VectorScale_2d(b->coords_center, -1.0f)); // distance vector between centers (From B to A)
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
// Surface2d snapped_aabb = CalculateSnappedAABB(space->basis, object->surface, object->coords_center);

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
//       if (obj->coords_origin.x < 0 || obj->coords_origin.x >= world->grid_space.space.resolution_ixj.x ||
//           obj->coords_origin.y < 0 || obj->coords_origin.y >= world->grid_space.space.resolution_ixj.y)
//       {
//          printf("WARNING: Object ID %d is out of bounds at coordinates (%.1f, %.1f). Skipping cell update.\n", polygonoids[i].id, obj->coords_origin.x, obj->coords_origin.y);

//          // Need to calculate a collision response to push the object back into the bounds of the world here, otherwise it will just keep moving out of bounds and we won't be able to track it anymore. For simplicity, let's just reverse the velocity of the object when it hits the boundary of the world, which will create a bouncing effect. We can also apply a damping factor to the velocity to simulate energy loss during the collision, which will prevent the object from bouncing indefinitely.
//          if (obj->coords_origin.x < 0 || obj->coords_origin.x >= world->grid_space.space.resolution_ixj.x)
//          {
//             obj->velocity.x = -obj->velocity.x; // Reverse and dampen the x velocity
//             // obj->coords_origin.x = obj->coords_origin.x < 0 ? 0 : world->grid_space.space.resolution_ixj.x - 1; // Move the object back within bounds
//          }
//          if (obj->coords_origin.y < 0 || obj->coords_origin.y >= world->grid_space.space.resolution_ixj.y)
//          {
//             obj->velocity.y = -obj->velocity.y; // Reverse and dampen the y velocity
//             // obj->coords_origin.y = obj->coords_origin.y < 0 ? 0 : world->grid_space.space.resolution_ixj.y - 1; // Move the object back within bounds
//          }
//          //Recalc inverse_mass in case mass was changed
//          obj->inverseMass = 1.0/obj->mass;
//          CalculateVectors(obj, delta_time); // Still update the object's vectors based on its acceleration and velocity so that it can move back into the bounds of the world
//          continue;
//       }

//       // Add the object's ID to the cell's object_ids array if there is space
//       Cell *target_cell = GetCellFromCoords(&world->grid_space.space, polygonoids[i].newtonian_properties.coords_origin);

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
//             Vector2d a_b_pos = VectorSum_2d(a->newtonian_properties.coords_origin, VectorScale_2d(b->newtonian_properties.coords_origin, -1));
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
//                 a->newtonian_properties.coords_origin.x,
//                 a->newtonian_properties.coords_origin.y,
//                 b->newtonian_properties.coords_origin.x,
//                 b->newtonian_properties.coords_origin.y,
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

//    Cell *target_cell = GetCellFromCoords(space, pts[i].newtonian_properties.coords_origin);

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
//              a->newtonian_properties.coords_origin.x,
//              a->newtonian_properties.coords_origin.y,
//              b->newtonian_properties.coords_origin.x,
//              b->newtonian_properties.coords_origin.y,
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


