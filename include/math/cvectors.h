/**********************************************************************************************
*
MEMORY MANAGEMENT MODULE
*
**********************************************************************************************/
#ifndef CVECTORS_H
#define CVECTORS_H
#include "common/common.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define ZERO_VECTOR_2D (Vector2d){0, 0}
#define INFINITY_VECTOR_2D (Vector2d){INFINITY, INFINITY}
#define INFINITY_MATRIX_2x2 (Matrix2x2){INFINITY_VECTOR_2D, INFINITY_VECTOR_2D}
#define ZERO_VECTOR_3D (Vector3d){0, 0, 0}
#define INFINITY_VECTOR_3D (Vector3d){INFINITY, INFINITY, INFINITY}
#define INFINITY_MATRIX_3x3 (Matrix3x3){INFINITY_VECTOR_3D, INFINITY_VECTOR_3D, INFINITY_VECTOR_3D}
#define IDENTITY_MATRIX_2x2 (Matrix2x2){(Vector2d){1, 0}, (Vector2d){0, 1}}
#define IDENTITY_BASIS_2D (Basis2d){(Vector2d){1, 0}, (Vector2d){0, 1}}
//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct Vector2d
{
    float x;
    float y;
} Vector2d;

typedef struct Basis2d
{
    Vector2d u; // The "x-ish/i-ish" step
    Vector2d v; // The "y-ish/j-ish" step
} Basis2d;

typedef struct Polar2d
{
    float magnitude;
    float radians;
} Polar2d;

typedef struct Transform2d
{
    Vector2d u;      // X-Axis basis
    Vector2d v;      // Y-Axis basis
    Vector2d origin; // The (0,0) point in world coordinates
} Transform2d;

typedef struct Frame2d
{
    Basis2d basis;
    Vector2d origin_in_parent;
    Vector2d local_min;
    Vector2d local_max;
} Frame2d;

typedef struct Vector3d
{
    float x;
    float y;
    float z;
} Vector3d;

typedef struct Basis3d
{
    Vector3d basis_u; // The "x-ish/i-ish" step
    Vector3d basis_v; // The "y-ish/j-ish" step
    Vector3d basis_w; // The "z-ish/k-ish" step
} Basis3d;

typedef struct Matrix3x3
{
    Vector3d col1; // BasisU.x, BasisU.y, 0 
    Vector3d col2; // BasisV.x, BasisV.y, 0
    Vector3d col3; // Trans.x, Trans.y, 1
} Matrix3x3;

typedef struct Matrix2x2
{
    Vector2d col1; // Row 1: BasisU.x, BasisV.x, Trans.x
    Vector2d col2;  // Row 2: BasisU.y, BasisV.y, Trans.y
} Matrix2x2;

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

Vector2d VectorSumArray_2d(Vector2d *array, size_t count);
Vector2d VectorSum_2d(Vector2d a, Vector2d b);
Vector2d VectorScale_2d(Vector2d vector, float scalar);
Polar2d PolarForm_2d(Vector2d vector);
float VectorMagnitude_2d(Vector2d vector);
float VectorBox_2d(Vector2d vector);
float VectorDot_2d(Vector2d a, Vector2d b);
float VectorRadians_2d(Vector2d vector);
Vector2d VectorComponents_2d(float magnitude, float radians);
Vector3d VectorSumArray_3d(Vector3d *array, size_t count);
// Matrix3x3 VectorTransform_Scale_Rotate_2d(Vector2d origin, float scale_u, float scale_v, float radians_u, float radians_v);

Matrix3x3 MatrixMultiply_3x3_3x3(Matrix3x3 A, Matrix3x3 B);
Matrix3x3 MatrixInvert_3x3(Matrix3x3 M);
Frame2d CreateFrame2d(Basis2d basis, Vector2d origin);
Matrix3x3 FrameTransform_2d(Frame2d source, Frame2d destination);
Matrix3x3 FrameChainTransform_2d(Frame2d source, Frame2d middle, Frame2d destination);
Vector2d BasisTransform_2d_Scale(Basis2d source, Basis2d destination);
//bool IsPointInPolygon(Vector2d point, Vector2d *vertices, Vector2d vertice_offset, int vertice_count);
#endif

