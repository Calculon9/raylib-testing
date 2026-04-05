/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/

#include <stdio.h>
#include "physics/newton_object.h"
#include "math/cvectors.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------

NewtonObject2d CreateNewtonObject2d(size_t mass, Vector2d world_position, Velocity2d velocity, Acceleration2d acceleration, Surface2d surface)
{
   NewtonObject2d newtOb = {0};
   // Initialize the NewtonObject2d properties here (e.g., set world_position, velocity, etc.)
   newtOb.mass = mass;
   newtOb.inverseMass = 1.0f / mass;
   newtOb.world_position = world_position;
   newtOb.world_position_center = world_position; // For now we will assume the center is the same as the world_position, but this can be adjusted later if we want to define the world_position of the object based on its surface or some other point
   newtOb.velocity = velocity;
   newtOb.acceleration = acceleration;
   newtOb.surface = surface;
   // newtOb.id = GenerateId();

   return newtOb;
   // Initialize momentum based on mass and velocity
}

// Creates an immobile, massless NewtonObject at the assigned world_position
NewtonObject2d CreateNewtonObject2d_Static(Vector2d world_position, Surface2d surface)
{
   NewtonObject2d newtOb = {0};
   // Initialize the NewtonObject2d properties here (e.g., set world_position, velocity, etc.)
   newtOb.mass = 0.0;
   newtOb.inverseMass = 0.0;
   newtOb.world_position = world_position;
   newtOb.world_position_center = world_position; // For now we will assume the center is the same as the world_position, but this can be adjusted later if we want to define the world_position of the object based on its surface or some other point
   newtOb.velocity = (Velocity2d){(Vector2d){0.0f, 0.0f}, 0.0f, 0.0f};
   newtOb.acceleration = (Acceleration2d){(Vector2d){0.0f, 0.0f}, 0.0f, 0.0f};
   newtOb.surface = surface;

   return newtOb;
   // Initialize momentum based on mass and velocity
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
   object->world_position.x += object->velocity.velocityXy.x * deltaTime;
   object->world_position.y += object->velocity.velocityXy.y * deltaTime;

   if (object->acceleration.accelerationXy.x != 0)
   {
      printf("DEBUG: Acceleration is NOT zero! It is: %f\n", object->acceleration.accelerationXy.x);
   }
}

Vector2d CalculateCenterRelativeToOrigin_Fast(NewtonObject2d *object)
{
   // Update velocity based on acceleration and time
   Collection *points = &object->surface.surface_vectors.coll;
}