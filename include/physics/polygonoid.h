/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef POLYGONOID_H
#define POLYGONOID_H
#include "common/common.h"
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
typedef struct Polygonoid {
    NewtonObject2d newtonian_properties;
    ColourRgba colourRgba;
    float radius;
} Polygonoid;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Polygonoid CreatePolygonoid_Symmetric(int vertice_count, float radius, ColourRgba colour, float mass, Vector2d origin, Vector2d velocity, Vector2d acceleration);
Polygonoid CreatePolygonoid_Irregular(int vertice_count, float min_radius, float max_radius, ColourRgba colour, float mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration);
//DynamicArray* GenerateRectangloidSurfaceVectors (float length, float width,int vertices);
//Circloid CreateCircloid_FromObject(NewtonObject2d newtOb, float radius, ColourRgba colour);
//void Circloid_GetCollisionObjects(Circloid circ);
#endif