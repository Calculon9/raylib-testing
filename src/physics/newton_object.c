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

NewtonObject2d CreateNewtonObject2d(float mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration, Surface2d surface)
{
   NewtonObject2d newtOb = {0};
   newtOb.coords_center = coords_center;
   newtOb.mass = mass;
   newtOb.inverse_mass = 1.0f / mass;
   newtOb.velocity = velocity;
   newtOb.acceleration = acceleration;
   newtOb.surface = surface;
   newtOb.boxed_dimensions = CalcAABBDimensions(&surface.surface_vectors);
   newtOb.coords_origin = (Vector2d){newtOb.coords_center.x - (newtOb.boxed_dimensions.x / 2.0), newtOb.coords_center.y - (newtOb.boxed_dimensions.y / 2.0)};
   newtOb.radius = (newtOb.boxed_dimensions.x  > newtOb.boxed_dimensions.y) ? newtOb.boxed_dimensions.x : newtOb.boxed_dimensions.y;

   // Initialize momentum based on mass and velocity
   newtOb.momentum.x = newtOb.mass * newtOb.velocity.x;
   newtOb.momentum.y = newtOb.mass * newtOb.velocity.y;

   // printf("CREATED OBJECT BOX: Top-Left (%.2f, %.2f) Bottom-Right (%.2f, %.2f)\n",
   //        newtOb.coords_origin.x,
   //        newtOb.coords_origin.y,
   //        newtOb.coords_origin.x + newtOb.boxed_dimensions.x,
   //        newtOb.coords_origin.y + newtOb.boxed_dimensions.y);
   return newtOb;
}

// Creates an immobile, massless NewtonObject at the assigned world_position
NewtonObject2d CreateNewtonObject2d_Static(Vector2d coords_center, Surface2d surface)
{
   NewtonObject2d newtOb = {0};
   // Initialize the NewtonObject2d properties here (e.g., set world_position, velocity, etc.)
   newtOb.mass = 0.0;
   newtOb.inverse_mass = 0.0;
   newtOb.boxed_dimensions = CalcAABBDimensions(&surface.surface_vectors);
   newtOb.coords_center = coords_center;
   newtOb.surface = surface;
   newtOb.coords_origin = (Vector2d){newtOb.coords_center.x - (newtOb.boxed_dimensions.x / 2.0), newtOb.coords_center.y - (newtOb.boxed_dimensions.y / 2.0)};
   newtOb.radius = (newtOb.boxed_dimensions.x  > newtOb.boxed_dimensions.y) ? newtOb.boxed_dimensions.x : newtOb.boxed_dimensions.y;

   return newtOb;
}

void CalcVectors(NewtonObject2d *object, float deltaTime)
{
   // Recalc inverse_mass up front in case gameplay code mutated mass this frame
   object->inverse_mass = (object->mass != 0.0f) ? (1.0f / object->mass) : 0.0f;

   // Compute the exact positional displacement using the kinematic formula:
   // displacement = (v * dt) + (0.5 * a * dt^2)
   // This perfectly accounts for acceleration happening during the frame!
   Vector2d displacement;
   displacement.x = (object->velocity.x * deltaTime) + (0.5f * object->acceleration.x * deltaTime * deltaTime);
   displacement.y = (object->velocity.y * deltaTime) + (0.5f * object->acceleration.y * deltaTime * deltaTime);

   // Displace reference systems 
   object->coords_origin = VectorSum_2d(object->coords_origin, displacement);
   object->coords_center = VectorSum_2d(object->coords_center, displacement);

   // Now update velocity based on acceleration for the next frame's baseline
   object->velocity.x += object->acceleration.x * deltaTime;
   object->velocity.y += object->acceleration.y * deltaTime;

   // Keep momentum synchronized with the newly updated velocity
   object->momentum.x = object->mass * object->velocity.x;
   object->momentum.y = object->mass * object->velocity.y;
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
