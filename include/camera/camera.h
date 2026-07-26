/**********************************************************************************************
 *
 CAMERA MODULE
 *
 **********************************************************************************************/
#ifndef CAMERA_H
#define CAMERA_H
#include "common/common.h"
#include "math/coordinate_space.h"

// Forward declaration - full definition included at end of file
typedef struct ViewportRegion ViewportRegion;

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct FrameTunnel
{
    Frame2d *source_frame;        // The Source coordinate frame
    Frame2d *destination_frame;   // The Destination coordinate frame
    Matrix3x3 source_to_dest_mtx; // Source-space coordinates mapped into destination space
    Matrix3x3 dest_to_source_mtx; // Destination-space coordinates mapped back into source space
} FrameTunnel;

// Camera is the adapter between a source coordinate space and a destination coordinate space.
typedef struct Camera2d
{
    Vector2d source_focus_coords; // Exactly where it is looking right now
    float zoom;                   // Exactly the zoom level right now
    float rotation;

    // Your framing and matrix data goes here
    Frame2d frame;
    FrameTunnel tunnel;
} Camera2d;

typedef struct CameraController
{
    Camera2d *camera; // The lens this controller is moving

    // Desired State (where it WANTS to be)
    Vector2d target_source_focus_coords; // Source-space point the camera is moving toward
    float target_zoom;

    // Tuning Variables (how it feels)
    float base_pan_speed; // Units per second
    float zoom_speed;     // Multiplier per scroll tick
    float pan_smoothing;  // How fast it lerps to target (e.g., 10.0f)
    float zoom_smoothing; // How fast it zooms to target

    // Constraints (where it's allowed to go)
    bool use_bounds;
    Vector2d bounds_min;
    Vector2d bounds_max;
} CameraController;

typedef struct CameraViewBox
{
    Vector2d origin;     // Top-left corner of the camera's view in source space
    Vector2d dimensions; // Width and height of the camera's view in source space
} CameraViewBox;
//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// Camera2d CreateCamera2d(Frame2d destination_frame, Frame2d source_frame);
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
CameraController CreateCameraController(Camera2d *camera);
void Controller_Pan(CameraController *cam_ctrl, Vector2d delta, float frame_time);
void Controller_Zoom(CameraController *ctrl, float zoom_factor);
void Controller_Rotate(CameraController *ctrl, float rotation_angle);
void Controller_FollowTarget(CameraController *ctrl, Vector2d target);
void Controller_Update(CameraController *ctrl);
void UpdateCameraFull(Camera2d *cam);
CameraViewBox GetCameraView(Camera2d *cam, ViewportRegion viewport, Matrix3x3 M_cam_to_pixel);
// Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis);
// Field UpdateFieldCellValues(Field field);

// Include full ViewportRegion definition at end to avoid circular dependency
#include "system/viewport_system.h"

#endif
