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

void AddObjectToWorld(World2d *world, Polygonoid *object)
{
   // We can calculate the cell indices based on the object's coordinates and the coordinate space's basis vectors and resolution
   // For simplicity, let's assume the object's coords_center is the point we will use to determine which cell it occupies
   Vector2d local_coords = object->newtonian_properties.coords_center; // These are the world coordinates of the object, which are the cell indices
   int cell_index = ((int)local_coords.y * (int)world->coord_space_grid.coord_space.resolution_ixj.x) + (int)local_coords.x;

   if (local_coords.x < 0 || local_coords.y < 0 ||
       local_coords.x >= world->coord_space_grid.coord_space.resolution_ixj.x || local_coords.y >= world->coord_space_grid.coord_space.resolution_ixj.y)
   {
      // Click is outside the structural world viewport boundaries! Avoid resolving cell.
      return;
   }

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
      printf("WARNING: Cell %d full. ID %d not tracked spatially.\n", cell_index, object->id);
      return;
   }

   // Add the newton_object to the world's objects array
   LArray_Push(&world->objects, object);

   printf("CREATED OBJECT (ID %d): Cell %d : Center(%.1f, %.1f)\n", object->id, cell_index, local_coords.x, local_coords.y);
}

void UpdateWorld(World2d *world, float delta_time)
{
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

   Polygonoid *polygonoids = (Polygonoid *)world->objects.items;
   Matrix2x2 min_coord_space = {0};
   int starting_flatmap_capacity = 2 + (int)(cell_count / 5);
   FlatMapInt entity_space_map = MakeFlatMapInt(starting_flatmap_capacity);
   for (size_t i = 0; i < obj_count; i++)
   {
      NewtonObject2d *obj = &polygonoids[i].newtonian_properties;

      // Ensure the object isn't outside the bounds of the world before we try to get the cell it's in, otherwise we could get an out of bounds error when we try to access the cell's object_ids array. We can just skip updating the cell for this object if it's out of bounds, but we should still update its vectors based on its acceleration and velocity so that it can move back into the bounds of the world.
      if (obj->coords_origin.x < 0 || obj->coords_origin.x >= world->coord_space_grid.coord_space.resolution_ixj.x || obj->coords_origin.y < 0 || obj->coords_origin.y >= world->coord_space_grid.coord_space.resolution_ixj.y)
      {
         printf("WARNING: Object ID %d is out of bounds at coordinates (%.1f, %.1f). Skipping cell update.\n", polygonoids[i].id, obj->coords_origin.x, obj->coords_origin.y);

         // Need to calculate a collision response to push the object back into the bounds of the world here, otherwise it will just keep moving out of bounds and we won't be able to track it anymore.
         if (obj->coords_origin.x < 0 || obj->coords_origin.x >= world->coord_space_grid.coord_space.resolution_ixj.x)
         {
            obj->velocity.x = -obj->velocity.x; // Reverse
            // obj->coords_origin.x = obj->coords_origin.x < 0 ? 0 : world->coord_space_grid.coord_space.resolution_ixj.x - 1; // Move the object back within bounds
         }
         if (obj->coords_origin.y < 0 || obj->coords_origin.y >= world->coord_space_grid.coord_space.resolution_ixj.y)
         {
            obj->velocity.y = -obj->velocity.y; // Reverse
            // obj->coords_origin.y = obj->coords_origin.y < 0 ? 0 : world->coord_space_grid.coord_space.resolution_ixj.y - 1; // Move the object back within bounds
         }
         // Recalc inverse_mass in case mass was changed
         obj->inverse_mass = obj->mass != 0 ? 1.0 / obj->mass : 0.0;
      }

      // Need assign an area of effect for the object based on its radius and update the occupancy of all cells that fall within that area, not just the cell that contains the object's origin coordinates,
      // otherwise we won't detect collisions until the objects are already overlapping significantly, which can cause tunneling issues where fast moving objects pass through each other without detecting a collision.
      // For simplicity, use a square area of effect based on AABB
      Surface2d snapped_aabb = CalcSnappedAABB(world->coord_space_grid.coord_space.basis, obj->surface, obj->coords_center);
      Matrix2x2 snapped_aabb_box = CalcAABBCoords_Tight(&snapped_aabb.surface_vectors, ZERO_VECTOR_2D);
      MapEntityToASpace(&world->coord_space_grid.coord_space, obj, snapped_aabb_box, &entity_space_map);

      CalcVectors(obj, delta_time);
   }

   // Check for collisions
   if (obj_count < 2) // early return
   {
      ClearFlatMapInt(&entity_space_map);
      return;
   }

   // i is the index of the first object in the collision check, and j is the index of the second object.
   // We can start j at i + 1 to avoid checking the same pair of objects twice and to avoid checking an object against itself.
   for (size_t i = 0; i < cell_count; i++)
   {
      // The index actually encodes the cell index value, e.g. i = 2 is the cell at index 2 in the cells array, which we can get the coordinates of from the cell's coords field, and we can also get the object IDs of the objects occupying that cell from the cell's object_ids array, and then we can use those IDs to find the corresponding objects in the world's objects array to check for collisions between them.
      int cell_occ = 0;
      FlatMapInt_Get(&entity_space_map, i, &cell_occ);
      if (cell_occ > 2)
      {
         // Implement some factorial stuff and check each possible interaction between occupying objects
         int max_collisions = (cell_occ * (cell_occ - 1)) / 2; // nC2 combinations of objects in the cell to check for collisions
         int checked_collisions = 0;
         Cell *cell = &cells[i];
         for (size_t m = 0; m < cell_occ; m++)
         {
            for (size_t n = m + 1; n < cell_occ; n++)
            {
               int obj_id_a = cell->object_ids[m];
               int obj_id_b = cell->object_ids[n];

               // Get the indices of the objects in the world's objects array based on their IDs
               int index_a = -1;
               int index_b = -1;
               for (size_t k = 0; k < obj_count; k++)
               {
                  if (polygonoids[k].id == obj_id_a)
                     index_a = k;
                  if (polygonoids[k].id == obj_id_b)
                     index_b = k;
                  if (index_a != -1 && index_b != -1)
                     break;
               }
               if (index_a == -1 || index_b == -1)
               {
                  printf("ERROR: Could not find objects with IDs %d and %d in the world's objects array.\n", obj_id_a, obj_id_b);
                  continue;
               }

               Polygonoid *a = &polygonoids[index_a];
               Polygonoid *b = &polygonoids[index_b];

               bool colliding = CheckForCollision(a->newtonian_properties, b->newtonian_properties);
               if (colliding)
               {
                  // Apply momentum conservation to determine velocities of a and b
                  float a_mom_1 = VectorMagnitude_2d(a->newtonian_properties.velocity) / a->newtonian_properties.inverse_mass;
                  float b_mom_1 = VectorMagnitude_2d(b->newtonian_properties.velocity) / b->newtonian_properties.inverse_mass;
                  Vector2d a_b_vel = VectorSum_2d(a->newtonian_properties.velocity, VectorScale_2d(b->newtonian_properties.velocity, -1));

                  // Get the collision normal - just use A as the reference object
                  // For simplicity, we'll assume the normal is in the direction that starts at A's origin and points to B's origin
                  Vector2d a_b_pos = VectorSum_2d(a->newtonian_properties.coords_origin, VectorScale_2d(b->newtonian_properties.coords_origin, -1));
                  Vector2d a_b_pos_normal = VectorScale_2d(a_b_pos, 1 / VectorMagnitude_2d(a_b_pos));

                  // Velocity along the Normal (The Dot Product)
                  float a_b_vel_dot = VectorDot_2d(a_b_vel, a_b_pos_normal);

                  float total_inverse_mass = a->newtonian_properties.inverse_mass + b->newtonian_properties.inverse_mass;
                  if (total_inverse_mass <= 0.0f) // We have 2 immovable/static objects so just continue to the next collision
                  {
                     printf("WARNING: Both objects in this collision have infinite mass (0 inverse mass). No collision response applied.\n");
                     continue;
                  }

                  // Get the Impulse: $$j = \frac{-(1 + e)(\mathbf{v}_{rel} \cdot \mathbf{n})}{\frac{1}{m_a} + \frac{1}{m_b}}$$
                  // Calculate Impulse Scalar
                  float e = 1; // Coefficient of Restitution
                  float j = -(1 + e) * a_b_vel_dot / total_inverse_mass;
                  // Apply impulse safely...

                  // Apply the Impulse
                  // Turn that scalar back into a vector and update the velocities
                  Vector2d impulse_vector = VectorScale_2d(a_b_pos_normal, j);

                  // Apply the impulse vector to A and B to get velocities
                  Vector2d a_vel_change = VectorScale_2d(impulse_vector, a->newtonian_properties.inverse_mass);
                  Vector2d b_vel_change = VectorScale_2d(impulse_vector, b->newtonian_properties.inverse_mass);

                  a->newtonian_properties.velocity = VectorSum_2d(a->newtonian_properties.velocity, a_vel_change);
                  b->newtonian_properties.velocity = VectorSum_2d(b->newtonian_properties.velocity, VectorScale_2d(b_vel_change, -1));
               }

               frame_counter.total_frames % 300 == 0 ? printf("COLLISION CHECK for A(%.0f,%.0f) B(%.0f,%.0f) = %s\n",
                                                              a->newtonian_properties.coords_origin.x,
                                                              a->newtonian_properties.coords_origin.y,
                                                              b->newtonian_properties.coords_origin.x,
                                                              b->newtonian_properties.coords_origin.y,
                                                              colliding ? "TRUE" : "FALSE")
                                                     : (void)0;
            }
         }
      }
   }
   ClearFlatMapInt(&entity_space_map);
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
         if (cell == NULL)
         {
            // printf("WARNING: Cell index %d out of bounds for object ID %d at coordinates (%.1f, %.1f). Skipping cell update for this cell.\n", cell_i, object->id, object->coords_center.x, object->coords_center.y);
            printf("WARNING: Cell not found in MapEntityToASpace. Skipping this object-->cell mapping.\n");
            continue;
         }

         // Map object ID to the cell
         if (cell->occupancy < MAX_CELL_OCCUPANCY)
         {
            cell->object_ids[cell->occupancy] = object->id;
            cell->occupancy++;

            // Increment the number of objects in this cell
            if (O_entity_to_space_index_map != NULL)
            {
               int cell_occu = 0;
               FlatMapInt_InsertOrUpdate(O_entity_to_space_index_map, cell_i, cell->occupancy);
               FlatMapInt_Get(O_entity_to_space_index_map, cell_i, &cell_occu);
               frame_counter.total_frames % 60 == 0 ? printf("ENTITY (ID:%d) mapped to CELL %d | CELL now has %d occupancy\n", object->id, cell_i, cell_occu) : (void)0;
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

bool CheckForCollision(NewtonObject2d a, NewtonObject2d b)
{
   // 1 Get the boxed regions of both objects and check for overlap
   // 1.1 Find the max x and max y across all surface vectors
   Matrix2x2 a_box_coords = CalcAABBCoords_Tight(&a.surface.surface_vectors, ZERO_VECTOR_2D);
   Matrix2x2 b_box_coords = CalcAABBCoords_Tight(&b.surface.surface_vectors, ZERO_VECTOR_2D);

   // a_box_coords.col1 = VectorSum_2d(a_box_coords.col1,a.coords_origin);
   // a_box_coords.col2 = VectorSum_2d(a_box_coords.col2,a.coords_origin);
   Vector2d a_top_left = VectorSum_2d(a_box_coords.col2, a.coords_origin);
   // b_box_coords.col1 = VectorSum_2d(b_box_coords.col1,b.coords_origin);
   // b_box_coords.col2 = VectorSum_2d(b_box_coords.col2,b.coords_origin);
   Vector2d b_top_left = VectorSum_2d(b_box_coords.col2, b.coords_origin);

   // Rule out based on x position
   float a_width = fabsf(a_box_coords.col2.x - a_box_coords.col1.x);
   float b_width = fabsf(b_box_coords.col2.x - b_box_coords.col1.x);
   if (a_top_left.x + a_width < b_top_left.x)
   {
      // printf("Object A is LEFT of Object B\n");
      return false; // A is left of B
   }
   if (a_top_left.x > b_top_left.x + b_width)
   {
      // printf("Object A is RIGHT of Object B\n");
      return false; // A is right of B
   }

   // Rule out based on y position
   float a_height = fabsf(a_box_coords.col2.y - a_box_coords.col1.y);
   float b_height = fabsf(b_box_coords.col2.y - b_box_coords.col1.y);
   if (a_top_left.y + a_height < b_top_left.y)
   {
      // printf("Object A is ABOVE Object B\n");
      return false; // A is above B
   }
   if (a_top_left.y > b_top_left.y + b_height)
   {
      // printf("Object A is BELOW Object B\n");
      return false; // A is below B
   }

   printf("Objects A and B are OVERLAPPING\nA Box Coords: Top Left (%.1f, %.1f) Bottom Right (%.1f, %.1f)\n", a_top_left.x, a_top_left.y, a_top_left.x + a_width, a_top_left.y + a_height);
   printf("B Box Coords: Top Left (%.1f, %.1f) Bottom Right (%.1f, %.1f)\n", b_top_left.x, b_top_left.y, b_top_left.x + b_width, b_top_left.y + b_height);
   return true;
   // 2 Check if the x is overlapping
   // if (a_box_coords.)
   // 1.2 Find the overall max x and max y across both boxes, use this as a localised coord space
   // Matrix2x2 local_subspace = FindBoxedCoords
   // 1.2 Check for overlap
   // 1.2.1 Calculate world area - (a + b area) combined area and get difference between entire world area and a + b area,
   // Vector2d a_box =
}

// World CalculateFieldLines(Field field);
// World InitialiseFieldCells(Field field);
