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

// Camera is the adapter between a source coordinate space and a destination coordinate space.
typedef struct Camera2d
{
    Matrix3x3 source_to_dest_mtx;             // Source-space coordinates mapped into destination space
    Matrix3x3 dest_to_source_mtx;             // Destination-space coordinates mapped back into source space
    Vector2d destination_space_origin_coords;  // Destination-space origin in destination units
    Vector2d source_space_origin_coords;       // Source-space origin in source units
    Vector2d source_focus_coords;              // Source-space point the camera is currently centered on
    Vector2d target_source_focus_coords;       // Source-space point the camera is moving toward
    Basis2d destination_basis;
    Basis2d source_basis;
    float rotation; // In radians
    float target_rotation;
    float zoom;     // Scaling factor (e.g., 2.0f for 2x zoom)
    float target_zoom;
} Camera2d;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Camera2d CreateCamera2d(Basis2d destination_basis, Basis2d source_basis, Vector2d destination_space_origin_coords, Vector2d source_space_origin_coords);
Vector2d TransformCoordinates(Matrix3x3 transformation_mtx, Vector2d coordinates_to_transform);
void ZoomCamera(Camera2d *cam, float zoom_factor);
void RotateCamera(Camera2d *cam, float rotation_angle);
void PanCamera(Camera2d *cam, Vector2d delta);
void UpdateCameraFull(Camera2d *cam);
void UpdateCameraSmoothingTick(Camera2d *cam);
// Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis);
// Field UpdateFieldCellValues(Field field);

#endif
