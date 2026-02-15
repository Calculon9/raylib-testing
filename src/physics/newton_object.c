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

NewtonObject2d CreateNewtonObject2d(size_t mass, Vector2d position, Velocity2d velocity, Acceleration2d acceleration)
{
   NewtonObject2d newtOb = {0};
   // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
   newtOb.mass = mass;
   newtOb.pos = position;
   newtOb.velocity = velocity;
   newtOb.acceleration = acceleration;

   return newtOb;
   // Initialize momentum based on mass and velocity
}

// Creates an immobile, massless NewtonObject at the assigned position
NewtonObject2d CreateNewtonObject2d_Static(Vector2d position)
{
   NewtonObject2d newtOb = {0};
   // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
   newtOb.mass = 0;
   newtOb.pos = position;
   newtOb.velocity = (Velocity2d){(Vector2d){0.0f, 0.0f},0.0f, 0.0f};
   newtOb.acceleration = (Acceleration2d){(Vector2d){0.0f, 0.0f},0.0f, 0.0f};

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

   // Update position based on velocity and time
   object->pos.x += object->velocity.velocityXy.x * deltaTime;
   object->pos.y += object->velocity.velocityXy.y * deltaTime;

   if (object->acceleration.accelerationXy.x != 0) {
    printf("DEBUG: Acceleration is NOT zero! It is: %f\n", object->acceleration.accelerationXy.x);
}
}
