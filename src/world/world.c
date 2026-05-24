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
static float field = {0};

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
bool CheckForCollision(NewtonObject2d a, NewtonObject2d b);
void UpdateWorldState(Collection *objects, CoordSpace2d *space, float delta_time);
// void UpdateObjectVectors(Collection *objects, float delta_time);

World2d CreateWorld(CoordSpace2d_Grid space_obj, float gravity)
{
   World2d world = {0};
   world.coord_space_grid = space_obj;
   //world.objects = objects;
   world.gravity = gravity;
   world.next_object_id = 1; // Initialize the next available ID for NewtonObjects

   return world;
}

void AddObjectToWorld(World2d *world, Polygonoid *object)
{
   // We can calculate the cell indices based on the object's coordinates and the coordinate space's basis vectors and resolution
   // For simplicity, let's assume the object's coords_origin is the point we will use to determine which cell it occupies
   Vector2d object_coords = object->newtonian_properties.coords_origin; // These are the world coordinates of the object, which are the cell indices
   int cell_index = ((int)object_coords.y * (int)world->coord_space_grid.coord_space.resolution_ixj.x) + (int)object_coords.x;

   // Add the object's ID to the cell's object_ids array if there is space
   Cell *cells = world->coord_space_grid.coord_space.cells.coll.items;
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
   LArray *objects = &world->objects; //.items;
   int count = objects->count;
   // 1. Update Object state first
   // 1.1 Update Grid cells while we're here
   if (count < 1)
      return;

   // Zero out the occupancy and object_ids of all cells in the grid before we update them based on the new positions of the objects
   Cell *cells = world->coord_space_grid.coord_space.cells.coll.items;
   int cell_count = world->coord_space_grid.coord_space.cells.coll.count;
   for (size_t i = 0; i < cell_count; i++)
   {
      Cell *target_cell = &cells[i];
      target_cell->occupancy = 0;
      memset(&cells[i].object_ids, 0, sizeof(cells[i].object_ids));
   }

   Polygonoid *polygonoids = objects->items;
   for (size_t i = 0; i < count; i++)
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
         //Recalc inverse_mass in case mass was changed
         obj->inverseMass = 1.0/obj->mass;
         CalculateVectors(obj, delta_time); // Still update the object's vectors based on its acceleration and velocity so that it can move back into the bounds of the world
         continue;
      }

      // Add the object's ID to the cell's object_ids array if there is space
      Cell *target_cell = GetCellFromCoords(&world->coord_space_grid.coord_space, polygonoids[i].newtonian_properties.coords_origin);

      if (target_cell != NULL && target_cell->occupancy < MAX_CELL_OCCUPANCY)
      {
         target_cell->object_ids[target_cell->occupancy] = polygonoids[i].id;
         target_cell->occupancy++;
      }
      else
      {
         printf("WARNING: Cell (%d,%d) full. ID %d not tracked spatially.\n", target_cell->coords.x, target_cell->coords.y, polygonoids[i].id);
         return;
      }

      // Update the object's vectors based on its current acceleration, velocity, and position, and the elapsed time since the last update
      CalculateVectors(obj, delta_time);
   }

   // 2. Check for collisions
   if (count < 2)
      return;

   for (size_t i = 0; i < count; i++)
   {
      for (size_t j = i + 1; j < count; j++) // Optimized j loop
      {
         Polygonoid *a = &polygonoids[i];
         Polygonoid *b = &polygonoids[j];

         bool colliding = CheckForCollision(a->newtonian_properties, b->newtonian_properties);

         if (colliding)
         {
            // Apply momentum conservation to determine velocities of a and b
            float a_mom_1 = VectorMagnitude_2d(a->newtonian_properties.velocity) / a->newtonian_properties.inverseMass;
            float b_mom_1 = VectorMagnitude_2d(b->newtonian_properties.velocity) / b->newtonian_properties.inverseMass;
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
            float j = -(1 + e) * a_b_vel_dot / (a->newtonian_properties.inverseMass + b->newtonian_properties.inverseMass);

            // Apply the Impulse
            // Turn that scalar back into a vector and update the velocities
            Vector2d impulse_vector = VectorScale_2d(a_b_pos_normal, j);

            // Apply the impulse vector to A and B to get velocities
            Vector2d a_vel_change = VectorScale_2d(impulse_vector, a->newtonian_properties.inverseMass);
            Vector2d b_vel_change = VectorScale_2d(impulse_vector, b->newtonian_properties.inverseMass);

            a->newtonian_properties.velocity = VectorSum_2d(a->newtonian_properties.velocity, a_vel_change);
            b->newtonian_properties.velocity = VectorSum_2d(b->newtonian_properties.velocity, VectorScale_2d(b_vel_change, -1));
         }

         frame_counter.total_frames % 300 == 0 ? printf("COLLISION CHECK for A(%.0f,%.0f) B(%.0f,%.0f) = %s\n",
                a->newtonian_properties.coords_origin.x,
                a->newtonian_properties.coords_origin.y,
                b->newtonian_properties.coords_origin.x,
                b->newtonian_properties.coords_origin.y,
                colliding ? "TRUE" : "FALSE") : (void)0;
         
      }
   }
   // UpdateObjectVectors(objs, delta_time);
   // UpdateWorldState(objs, &world->coord_space_grid.coord_space, delta_time);
}

void UpdateWorldState(Collection *polygonoids, CoordSpace2d *space, float delta_time)
{
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
}

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
   Matrix2x2 a_box_coords = FindBoxedCoords(a.surface.surface_vectors);
   Matrix2x2 b_box_coords = FindBoxedCoords(b.surface.surface_vectors);

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
      printf("Object A is LEFT of Object B\n");
      return false; // A is left of B
   }
   if (a_top_left.x > b_top_left.x + b_width)
   {
      printf("Object A is RIGHT of Object B\n");
      return false; // A is right of B
   }

   // Rule out based on y position
   float a_height = fabsf(a_box_coords.col2.y - a_box_coords.col1.y);
   float b_height = fabsf(b_box_coords.col2.y - b_box_coords.col1.y);
   if (a_top_left.y + a_height < b_top_left.y)
   {
      //printf("Object A is ABOVE Object B\n");
      return false; // A is above B
   }
   if (a_top_left.y > b_top_left.y + b_height)
   {
      //printf("Object A is BELOW Object B\n");
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
