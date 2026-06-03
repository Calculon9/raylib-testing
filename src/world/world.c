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
//static float field = {0};

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
bool CheckForCollision(NewtonObject2d a, NewtonObject2d b);
//void UpdateWorldState(Collection *objects, CoordSpace2d *space, float delta_time);
// void UpdateObjectVectors(Collection *objects, float delta_time);

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
   // For simplicity, let's assume the object's coords_origin is the point we will use to determine which cell it occupies
   Vector2d object_coords = object->newtonian_properties.coords_center; // These are the world coordinates of the object, which are the cell indices
   int cell_index = ((int)object_coords.y * (int)world->coord_space_grid.coord_space.resolution_ixj.x) + (int)object_coords.x;

   // Add the object's ID to the cell's object_ids array if there is space
   Cell *cells = world->coord_space_grid.coord_space.cells.items;
   Cell *target_cell = &cells[cell_index];

   if (target_cell->occupancy < MAX_CELL_OCCUPANCY)
   {
      object->id = world->next_object_id++;
      target_cell->object_ids[target_cell->occupancy] = object->id;
      target_cell->occupancy++;
   }
   else
   {
      printf("WARNING: Cell %d full. ID %d not tracked spatially.\n", cell_index, object->id);
      return;
   }

   // Add the newton_object to the world's objects array
   LArray_Push(&world->objects, object);

   printf("CREATED OBJECT (ID %d): Cell %d (%.1f, %.1f)\n", object->id, cell_index, object_coords.x, object_coords.y);
}

