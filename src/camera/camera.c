/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/
#include "common/common.h"
#include "camera/camera.h"
#include "math/affine_space_ops.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
// static Basis2d BuildSourceBasisFromView(const Camera2d *cam)
// {
//     if (!cam)
//     {
//         return IDENTITY_BASIS_2D;
//     }

//     float source_scale = 1.0f / cam->zoom;
//     float cos_r = cosf(cam->rotation);
//     float sin_r = sinf(cam->rotation);

//     return (Basis2d){
//         .u = {cos_r * source_scale, sin_r * source_scale},
//         .v = {-sin_r * source_scale, cos_r * source_scale}};
// }

// static Matrix3x3 BuildSourceToDestinationMatrix(Frame2d source_frame, Frame2d destination_frame)
// {
//     Matrix3x3 source_to_destination = CalcTransform_FromFrame(source_frame, destination_frame);
//     return MatrixInvert_3x3(source_to_destination);
// }

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
// Requires passing the address of the actual frames managed by your Space2d instances
Camera2d CreateCamera2d(Frame2d *source_space_frame, Frame2d *destination_space_frame)
{
    Camera2d camera = {0};
    camera.tunnel.source_frame = source_space_frame;
    camera.tunnel.destination_frame = destination_space_frame;

    // Derived baseline logic straight from the frames
    if (source_space_frame)
    {
        float source_scale = VectorMagnitude_2d(source_space_frame->basis.u);

        // Match the direct proportional zoom scale setup used in UpdateCameraFull
        camera.zoom = source_scale > 0.0001f ? source_scale : 1.0f;

        camera.rotation = VectorRadians_2d(source_space_frame->basis.u);
        camera.source_focus_coords = source_space_frame->origin_in_parent;
    }
    else
    {
        camera.zoom = 1.0f;
    }

    // Safely bake initial column-vector matrices using unified pipeline function
    UpdateCameraFull(&camera);

    return camera;
}

void Camera_SetDestinationFrame(Camera2d *cam, Frame2d *destination_space_frame)
{
    if (!cam)
        return;
    Tunnel_SetDestinationFrame(&cam->tunnel, destination_space_frame);
    UpdateCameraFull(cam);
}

void Tunnel_SetDestinationFrame(FrameTunnel *tunnel, Frame2d *destination_space_frame)
{
    if (!tunnel)
        return;
    tunnel->destination_frame = destination_space_frame;
}

void Camera_SetSourceFrame(Camera2d *cam, Frame2d *source_space_frame)
{
    if (!cam || !source_space_frame)
        return;

    // Assign the reference tracking link safely inside the tunnel
    Tunnel_SetSourceFrame(&cam->tunnel, source_space_frame);

    // Avoid circular self-extraction if setting its own internal frame
    // =========================================================================
    if (source_space_frame == &cam->frame)
    {
        // It's already viewing itself! Just rebuild matrices using existing states.
        UpdateCameraFull(cam);
        return;
    }

    // If viewing a completely separate external frame, extract its properties proportionally
    float source_scale = (float)VectorMagnitude_2d(source_space_frame->basis.u);
    if (source_scale > 0.0001f)
    {
        cam->zoom = source_scale;
    }

    cam->rotation = (float)VectorRadians_2d(source_space_frame->basis.u);
    cam->source_focus_coords = source_space_frame->origin_in_parent;

    // Rebake the column-vector matrices
    UpdateCameraFull(cam);
}

void Tunnel_SetSourceFrame(FrameTunnel *tunnel, Frame2d *source_space_frame)
{
    if (!tunnel)
        return;
    tunnel->source_frame = source_space_frame;
}

Basis2d Camera_GetSourceBasis(const Camera2d *cam)
{
    if (!cam || !cam->tunnel.source_frame)
        return IDENTITY_BASIS_2D;
    return cam->tunnel.source_frame->basis;
}

Vector2d Camera_GetBasisScale(const Camera2d *cam)
{
    if (!cam || !cam->tunnel.source_frame || !cam->tunnel.destination_frame)
        return ZERO_VECTOR_2D;
    return Frame_GetBasisScaling(cam->tunnel.source_frame->basis, cam->tunnel.destination_frame->basis);
}

Vector2d Camera_GetSourceOriginInDestination(const Camera2d *cam)
{
    if (!cam)
        return ZERO_VECTOR_2D;
    return (Vector2d){cam->tunnel.source_to_dest_mtx.col3.x, cam->tunnel.source_to_dest_mtx.col3.y};
}

