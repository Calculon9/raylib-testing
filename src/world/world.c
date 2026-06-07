/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/
#include "common/common.h"
#include "world/world.h"
#include "system/systems.h"
#include "physics/rectangloid.h"
#include "physics/field.h"
#include "physics/polygonoid.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

// Physical state variables
static int next_id = 1;      // Global variable to keep track of the next available ID for NewtonObjects
static float gravity = 9.8f; // Gravitational acceleration (m/s^2)
// static float field = {0};

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
bool CheckForCollision(NewtonObject2d a, NewtonObject2d b);
void ResolveCollision(NewtonObject2d *a, NewtonObject2d *b);
void ResolveCollision_ContainerRect(NewtonObject2d *entity, NewtonObject2d *container);
void ResolvePenetration(NewtonObject2d *a, NewtonObject2d *b);
void ResolveVelocity(NewtonObject2d *a, NewtonObject2d *b);
void MapEntityToASpace(CoordSpace2d *space, NewtonObject2d *object, Matrix2x2 snapped_aabb_box, FlatMapInt *O_entity_to_space_index_map);
// void UpdateWorldState(Collection *objects, CoordSpace2d *space, float delta_time);
//  void UpdateObjectVectors(Collection *objects, float delta_time);

World2d CreateWorld(CoordSpace2d_Grid space_obj, float gravity)
{
   World2d world = {0};
   world.coord_space_grid = space_obj;
   // world.objects = objects;
   world.gravity = gravity;
   world.next_object_id = 1; // Initialize the next available ID for NewtonObjects

   return world;
}

void AddObjectToWorld(World2d *world, Polygonoid *object, int parent_id)
{
   // We can calculate the cell indices based on the object's coordinates and the coordinate space's basis vectors and resolution
   // For simplicity, let's assume the object's coords_center is the point we will use to determine which cell it occupies
   Vector2d local_coords = object->newtonian_properties.coords_center; // These are the world coordinates of the object, which are the cell indices
   object->newtonian_properties.parent_id = parent_id;
   if (local_coords.x < 0 || local_coords.y < 0 || local_coords.x >= world->coord_space_grid.coord_space.resolution_ixj.x || local_coords.y >= world->coord_space_grid.coord_space.resolution_ixj.y)
   {
      return; // Click is outside the structural world viewport boundaries! Avoid resolving cell.
   }

   int cell_index = ((int)local_coords.y * (int)world->coord_space_grid.coord_space.resolution_ixj.x) + (int)local_coords.x;
   // Add the object's ID to the cell's object_ids array if there is space, and update the object's footprint based on its surface and the coordinate space's basis vectors.
   // We also need to update the occupancy of the cell and ensure that we don't exceed the maximum
   Cell *cells = world->coord_space_grid.coord_space.cells.items;
   Cell *target_cell = &cells[cell_index];
   if (target_cell->occupancy < MAX_CELL_OCCUPANCY)
   {
      object->newtonian_properties.id = world->next_object_id++;
      target_cell->object_ids[target_cell->occupancy] = object->newtonian_properties.id;
      target_cell->occupancy++;
      // object->newtonian_properties.footprint = CalcSnappedAABB(world->coord_space_grid.coord_space.basis, object->newtonian_properties.surface, ZERO_VECTOR_2D);
   }
   else
   {
      printf("WARNING: Cell %d full. ID %d not tracked spatially.\n", cell_index, object->newtonian_properties.id);
      return;
   }

   // Add the newton_object to the world's objects array
   LArray_Push(&world->objects, object);

   printf("CREATED OBJECT (ID %d): Cell %d : Center(%.1f, %.1f)\n", object->newtonian_properties.id, cell_index, local_coords.x, local_coords.y);
}

