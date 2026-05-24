/**********************************************************************************************
*
GEOMETRY MODULE
*
**********************************************************************************************/
#ifndef GEOMETRY_H
#define GEOMETRY_H
#include "common/common.h"
#include "math/cvectors.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// typedef struct {
//     float x, y, width, height;
// } Rectangle;

typedef struct {
    DynamicArray vertices;
} Polygon;

// typedef struct Matrix2d {
//      DynamicArray *items; // The flat array of data
//      int cols;        // Number of columns
//      int rows;        // Number of rows
//  } Matrix2d;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Matrix2x2 GetBoxedCoords(LArray vertices);
bool IsPointInPolygon(Vector2d point, Vector2d* vertices, int count);
bool ShapeFitsWithinShape(LArray shape1_vertices, LArray shape2_vertices);
bool BoxFitsWithin(Matrix2x2 box1, Matrix2x2 box2);

#endif