Vector2d Camera_GetDestinationOriginInSource(const Camera2d *cam)
{
    if (!cam)
        return ZERO_VECTOR_2D;
    return (Vector2d){cam->tunnel.dest_to_source_mtx.col3.x, cam->tunnel.dest_to_source_mtx.col3.y};
}

CameraController CreateCameraController(Camera2d *camera)
{
    CameraController ctrl = {0};
    ctrl.camera = camera;
    if (camera)
    {
        ctrl.target_source_focus_coords = camera->source_focus_coords;
        ctrl.target_zoom = camera->zoom;
    }
    else
    {
        ctrl.target_zoom = 1.0f;
    }
    // Set default tuning values for smooth camera control
    ctrl.pan_smoothing = 0.15f;  // px/frame
    ctrl.zoom_smoothing = 0.15f; // px/frame
    ctrl.zoom_speed = 1.1f;
    ctrl.base_pan_speed = 10.0f; // px/frame
    return ctrl;
}

void Controller_Pan(CameraController *cam_ctrl, Vector2d delta, float frame_time)
{
    if (!cam_ctrl)
        return;

    // Rotate the screen-space pan_delta into world-space movement
    Basis2d basis = Basis_BuildFromRotationScale(cam_ctrl->camera->rotation, (Vector2d){1.0f, 1.0f});
    float world_dx = delta.x * basis.u.x - delta.y * basis.u.y;
    float world_dy = delta.x * basis.v.x - delta.y * basis.v.y;

    // Apply the rotated delta
    cam_ctrl->target_source_focus_coords.x += world_dx * frame_time;
    cam_ctrl->target_source_focus_coords.y += world_dy * frame_time;
}

void Controller_FollowTarget(CameraController *ctrl, Vector2d target)
{
    if (!ctrl)
        return;
    ctrl->target_source_focus_coords = target;
}

void Controller_Zoom(CameraController *ctrl, float zoom_factor)
{
    if (!ctrl)
        return;
    ctrl->target_zoom *= zoom_factor;
}

void Controller_Rotate(CameraController *ctrl, float rotation_angle)
{
    if (!ctrl || !ctrl->camera)
        return;
    ctrl->camera->rotation += rotation_angle;
    UpdateCameraFull(ctrl->camera);
}

void Controller_Update(CameraController *ctrl)
{
    if (!ctrl || !ctrl->camera)
        return;
    Camera2d *cam = ctrl->camera;
    if (!cam->tunnel.source_frame || !cam->tunnel.destination_frame)
        return;

    bool state_changed = false;

    // Smoothly interpolate zoom
    float zoom_delta = ctrl->target_zoom - cam->zoom;
    if (fabsf(zoom_delta) > 0.001f)
    {
        cam->zoom += zoom_delta * ctrl->zoom_smoothing;
        state_changed = true;
    }
    else if (cam->zoom != ctrl->target_zoom)
    {
        cam->zoom = ctrl->target_zoom;
        state_changed = true;
    }

    // Smoothly interpolate position
    float dx = ctrl->target_source_focus_coords.x - cam->source_focus_coords.x;
    float dy = ctrl->target_source_focus_coords.y - cam->source_focus_coords.y;
    if (fabsf(dx) > 0.01f || fabsf(dy) > 0.01f)
    {
        cam->source_focus_coords.x += dx * ctrl->pan_smoothing;
        cam->source_focus_coords.y += dy * ctrl->pan_smoothing;
        state_changed = true;
    }
    else if (cam->source_focus_coords.x != ctrl->target_source_focus_coords.x ||
             cam->source_focus_coords.y != ctrl->target_source_focus_coords.y)
    {
        cam->source_focus_coords = ctrl->target_source_focus_coords;
        state_changed = true;
    }

    if (state_changed)
        UpdateCameraFull(cam);
}