void UpdateWorld(World2d *world, float delta_time)
{
   // PrintCurrentBytesAlloc();
   int obj_count = world->objects.count;

   if (obj_count < 1)
      return;

   // Zero out the occupancy and object_ids of all cells in the grid before we update them based on the new positions of the objects
   Cell *cells = world->coord_space_grid.coord_space.cells.items;
   int cell_count = world->coord_space_grid.coord_space.cells.count;
   for (size_t i = 0; i < cell_count; i++)
   {
      Cell *target_cell = &cells[i];
      target_cell->occupancy = 0;
      memset(&cells[i].object_ids, 0, sizeof(cells[i].object_ids));
   }
   // PrintCurrentBytesAlloc();
   Polygonoid *polygonoids = (Polygonoid *)world->objects.items;
   Matrix2x2 min_coord_space = {0};
   int starting_flatmap_capacity = 1 + (int)(cell_count / 4); // Start with a smaller capacity than the number of cells since we won't have an object in every cell, but we can resize if needed
   FlatMapInt entity_space_map = MakeFlatMapInt(starting_flatmap_capacity);

   for (size_t i = 0; i < obj_count; i++)
   {
      NewtonObject2d *obj = &polygonoids[i].newtonian_properties;
      Matrix2x2 obj_aabb = CalcAABBCoords_Tight(&obj->surface.surface_vectors, obj->coords_center);
      Matrix2x2 space_aabb = CalcSpaceAABB(&world->coord_space_grid.coord_space);
      bool is_bounded = BoxFitsWithinBox(obj_aabb, space_aabb);

      // RESOLVE ENTITY-CONTAINER COLLISIONS
      if (obj->parent_id == world->coord_space_grid.object.id)
      {

         // Need assign an area of effect for the object based on its radius and update the occupancy of all cells that fall within that area, not just the cell that contains the object's origin coordinates,
         // otherwise we won't detect collisions until the objects are already overlapping significantly, which can cause tunneling issues where fast moving objects pass through each other without detecting a collision.
         // For simplicity, use a square area of effect based on AABB
         LArray snapped_aabb_verts = CalcSnappedAABB(world->coord_space_grid.coord_space.basis, obj->surface.surface_vectors, obj->coords_center);
         Matrix2x2 snapped_aabb_box = CalcAABBCoords_Tight(&snapped_aabb_verts, ZERO_VECTOR_2D);

         // MAP ENTITY TO WORLD SPACE + UPDATE STATE
         CalcVectors(obj, delta_time);
         MapEntityToASpace(&world->coord_space_grid.coord_space, obj, snapped_aabb_box, &entity_space_map);
         ClearLArray(&snapped_aabb_verts);

         // Entity lives in the world - make sure it doesn't leave
         ResolveCollision_ContainerRect(obj, &world->coord_space_grid.object);
      }
   }

   // Check for collisions
   if (obj_count < 2) // early return
   {
      ClearFlatMapInt(&entity_space_map);
      // PrintCurrentBytesAlloc();
      return;
   }

   // RESOLVE ENTITY-ENTITY COLLISIONS
   // Track object-object collions that have been resolved
   FlatMapInt resolved_collisions = MakeFlatMapInt(1 + (int)(entity_space_map.count / 2)); // Start with a smaller capacity than the entity_space_map since we won't have a collision for every cell that has an object in it, but we can resize if needed
   for (size_t i = 0; i < entity_space_map.capacity; i++)
   {
      if (entity_space_map.slots[i].key == 0 && entity_space_map.slots[i].value == 0)
         continue;

      int cell_i = entity_space_map.slots[i].key;
      int cell_occ = 0;
      FlatMapInt_GetValue(&entity_space_map, cell_i, &cell_occ);

      Cell *cell = &cells[cell_i];
      if (cell_occ >= 2)
      {
         int max_collisions = (cell_occ * (cell_occ - 1)) / 2; // nC2 combinations of objects in the cell to check for collisions
         int checked_collisions = 0;
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
                  printf("ERROR: Could not find objects with IDs %d and %d in Cell (index = %d).\n", obj_id_a, obj_id_b, cell_i);
                  continue;
               }
               // Check whether this object-pair has been resolved already
               unsigned long obj_pair_hash_key = CalcHashFromInts(obj_id_a, obj_id_b);
               short is_resolved = 0;
               if (FlatMapInt_GetValue(&resolved_collisions, obj_pair_hash_key, (int *)&is_resolved))
                  continue;
               if (obj_id_a < 1 || obj_id_b < 1)
                  continue; // Early safety rejection
               Polygonoid *a = &polygonoids[obj_id_a - 1];
               Polygonoid *b = &polygonoids[obj_id_b - 1];

               bool colliding = CheckForCollision(a->newtonian_properties, b->newtonian_properties);
               if (colliding)
               {
                  ResolveCollision(&a->newtonian_properties, &b->newtonian_properties);

                  // Add object pair's key to hash map of resolved collisions
                  FlatMapInt_InsertOrUpdate(&resolved_collisions, obj_pair_hash_key, 1); // a value of 1 means resolved
               }

               frame_counter.total_frames % 300 == 0 ? printf("COLLISION CHECK for A(%.0f,%.0f) B(%.0f,%.0f) = %s\n", a->newtonian_properties.coords_origin.x, a->newtonian_properties.coords_origin.y,
                                                              b->newtonian_properties.coords_origin.x,
                                                              b->newtonian_properties.coords_origin.y,
                                                              colliding ? "TRUE" : "FALSE")
                                                     : (void)0;
            }
         }
      }
   }
   ClearFlatMapInt(&resolved_collisions);
   ClearFlatMapInt(&entity_space_map);
}

