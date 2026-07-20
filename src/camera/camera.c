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
        camera.target_zoom = camera.zoom;
        
        camera.rotation = VectorRadians_2d(source_space_frame->basis.u);
        camera.source_focus_coords = source_space_frame->origin_in_parent;
        camera.target_source_focus_coords = source_space_frame->origin_in_parent;
    }
    else
    {
        camera.zoom = 1.0f;
        camera.target_zoom = 1.0f;
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
        cam->target_zoom = cam->zoom;
    }

    cam->rotation = (float)VectorRadians_2d(source_space_frame->basis.u);
    cam->source_focus_coords = source_space_frame->origin_in_parent;
    cam->target_source_focus_coords = source_space_frame->origin_in_parent;

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

void PanCamera(Camera2d *cam, Vector2d delta)
{
    if (!cam)
        return;
    cam->target_source_focus_coords = VectorSum_2d(cam->target_source_focus_coords, delta);
}

void FollowTarget(Camera2d *cam, Vector2d source_focus_coords)
{
    if (!cam)
        return;
    cam->target_source_focus_coords = source_focus_coords;
}

void ZoomCamera(Camera2d *cam, float zoom_factor)
{
    if (!cam)
        return;
    cam->target_zoom *= zoom_factor;
    UpdateCameraFull(cam);
}

void RotateCamera(Camera2d *cam, float rotation_angle)
{
    if (!cam)
        return;
    cam->rotation += rotation_angle;
    UpdateCameraFull(cam);
}

void UpdateCameraSmoothingTick(Camera2d *cam)
{
    if (!cam || !cam->tunnel.source_frame || !cam->tunnel.destination_frame)
        return;

    bool state_changed = false;

    // Smoothly interpolate zoom
    float zoom_delta = cam->target_zoom - cam->zoom;
    if (fabsf(zoom_delta) > 0.001f)
    {
        cam->zoom += zoom_delta * 0.15f;
        state_changed = true;
    }
    else if (cam->zoom != cam->target_zoom)
    {
        cam->zoom = cam->target_zoom;
        state_changed = true;
    }

    // Smoothly interpolate position tracking (using a safe floating-point delta check)
    float dx = cam->target_source_focus_coords.x - cam->source_focus_coords.x;
    float dy = cam->target_source_focus_coords.y - cam->source_focus_coords.y;
    
    // Check if we are outside a tiny error threshold (e.g., 0.01 units)
    if (fabsf(dx) > 0.01f || fabsf(dy) > 0.01f)
    {
        cam->source_focus_coords.x += dx * 0.15f;
        cam->source_focus_coords.y += dy * 0.15f;
        state_changed = true;
    }
    else if (cam->source_focus_coords.x != cam->target_source_focus_coords.x || 
             cam->source_focus_coords.y != cam->target_source_focus_coords.y)
    {
        // Snap directly to target to stop calculations completely
        cam->source_focus_coords = cam->target_source_focus_coords;
        state_changed = true;
    }

    // Delegate matrix baking to our fixed column-vector pipeline function
    if (state_changed)
    {
        UpdateCameraFull(cam);
    }
}

void UpdateCameraFull(Camera2d *cam)
{
    if (!cam || !cam->tunnel.source_frame || !cam->tunnel.destination_frame)
        return;

    // Establish frame metrics cleanly. Keep zoom proportional.
    float cos_r = cosf(cam->rotation);
    float sin_r = sinf(cam->rotation);
    float scale = cam->zoom;

    // Sync the gameplay tracking variables straight into the source frame struct
    cam->tunnel.source_frame->origin_in_parent = cam->source_focus_coords;
    cam->tunnel.source_frame->basis.u = (Vector2d){cos_r * scale, sin_r * scale};
    cam->tunnel.source_frame->basis.v = (Vector2d){-sin_r * scale, cos_r * scale};

    // Build the Camera View Space Transform Matrix manually
    // This shifts world points so that target_focus coordinates land exactly at (0,0)
    Matrix3x3 M_cam_view;

    // X Basis Column (Rotation + Scale)
    M_cam_view.col1.x = cos_r * scale;
    M_cam_view.col1.y = sin_r * scale;
    M_cam_view.col1.z = 0.0f;

    // Y Basis Column (Rotation + Scale)
    M_cam_view.col2.x = -sin_r * scale;
    M_cam_view.col2.y = cos_r * scale;
    M_cam_view.col2.z = 0.0f;

    // Translation Column (Inverse translation to focus point, scaled)
    M_cam_view.col3.x = -(cam->source_focus_coords.x * cos_r + cam->source_focus_coords.y * sin_r) * scale;
    M_cam_view.col3.y = -(cam->source_focus_coords.x * -sin_r + cam->source_focus_coords.y * cos_r) * scale;
    M_cam_view.col3.z = 1.0f;

    // Build Viewport Center Matrix (Offsets origin to screen viewport center)
    Vector2d view_center = {
        cam->tunnel.destination_frame->origin_in_parent.x + (cam->tunnel.destination_frame->local_max.x - cam->tunnel.destination_frame->local_min.x) * 0.5f,
        cam->tunnel.destination_frame->origin_in_parent.y + (cam->tunnel.destination_frame->local_max.y - cam->tunnel.destination_frame->local_min.y) * 0.5f};

    Matrix3x3 M_view_center;
    // Identity for basis columns
    M_view_center.col1 = (Vector3d){1.0f, 0.0f, 0.0f};
    M_view_center.col2 = (Vector3d){0.0f, 1.0f, 0.0f};
    // Translation to screen space center
    M_view_center.col3 = (Vector3d){view_center.x, view_center.y, 1.0f};

    // Combine: M_total = M_view_center * M_cam_view
    // Vectors are transformed right-to-left: v_pixel = M_view_center * (M_cam_view * v_world)
    cam->tunnel.source_to_dest_mtx = MatrixMultiply_3x3_3x3(M_view_center, M_cam_view);
    cam->tunnel.dest_to_source_mtx = MatrixInvert_3x3(cam->tunnel.source_to_dest_mtx);
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
