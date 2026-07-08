/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/
#include "common/common.h"
#include "camera/camera.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
Camera2d CreateCamera2d(Basis2d destination_basis, Basis2d source_basis, Vector2d destination_origin_coords, Vector2d source_origin_coords, float zoom, float rotation)
{
    Camera2d camera = {0};
    camera.destination_basis = destination_basis;
    camera.source_basis = source_basis;
    camera.destination_origin_coords = destination_origin_coords;
    camera.source_origin_coords = source_origin_coords;
    camera.zoom = zoom;
    camera.rotation = rotation;

    // Calculate the transformation matrix that will be used to translate any world space to screen space
    camera.source_to_dest_mtx = CoordSpaceTransform_2d(source_basis, destination_basis, destination_origin_coords);
    camera.dest_to_source_mtx = MatrixInvert_3x3(camera.source_to_dest_mtx);
    // camera.dest_to_source_mtx = CoordSpaceTransform_2d(destination_basis, source_basis, source_origin_coords);
    return camera;
}

void ZoomCamera(Camera2d *cam, float zoom_factor)
{
    cam->zoom *= zoom_factor;
    UpdateCameraTransforms(cam);
}

void RotateCamera(Camera2d *cam, float rotation_angle)
{
    cam->rotation += rotation_angle;
    UpdateCameraTransforms(cam);
}

Trying to separate the pan update so there is no need to recalculate the source basis and transform matrix when panning, since panning only affects the camera's position in world space, not its orientation or scale.
UpdateCameraTransforms_Pan(Camera2d *cam, Vector2d pan_delta)
{
    // Calculate the reciprocal of zoom to fix the camera matrix paradox.
    float dest_scale = 1.0f / cam->zoom; 

    // Derive the rotated and scaled world-view basis vectors for the camera window.
    float cos_r = cosf(cam->rotation);
    float sin_r = sinf(cam->rotation);

    Basis2d resolved_source_basis = {
        .u = { cos_r * dest_scale, sin_r * dest_scale},
        .v = {-sin_r * dest_scale, cos_r * dest_scale}
    };
    cam->source_basis = resolved_source_basis;

    // Generate the initial transform matrix from source basis to destination basis
    cam->source_to_dest_mtx = CoordSpaceTransform_2d(
        resolved_source_basis,
        cam->destination_basis,
        cam->destination_origin_coords
    );

    // Re-anchor translation using the explicit, non-mutating screen destination target
    // This perfectly projects the camera's world position into your screen space baseline
    cam->source_to_dest_mtx.col3.x = cam->destination_origin_coords.x -
                                     ((cam->camera_coords.x * cam->source_to_dest_mtx.col1.x) +
                                      (cam->camera_coords.y * cam->source_to_dest_mtx.col2.x));
                                      
    cam->source_to_dest_mtx.col3.y = cam->destination_origin_coords.y -
                                     ((cam->camera_coords.x * cam->source_to_dest_mtx.col1.y) +
                                      (cam->camera_coords.y * cam->source_to_dest_mtx.col2.y));

    // Always maintain the inverse matrix for flawless pixel-to-world mouse picking
    cam->dest_to_source_mtx = MatrixInvert_3x3(cam->source_to_dest_mtx);
}

void UpdateCameraTransforms(Camera2d *cam)
{
    // Calculate the reciprocal of zoom to fix the camera matrix paradox.
    float dest_scale = 1.0f / cam->zoom; 

    // Derive the rotated and scaled world-view basis vectors for the camera window.
    float cos_r = cosf(cam->rotation);
    float sin_r = sinf(cam->rotation);

    Basis2d resolved_source_basis = {
        .u = { cos_r * dest_scale, sin_r * dest_scale},
        .v = {-sin_r * dest_scale, cos_r * dest_scale}
    };
    cam->source_basis = resolved_source_basis;

    // Generate the initial transform matrix from source basis to destination basis
    cam->source_to_dest_mtx = CoordSpaceTransform_2d(
        resolved_source_basis,
        cam->destination_basis,
        cam->destination_origin_coords
    );

    // Re-anchor translation using the explicit, non-mutating screen destination target
    // This perfectly projects the camera's world position into your screen space baseline
    cam->source_to_dest_mtx.col3.x = cam->destination_origin_coords.x -
                                     ((cam->camera_coords.x * cam->source_to_dest_mtx.col1.x) +
                                      (cam->camera_coords.y * cam->source_to_dest_mtx.col2.x));
                                      
    cam->source_to_dest_mtx.col3.y = cam->destination_origin_coords.y -
                                     ((cam->camera_coords.x * cam->source_to_dest_mtx.col1.y) +
                                      (cam->camera_coords.y * cam->source_to_dest_mtx.col2.y));

    // Always maintain the inverse matrix for flawless pixel-to-world mouse picking
    cam->dest_to_source_mtx = MatrixInvert_3x3(cam->source_to_dest_mtx);
}

Vector2d TransformCoordinates(Matrix3x3 transformation_mtx, Vector2d coordinates_to_transform)
{
    Vector2d output_coords;

    // 1. Get the "transformation" or "mapping" basis to go from world to screen.
    // 2. Get the scaling factor to go from world basis magnitude to screen basis magnitude.

    // Since we are using a 3x matrix for 2D, treat point as (x, y, 1).
    output_coords.x = (coordinates_to_transform.x * transformation_mtx.col1.x) + (coordinates_to_transform.y * transformation_mtx.col2.x) + transformation_mtx.col3.x;
    output_coords.y = (coordinates_to_transform.x * transformation_mtx.col1.y) + (coordinates_to_transform.y * transformation_mtx.col2.y) + transformation_mtx.col3.y;

    return output_coords;
}
// World CalculateFieldLines(Field field);
// World InitialiseFieldCells(Field field);