bool CheckForCollision(NewtonObject2d a, NewtonObject2d b)
{
   // Quick elimination check based on bounding circle
   Vector2d diff = VectorSum_2d(a.coords_center, VectorScale_2d(b.coords_center, -1.0f));
   float dist_sqr = VectorDot_2d(diff, diff); // this is the same as getting the magnitude & multiplying them, i.e. squaring
   float total_radius = a.radius + b.radius;
   if (dist_sqr > (total_radius * total_radius))
   {
      return false; // Fast broadphase rejection!
   }
   // 1 Get the boxed regions of both objects and check for overlap
   float a_min_x = a.coords_origin.x;
   float a_max_x = a.coords_origin.x + a.boxed_dimensions.x;
   float a_min_y = a.coords_origin.y;
   float a_max_y = a.coords_origin.y + a.boxed_dimensions.y;

   float b_min_x = b.coords_origin.x;
   float b_max_x = b.coords_origin.x + b.boxed_dimensions.x;
   float b_min_y = b.coords_origin.y;
   float b_max_y = b.coords_origin.y + b.boxed_dimensions.y;

   // RULE OUT BASED ON POSITION
   if (a_max_x < b_min_x || a_min_x > b_max_x)
      return false; // Separation on X
   if (a_max_y < b_min_y || a_min_y > b_max_y)
      return false; // Separation on Y
   // If the execution passes all positional and velocity checks, a collision is happening!
   // return true;

   // printf("Objects A and B are OVERLAPPING\nA Box Coords: Top Left (%.1f, %.1f) Bottom Right (%.1f, %.1f)\n", a_top_left.x, a_top_left.y, a_top_left.x + a_width, a_top_left.y + a_height);
   // printf("B Box Coords: Top Left (%.1f, %.1f) Bottom Right (%.1f, %.1f)\n", b_top_left.x, b_top_left.y, b_top_left.x + b_width, b_top_left.y + b_height);
   return true;
   // 2 Check if the x is overlapping
   // if (a_box_coords.)
   // 1.2 Find the overall max x and max y across both boxes, use this as a localised coord space
   // Matrix2x2 local_subspace = FindBoxedCoords
   // 1.2 Check for overlap
   // 1.2.1 Calculate world area - (a + b area) combined area and get difference between entire world area and a + b area,
   // Vector2d a_box =
}

