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
#define MAX_SHAPE_VERTICES 64  // Vertex budget cap. Bounds every per-shape stack array and the
                               // O(v²) SAT narrow phase, so keep it modest; raised from 32 to
                               // allow higher gear tooth counts (teeth * GEAR_POINTS_PER_TOOTH).

// Gear geometry contract (shared by CreateVertices_Gear and gear entity validation).
// A gear outline emits GEAR_POINTS_PER_TOOTH vertices per tooth, so the maximum tooth
// count is MAX_SHAPE_VERTICES / GEAR_POINTS_PER_TOOTH.
#define GEAR_POINTS_PER_TOOTH 3
#define GEAR_MIN_TEETH 3
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
    SHAPE_ELLIPSE,
    SHAPE_POLYGON,
    SHAPE_RECTANGLE,
    SHAPE_TRIANGLE_EQUILATERAL,
    SHAPE_TRIANGLE_ISOSCELES,
    SHAPE_ARROW,
    SHAPE_ROTOR,
    SHAPE_GEAR
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
Surface2d CreateSurface_Rectangular(Vector2d dimensions);
Matrix2x2 AABB2d_FromPoints(const Vector2d *points, int point_count);
Matrix2x2 AABB2d_FromOriginDimensions(Vector2d origin, Vector2d dimensions);
bool AABB2d_Overlaps(Matrix2x2 box1, Matrix2x2 box2);
bool AABB2d_Contains(Matrix2x2 container, Matrix2x2 box);
// Clips segment [*a,*b] to the axis-aligned rectangle [rect_min, rect_max] using the
// Liang-Barsky algorithm. Mutates *a and *b to the clipped endpoints. Returns true if
// any part of the segment lies inside the rectangle, false if it is entirely outside
// (in which case *a and *b are left unmodified). Pure math; no raylib dependency.
bool ClipSegmentToRect(Vector2d *a, Vector2d *b, Vector2d rect_min, Vector2d rect_max);
Matrix2x2 CalcAABBCoords_Tight(Vector2d *vertices, int vertice_count, Vector2d vertice_offset);
Vector2d CalcGeometricCentre_FromBox(Matrix2x2 box_coords);
void CenterVerticesToExtents(LArray *points);
void NormaliseVerticesToLocal(LArray *points);
bool IsPointInPolygon(Vector2d point, Vector2d *polygon_vertices, Vector2d vertice_offset, int vertice_count);
void CalcBoxVertices(Vector2d dimensions, Vector2d anchor_position, Vector2d out_vertices[4]);
LArray ShapeAVerticesInShapeB(LArray *shape1_vertices, LArray *shape2_vertices, Vector2d shape1_vertice_offset, Vector2d shape2_vertice_offset);
LArray CreateVertices_Symmetric (int vertice_count, float radius_x, float radius_y);
LArray CreateVertices_Irregular (int vertice_count, float min_radius, float max_radius);
LArray CreateVertices_Rotor(int blade_count, float radius_x, float radius_y);
LArray CreateVertices_Gear(int tooth_count, float radius_x, float radius_y);
LArray CreateVertices_Portal(Vector2d dimensions);
#endif