void UpdateCameraFull(Camera2d *cam)
{
    if (!cam || !cam->tunnel.source_frame || !cam->tunnel.destination_frame)
        return;

    // Establish frame metrics cleanly. Keep zoom proportional.
    Basis2d basis = Basis_BuildFromRotationScale(cam->rotation, (Vector2d){cam->zoom, cam->zoom});

    // Sync the gameplay tracking variables straight into the source frame struct
    cam->tunnel.source_frame->origin_in_parent = cam->source_focus_coords;
    cam->tunnel.source_frame->basis = basis;

    // Build the Camera View Space Transform Matrix manually
    // This shifts world points so that target_focus coordinates land exactly at (0,0)
    Matrix3x3 M_cam_view;

    // X/Y Basis Columns (Rotation + Scale)
    M_cam_view.col1 = (Vector3d){basis.u.x, basis.u.y, 0.0f};
    M_cam_view.col2 = (Vector3d){basis.v.x, basis.v.y, 0.0f};

    // Translation Column (inverse focus translation projected onto the basis)
    M_cam_view.col3.x = -cam->source_focus_coords.x * basis.u.x - cam->source_focus_coords.y * basis.v.x;
    M_cam_view.col3.y = -cam->source_focus_coords.x * basis.u.y - cam->source_focus_coords.y * basis.v.y;
    M_cam_view.col3.z = 1.0f;

    // Build Viewport Center Matrix
    // Ensures (0,0) from the camera view lands dead-center in the destination
    Vector2d view_center = {
        (cam->tunnel.source_frame->local_max.x - cam->tunnel.source_frame->local_min.x) * 0.5f,
        (cam->tunnel.source_frame->local_max.y - cam->tunnel.source_frame->local_min.y) * 0.5f};

    Matrix3x3 M_view_center;
    M_view_center.col1 = (Vector3d){1.0f, 0.0f, 0.0f};
    M_view_center.col2 = (Vector3d){0.0f, 1.0f, 0.0f};
    M_view_center.col3 = (Vector3d){view_center.x, view_center.y, 1.0f};

    // Combine and invert
    cam->tunnel.source_to_dest_mtx = MatrixMultiply_3x3_3x3(M_view_center, M_cam_view);
    cam->tunnel.dest_to_source_mtx = MatrixInvert_3x3(cam->tunnel.source_to_dest_mtx);

    // Combine: M_total = M_view_center * M_cam_view
    // Vectors are transformed right-to-left: v_pixel = M_view_center * (M_cam_view * v_world)
    // cam->tunnel.source_to_dest_mtx = M_cam_view;
    // cam->tunnel.source_to_dest_mtx = MatrixMultiply_3x3_3x3(M_view_center, M_cam_view);
    // cam->tunnel.dest_to_source_mtx = MatrixInvert_3x3(cam->tunnel.source_to_dest_mtx);
}

// This function calculates the camera's view box in coordinates based on the provided transformation.
// The viewport region constrains the area of interest, and the transformation matrix maps camera coordinates to pixel coordinates. The function returns a CameraViewBox struct containing the origin and dimensions of the camera's view in source space.
CameraViewBox GetCameraView(Camera2d *cam, ViewportRegion viewport, Matrix3x3 M_cam_to_pixel)
{
    Matrix3x3 M_pixel_to_world = MatrixInvert_3x3(M_cam_to_pixel);

    // Using your exact logic from the grid function:
    Vector2d viewport_coords[] = {
        TransformCoordinates(M_pixel_to_world, viewport.pixel_origin),
        TransformCoordinates(M_pixel_to_world, (Vector2d){(float)viewport.pixel_end.x, viewport.pixel_origin.y}),
        TransformCoordinates(M_pixel_to_world, (Vector2d){viewport.pixel_origin.x, (float)viewport.pixel_end.y}),
        TransformCoordinates(M_pixel_to_world, viewport.pixel_end) // First corner calculation
    };

    Matrix2x2 aabb = CalcAABBCoords_Tight(viewport_coords, 4, ZERO_VECTOR_2D);

    CameraViewBox bounds;
    bounds.origin = (Vector2d){aabb.col1.x, aabb.col1.y};
    bounds.dimensions = (Vector2d){aabb.col2.x - aabb.col1.x, aabb.col2.y - aabb.col1.y};
    return bounds;
}

Vector2d TransformCoordinates(Matrix3x3 transformation_mtx, Vector2d coordinates_to_transform)
{
    return MatrixMultiply_3x3_Vector2d(transformation_mtx, coordinates_to_transform);
}
// Camera2d CreateCamera2d(Frame2d destination_frame, Frame2d source_frame)
// {
//     Camera2d camera = {0};
//     camera.destination_origin = destination_frame.origin_in_parent;
//     camera.destination_basis = destination_frame.basis;
//     float source_scale = VectorMagnitude_2d(source_frame.basis.u);
//     camera.zoom = source_scale > 0.0001f ? (1.0f / source_scale) : 1.0f;
//     camera.target_zoom = camera.zoom;
//     camera.rotation = VectorRadians_2d(source_frame.basis.u);
//     camera.source_basis = source_frame.basis;
//     camera.source_focus_coords = source_frame.origin_in_parent;
//     camera.target_source_focus_coords = source_frame.origin_in_parent;

