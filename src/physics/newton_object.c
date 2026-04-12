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

NewtonObject2d CreateNewtonObject2d(size_t mass, Vector2d origin, Velocity2d velocity, Acceleration2d acceleration, Surface2d surface)
{
   NewtonObject2d newtOb = {0};
   // Initialize the NewtonObject2d properties here (e.g., set world_position, velocity, etc.)
   newtOb.mass = mass;
   newtOb.inverseMass = 1.0f / mass;
   newtOb.coords_origin = origin;
   newtOb.coords_center = origin; // For now we will assume the center is the same as the world_position, but this can be adjusted later if we want to define the world_position of the object based on its surface or some other point
   newtOb.velocity = velocity;
   newtOb.acceleration = acceleration;
   newtOb.surface = surface;

   // newtOb.id = GenerateId();

   // Initialize momentum based on mass and velocity

   return newtOb;
}

// Creates an immobile, massless NewtonObject at the assigned world_position
NewtonObject2d_Static CreateNewtonObject2d_Static(Vector2d origin, Surface2d surface)
{
   NewtonObject2d_Static newtOb = {0};
   // Initialize the NewtonObject2d properties here (e.g., set world_position, velocity, etc.)
   newtOb.mass = 0.0;
   newtOb.inverseMass = 0.0;
   newtOb.coords_origin = origin;
   newtOb.coords_center = origin; // For now we will assume the center is the same as the world_position, but this can be adjusted later if we want to define the world_position of the object based on its surface or some other point
   newtOb.surface = surface;

   return newtOb;
}

void CalculateVectors(NewtonObject2d *object, float deltaTime)
{
   // Update velocity based on acceleration and time
   object->velocity.velocityXy.x += object->acceleration.accelerationXy.x * deltaTime;
   object->velocity.velocityXy.y += object->acceleration.accelerationXy.y * deltaTime;

   // Update momentum based on mass and velocity
   object->momentum.momentumXy.x = object->mass * object->velocity.velocityXy.x;
   object->momentum.momentumXy.y = object->mass * object->velocity.velocityXy.y;

   // Update world_position based on velocity and time
   object->coords_origin.x += object->velocity.velocityXy.x * deltaTime;
   object->coords_origin.y += object->velocity.velocityXy.y * deltaTime;

   if (object->acceleration.accelerationXy.x != 0)
   {
      printf("DEBUG: Acceleration is NOT zero! It is: %f\n", object->acceleration.accelerationXy.x);
   }
}

// Creates surface vectors with an offset of 0. Apply to an object's coords to associate the returned surface with it.
// 4----3
// |    | 
// 1----2
Surface2d CreateSurface_Rectangular(Vector2d resolution)
{
   Surface2d surf = {0};
   surf.surface_vectors = *NEW_DYNAMIC_ARRAY(4, Vector2d);
   Vector2d *items = (Vector2d *)surf.surface_vectors.coll.items;

   // Vector2d vertice_1 = {origin.x, origin.y};
   // Vector2d vertice_2 = {origin.x + resolution_ixj.x, origin.y};
   // Vector2d vertice_3 = {origin.x + resolution_ixj.x, origin.y + resolution_ixj.y};
   // Vector2d vertice_4 = {origin.x, origin.y + resolution_ixj.y};
   // Do 1-2
   Vector2d vertice = {0, 0};
   int i,j = 0;
   for (int i = 0; i < 2; i++)
   {
      vertice.x = i * resolution.x;
      //Vector2d *pVec = items + (i * sizeof(Vector2d));
      Array_Push(&surf.surface_vectors, &vertice);
      //memcpy(pVec, &vertice, sizeof(Vector2d));
   }
   vertice.y = resolution.y; //go up (2-->3)
   // Do 3-4
   for (int i = 1; i > -1; i--)
   {
      vertice.x = (i * resolution.x);
      //Vector2d *pVec = items + ((3 - i) * sizeof(Vector2d));
      Array_Push(&surf.surface_vectors, &vertice);
      //memcpy(pVec, &vertice, sizeof(Vector2d));
   }
}

Vector2d CalculateCenterRelativeToOrigin_Fast(NewtonObject2d *object)
{
   // Update velocity based on acceleration and time
   Collection *points = &object->surface.surface_vectors.coll;
}