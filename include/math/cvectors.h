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

typedef struct Vector3d {
    float x;
    float y;
    float z;
} Vector3d;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

Vector2d Vector2dSumArray(Vector2d *array, size_t count);
Vector2d* Vector2dSumArrayDynamic(Vector2d *array, size_t count);
Vector3d Vector3dSumArray (Vector3d *array, size_t count);
Vector3d* Vector3dSumArrayDynamic(Vector3d *array, size_t count);

#endif