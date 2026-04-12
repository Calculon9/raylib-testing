/**********************************************************************************************
*
CAMERA MODULE
*
**********************************************************************************************/
#ifndef CAMERA_H
#define CAMERA_H
#include "common/common.h"
#include "math/coordinate_space.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Camera is the adapter between an input/source space and some other space, the target/output space. Converts the coordinates/vector from the input/source space to coordinates/vector in the target/output space.
typedef struct Camera2d
{
    Vector2d destination_origin_coords; // Where on the screen is the camera "target"? (e.g., center of screen)
    Vector2d source_coords; // What source/input space coordinate is the camera looking at?
    Basis2d destination_basis;
    Basis2d source_basis;
    float rotation;               // In radians
    float zoom;                   // Scaling factor (e.g., 2.0f for 2x zoom)
    Matrix3x3 transformation_mtx; // The calculated input-to-output space matrix
} Camera2d;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
// extern World world_1;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Camera2d CreateCamera2d(Basis2d target_basis, Basis2d source_basis, Vector2d target_origin_coords, float zoom, float rotation);
void UpdateCameraMatrix(Camera2d *cam, Basis2d input_basis);
Vector2d TransformCoordinates(Matrix3x3 transformation_mtx, Vector2d coordinates_to_transform);

// Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis);
// Field UpdateFieldCellValues(Field field);

#endif