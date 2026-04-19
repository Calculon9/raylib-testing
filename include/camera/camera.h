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
    Matrix3x3 source_to_dest_mtx;       // The calculated input-to-output space matrix
    Matrix3x3 dest_to_source_mtx;       // The calculated output-to-input space matrix
    Vector2d destination_origin_coords; // The destination spaces's origin coordinates (in dest. space units) => e.g. origin point of viewport in pixel coords
    Vector2d source_origin_coords;      // The source spaces's origin coordinates (in source space units) => e.g. origin point of local (or root, encoded, topological ... you get the idea) space in its defined coords
    Vector2d source_focal_coords;       // What source space coordinate is the camera looking at?
    Basis2d destination_basis;
    Basis2d source_basis;
    float rotation; // In radians
    float zoom;     // Scaling factor (e.g., 2.0f for 2x zoom)
} Camera2d;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
// extern World world_1;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Camera2d CreateCamera2d(Basis2d destination_basis, Basis2d source_basis, Vector2d destination_origin_coords, Vector2d source_origin_coords, float zoom, float rotation);
void UpdateCamera_Source_To_Dest(Camera2d *cam, Basis2d input_basis);
Vector2d TransformCoordinates(Matrix3x3 transformation_mtx, Vector2d coordinates_to_transform);

// Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis);
// Field UpdateFieldCellValues(Field field);

#endif