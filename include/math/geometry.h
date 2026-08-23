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
#define MAX_SHAPE_VERTICES 32  // Reduced from 512 to prevent O(v²) SAT explosion
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

typedef enum {
    SHAPE_LAYOUT_REGULAR,   // Mathematically perfect, balanced shapes (even vertex distributions)
    SHAPE_LAYOUT_COMPLEX,   // Hand-crafted, asymmetric, or irregular vertex layouts
} ShapeLayoutType;

typedef enum {
    SHAPE_AUTO,
    SHAPE_TRIANGLE,
    SHAPE_SQUARE,
    SHAPE_CIRCLE,
    SHAPE_POLYGON,
    SHAPE_RECTANGLE,
    SHAPE_TRIANGLE_EQUILATERAL,
    SHAPE_TRIANGLE_ISOSCELES,
    SHAPE_ARROW
} ShapeType;

typedef enum {
    SHAPE_BUILD_REGULAR,
    SHAPE_BUILD_IRREGULAR
} ShapeBuildType;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Surface2d CreateSurface_Rectangular(Vector2d dimensions, Vector2d vertice_offset);
Matrix2x2 AABB2d_FromPoints(const Vector2d *points, int point_count);
Matrix2x2 AABB2d_FromOriginDimensions(Vector2d origin, Vector2d dimensions);
bool AABB2d_Overlaps(Matrix2x2 box1, Matrix2x2 box2);
bool AABB2d_Contains(Matrix2x2 container, Matrix2x2 box);
Matrix2x2 CalcAABBCoords_Tight(Vector2d *vertices, int vertice_count, Vector2d vertice_offset);
Vector2d CalcAABBDimensions(Vector2d *vertices, int vertice_count);
Vector2d CalcCenteredBoxOffset(Vector2d box_a_dimensions, Vector2d box_b_dimensions);
Vector2d CalcGeometricCentre_FromBox(Matrix2x2 box_coords);
Vector2d CalcGeometricCentre_FromSurface(Surface2d object_surface, Vector2d vertice_offset);
void CenterVerticesToExtents(LArray *points);
void NormaliseVerticesToLocal(LArray *points);
bool IsPointInPolygon(Vector2d point, Vector2d *polygon_vertices, Vector2d vertice_offset, int vertice_count);
bool ShapeFitsWithinShape(LArray *shape1_vertices, LArray *shape2_vertices, Vector2d shape1_vertice_offset, Vector2d shape2_vertice_offset);
void CalcBoxVertices(Vector2d dimensions, Vector2d anchor_position, Vector2d out_vertices[4]);
LArray ShapeAVerticesInShapeB(LArray *shape1_vertices, LArray *shape2_vertices, Vector2d shape1_vertice_offset, Vector2d shape2_vertice_offset);
LArray CreateVertices_Symmetric (int vertice_count, float radius_x, float radius_y);
LArray CreateVertices_Irregular (int vertice_count, float min_radius, float max_radius);
#endif
