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

void UpdateCameraTransforms(Camera2d *cam)
{
    // Preserve where the camera pivot currently appears on screen.
    Vector2d pivot_dest_coords = cam->destination_origin_coords;
    if ((cam->source_to_dest_mtx.row1.x != 0.0f) || (cam->source_to_dest_mtx.row2.x != 0.0f) ||
        (cam->source_to_dest_mtx.row1.y != 0.0f) || (cam->source_to_dest_mtx.row2.y != 0.0f))
    {
        pivot_dest_coords = TransformCoordinates(cam->source_to_dest_mtx, cam->camera_coords);
    }

    // Calculate the reciprocal of zoom to fix the camera matrix paradox.
    // If cam->zoom is 2.0 (Zoom in), our world camera window needs to shrink by 0.5.
    float dest_scale = 1 / cam->zoom; // Correct scale factor calculation for zoom

    // Derive the rotated and scaled world-view basis vectors for the camera window.
    float cos_r = cosf(cam->rotation);
    float sin_r = sinf(cam->rotation);

    Basis2d resolved_source_basis = {
        .u = {cos_r * dest_scale, sin_r * dest_scale},
        .v = {-sin_r * dest_scale, cos_r * dest_scale}};
    cam->source_basis = resolved_source_basis;
    // Generate the absolute transform matrix.
    // We transform from our zoomed/rotated camera view space into our raw destination screen space.
    cam->source_to_dest_mtx = CoordSpaceTransform_2d(
        resolved_source_basis,
        cam->destination_basis,
        cam->destination_origin_coords);

    // Re-anchor translation so camera_coords remains the zoom/rotation pivot.
    cam->source_to_dest_mtx.row1.z = pivot_dest_coords.x -
                                     ((cam->camera_coords.x * cam->source_to_dest_mtx.row1.x) +
                                      (cam->camera_coords.y * cam->source_to_dest_mtx.row1.y));
    cam->source_to_dest_mtx.row2.z = pivot_dest_coords.y -
                                     ((cam->camera_coords.x * cam->source_to_dest_mtx.row2.x) +
                                      (cam->camera_coords.y * cam->source_to_dest_mtx.row2.y));

    // Always maintain the inverse matrix so you can click the screen
    // and find the matching world coordinates (Mouse picking!)
    cam->dest_to_source_mtx = MatrixInvert_3x3(cam->source_to_dest_mtx);
}

Vector2d TransformCoordinates(Matrix3x3 transformation_mtx, Vector2d coordinates_to_transform)
{
    Vector2d output_coords;

    // 1. Get the "transformation" or "mapping" basis to go from world to screen.
    // 2. Get the scaling factor to go from world basis magnitude to screen basis magnitude.

    // Since we are using a 3x  matrix for 2D, we treat the 2D point as a 3D vector where z=1. This is a trick called Homogeneous Coordinates that allows the matrix to move (translate) the point, not just rotate or scale it.
    //  Multiply: (Row 1 * WorldColumn)
    //  screenX = (row1.x * x) + (row1.y * y) + row1.z
    output_coords.x = (coordinates_to_transform.x * transformation_mtx.row1.x) + (coordinates_to_transform.y * transformation_mtx.row1.y) + transformation_mtx.row1.z;

    // Multiply: (Row 2 * WorldColumn)
    // screenY = (row2.x * x) + (row2.y * y) + row2.z
    output_coords.y = (coordinates_to_transform.x * transformation_mtx.row2.x) + (coordinates_to_transform.y * transformation_mtx.row2.y) + transformation_mtx.row2.z;

    return output_coords;
}
// World CalculateFieldLines(Field field);
// World InitialiseFieldCells(Field field);
