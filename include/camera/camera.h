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
typedef struct FrameTunnel
{
    Frame2d *source_frame;      // The Source coordinate frame
    Frame2d *destination_frame; // The Destination coordinate frame
    Matrix3x3 source_to_dest_mtx; // Source-space coordinates mapped into destination space
    Matrix3x3 dest_to_source_mtx; // Destination-space coordinates mapped back into source space
} FrameTunnel;

// Camera is the adapter between a source coordinate space and a destination coordinate space.
typedef struct Camera2d
{
    // Structural References (Points directly to the frames owned by your spaces)
    Frame2d frame;
    FrameTunnel tunnel; // The horizontal link between the two frames

    // Gameplay / Logic State
    Vector2d source_focus_coords;        // Source-space point the camera is currently centered on
    Vector2d target_source_focus_coords; // Source-space point the camera is moving toward
    float rotation;                      // In radians
    float zoom;                          // Scaling factor (e.g., 2.0f for 2x zoom)
    float target_zoom;
} Camera2d;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
//Camera2d CreateCamera2d(Frame2d destination_frame, Frame2d source_frame);
Camera2d CreateCamera2d(Frame2d *source_space_frame, Frame2d *destination_space_frame);
void Camera_SetDestinationFrame(Camera2d *cam, Frame2d *destination_frame);
void Camera_SetSourceFrame(Camera2d *cam, Frame2d *source_frame);
void Tunnel_SetSourceFrame(FrameTunnel *tunnel, Frame2d *source_space_frame);
void Tunnel_SetDestinationFrame(FrameTunnel *tunnel, Frame2d *destination_space_frame);
Basis2d Camera_GetSourceBasis(const Camera2d *cam);
Vector2d Camera_GetBasisScale(const Camera2d *cam);
Vector2d Camera_GetSourceOriginInDestination(const Camera2d *cam);
Vector2d Camera_GetDestinationOriginInSource(const Camera2d *cam);
Vector2d TransformCoordinates(Matrix3x3 transformation_mtx, Vector2d coordinates_to_transform);
void ZoomCamera(Camera2d *cam, float zoom_factor);
void RotateCamera(Camera2d *cam, float rotation_angle);
void PanCamera(Camera2d *cam, Vector2d delta);
void UpdateCameraFull(Camera2d *cam);
void UpdateCameraSmoothingTick(Camera2d *cam);
void FollowTarget(Camera2d *cam, Vector2d source_focus_coords);
// Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis);
// Field UpdateFieldCellValues(Field field);

#endif