void ResolveCollision(NewtonObject2d *a, NewtonObject2d *b)
{
   // COMMON DATA FOR PENETRATION & VELOCITY RESOLUTIONS
   //Vector2d a_b_vel_diff = VectorSum_2d(a->velocity, VectorScale_2d(b->velocity, -1));              // relative velocity, or velocity felt by a
   //Vector2d a_b_pos_diff = VectorSum_2d(a->coords_center, VectorScale_2d(b->coords_center, -1.0f)); // distance vector between centers (From B to A)
   //float a_b_pos_diff_mag = VectorMagnitude_2d(a_b_pos_diff);                                       // actual distance when just touching
   //float a_b_min_diff_mag = a->radius + b->radius;
   float total_inv_mass = a->inverse_mass + b->inverse_mass;
   if (total_inv_mass <= 0.0f) return; // Both are static immovable objects, skip

   // 1. Calculate half-extents
   float a_half_w = a->boxed_dimensions.x * 0.5f;
   float a_half_h = a->boxed_dimensions.y * 0.5f;
   float b_half_w = b->boxed_dimensions.x * 0.5f;
   float b_half_h = b->boxed_dimensions.y * 0.5f;

   // Vector pointing from A's center to B's center
   Vector2d distance_vec = VectorSum_2d(b->coords_center, VectorScale_2d(a->coords_center, -1.0f));

   // Calculate absolute overlap on both axes
   float x_overlap = (a_half_w + b_half_w) - fabsf(distance_vec.x);
   float y_overlap = (a_half_h + b_half_h) - fabsf(distance_vec.y);

   // If either overlap is negative, they aren't actually colliding
   if (x_overlap <= 0.0f || y_overlap <= 0.0f) return;

   Vector2d normal = {0.0f, 0.0f};
   float penetration_depth = 0.0f;

   // CHOOSE THE AXIS OF MINIMUM OVERLAP
   if (x_overlap < y_overlap)
   {
      penetration_depth = x_overlap;
      // Normal points from A towards B along X axis
      normal = (distance_vec.x > 0.0f) ? (Vector2d){1.0f, 0.0f} : (Vector2d){-1.0f, 0.0f};
   }
   else
   {
      penetration_depth = y_overlap;
      // Normal points from A towards B along Y axis
      normal = (distance_vec.y > 0.0f) ? (Vector2d){0.0f, 1.0f} : (Vector2d){0.0f, -1.0f};
   }

   // RESOLVE PENETRATION (DE-PENETRATION)
   float a_move_fraction = a->inverse_mass / total_inv_mass;
   float b_move_fraction = b->inverse_mass / total_inv_mass;

   // Total resolution vector required
   Vector2d separation_vector = VectorScale_2d(normal, penetration_depth);

   // A moves backward away from the normal, B moves forward with it
   a->coords_center = VectorSum_2d(a->coords_center, VectorScale_2d(separation_vector, -a_move_fraction));
   a->coords_origin = VectorSum_2d(a->coords_origin, VectorScale_2d(separation_vector, -a_move_fraction));

   b->coords_center = VectorSum_2d(b->coords_center, VectorScale_2d(separation_vector, b_move_fraction));
   b->coords_origin = VectorSum_2d(b->coords_origin, VectorScale_2d(separation_vector, b_move_fraction));

   // RESOLVE VELOCITY (IMPULSE RESPONSE)
   // Relative velocity: v_rel = v_a - v_b
   Vector2d a_b_vel_diff = VectorSum_2d(a->velocity, VectorScale_2d(b->velocity, -1.0f));

   // AABB perpendicular contact normal
   float a_b_vel_dot = VectorDot_2d(a_b_vel_diff, normal); 

   // Only apply impulse if objects are actively moving toward each other
   // (Since normal points from A to B, a positive dot means they are approaching)
   if (a_b_vel_dot > 0.0f)
   {
      float e = 1.0f; // Coefficient of Restitution (Fully Elastic)
      float j = -(1.0f + e) * a_b_vel_dot / total_inv_mass;

      Vector2d impulse_vector = VectorScale_2d(normal, j);

      // Apply the impulse vector to velocities weighted by inverse mass
      a->velocity = VectorSum_2d(a->velocity, VectorScale_2d(impulse_vector, a->inverse_mass));
      b->velocity = VectorSum_2d(b->velocity, VectorScale_2d(impulse_vector, -b->inverse_mass));
   }
}