//     // Calculate the source-to-destination transform for the two coordinate spaces.
//     camera.source_to_dest_mtx = MtxTransform_GetLocalToParent(source_frame);
//     camera.dest_to_source_mtx = MatrixInvert_3x3(camera.source_to_dest_mtx);
//     return camera;
// }

// void Camera_SetDestinationFrame(Camera2d *cam, Frame2d destination_frame)
// {
//     if (!cam)
//     {
//         return;
//     }

//     cam->destination_basis = destination_frame.basis;
//     cam->destination_origin = destination_frame.origin_in_parent;
//     UpdateCameraFull(cam);
// }

// void Camera_SetSourceFrame(Camera2d *cam, Frame2d source_frame)
// {
//     if (!cam)
//     {
//         return;
//     }

//     // Source frame controls the focus anchor and implied zoom/rotation basis context.
//     float source_scale = VectorMagnitude_2d(source_frame.basis.u);
//     if (source_scale > 0.0001f)
//     {
//         cam->zoom = 1.0f / source_scale;
//         cam->target_zoom = cam->zoom;
//     }
//     cam->rotation = VectorRadians_2d(source_frame.basis.u);
//     cam->source_basis = source_frame.basis;
//     cam->source_focus_coords = source_frame.origin_in_parent;
//     cam->target_source_focus_coords = source_frame.origin_in_parent;
//     UpdateCameraFull(cam);
// }

// Basis2d Camera_GetSourceBasis(const Camera2d *cam)
// {
//     if (!cam)
//     {
//         return IDENTITY_BASIS_2D;
//     }

//     return cam->source_basis;
// }

// Vector2d Camera_GetBasisScale(const Camera2d *cam)
// {
//     if (!cam)
//     {
//         return ZERO_VECTOR_2D;
//     }

//     Basis2d source_basis = Camera_GetSourceBasis(cam);
//     return Frame_GetBasisScaling(source_basis, cam->destination_basis);
// }

// Vector2d Camera_GetSourceOriginInDestination(const Camera2d *cam)
// {
//     if (!cam)
//     {
//         return ZERO_VECTOR_2D;
//     }

//     return (Vector2d){cam->source_to_dest_mtx.col3.x, cam->source_to_dest_mtx.col3.y};
// }

// Vector2d Camera_GetDestinationOriginInSource(const Camera2d *cam)
// {
//     if (!cam)
//     {
//         return ZERO_VECTOR_2D;
//     }

//     return (Vector2d){cam->dest_to_source_mtx.col3.x, cam->dest_to_source_mtx.col3.y};
// }

// void PanCamera(Camera2d *cam, Vector2d delta)
// {
//     // Simply move the target ahead. The smooth loop will handle the rest.
//     cam->target_source_focus_coords = VectorSum_2d(cam->target_source_focus_coords, delta);
// }

// void FollowTarget(Camera2d *cam, Vector2d source_focus_coords)
// {
//     // If tracking, snap the camera's target directly to them
//     cam->target_source_focus_coords = source_focus_coords;
// }

// void ZoomCamera(Camera2d *cam, float zoom_factor)
// {
//     cam->target_zoom *= zoom_factor;
//     UpdateCameraFull(cam); // Needs full re-bake because basis changes
// }

// void RotateCamera(Camera2d *cam, float rotation_angle)
// {
//     cam->rotation += rotation_angle;
//     UpdateCameraFull(cam); // Needs full re-bake because basis changes
// }

// // This handles the smooth catch-up and re-anchors the translation columns.
// void UpdateCameraSmoothingTick(Camera2d *cam)
// {
//     // Smoothly slide 15% of the remaining distance toward the target destination
//     // Track if we need to completely rebuild the basis vectors this frame
//     bool basis_changed = false;
//     bool pan_changed = false;
//     // Smoothly interpolate zoom scalar (using an epsilon check to prevent endless micro-math)
//     if (fabsf(cam->target_zoom - cam->zoom) > 0.01f)
//     {
//         cam->zoom += (cam->target_zoom - cam->zoom) * 0.15f;
//         basis_changed = true;
//     }
//     else
//     {
//         cam->zoom = cam->target_zoom; // Snap to target when incredibly close
//     }

