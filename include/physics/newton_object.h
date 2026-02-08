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
typedef struct {
    Vector2d velocityXy;
    float radians;
    float magnitude;
} Velocity2d;

// 2D acceleration state 
typedef struct {
    Vector2d accelerationXy;
    float radians;
    float magnitude;
} Acceleration2d;

// 2D mommentum state 
typedef struct {
    Vector2d momentumXy;
    float radians;
    float magnitude;
} Momentum2d;

// 2D object with Newtonian properties; mass, position, velocity, acceleration, momentum 
typedef struct {
    Vector2d pos;
    Velocity2d velocity;
    Acceleration2d acceleration;
    Momentum2d momentum;
    float mass;
} NewtonObject2d;



//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

//NewtonObject2 *CreateNewtonObject2(size_t element_count, size_t element_bytes);


#endif