void ResolveCollision_ContainerRect(NewtonObject2d *entity, NewtonObject2d *container)
{
   // COMMON DATA FOR PENETRATION & VELOCITY RESOLUTIONS
   // Distance vector from Container center to Entity center
   // Guard: Ensure there is an actual parent-child containment relationship to resolve
   if (entity->parent_id != container->id)
      return;

   // Because coordinates are relative, the parent container's inner space starts at (0,0)
   float c_min_x = 0.0f;
   float c_min_y = 0.0f;
   float c_max_x = container->boxed_dimensions.x;
   float c_max_y = container->boxed_dimensions.y;

   // The entity's boundaries relative to the container space
   float e_min_x = entity->coords_origin.x;
   float e_min_y = entity->coords_origin.y;
   float e_max_x = entity->coords_origin.x + entity->boxed_dimensions.x;
   float e_max_y = entity->coords_origin.y + entity->boxed_dimensions.y;

   float penetration_depth = 0.0f;
   Vector2d inward_normal = {0.0f, 0.0f};

   // --- CHECK HORIZONTAL BOUNDS (X-AXIS) ---
   if (e_min_x < c_min_x) // Breached Left Wall
   {
      penetration_depth = c_min_x - e_min_x;
      inward_normal = (Vector2d){1.0f, 0.0f}; // Push right
   }
   else if (e_max_x > c_max_x) // Breached Right Wall
   {
      penetration_depth = e_max_x - c_max_x;
      inward_normal = (Vector2d){-1.0f, 0.0f}; // Push left
   }

   // --- CHECK VERTICAL BOUNDS (Y-AXIS) ---
   if (e_min_y < c_min_y) // Breached Top Wall
   {
      penetration_depth = c_min_y - e_min_y;
      inward_normal = (Vector2d){0.0f, 1.0f}; // Push down
   }
   else if (e_max_y > c_max_y) // Breached Bottom Wall
   {
      penetration_depth = e_max_y - c_max_y;
      inward_normal = (Vector2d){0.0f, -1.0f}; // Push up
   }

   // --- RESOLVE IF PENETRATION OCCURRED ---
   if (penetration_depth > 0.0f)
   {
      float total_inv_mass = entity->inverse_mass + container->inverse_mass;
      if (total_inv_mass <= 0.0f)
         return;

      float entity_move_fraction = entity->inverse_mass / total_inv_mass;
      Vector2d separation_vector = VectorScale_2d(inward_normal, penetration_depth);

      // Resolve Position: Push entity center back inside bounds
      entity->coords_center = VectorSum_2d(entity->coords_center, VectorScale_2d(separation_vector, entity_move_fraction));
      // Sync origin back up with shifted center position
      entity->coords_origin = VectorSum_2d(entity->coords_origin, VectorScale_2d(separation_vector, entity_move_fraction));

      // Resolve Velocity (Impulse Response)
      // Account for container's velocity baseline to keep impulse math absolute
      Vector2d c_e_vel_diff = VectorSum_2d(entity->velocity, VectorScale_2d(container->velocity, -1.0f));
      float c_e_vel_dot = VectorDot_2d(c_e_vel_diff, inward_normal);

      // Only bounce if the entity is aggressively tracking outward past the wall
      if (c_e_vel_dot < 0.0f)
      {
         float e = 1.0f; // Coefficient of Restitution
         float j = -(1.0f + e) * c_e_vel_dot / total_inv_mass;

         Vector2d impulse_vector = VectorScale_2d(inward_normal, j);
         entity->velocity = VectorSum_2d(entity->velocity, VectorScale_2d(impulse_vector, entity->inverse_mass));
      }
   }
}

