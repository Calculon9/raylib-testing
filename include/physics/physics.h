/**********************************************************************************************
*
PHYSICS MODULE
*
**********************************************************************************************/
#ifndef PHYSICS_H
#define PHYSICS_H
#include "common/common.h"
#include "math/cvectors.h"
#include "physics/newtonoid.h"
//#include "physics/polygonoid.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------


//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// typedef struct {
//     NewtonObject2d object;
//     ColourRgba colourRgba;
//     float radius;
// } Circloid;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// Circloid CreateCircloid(float radius, ColourRgba colour, size_t mass, Vector2d position, Velocity2d velocity, Acceleration2d acceleration, Surface2d surface);
// DynamicArray* GenerateCircloidSurfaceVectors (Circloid *circloid, int vertices);
// Circloid CreateCircloid_FromObject(NewtonObject2d newtOb, float radius, ColourRgba colour);
// void Circloid_GetCollisionObjects(Circloid circ);
#endif