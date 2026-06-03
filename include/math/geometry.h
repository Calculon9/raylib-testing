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
    LArray vertices;
} Polygon;

// 2D Surface Properties
typedef struct Surface2d {
    LArray surface_vectors; // Sorted vectors (vector[i] connects to vector[i+1]) defining the points & shape of the object's surface
} Surface2d;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Surface2d CreateSurface_Rectangular(Vector2d dimensions, Vector2d vertice_offset);
Matrix2x2 GetBoxedCoords(LArray *vertices, Vector2d vertice_offset);
Vector2d GetBoxedDimensions(LArray *vertices);
Vector2d GetCenteredBoxOffset(Vector2d box_a_dimensions, Vector2d box_b_dimensions);
Vector2d GetGeometricCentre_FromBox(Matrix2x2 box_coords);
Vector2d GetGeometricCentre_FromSurface(Surface2d object_surface, Vector2d vertice_offset);
void CenterVerticesToExtents(LArray *points);
void NormaliseVerticesToLocal(LArray *points);
bool IsPointInPolygon(Vector2d point, Vector2d *polygon_vertices, Vector2d vertice_offset, int vertice_count);
bool ShapeFitsWithinShape(LArray *shape1_vertices, LArray *shape2_vertices, Vector2d shape1_vertice_offset, Vector2d shape2_vertice_offset);
bool BoxFitsWithinBox(Matrix2x2 box1, Matrix2x2 box2);
Matrix2x2 BoxIntersectionPointsWithBox(Matrix2x2 box1, Matrix2x2 box2);
LArray ShapeAVerticesInShapeB(LArray *shape1_vertices, LArray *shape2_vertices, Vector2d shape1_vertice_offset, Vector2d shape2_vertice_offset);

#endif