void UpdateWorld(World2d *world, float delta_time)
{
   LArray objects = world->objects; //.items;
   int obj_count = objects.count;
   // 1. Update Object state first
   // 1.1 Update Grid cells while we're here
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

   LArray tally_arr = MakeLArray(cell_count, sizeof(short));
   short *tally = (short *)(tally_arr.items); //->items;
   Polygonoid *polygonoids = objects.items;
   for (size_t i = 0; i < obj_count; i++)
   {
      NewtonObject2d *obj = &polygonoids[i].newtonian_properties;

      // Ensure the object isn't outside the bounds of the world before we try to get the cell it's in, otherwise we could get an out of bounds error when we try to access the cell's object_ids array. We can just skip updating the cell for this object if it's out of bounds, but we should still update its vectors based on its acceleration and velocity so that it can move back into the bounds of the world.
      if (obj->coords_origin.x < 0 || obj->coords_origin.x >= world->coord_space_grid.coord_space.resolution_ixj.x ||
          obj->coords_origin.y < 0 || obj->coords_origin.y >= world->coord_space_grid.coord_space.resolution_ixj.y)
      {
         printf("WARNING: Object ID %d is out of bounds at coordinates (%.1f, %.1f). Skipping cell update.\n", polygonoids[i].id, obj->coords_origin.x, obj->coords_origin.y);

         // Need to calculate a collision response to push the object back into the bounds of the world here, otherwise it will just keep moving out of bounds and we won't be able to track it anymore. For simplicity, let's just reverse the velocity of the object when it hits the boundary of the world, which will create a bouncing effect. We can also apply a damping factor to the velocity to simulate energy loss during the collision, which will prevent the object from bouncing indefinitely.
         if (obj->coords_origin.x < 0 || obj->coords_origin.x >= world->coord_space_grid.coord_space.resolution_ixj.x)
         {
            obj->velocity.x = -obj->velocity.x; // Reverse and dampen the x velocity
            // obj->coords_origin.x = obj->coords_origin.x < 0 ? 0 : world->coord_space_grid.coord_space.resolution_ixj.x - 1; // Move the object back within bounds
         }
         if (obj->coords_origin.y < 0 || obj->coords_origin.y >= world->coord_space_grid.coord_space.resolution_ixj.y)
         {
            obj->velocity.y = -obj->velocity.y; // Reverse and dampen the y velocity
            // obj->coords_origin.y = obj->coords_origin.y < 0 ? 0 : world->coord_space_grid.coord_space.resolution_ixj.y - 1; // Move the object back within bounds
         }
         // Recalc inverse_mass in case mass was changed
         obj->inverse_mass = obj->mass != 0 ? 1.0 / obj->mass : 0.0;
         CalculateVectors(obj, delta_time); // Still update the object's vectors based on its acceleration and velocity so that it can move back into the bounds of the world
         continue;
      }

      // Add the object's ID to the cell's object_ids array if there is space
      Cell *target_cell = GetCellFromCoords(&world->coord_space_grid.coord_space, polygonoids[i].newtonian_properties.coords_center);
      int cell_index = GetIndexFromCoords(&world->coord_space_grid.coord_space, target_cell->coords_origin);

      // Need assign an area of effect for the object based on its radius and update the occupancy of all cells that fall within that area, not just the cell that contains the object's origin coordinates, otherwise we won't detect collisions until the objects are already overlapping significantly, which can cause tunneling issues where fast moving objects pass through each other without detecting a collision. For simplicity, let's just use a square area of effect based on the object's radius, which will be easier to calculate than a circular area of effect. We can calculate the min and max cell indices in both x and y directions that fall within the object's area of effect and update the occupancy of all those cells accordingly.

      // Calculate the min and max cell indices in both x and y directions that fall within the object's area of effect based on box-shaped area of effect for simplicity
      // What I need is the max across and max down - this defines the area of effect. Then get the indexes of the cells that fall within that area and update their occupancy based on the object's ID. We can calculate the max across and max down based on the object's radius and the coordinate space's basis vectors, which will give us the dimensions of the area of effect in terms of how many cells it covers in each direction. Then we can loop through all the cells that fall within that area and update their occupancy accordingly.
      Matrix2x2 box_coords = GetBoxedCoords(&obj->surface.surface_vectors, ZERO_VECTOR_2D);
      Vector2d min_coords = box_coords.col1;
      Vector2d max_coords = box_coords.col2;
      float obj_width = max_coords.x - min_coords.x;
      float obj_height = max_coords.y - min_coords.y;

      float obj_cell_width_ratio = obj_width/world->coord_space_grid.coord_space.basis.u.x;
      float obj_cell_height_ratio = obj_width/world->coord_space_grid.coord_space.basis.v.y;

      // Min area of effect will be the cell in the middle + all bordering cells - this will apply if the obj width and height are < cell width and height
      float start_x = min_coords.x - 1;
      float end_x = max_coords.x + 1;
      float start_y = min_coords.y - 1;
      float end_y = max_coords.y + 1;

      float obj_effect_width = end_x - start_x;
      float obj_effect_height = end_y - start_y;
      Vector2d area = (Vector2d){obj_effect_width, obj_effect_height};
      Surface2d area_of_effect = CreateSurface_Rectangular(area, ZERO_VECTOR_2D);
      // Otherwise, ..
      // if(obj_width < world->coord_space_grid.coord_space.basis.u.x)
      // {

      // }
      //float min_cell_x = min_coords.x;
      //float max_cell_x = max_coords.x;
      //float min_cell_y = min_coords.y;
      //float max_cell_y = max_coords.y;

      // int start_i = GetIndexFromCoords(&world->coord_space_grid.coord_space, (Vector2d){min_cell_x + obj->coords_origin.x, min_cell_y + obj->coords_origin.y});
      // int end_i = GetIndexFromCoords(&world->coord_space_grid.coord_space, (Vector2d){max_cell_x + obj->coords_origin.x, max_cell_y + obj->coords_origin.y});

      // for (int i = start_i; i <= end_i; i++)
      // {
      //    if (target_cell != NULL && target_cell->occupancy < MAX_CELL_OCCUPANCY)
      //    {
      //       target_cell->object_ids[target_cell->occupancy] = polygonoids[i].id;
      //       target_cell->occupancy++;
      //       tally[cell_index]++;
      //       frame_counter.total_frames % 60 == 0 ? printf("CELL INDEX %d now has %d occupancy\n", cell_index, tally[cell_index]) : (void)0;
      //    }
      //    else
      //    {
      //       printf("WARNING: Cell (%d,%d) full. ID %d not tracked spatially.\n", target_cell->coords.x, target_cell->coords.y, polygonoids[i].id);
      //       return;
      //    }
      // }

      // Update the object's vectors based on its current acceleration, velocity, and position, and the elapsed time since the last update
      CalculateVectors(obj, delta_time);
   }
   // 2. Check for collisions
   if (obj_count < 2)
      return;

   // i is the index of the first object in the collision check, and j is the index of the second object.
   // We can start j at i + 1 to avoid checking the same pair of objects twice and to avoid checking an object against itself.
   for (size_t i = 0; i < cell_count; i++)
   {
      // The index actually encodes the cell index value, e.g. i = 2 is the cell at index 2 in the cells array, which we can get the coordinates of from the cell's coords field, and we can also get the object IDs of the objects occupying that cell from the cell's object_ids array, and then we can use those IDs to find the corresponding objects in the world's objects array to check for collisions between them.
      short cell_occ = tally[i];
      if (cell_occ > 1)
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

                  // Get the Impulse: $$j = \frac{-(1 + e)(\mathbf{v}_{rel} \cdot \mathbf{n})}{\frac{1}{m_a} + \frac{1}{m_b}}$$
                  // Calculate Impulse Scalar
                  float e = 1; // Coefficient of Restitution
                  float j = -(1 + e) * a_b_vel_dot / (a->newtonian_properties.inverse_mass + b->newtonian_properties.inverse_mass);

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
   ClearLArray(&tally_arr);
   // UpdateObjectVectors(objs, delta_time);
   // UpdateWorldState(objs, &world->coord_space_grid.coord_space, delta_time);
}

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

//void UpdateWorldState(Collection *polygonoids, CoordSpace2d *space, float delta_time)
//{
   // // 1. Update Object state first
   // // 1.1 Update Grid cells while we're here
   // if (polygonoids->count < 1)
   //    return;

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
   Matrix2x2 a_box_coords = GetBoxedCoords(&a.surface.surface_vectors, ZERO_VECTOR_2D);
   Matrix2x2 b_box_coords = GetBoxedCoords(&b.surface.surface_vectors, ZERO_VECTOR_2D);

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
