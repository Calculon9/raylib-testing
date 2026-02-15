/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef RECTANGLOID_H
#define RECTANGLOID_H
#include <stddef.h>
#include "math/cvectors.h"
#include "colour/colour.h"
#include "physics/newton_object.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
//#define NEW_CIRCLOID() AllocateBytes(sizeof(Circloid))
//#define NEW_CIRCLOID() allocate_block(sizeof(Circloid))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct Rectangloid {
    NewtonObject2d object;
    ColourRgba colourRgba;
    float height, width;
} Rectangloid;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Rectangloid CreateRectangloid(float height, float width, ColourRgba colour, size_t mass, Vector2d position, Velocity2d velocity, Acceleration2d acceleration);
Rectangloid CreateRectangloid_FromObject(NewtonObject2d newtOb, float height, float width, ColourRgba colour);
void Rectangloid_GetCollisionObjects(Rectangloid rect);
#endif