void ResolvePenetration(NewtonObject2d *a, NewtonObject2d *b)
{
   // COMMON DATA FOR PENETRATION & VELOCITY RESOLUTIONS
   Vector2d a_b_vel_diff = VectorSum_2d(a->velocity, VectorScale_2d(b->velocity, -1));              // relative velocity, or velocity felt by a
   Vector2d a_b_pos_diff = VectorSum_2d(a->coords_center, VectorScale_2d(b->coords_center, -1.0f)); // distance vector between centers (From B to A)
   float a_b_pos_diff_mag = VectorMagnitude_2d(a_b_pos_diff);                                       // actual distance when just touching
   float a_b_min_diff_mag = a->radius + b->radius;                                                  // target distance when just touching

   // RESOLVE PENETRATION
   if (a_b_pos_diff_mag < a_b_min_diff_mag)
   {
      // Calc scalar penetration depth
      float penetration_depth = a_b_min_diff_mag - a_b_pos_diff_mag;

      // Calc the collision normal unit vector
      Vector2d normal = {0.0f, 0.0f};
      if (a_b_pos_diff_mag > 0.0f)
      {
         normal = VectorScale_2d(a_b_pos_diff, 1.0f / a_b_pos_diff_mag);
      }
      else
      {
         // Edge case: Objects are perfectly stacked on top of each other. Pick an arbitrary up normal to push them apart.
         normal = (Vector2d){0.0f, -1.0f};
      }
      // Distribute the overlapping vector between A and B to shift their positions (weighted inversely to their mass)
      float total_inv_mass = a->inverse_mass + b->inverse_mass;
      if (total_inv_mass <= 0.0f)
         return; // both are static immovable objects, skip push out

      // Calc Resolution Shifting Multipliers
      float a_move_fraction = a->inverse_mass / total_inv_mass;
      float b_move_fraction = b->inverse_mass / total_inv_mass;

      // Total resolution vector required
      Vector2d separation_vector = VectorScale_2d(normal, penetration_depth);

      // A moves forward along the normal vector direction, B moves backward along the normal vector direction
      a->coords_center = VectorSum_2d(a->coords_center, VectorScale_2d(separation_vector, a_move_fraction));
      b->coords_center = VectorSum_2d(b->coords_center, VectorScale_2d(separation_vector, -b_move_fraction));
   }
}