//     // Smoothly interpolate position vectors
//     if (cam->target_source_focus_coords.x != cam->source_focus_coords.x || cam->target_source_focus_coords.y != cam->source_focus_coords.y)
//     {
//         cam->source_focus_coords.x += (cam->target_source_focus_coords.x - cam->source_focus_coords.x) * 0.15f;
//         cam->source_focus_coords.y += (cam->target_source_focus_coords.y - cam->source_focus_coords.y) * 0.15f;
//         pan_changed = true;
//     }

//     // Re-bake Matrix based on what changed
//     if (basis_changed)
//     {
//         // cam->source_basis = Basis_BuildLocalToParent(cam->source_focus_coords, cam->rotation, (Vector2d){1.0f / cam->zoom, 1.0f / cam->zoom});
//         // Frame2d source_frame = CreateFrame2d(cam->source_basis, cam->destination_origin, ZERO_VECTOR_2D);
//         // Frame2d destination_frame = CreateFrame2d(cam->destination_basis, cam->destination_origin, ZERO_VECTOR_2D);

//         // Rebuild the source-to-destination mapping with the updated source focus.
//         cam->source_to_dest_mtx = MtxTransform_BuildLocalToParent(cam->source_focus_coords, cam->rotation, (Vector2d){1.0f / cam->zoom, 1.0f / cam->zoom});
//         cam->source_basis = (Basis2d){.u = {cam->source_to_dest_mtx.col1.x, cam->source_to_dest_mtx.col1.y},
//                                       .v = {cam->source_to_dest_mtx.col2.x, cam->source_to_dest_mtx.col2.y}};
//     }

//     // Always apply the current frame's smoothed camera position to translation columns
//     if (pan_changed)
//     {
//         cam->source_to_dest_mtx.col3.x = cam->destination_origin.x -
//                                          ((cam->source_focus_coords.x * cam->source_to_dest_mtx.col1.x) +
//                                           (cam->source_focus_coords.y * cam->source_to_dest_mtx.col2.x));

//         cam->source_to_dest_mtx.col3.y = cam->destination_origin.y -
//                                          ((cam->source_focus_coords.x * cam->source_to_dest_mtx.col1.y) +
//                                           (cam->source_focus_coords.y * cam->source_to_dest_mtx.col2.y));
//     }

//     // Keep inverse matrix updated for pixel picking
//     if (basis_changed || pan_changed)
//     {
//         cam->dest_to_source_mtx = MatrixInvert_3x3(cam->source_to_dest_mtx);
//     }
// }

// void UpdateCameraFull(Camera2d *cam)
// {
//     // cam->source_basis = BuildSourceBasisFromView(cam);
//     // Frame2d source_frame = CreateFrame2d(cam->source_basis, cam->source_focus_coords, ZERO_VECTOR_2D);
//     // Frame2d destination_frame = CreateFrame2d(cam->destination_basis, cam->destination_origin, ZERO_VECTOR_2D);

//     // Generate the source-to-destination transform from the updated camera basis and focus.
//     cam->source_to_dest_mtx = MtxTransform_BuildLocalToParent(cam->source_focus_coords, cam->rotation, (Vector2d){1.0f / cam->zoom, 1.0f / cam->zoom});
//     cam->source_basis = (Basis2d){.u = {cam->source_to_dest_mtx.col1.x, cam->source_to_dest_mtx.col1.y},
//                                       .v = {cam->source_to_dest_mtx.col2.x, cam->source_to_dest_mtx.col2.y}};

//     // Re-anchor translation using the destination-space origin so the current source focus lands there.
//     cam->source_to_dest_mtx.col3.x = cam->destination_origin.x -
//                                      ((cam->source_focus_coords.x * cam->source_to_dest_mtx.col1.x) +
//                                       (cam->source_focus_coords.y * cam->source_to_dest_mtx.col2.x));

//     cam->source_to_dest_mtx.col3.y = cam->destination_origin.y -
//                                      ((cam->source_focus_coords.x * cam->source_to_dest_mtx.col1.y) +
//                                       (cam->source_focus_coords.y * cam->source_to_dest_mtx.col2.y));

//     // Always maintain the inverse matrix for parent-to-child picking.
//     cam->dest_to_source_mtx = MatrixInvert_3x3(cam->source_to_dest_mtx);
// }
