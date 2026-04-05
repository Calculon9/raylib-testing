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
    Vector2d origin; // The (0,0) point in world pixels
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

Vector2d Vector2dSumArray(Vector2d *array, size_t count);
Vector2d* Vector2dSumArrayDynamic(Vector2d *array, size_t count);
Vector2d Vector2dScale(Vector2d vector, float scalar);
Polar2d GetPolarForm2d(Vector2d vector);
Vector3d Vector3dSumArray (Vector3d *array, size_t count);
Vector3d* Vector3dSumArrayDynamic(Vector3d *array, size_t count);

#endif