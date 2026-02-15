/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef NEWTON_OBJECT_H
#define NEWTON_OBJECT_H
#include <stddef.h>
#include "math/cvectors.h"
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
//#define NEW_CIRCLOID(count, type) allocate_array(count, sizeof(type))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// 2D velocity state 
typedef struct Velocity2d {
    Vector2d velocityXy;
    float radians;
    float magnitude;
} Velocity2d;

// 2D acceleration state 
typedef struct Acceleration2d {
    Vector2d accelerationXy;
    float radians;
    float magnitude;
} Acceleration2d;

// 2D mommentum state 
typedef struct Momentum2d {
    Vector2d momentumXy;
    float radians;
    float magnitude;
} Momentum2d;

// 2D object with Newtonian properties; mass, position, velocity, acceleration, momentum 
typedef struct NewtonObject2d {
    Vector2d pos;
    Velocity2d velocity;
    Acceleration2d acceleration;
    Momentum2d momentum;
    float mass;
    float inverseMass;
} NewtonObject2d;



//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

NewtonObject2d CreateNewtonObject2d(size_t mass, Vector2d position, Velocity2d velocity, Acceleration2d acceleration);
NewtonObject2d CreateNewtonObject2d_Static(Vector2d position);
void CalculateVectors(NewtonObject2d *object, float deltaTime);

#endif