void ResolveVelocity(NewtonObject2d *a, NewtonObject2d *b)
{
   // COMMON DATA FOR PENETRATION & VELOCITY RESOLUTIONS
   Vector2d a_b_vel_diff = VectorSum_2d(a->velocity, VectorScale_2d(b->velocity, -1));              // relative velocity, or velocity felt by a
   Vector2d a_b_pos_diff = VectorSum_2d(a->coords_center, VectorScale_2d(b->coords_center, -1.0f)); // distance vector between centers (From B to A)
   float a_b_pos_diff_mag = VectorMagnitude_2d(a_b_pos_diff);                                       // actual distance when just touching
   float a_b_min_diff_mag = a->radius + b->radius;                                                  // target distance when just touching

   // RESOLVE VELOCITY
   float total_inverse_mass = a->inverse_mass + b->inverse_mass;
   if (total_inverse_mass <= 0.0f) // We have 2 immovable/static objects so just continue to the next collision
   {
      printf("WARNING: Both objects in this collision have infinite mass (0 inverse mass). No collision response applied.\n");
      return;
   }
   // Apply momentum conservation to determine velocities of a and b
   float a_mom_1 = VectorMagnitude_2d(a->velocity) / a->inverse_mass;
   float b_mom_1 = VectorMagnitude_2d(b->velocity) / b->inverse_mass;
   // For simplicity, we'll assume the normal is in the direction that starts at A's origin and points to B's origin
   Vector2d a_b_pos_normal = VectorScale_2d(a_b_pos_diff, 1 / VectorMagnitude_2d(a_b_pos_diff)); // this is the unit-direction
   float a_b_vel_dot = VectorDot_2d(a_b_vel_diff, a_b_pos_normal);                               // Velocity along the Normal (The Dot Product)

   // Get the Impulse: $$j = \frac{-(1 + e)(\mathbf{v}_{rel} \cdot \mathbf{n})}{\frac{1}{m_a} + \frac{1}{m_b}}$$
   // Calculate Impulse Scalar
   float e = 1; // Coefficient of Restitution
   float j = -(1 + e) * a_b_vel_dot / total_inverse_mass;

   // Apply the Impulse
   // Turn that scalar back into a vector and update the velocities
   Vector2d impulse_vector = VectorScale_2d(a_b_pos_normal, j);

   // Apply the impulse vector to A and B to get velocities
   Vector2d a_vel_change = VectorScale_2d(impulse_vector, a->inverse_mass);
   Vector2d b_vel_change = VectorScale_2d(impulse_vector, b->inverse_mass);

   a->velocity = VectorSum_2d(a->velocity, a_vel_change);
   b->velocity = VectorSum_2d(b->velocity, VectorScale_2d(b_vel_change, -1));
}

// Associates the Entity to the Space through object->space-cell mapping. Best used when Entity count << Space Cell count.
void MapEntityToASpace(CoordSpace2d *space, NewtonObject2d *object, Matrix2x2 snapped_aabb_box, FlatMapInt *O_entity_to_space_index_map)
{
   // Get the entity's footprint in terms of cell index coverage and map entity (via its ID) to the corresponding Cell (retireved via its index) (store entity ID as a cell occupant)
   float snapped_w = (snapped_aabb_box.col2.x - snapped_aabb_box.col1.x);
   float snapped_h = (snapped_aabb_box.col2.y - snapped_aabb_box.col1.y);
   Vector2d snapped_t_left = snapped_aabb_box.col1;
   Vector2d snapped_b_right = snapped_aabb_box.col2;
   // Cell *cells = (Cell *)space->cells.items;
   for (size_t y = 0; y < snapped_h; y++)
   {
      for (size_t x = 0; x < snapped_w; x++)
      {
         Vector2d cell_coords = (Vector2d){snapped_t_left.x + x, snapped_t_left.y + y};
         int cell_i = GetIndexFromCoords(space, cell_coords);
         Cell *cell = GetCellFromCoords(space, cell_coords);
         bool out_of_bounds = VectorIsInSpace_2d(cell_coords, space);
         if (cell == NULL || out_of_bounds == false)
         {
            // printf("WARNING: Cell index %d out of bounds for object ID %d at coordinates (%.1f, %.1f). Skipping cell update for this cell.\n", cell_i, object->id, object->coords_center.x, object->coords_center.y);
            printf("WARNING: Cell (index %d) not found in MapEntityToASpace or its coordinates are out of bounds. Skipping this object-->cell mapping.\n", cell_i);
            continue;
         }

         // Map object ID to the cell
         if (cell->occupancy < MAX_CELL_OCCUPANCY)
         {
            cell->object_ids[cell->occupancy] = object->id;
            cell->occupancy++;

            // Increment the number of objects in this cell
            if (O_entity_to_space_index_map != NULL && cell->occupancy >= 1)
            {
               int cell_occu = 0;
               FlatMapInt_InsertOrUpdate(O_entity_to_space_index_map, cell_i, cell->occupancy);
               FlatMapInt_GetValue(O_entity_to_space_index_map, cell_i, &cell_occu);
               frame_counter.total_frames % 60 == 0 ? printf("ENTITY (ID:%d) mapped to CELL %d (now has %d occupancy)\n", object->id, cell_i, cell_occu) : (void)0;
            }
         }
         else
         {
            printf("WARNING: Cell index %d full. ID %d not tracked spatially.\n", cell_i, object->id);
            continue;
         }
      }
   }
}

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
//    Cell *cells = world->coord_space_grid.coord_space.cells.coll.items;
//    int cell_count = world->coord_space_grid.coord_space.cells.coll.count;
//    for (size_t i = 0; i < cell_count; i++)
//    {
//       Cell *target_cell = &cells[i];
//       target_cell->occupancy = 0;
//       memset(&cells[i].object_ids, 0, sizeof(cells[i].object_ids));
//    }

