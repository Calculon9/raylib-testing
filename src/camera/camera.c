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
Camera2d CreateCamera2d(Basis2d destination_basis, Basis2d source_basis, Vector2d destination_space_origin_coords, Vector2d source_space_origin_coords)
{
    Camera2d camera = {0};
    camera.destination_basis = destination_basis;
    camera.source_basis = source_basis;
    camera.destination_space_origin_coords = destination_space_origin_coords;
    camera.source_space_origin_coords = source_space_origin_coords;
    camera.zoom = 1.0f;
    camera.target_zoom = 1.0f;
    camera.rotation = 0.0f;
    camera.target_rotation = 0.0f;
    camera.source_focus_coords = source_space_origin_coords;
    camera.target_source_focus_coords = source_space_origin_coords;

    CoordSystem2d source_system = CreateCoordSystem2d(source_basis, source_space_origin_coords);
    CoordSystem2d destination_system = CreateCoordSystem2d(destination_basis, destination_space_origin_coords);

    // Calculate the source-to-destination transform for the two coordinate spaces.
    camera.source_to_dest_mtx = CoordSystemTransform_2d(source_system, destination_system);
    camera.dest_to_source_mtx = MatrixInvert_3x3(camera.source_to_dest_mtx);
    return camera;
}

void PanCamera(Camera2d *cam, Vector2d delta)
{
    // Simply move the target ahead. The smooth loop will handle the rest.
    cam->target_source_focus_coords = VectorSum_2d(cam->target_source_focus_coords, delta);
}

void FollowTarget(Camera2d *cam, Vector2d source_focus_coords)
{
    // If tracking, snap the camera's target directly to them
    cam->target_source_focus_coords = source_focus_coords;
}

void ZoomCamera(Camera2d *cam, float zoom_factor)
{
    cam->target_zoom *= zoom_factor;
    UpdateCameraFull(cam); // Needs full re-bake because basis changes
}

void RotateCamera(Camera2d *cam, float rotation_angle)
{
    cam->rotation += rotation_angle;
    UpdateCameraFull(cam); // Needs full re-bake because basis changes
}

// This handles the smooth catch-up and re-anchors the translation columns.
void UpdateCameraSmoothingTick(Camera2d *cam)
{
    // Smoothly slide 15% of the remaining distance toward the target destination
    // Track if we need to completely rebuild the basis vectors this frame
    bool basis_changed = false;
    bool pan_changed = false;
    // Smoothly interpolate zoom scalar (using an epsilon check to prevent endless micro-math)
    if (fabsf(cam->target_zoom - cam->zoom) > 0.01f)
    {
        cam->zoom += (cam->target_zoom - cam->zoom) * 0.15f;
        basis_changed = true;
    }
    else
    {
        cam->zoom = cam->target_zoom; // Snap to target when incredibly close
    }

    // Smoothly interpolate position vectors
    if (cam->target_source_focus_coords.x != cam->source_focus_coords.x || cam->target_source_focus_coords.y != cam->source_focus_coords.y)
    {
        cam->source_focus_coords.x += (cam->target_source_focus_coords.x - cam->source_focus_coords.x) * 0.15f;
        cam->source_focus_coords.y += (cam->target_source_focus_coords.y - cam->source_focus_coords.y) * 0.15f;
        pan_changed = true;
    }

    // Re-bake Matrix based on what changed
    if (basis_changed)
    {
        // Zoom changed! We must completely recalculate the source basis vectors
        float dest_scale = 1.0f / cam->zoom;
        float cos_r = cosf(cam->rotation);
        float sin_r = sinf(cam->rotation);

        Basis2d resolved_source_basis = {
            .u = {cos_r * dest_scale, sin_r * dest_scale},
            .v = {-sin_r * dest_scale, cos_r * dest_scale}};
        cam->source_basis = resolved_source_basis;

        // Rebuild the source-to-destination mapping with the updated source focus.
        CoordSystem2d source_system = CreateCoordSystem2d(resolved_source_basis, cam->source_focus_coords);
        CoordSystem2d destination_system = CreateCoordSystem2d(cam->destination_basis, cam->destination_space_origin_coords);
        cam->source_to_dest_mtx = CoordSystemTransform_2d(source_system, destination_system);
    }

    // Always apply the current frame's smoothed camera position to translation columns
    if (pan_changed)
    {
        cam->source_to_dest_mtx.col3.x = cam->destination_space_origin_coords.x -
                                         ((cam->source_focus_coords.x * cam->source_to_dest_mtx.col1.x) +
                                          (cam->source_focus_coords.y * cam->source_to_dest_mtx.col2.x));

        cam->source_to_dest_mtx.col3.y = cam->destination_space_origin_coords.y -
                                         ((cam->source_focus_coords.x * cam->source_to_dest_mtx.col1.y) +
                                          (cam->source_focus_coords.y * cam->source_to_dest_mtx.col2.y));
    }

    // Keep inverse matrix updated for pixel picking
    if (basis_changed || pan_changed)
    {
        cam->dest_to_source_mtx = MatrixInvert_3x3(cam->source_to_dest_mtx);
    }
}

void UpdateCameraFull(Camera2d *cam)
{
    // Calculate the reciprocal of zoom to fix the camera matrix paradox.
    float dest_scale = 1.0f / cam->zoom;

    // Derive the rotated and scaled world-view basis vectors for the camera window.
    float cos_r = cosf(cam->rotation);
    float sin_r = sinf(cam->rotation);

    Basis2d resolved_source_basis = {
        .u = {cos_r * dest_scale, sin_r * dest_scale},
        .v = {-sin_r * dest_scale, cos_r * dest_scale}};
    cam->source_basis = resolved_source_basis;

    // Generate the source-to-destination transform from the updated camera basis and focus.
    CoordSystem2d source_system = CreateCoordSystem2d(resolved_source_basis, cam->source_focus_coords);
    CoordSystem2d destination_system = CreateCoordSystem2d(cam->destination_basis, cam->destination_space_origin_coords);
    cam->source_to_dest_mtx = CoordSystemTransform_2d(source_system, destination_system);

    // Re-anchor translation using the destination-space origin so the current source focus lands there.
    cam->source_to_dest_mtx.col3.x = cam->destination_space_origin_coords.x -
                                     ((cam->source_focus_coords.x * cam->source_to_dest_mtx.col1.x) +
                                      (cam->source_focus_coords.y * cam->source_to_dest_mtx.col2.x));

    cam->source_to_dest_mtx.col3.y = cam->destination_space_origin_coords.y -
                                     ((cam->source_focus_coords.x * cam->source_to_dest_mtx.col1.y) +
                                      (cam->source_focus_coords.y * cam->source_to_dest_mtx.col2.y));

    // Always maintain the inverse matrix for pixel-to-source picking.
    cam->dest_to_source_mtx = MatrixInvert_3x3(cam->source_to_dest_mtx);
}

Vector2d TransformCoordinates(Matrix3x3 transformation_mtx, Vector2d coordinates_to_transform)
{
    Vector2d output_coords;

    // Apply the 2D affine transform stored in column form.

    // Since we are using a 3x matrix for 2D, treat point as (x, y, 1).
    output_coords.x = (coordinates_to_transform.x * transformation_mtx.col1.x) + (coordinates_to_transform.y * transformation_mtx.col2.x) + transformation_mtx.col3.x;
    output_coords.y = (coordinates_to_transform.x * transformation_mtx.col1.y) + (coordinates_to_transform.y * transformation_mtx.col2.y) + transformation_mtx.col3.y;

    return output_coords;
}

