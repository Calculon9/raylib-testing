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
Camera2d CreateCamera2d(Basis2d destination_basis, Basis2d source_basis, Vector2d destination_origin_coords, float zoom, float rotation)
{
   Camera2d camera = {0};
   camera.destination_basis = destination_basis;
   camera.source_basis = source_basis;
   camera.destination_origin_coords = destination_origin_coords;
   camera.zoom = zoom;
   camera.rotation = rotation;

   // Calculate the transformation matrix that will be used to translate any world space to screen space 
   camera.transformation_mtx = BasisTransform_2d(source_basis, destination_basis, destination_origin_coords);

   return camera;
}

// Camera2d CreateCamera2d_(Basis2d destination_basis, Basis2d source_basis, Vector2d destination_origin_coords, float zoom, float rotation)
// {
//    Camera2d camera = {0};
//    camera.destination_basis = destination_basis;
//    camera.source_basis = source_basis;
//    camera.destination_origin_coords = destination_origin_coords;
//    camera.zoom = zoom;
//    camera.rotation = rotation;

//    // Calculate the transformation matrix that will be used to translate any world space to screen space 


//    return camera;
// }

void UpdateCameraMatrix(Camera2d *cam, Basis2d input_basis) {
    // 1. Calculate the Screen Basis based on Zoom
    Basis2d screen_basis = {
        .u = { cam->zoom, 0 },
        .v = { 0, cam->zoom }
    };

    // 2. Generate the transform
    // Pass the 'offset' as the screen origin
    cam->transformation_mtx = BasisTransform_2d(input_basis, screen_basis, cam->destination_origin_coords);
}

Vector2d TransformCoordinates(Matrix3x3 transformation_mtx, Vector2d coordinates_to_transform)
{
   Vector2d output_coords;

    // 1. Get the "transformation" or "mapping" basis to go from world to screen.
    // 2. Get the scaling factor to go from world basis magnitude to screen basis magnitude.

    // Since we are using a 3x  matrix for 2D, we treat the 2D point as a 3D vector where z=1. This is a trick called Homogeneous Coordinates that allows the matrix to move (translate) the point, not just rotate or scale it.
    //  Multiply: (Row 1 * WorldColumn)
    //  screenX = (m0 * x) + (m3 * y) + m6
    output_coords.x = (coordinates_to_transform.x * transformation_mtx.m0) + (coordinates_to_transform.y * transformation_mtx.m3) + transformation_mtx.m6;

    // Multiply: (Row 2 * WorldColumn)
    // screenY = (m1 * x) + (m4 * y) + m7
    output_coords.y = (coordinates_to_transform.x * transformation_mtx.m1) + (coordinates_to_transform.y * transformation_mtx.m4) + transformation_mtx.m7;

    return output_coords;
}
//World CalculateFieldLines(Field field);
//World InitialiseFieldCells(Field field);