//    Polygonoid *polygonoids = objects->items;
//    for (size_t i = 0; i < count; i++)
//    {
//       NewtonObject2d *obj = &polygonoids[i].newtonian_properties;

//       // Ensure the object isn't outside the bounds of the world before we try to get the cell it's in, otherwise we could get an out of bounds error when we try to access the cell's object_ids array. We can just skip updating the cell for this object if it's out of bounds, but we should still update its vectors based on its acceleration and velocity so that it can move back into the bounds of the world.
//       if (obj->coords_origin.x < 0 || obj->coords_origin.x >= world->coord_space_grid.coord_space.resolution_ixj.x ||
//           obj->coords_origin.y < 0 || obj->coords_origin.y >= world->coord_space_grid.coord_space.resolution_ixj.y)
//       {
//          printf("WARNING: Object ID %d is out of bounds at coordinates (%.1f, %.1f). Skipping cell update.\n", polygonoids[i].id, obj->coords_origin.x, obj->coords_origin.y);

//          // Need to calculate a collision response to push the object back into the bounds of the world here, otherwise it will just keep moving out of bounds and we won't be able to track it anymore. For simplicity, let's just reverse the velocity of the object when it hits the boundary of the world, which will create a bouncing effect. We can also apply a damping factor to the velocity to simulate energy loss during the collision, which will prevent the object from bouncing indefinitely.
//          if (obj->coords_origin.x < 0 || obj->coords_origin.x >= world->coord_space_grid.coord_space.resolution_ixj.x)
//          {
//             obj->velocity.x = -obj->velocity.x; // Reverse and dampen the x velocity
//             // obj->coords_origin.x = obj->coords_origin.x < 0 ? 0 : world->coord_space_grid.coord_space.resolution_ixj.x - 1; // Move the object back within bounds
//          }
//          if (obj->coords_origin.y < 0 || obj->coords_origin.y >= world->coord_space_grid.coord_space.resolution_ixj.y)
//          {
//             obj->velocity.y = -obj->velocity.y; // Reverse and dampen the y velocity
//             // obj->coords_origin.y = obj->coords_origin.y < 0 ? 0 : world->coord_space_grid.coord_space.resolution_ixj.y - 1; // Move the object back within bounds
//          }
//          //Recalc inverse_mass in case mass was changed
//          obj->inverseMass = 1.0/obj->mass;
//          CalculateVectors(obj, delta_time); // Still update the object's vectors based on its acceleration and velocity so that it can move back into the bounds of the world
//          continue;
//       }

//       // Add the object's ID to the cell's object_ids array if there is space
//       Cell *target_cell = GetCellFromCoords(&world->coord_space_grid.coord_space, polygonoids[i].newtonian_properties.coords_origin);

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
//    // UpdateWorldState(objs, &world->coord_space_grid.coord_space, delta_time);
// }

// void UpdateWorldState(Collection *polygonoids, CoordSpace2d *space, float delta_time)
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
