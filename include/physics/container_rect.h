/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef CONTAINER_RECT_H
#define CONTAINER_RECT_H
#include <stddef.h>
#include "math/cvectors.h"
#include "physics/rectangloid.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
//#define NEW_CIRCLOID() AllocateBytes(sizeof(Circloid))
//#define NEW_CIRCLOID() allocate_block(sizeof(Circloid))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct Container_Rect {
    Rectangloid rect;
    void *items;
} Container_Rect;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Container_Rect CreateContainer_Rect(Rectangloid object, void *items);
//Rectangloid CreateRectangloid_FromObject(NewtonObject2d newtOb, float height, float width, ColourRgba colour);
void Container_Rect_GetCollisionObjects(Rectangloid rect);
#endif