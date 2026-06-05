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

NewtonObject2d CreateNewtonObject2d(size_t mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration, Surface2d surface)
{
   NewtonObject2d newtOb = {0};
   newtOb.mass = mass;
   newtOb.inverse_mass = 1.0f / mass;
   newtOb.velocity = velocity;
   newtOb.acceleration = acceleration;
   newtOb.surface = surface;
   newtOb.boxed_dimensions = CalcAABBDimensions(&surface.surface_vectors);
   newtOb.coords_center = coords_center;// (Vector2d){origin.x + (newtOb.boxed_dimensions.x / 2), origin.y + (newtOb.boxed_dimensions.y / 2)};
   newtOb.coords_origin = (Vector2d){newtOb.coords_center.x - (newtOb.boxed_dimensions.x / 2), newtOb.coords_center.y - (newtOb.boxed_dimensions.y / 2)};
   
   // Initialize momentum based on mass and velocity
   newtOb.momentum.x = newtOb.mass * newtOb.velocity.x;
   newtOb.momentum.y = newtOb.mass * newtOb.velocity.y;

   printf("CREATED OBJECT BOX: Top-Left (%.2f, %.2f) Bottom-Right (%.2f, %.2f)\n",
          newtOb.coords_origin.x,
          newtOb.coords_origin.y,
          newtOb.coords_origin.x + newtOb.boxed_dimensions.x,
          newtOb.coords_origin.y + newtOb.boxed_dimensions.y);
   return newtOb;
}

// Creates an immobile, massless NewtonObject at the assigned world_position
NewtonObject2d CreateNewtonObject2d_Static(Vector2d coords_center, Surface2d surface)
{
   NewtonObject2d newtOb = {0};
   // Initialize the NewtonObject2d properties here (e.g., set world_position, velocity, etc.)
   newtOb.mass = 0.0;
   newtOb.inverse_mass = 0.0;
   //newtOb.coords_origin = origin;
   newtOb.coords_center = coords_center;
   newtOb.surface = surface;
   // newtOb.id = id;

   return newtOb;
}

void CalcVectors(NewtonObject2d *object, float deltaTime)
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
   object->coords_center.x += object->velocity.x * deltaTime;
   object->coords_center.y += object->velocity.y * deltaTime;

   // if (object->acceleration.x != 0)
   // {
   //    printf("DEBUG: Acceleration is NOT zero! It is: %f\n", object->acceleration.x);
   // }
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

