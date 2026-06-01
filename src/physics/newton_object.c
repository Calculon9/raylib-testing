/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/

#include "common/common.h"
#include "physics/newton_object.h"
#include "math/cvectors.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------

NewtonObject2d CreateNewtonObject2d(size_t mass, Vector2d origin, Vector2d velocity, Vector2d acceleration, Surface2d surface)
{
   NewtonObject2d newtOb = {0};
   // Initialize the NewtonObject2d properties here (e.g., set world_position, velocity, etc.)
   newtOb.mass = mass;
   newtOb.inverseMass = 1.0f / mass;
   newtOb.coords_origin = origin;
   newtOb.velocity = velocity;
   newtOb.acceleration = acceleration;
   newtOb.surface = surface;
   newtOb.boxed_dimensions = GetBoxedDimensions(&surface.surface_vectors);
   newtOb.coords_center = (Vector2d){origin.x + (newtOb.boxed_dimensions.x / 2), origin.y + (newtOb.boxed_dimensions.y / 2)};
   // Initialize momentum based on mass and velocity
   printf("CREATED OBJECT BOX: Top-Left (%.2f, %.2f) Bottom-Right (%.2f, %.2f)\n",
          newtOb.coords_origin.x,
          newtOb.coords_origin.y,
          newtOb.coords_origin.x + newtOb.boxed_dimensions.x,
          newtOb.coords_origin.y + newtOb.boxed_dimensions.y);
   return newtOb;
}

// Creates an immobile, massless NewtonObject at the assigned world_position
NewtonObject2d CreateNewtonObject2d_Static(Vector2d origin, Surface2d surface)
{
   NewtonObject2d newtOb = {0};
   // Initialize the NewtonObject2d properties here (e.g., set world_position, velocity, etc.)
   newtOb.mass = 0.0;
   newtOb.inverseMass = 0.0;
   newtOb.coords_origin = origin;
   newtOb.coords_center = origin; // For now we will assume the center is the same as the world_position, but this can be adjusted later if we want to define the world_position of the object based on its surface or some other point
   newtOb.surface = surface;
   // newtOb.id = id;

   return newtOb;
}

void CalculateVectors(NewtonObject2d *object, float deltaTime)
{
   // Update velocity based on acceleration and time
   object->velocity.x += object->acceleration.x * deltaTime;
   object->velocity.y += object->acceleration.y * deltaTime;

   // Update momentum based on mass and velocity
   object->momentum.x = object->mass * object->velocity.x;
   object->momentum.y = object->mass * object->velocity.y;

   // Update world_position based on velocity and time
   object->coords_origin.x += object->velocity.x * deltaTime;
   object->coords_origin.y += object->velocity.y * deltaTime;

   // if (object->acceleration.x != 0)
   // {
   //    printf("DEBUG: Acceleration is NOT zero! It is: %f\n", object->acceleration.x);
   // }
}

// Creates surface vectors with an offset of 0. Apply to an object's coords to associate the returned surface with it.
// 4----3
// |    |
// 1----2
Surface2d CreateSurface_Rectangular(Vector2d dimensions)
{
   Surface2d surf = {0};
   surf.surface_vectors.items = calloc(4, sizeof(Vector2d));
   surf.surface_vectors.capacity = 4;
   surf.surface_vectors.count = 0;
   surf.surface_vectors.elem_bytes = sizeof(Vector2d);

   // Calculate the half-extents
   float hx = dimensions.x / 2.0f;
   float hy = dimensions.y / 2.0f;

   // Define the 4 corners relative to a center point of (0,0)
   // Typically ordered clockwise or counter-clockwise
   Vector2d vertices[4] = {
      { -hx, -hy }, // Top-Left corner
      {  hx, -hy }, // Top-Right corner
      {  hx,  hy }, // Bottom-Right corner
      { -hx,  hy }  // Bottom-Left corner
   };

   // Push the centered vertices into the dynamic array
   for (int i = 0; i < 4; i++)
   {
      LArray_Push(&surf.surface_vectors, &vertices[i]);
   }

   return surf;
}

// Vector2d CalculateCenterRelativeToOrigin_Fast(NewtonObject2d *object)
// {
//    // Update velocity based on acceleration and time
//    Collection *points = &object->surface.surface_vectors;
// }

// Returns the boxed coords from a collection of vertice vectors (must all be relative to the associated object's coords)
// Matrix2x2 FindBoxedCoords(DArray vertices)
// {
//    Matrix2x2 box_coords = {0};
//    if (vertices.count < 2)
//    {
//       return box_coords;
//    }
//    Vector2d *pts = vertices.items;

//    // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
//    box_coords.col1 = pts[0];
//    box_coords.col2 = pts[0];
//    Vector2d vertice = {0};
//    for (size_t i = 1; i < vertices.count; i++)
//    {
//       vertice = pts[i];

//       box_coords.col1.x = fminf(box_coords.col1.x, vertice.x);
//       box_coords.col2.x = fmaxf(box_coords.col2.x, vertice.x);

//       box_coords.col1.y = fminf(box_coords.col1.y, vertice.y);
//       box_coords.col2.y = fmaxf(box_coords.col2.y, vertice.y);

//       // // Check if x is a min or max
//       // if (vertice.x > box_coords.col2.x)
//       // {
//       //    box_coords.col2.x = vertice.x;
//       // }
//       // else if (vertice.x < box_coords.col1.x)
//       // {
//       //    box_coords.col1.x = vertice.x;
//       // }

//       // // Check if y is a min or max
//       // if (vertice.y > box_coords.col2.y)
//       // {
//       //    box_coords.col2.y = vertice.y;
//       // }
//       // else if (vertice.y < box_coords.col1.y)
//       // {
//       //    box_coords.col1.y = vertice.y;
//       // }
//    }
//    return box_coords;
// }

Vector2d GetObjectCentre(Surface2d object_surface)
{
   Vector2d mid = {0};
   if (object_surface.surface_vectors.items != NULL)
   {
      Matrix2x2 box_coords = GetBoxedCoords(&object_surface.surface_vectors);
      float mid_x = (box_coords.col1.x + box_coords.col2.x) / 2;
      float mid_y = (box_coords.col1.y + box_coords.col2.y) / 2;
      mid.x = mid_x;
      mid.y = mid_y;
   }

   return mid;
}