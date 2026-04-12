/**********************************************************************************************
*
MEMORY MANAGEMENT MODULE
*
**********************************************************************************************/
#ifndef CVECTORS_H
#define CVECTORS_H
#include <stddef.h>

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct Vector2d {
    float x;
    float y;
} Vector2d;

typedef struct Basis2d {
    Vector2d u; // The "x-ish/i-ish" step
    Vector2d v; // The "y-ish/j-ish" step
} Basis2d;

typedef struct Polar2d {
    float magnitude;
    float radians;
} Polar2d;

typedef struct Transform2d {
    Vector2d u;      // X-Axis basis
    Vector2d v;      // Y-Axis basis
    Vector2d origin; // The (0,0) point in world coordinates
} Transform2d;

typedef struct Vector3d {
    float x;
    float y;
    float z;
} Vector3d;

typedef struct Basis3d {
    Vector3d basis_u; // The "x-ish/i-ish" step
    Vector3d basis_v; // The "y-ish/j-ish" step
    Vector3d basis_w; // The "z-ish/k-ish" step
} Basis3d;

typedef struct Matrix3x3 {
    float m0, m3, m6; // Row 1: BasisU.x, BasisV.x, Trans.x
    float m1, m4, m7; // Row 2: BasisU.y, BasisV.y, Trans.y
    float m2, m5, m8; // Row 3: 0, 0, 1
} Matrix3x3;

typedef struct Matrix2x2 {
    float m0, m3, m6; // Row 1: BasisU.x, BasisV.x, Trans.x
    float m1, m4, m7; // Row 2: BasisU.y, BasisV.y, Trans.y
} Matrix2x2;

//typedef struct Matrix2d {
//     DynamicArray *items; // The flat array of data
//     int cols;        // Number of columns
//     int rows;        // Number of rows
// } Matrix2d;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

Vector2d VectorSumArray_2d(Vector2d *array, size_t count);
Vector2d VectorSum_2d(Vector2d a, Vector2d b);
Vector2d* VectorSumArray_2d_Dynamic(Vector2d *array, size_t count);
Vector2d VectorScale_2d(Vector2d vector, float scalar);
Polar2d PolarForm_2d(Vector2d vector);
float VectorMagnitude_2d(Vector2d vector);
float Vector_2d_Dot(Vector2d a, Vector2d b);
float VectorRadians_2d(Vector2d vector);
Vector3d VectorSumArray_3d (Vector3d *array, size_t count);
Vector3d* VectorSumArray_3d_Dynamic(Vector3d *array, size_t count);
//Matrix3x3 VectorTransform_Scale_Rotate_2d(Vector2d origin, float scale_u, float scale_v, float radians_u, float radians_v);
Matrix3x3 MatrixMultiply_3x3_3x3(Matrix3x3 A, Matrix3x3 B);
Matrix3x3 MatrixInvert_3x3(Matrix3x3 M);
//For transforming coordinates from one basis to another, we can create a basis transform matrix that represents the transformation from the source basis to the destination basis, and then apply this matrix to the coordinates we want to transform. The destination origin is needed to account for any translation between the two bases.
//Specify (0,0) as the destination origin if the destination basis shares the same origin as the source basis.
Matrix3x3 BasisTransform_2d(Basis2d source, Basis2d destination, Vector2d destination_origin);
Vector2d MatrixMultiply_3x3_2x2(Matrix3x3 matrix_function, Vector2d vector_input);

#endif