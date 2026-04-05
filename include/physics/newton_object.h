/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef NEWTON_OBJECT_H
#define NEWTON_OBJECT_H
#include "common/common.h"
#include "collections/dynamic_array.h"
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

// 2D Surface Properties
typedef struct Surface2d {
    DynamicArray surface_vectors; // Sorted vectors (vector[i] connects to vector[i+1]) defining the points & shape of the object's surface
} Surface2d;

// 2D Object with Newtonian properties; mass, position, velocity, acceleration, momentum 
typedef struct NewtonObject2d {
    Vector2d world_position_center;
    Vector2d world_position;
    Velocity2d velocity;
    Acceleration2d acceleration;
    Momentum2d momentum;
    Surface2d surface; 
    float mass;
    float inverseMass;
} NewtonObject2d;



//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

NewtonObject2d CreateNewtonObject2d(size_t mass, Vector2d position, Velocity2d velocity, Acceleration2d acceleration, Surface2d surface);
NewtonObject2d CreateNewtonObject2d_Static(Vector2d position, Surface2d surface);
void CalculateVectors(NewtonObject2d *object, float deltaTime);

#endif