// /**********************************************************************************************
//  *
//     INCLUDES/DEFINITIONS
//  *
//  **********************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "math/affine_space_ops.h"

// //----------------------------------------------------------------------------------
// // Module Variables Definition (local)
// //----------------------------------------------------------------------------------

// //----------------------------------------------------------------------------------
// // Functions Definition
// //----------------------------------------------------------------------------------
// // void CalculateLineSegmentVectors(Space2d *space);
// void InitUnitCells(Space2d *coordinate_space);
// void Frame_InitBounds_Local(Vector2d local_resolution, const Frame2d *frame)
// {
//     if (!frame)
//         return local_point;

//     // Projecting local point coordinates onto the parent space basis lines
//     Vector2d u_part = VectorScale_2d(frame->basis.u, local_point.x);
//     Vector2d v_part = VectorScale_2d(frame->basis.v, local_point.y);

//     return VectorSum_2d(frame->origin_in_parent, VectorSum_2d(u_part, v_part));
// }

void Frame_CentreBounds(Frame2d *frame, Vector2d local_point)
{
    if (!frame)
        return;
    Vector2d extent = Frame_CalcExtent_Local(frame);
    // Projecting local point coordinates onto the parent space basis lines
    frame->local_min = VectorSum_2d(local_point, VectorScale_2d(extent, -0.5f));
    frame->local_max = VectorSum_2d(local_point, VectorScale_2d(extent, 0.5f));
}

Vector2d Frame_TransformPoint_ToParent(Vector2d local_point, const Frame2d *frame)
{
    if (!frame)
        return local_point;

    // Projecting local point coordinates onto the parent space basis lines
    Vector2d u_part = VectorScale_2d(frame->basis.u, local_point.x);
    Vector2d v_part = VectorScale_2d(frame->basis.v, local_point.y);

    return VectorSum_2d(frame->origin_in_parent, VectorSum_2d(u_part, v_part));
}

Vector2d Frame_TransformPoint_FromParent(Vector2d parent_point, const Frame2d *frame)
{
    if (!frame)
        return parent_point;

    // Shift the vector relative to our local origin position
    Vector2d relative_pos = VectorDiff_2d(parent_point, frame->origin_in_parent);

    // Project onto our basis vectors via a 2D Dot Product operation
    // (Assumes basis vectors are orthonormal: unit length and perpendicular)
    Vector2d local_point;
    local_point.x = VectorDot_2d(relative_pos, frame->basis.u);
    local_point.y = VectorDot_2d(relative_pos, frame->basis.v);

    return local_point;
}

// Gets the matrix that transforms a vector from this child (Source) frame directly into its parent (Destination) frame.
Matrix3x3 MtxTransform_GetLocalToParent(Frame2d frame)
{
    // The child's basis vectors and origin are in the parent's (destination) space. We just pack them directly.
    return (Matrix3x3){
        .col1 = {frame.basis.u.x, frame.basis.u.y, 0.0f},
        .col2 = {frame.basis.v.x, frame.basis.v.y, 0.0f},
        .col3 = {frame.origin_in_parent.x, frame.origin_in_parent.y, 1.0f},
    };
}

// Every object in your hierarchy needs a matrix that describes its position, rotation, and scale relative to its immediate parent.
Matrix3x3 MtxTransform_BuildLocalToParent(Vector2d origin_in_parent, float rotation_radians, Vector2d scale)
{
    // Compute local basis vectors using rotation and scale
    Basis2d basis = Basis_BuildLocalToParent(origin_in_parent, rotation_radians, scale);

    Frame2d object_frame = {
        .basis = basis,
        .origin_in_parent = origin_in_parent};

    // This matrix maps Child coordinates into Parent coordinates
    return MtxTransform_GetLocalToParent(object_frame);
}

// Shared basis construction: builds the local u/v axes from a rotation angle and
// non-uniform scale. All rotation-derived basis code routes through this helper.
Basis2d Basis_BuildFromRotationScale(float rotation_radians, Vector2d scale)
{
    float cos_r = cosf(rotation_radians);
    float sin_r = sinf(rotation_radians);

    return (Basis2d){
        .u = {cos_r * scale.x, sin_r * scale.x},
        .v = {-sin_r * scale.y, cos_r * scale.y}};
}

// Every object in your hierarchy needs a matrix that describes its position, rotation, and scale relative to its immediate parent.
Basis2d Basis_BuildLocalToParent(Vector2d origin_in_parent, float rotation_radians, Vector2d scale)
{
    // Origin belongs to the frame, not the basis; reuse the shared rotation/scale builder.
    (void)origin_in_parent;
    return Basis_BuildFromRotationScale(rotation_radians, scale);
}

// Validate a basis and normalize its orientation while preserving axis magnitudes.
bool Basis2d_NormaliseAndValidate(Basis2d requested, Basis2d *out_basis)
{
    const float epsilon = 0.0001f;
    if (!out_basis)
    {
        return false;
    }

    *out_basis = IDENTITY_BASIS_2D;

    float u_magnitude = VectorMagnitude_2d(requested.u);
    float v_magnitude = VectorMagnitude_2d(requested.v);
    if (u_magnitude < epsilon || v_magnitude < epsilon)
    {
        return false;
    }

    Basis2d normalised = requested;
    float determinant = (normalised.u.x * normalised.v.y) -
                       (normalised.u.y * normalised.v.x);
    if (fabsf(determinant) < epsilon)
    {
        Vector2d u_unit = VectorScale_2d(normalised.u, 1.0f / u_magnitude);
        normalised.v = (Vector2d){-u_unit.y * v_magnitude, u_unit.x * v_magnitude};
        determinant = (normalised.u.x * normalised.v.y) -
                      (normalised.u.y * normalised.v.x);
    }

    if (determinant < 0.0f)
    {
        normalised.v = VectorScale_2d(normalised.v, -1.0f);
    }

    *out_basis = normalised;
    return true;
}

// Assumes both frames are in the same parent space. This is a common case for sibling objects in a hierarchy.
Matrix3x3 MtxTransform_CalcSiblingToSibling_Mtx(Matrix3x3 source_mtx, Matrix3x3 destination_mtx)
{
    // Invert the destination matrix to get the path from Parent -> Destination sibling
    Matrix3x3 inv_dest_mtx = MatrixInvert_3x3(destination_mtx);
    // Combine them (Right-to-Left): Source -> Parent -> Destination
    return MatrixMultiply_3x3_3x3(inv_dest_mtx, source_mtx);
}

// Assumes both frames are in the same parent space. This is a common case for sibling objects in a hierarchy.
Matrix3x3 MtxTransform_CalcSiblingToSibling_Frame(Frame2d source, Frame2d destination)
{
    // Turn both frames into standard parent-relative matrices
    Matrix3x3 source_matrix = MtxTransform_GetLocalToParent(source);
    Matrix3x3 dest_matrix = MtxTransform_GetLocalToParent(destination);

    return MtxTransform_CalcSiblingToSibling_Mtx(source_matrix, dest_matrix);
}

// Assumes frames are in a hierarchy of destination <-> middle <-> source.
Matrix3x3 MtxTransform_CalcChainToAncestor_Frame(Frame2d source, Frame2d middle)
{
    // Step up from deep child to middle parent space
    Matrix3x3 source_to_middle = MtxTransform_GetLocalToParent(source);

    // Step up from middle parent space to grandparent (destination) space
    Matrix3x3 middle_to_destination = MtxTransform_GetLocalToParent(middle);

    // Combine right-to-left: Source -> Middle -> Destination
    return MatrixMultiply_3x3_3x3(middle_to_destination, source_to_middle);
}

// Basis2d BasisTransform_CalcChainToAncestor_Basis(Basis2d source, Basis2d middle)
// {
//     // Step up from deep child to middle parent space
//     Basis2d basis_tfrm = {
//         .u = (middle.u.x * source.u.x) + (middle.u.x * source.u.y),
//         .v = (middle.v, source.v.x), VectorScale_2d(middle.v, source.v.y))};
//     Matrix2x2 basis_matrix = {
//         {M_ui_to_pixel.col1.x, M_ui_to_pixel.col1.y},
//         {M_ui_to_pixel.col2.x, M_ui_to_pixel.col2.y}};
//     Matrix3x3 source_to_middle = MtxTransform_GetLocalToParent(source);

//     // Step up from middle parent space to grandparent (destination) space
//     Matrix3x3 middle_to_destination = MtxTransform_GetLocalToParent(middle);

//     // Combine right-to-left: Source -> Middle -> Destination
//     return MatrixMultiply_3x3_3x3(middle_to_destination, source_to_middle);
// }

Vector2d Frame_GetBasisScaling(Basis2d source, Basis2d destination)
{
    float source_u_magnitude = VectorMagnitude_2d(source.u);
    float source_v_magnitude = VectorMagnitude_2d(source.v);
    float destination_u_magnitude = VectorMagnitude_2d(destination.u);
    float destination_v_magnitude = VectorMagnitude_2d(destination.v);

    Vector2d scale = {0.0f, 0.0f};
    if ((source_u_magnitude > 0.0f) && (source_v_magnitude > 0.0f))
    {
        scale.x = destination_u_magnitude / source_u_magnitude;
        scale.y = destination_v_magnitude / source_v_magnitude;
    }

    return scale;
}

// Legacy draft block kept for reference only.
Vector2d Frame_CalcExtent_Local(const Frame2d *frame)
{
    if (!frame)
    {
        return ZERO_VECTOR_2D;
    }
    // Pure Cartesian bounding math: Max minus Min.
    // E.g., If min is -50 and max is 50, extent is 50 - (-50) = 100.
    Vector2d extent;
    extent.x = frame->local_max.x - frame->local_min.x;
    extent.y = frame->local_max.y - frame->local_min.y;

    return extent;
}

Vector2d Frame_GetTopLeft_Local(const Frame2d *frame)
{
    if (!frame)
        return ZERO_VECTOR_2D;

    // In a Cartesian system (+Y is Up):
    // The leftmost point is local_min.x
    // The topmost point is local_max.y
    Vector2d top_left = {frame->local_min.x, frame->local_max.y};

    return top_left;
}

Matrix2x2 Frame_CalcAABB_Local(const Frame2d *frame)
{
    if (!frame)
        return INFINITY_MATRIX_2x2;

    Vector2d corners[2] = {frame->local_min, frame->local_max};
    return AABB2d_FromPoints(corners, 2);
}

Vector2d Frame_CalcTopLeft_InParent(const Frame2d *frame)
{
    if (!frame)
        return ZERO_VECTOR_2D;

    // Get the local coordinate of the corner
    Vector2d local_tl = Frame_GetTopLeft_Local(frame);

    // Project it into parent space using the basis and origin
    Vector2d u_part = VectorScale_2d(frame->basis.u, local_tl.x);
    Vector2d v_part = VectorScale_2d(frame->basis.v, local_tl.y);

    Vector2d combined_basis = VectorSum_2d(u_part, v_part);
    return VectorSum_2d(frame->origin_in_parent, combined_basis);
}

Matrix2x2 Frame_CalcExtents_InParent(const Frame2d *frame)
{
    if (!frame)
        return INFINITY_MATRIX_2x2;

    // Calculate the extent in local coordinates
    Vector2d local_extent = Frame_CalcExtent_Local(frame);

    // Project the local extent into parent space using the basis vectors
    Vector2d u_part = VectorScale_2d(frame->basis.u, local_extent.x);
    Vector2d v_part = VectorScale_2d(frame->basis.v, local_extent.y);

    return (Matrix2x2){u_part, v_part};
}

Matrix2x2 Frame_CalcAABB_InParent(const Frame2d *frame)
{
    if (!frame)
        return INFINITY_MATRIX_2x2;

    // Define the 4 local corners of the frame
    Vector2d local_corners[4] = {
        {frame->local_min.x, frame->local_min.y}, // Bottom-Left
        {frame->local_max.x, frame->local_min.y}, // Bottom-Right
        {frame->local_min.x, frame->local_max.y}, // Top-Left
        {frame->local_max.x, frame->local_max.y}  // Top-Right
    };

    // Transform all corners to parent space, then let the shared AABB helper
    // compute the tight bounds instead of duplicating the min/max expansion loop.
    Vector2d transformed_corners[4];
    for (int i = 0; i < 4; i++)
    {
        transformed_corners[i] = Frame_TransformPoint_ToParent(local_corners[i], frame);
    }

    return AABB2d_FromPoints(transformed_corners, 4);
}

bool Frame_ContainsPoint_InParent(Vector2d parent_point, const Frame2d *frame)
{
    if (!frame)
        return false;

    // Push the point down into local reality coordinates
    Vector2d local_p = Frame_TransformPoint_FromParent(parent_point, frame);

    // Simple, clean bounding checks against Cartesian limits
    return (local_p.x >= frame->local_min.x && local_p.x <= frame->local_max.x &&
            local_p.y >= frame->local_min.y && local_p.y <= frame->local_max.y);
}

bool Frame_ContainsPoint_Local(Vector2d local_point, const Frame2d *frame)
{
    if (!frame)
        return false;

    // Simple, clean bounding checks against Cartesian limits
    return (local_point.x >= frame->local_min.x && local_point.x <= frame->local_max.x &&
            local_point.y >= frame->local_min.y && local_point.y <= frame->local_max.y);
}

// Vector2d CalcSpaceHalfExtent(const Space2d *space)
// {
//     if (!space)
//         return ZERO_VECTOR_2D;

//     Vector2d half_resolution = {(float)space->columns * 0.5f, (float)space->rows * 0.5f};
//     Vector2d half_u_extent = VectorScale_2d(space->system.basis.u, half_resolution.x);
//     Vector2d half_v_extent = VectorScale_2d(space->system.basis.v, half_resolution.y);
//     return VectorSum_2d(half_u_extent, half_v_extent);
// }

// Vector2d CalcSpaceOriginFromCenter(const Space2d *space, Vector2d center)
// {
//     return VectorSum_2d(center, VectorScale_2d(CalcSpaceHalfExtent(space), -1.0f));
// }

// Matrix2x2 CalcSpaceBoundsFromCenter(const Space2d *space, Vector2d center)
// {
//     Matrix2x2 bounds = {0};
//     if (!space)
//         return bounds;

//     Vector2d origin = CalcSpaceOriginFromCenter(space, center);
//     Vector2d corners[4] = {
//         origin,
//         VectorSum_2d(origin, VectorScale_2d(space->system.basis.u, (float)space->columns)),
//         VectorSum_2d(VectorSum_2d(origin, VectorScale_2d(space->system.basis.u, (float)space->columns)), VectorScale_2d(space->system.basis.v, (float)space->rows)),
//         VectorSum_2d(origin, VectorScale_2d(space->system.basis.v, (float)space->rows)),
//     };

//     bounds.col1 = corners[0];
//     bounds.col2 = corners[0];

//     for (int i = 1; i < 4; i++)
//     {
//         if (corners[i].x < bounds.col1.x)
//             bounds.col1.x = corners[i].x;
//         if (corners[i].y < bounds.col1.y)
//             bounds.col1.y = corners[i].y;
//         if (corners[i].x > bounds.col2.x)
//             bounds.col2.x = corners[i].x;
//         if (corners[i].y > bounds.col2.y)
//             bounds.col2.y = corners[i].y;
//     }

//     return bounds;
// }

// // Creates a local child coordinate space with physical attributes.
// // Basis vectors in most cases (orthogonal dimensions) should be u = {1,0}, v = {0,1}.
// // The origin is in the child/local frame; parent-space placement is handled externally.
// GridSpace2d NewGridSpace2d(Vector2d origin, Vector2d resolution_ixj, Basis2d basis, ColourRgba colour_fill, ColourRgba colour_line)
// {
//    GridSpace2d space_obj = {0};
//    space_obj.space = NewSpace2d(origin, resolution_ixj, basis);
//    space_obj.colour_line = colour_line;
//    space_obj.colour_fill = colour_fill;

//    Surface2d surface = CreateSurface_Rectangular(resolution_ixj, ZERO_VECTOR_2D);

//    // Calc anchor_position - this is required when creating new objects
//    Vector2d anchor_position = (Vector2d){origin.x + (resolution_ixj.x / 2.0), origin.y + (resolution_ixj.y / 2.0)};
//    space_obj.object = CreateNewtonoid2d(0.0f, anchor_position, ZERO_VECTOR_2D, ZERO_VECTOR_2D, surface);
//    return space_obj;
// }

// // Creates a local coordinate space. Basis vectors in most cases (orthogonal dimensions) should be u = {1,0}, v = {0,1}.
// // The origin is in local coordinates of this space, not a parent-space mapped coordinate.
// Space2d NewSpace2d(Vector2d origin, Vector2d resolution_ixj, Basis2d basis)
// {
//    Space2d space = {0};
//    space.local_origin = origin;
//    // Needs a grid with resolution according to unit height and width - divide it up and handle the leftover height and width
//    // if (unitHeight > object.height || unitWidth > object.width)
//    // {
//    //    fprintf(stderr, "ERROR: unitHeight and/or unitWidth are greater than the desired height and/or width. Cannot create such a field!");
//    //    return field;
//    // };
//    // Create the underlying object and define its surface

//    space.basis = basis;
//    space.resolution_ixj = resolution_ixj;
//    // Resolution is the number of addressable cells in local i/j coordinates.
//    space.stepsU = fmaxf(1.0f, ceilf(resolution_ixj.x));
//    space.stepsV = fmaxf(1.0f, ceilf(resolution_ixj.y));
//    space.unitArea = fabsf((basis.u.x * basis.v.y) - (basis.u.y * basis.v.x)); // Calculate the 'Area' of a single basis tile; this is the determinant of the basis matrix, which gives us the area of the parallelogram formed by the basis vectors, which is the area of each cell in the coordinate space. We can then divide the total area of the field by this cell area to get the total number of cells needed to fill the field.

//    // If cellArea is 0, the basis is invalid (it's a line, not a space)
//    if (space.unitArea < 0.0001f)
//    {
//       fprintf(stderr, "ERROR: Invalid basis vectors. The area of the basis tile is too small (close to zero). Cannot create coordinate space with these basis vectors!");
//    }

//    // Total units are deterministic from requested i/j resolution.
//    int totalUnits = (int)(space.stepsU * space.stepsV);

//    space.cells = MakeDArray(totalUnits, sizeof(Cell));

//    // CalculateLineSegmentVectors(&coordinate_space);
//    InitUnitCells(&space);

//    // LOG FIELD INFO
//    Vector2d basis_u = space.basis.u;
//    Vector2d basis_v = space.basis.v;

//    // char text[64]; // Buffer to hold the text
//    // snprintf(text, sizeof(text), "FIELD INITIALISED:  Dimensions (%d, %d); Units (%d); Basis -> u = [%d,%d], v = [%d,%d].\n", unitW, unitH, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);
//    printf("COORD.SPACE INITIALISED:  Dimensions (W:%0.2f, H:%0.2f); Units (%d); Basis -> u = [%0.2f,%0.2f], v = [%0.2f,%0.2f].\n", resolution_ixj.x, resolution_ixj.y, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);

//    return space;
// }

// Vector2d CalcSpaceHalfExtent(const Space2d *space)
// {
//    if (!space)
//    {
//       return ZERO_VECTOR_2D;
//    }

//    Vector2d half_resolution = VectorScale_2d(space->resolution_ixj, 0.5f);
//    Vector2d half_u_extent = VectorScale_2d(space->basis.u, half_resolution.x);
//    Vector2d half_v_extent = VectorScale_2d(space->basis.v, half_resolution.y);
//    return VectorSum_2d(half_u_extent, half_v_extent);
// }

// Vector2d CalcSpaceOriginFromCenter(const Space2d *space, Vector2d center)
// {
//    return VectorSum_2d(center, VectorScale_2d(CalcSpaceHalfExtent(space), -1.0f));
// }

// Matrix2x2 CalcSpaceBoundsFromCenter(const Space2d *space, Vector2d center)
// {
//    Matrix2x2 bounds = {0};
//    if (!space)
//    {
//       return bounds;
//    }

//    Vector2d origin = CalcSpaceOriginFromCenter(space, center);
//    Vector2d corners[4] = {
//       origin,
//       VectorSum_2d(origin, VectorScale_2d(space->basis.u, space->resolution_ixj.x)),
//       VectorSum_2d(VectorSum_2d(origin, VectorScale_2d(space->basis.u, space->resolution_ixj.x)), VectorScale_2d(space->basis.v, space->resolution_ixj.y)),
//       VectorSum_2d(origin, VectorScale_2d(space->basis.v, space->resolution_ixj.y)),
//    };

//    bounds.col1 = corners[0];
//    bounds.col2 = corners[0];

//    for (int i = 1; i < 4; i++)
//    {
//       if (corners[i].x < bounds.col1.x)
//          bounds.col1.x = corners[i].x;
//       if (corners[i].y < bounds.col1.y)
//          bounds.col1.y = corners[i].y;
//       if (corners[i].x > bounds.col2.x)
//          bounds.col2.x = corners[i].x;
//       if (corners[i].y > bounds.col2.y)
//          bounds.col2.y = corners[i].y;
//    }

//    return bounds;
// }

// void InitUnitCells(Space2d *space)
// {
//    DArray *cells = &(space->cells);
//    size_t cells_capacity = cells->capacity;
//    memset(cells->items, 0, cells->elem_bytes * cells_capacity);

//    Vector2d local_origin = space->local_origin;
//    int stepsU = space->stepsU;
//    int stepsV = space->stepsV;

//    int count = 0;
//    for (int k = 0; k < stepsU * stepsV; k++)
//    {
//       int i = k / stepsU; // Row index (based on horizontal lines)
//       int j = k % stepsU; // Column index (based on vertical lines)

//       Cell cell = {0}; // Create a new cell and initialize it to zero

//       // Scale the basis vectors (u,v) and add them to get the displacement from the origin
//       Vector2d scaled_u = {j * space->basis.u.x, j * space->basis.u.y};
//       Vector2d scaled_v = {i * space->basis.v.x, i * space->basis.v.y};
//       Vector2d displacement = {scaled_u.x + scaled_v.x, scaled_u.y + scaled_v.y};

//       // Add the displacement vector to the origin to get the coordinates of the cell
//       cell.local_origin.x = local_origin.x + displacement.x;
//       cell.local_origin.y = local_origin.y + displacement.y;
//       // Centre will always be 0.5 basis units since a cell is by definition the object representing the 2 basis vectors
//       cell.local_center.x = cell.local_origin.x + (0.5 * (space->basis.u.x + space->basis.v.x));
//       cell.local_center.y = cell.local_origin.y + (0.5 * (space->basis.u.y + space->basis.v.y));

//       // Write the cell to the array
//       Cell *address = (Cell *)((char *)cells->items + (k * cells->elem_bytes));
//       memcpy(address, &cell, cells->elem_bytes);
//       count++;
//    }
//    cells->count = stepsU * stepsV;
//    LOG_INFO("Initialised %d cells\n", count);
// }

// int GetIndexFromCoords(Space2d *space, Vector2d space_coords)
// {
//    int i = (int)floorf(space_coords.y);
//    int j = (int)floorf(space_coords.x);
//    int stepsU = (int)space->stepsU;
//    int stepsV = (int)space->stepsV;

//    if (i < 0 || j < 0 || i >= stepsV || j >= stepsU)
//    {
//       return -1;
//    }

//    int cell_index = (i * stepsU) + j;
//    return cell_index;
// }

// Cell *GetCellFromCoords(Space2d *space, Vector2d coords)
// {
//    int cell_index = GetIndexFromCoords(space, coords);
//    if (cell_index < 0 || cell_index >= space->cells.count)
//    {
//       return NULL;
//    }
//    Cell *cells = space->cells.items;
//    Cell *target_cell = &cells[cell_index];

//    return target_cell;
// }

// Matrix2x2 CalcSpaceAABB(Space2d *space)
// {
//    Matrix2x2 u_v_extents = CalcSpaceExtents_2d(space);
//    Matrix2x2 aabb_box = {0};
//    aabb_box.col1.x = fminf(u_v_extents.col1.x, u_v_extents.col2.x);
//    aabb_box.col2.x = fmaxf(u_v_extents.col1.x, u_v_extents.col2.x);

//    aabb_box.col1.y = fminf(u_v_extents.col1.y, u_v_extents.col2.y);
//    aabb_box.col2.y = fmaxf(u_v_extents.col1.y, u_v_extents.col2.y);

//    return aabb_box;
// }

// // This function creates a footprint surface that represents the area of effect of an object based on its surface and the coordinate space's basis vectors.
// // It calculates the bounding box of the object's surface in world coordinates, determines which cells in the coordinate space it overlaps with, and then creates a rectangular surface that encompasses all those cells.
// // The object offset parameter allows you to specify the world coordinates of the object's center, which is necessary to correctly position the footprint in the coordinate space. Provide a zero vector if the object's surface vertices are already in world coordinates.
// void CalcSnappedAABB_Vertices(Vector2d *object_surface_vertices, int object_surface_vertices_count, Vector2d object_offset, Basis2d space_basis, Vector2d out_vertices[4])
// {
//    // Correctly extract the physical grid cell size from the basis
//    float cell_w = sqrtf(space_basis.u.x * space_basis.u.x +
//                         space_basis.u.y * space_basis.u.y);
//    float cell_h = sqrtf(space_basis.v.x * space_basis.v.x +
//                         space_basis.v.y * space_basis.v.y);

//    if (object_surface_vertices_count == 0)
//    {
//       return;
//    }

//    Matrix2x2 box_coords = {0};

//    // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
//    box_coords.col1 = VectorSum_2d(object_offset, object_surface_vertices[0]);
//    box_coords.col2 = VectorSum_2d(object_offset, object_surface_vertices[0]);
//    Vector2d vertice = {0};
//    for (size_t i = 1; i < object_surface_vertices_count; i++)
//    {
//       vertice = VectorSum_2d(object_offset, object_surface_vertices[i]); // Convert vertices to absolute world positions to find the global AABB limits

//       box_coords.col1.x = fminf(box_coords.col1.x, vertice.x);
//       box_coords.col2.x = fmaxf(box_coords.col2.x, vertice.x);

//       box_coords.col1.y = fminf(box_coords.col1.y, vertice.y);
//       box_coords.col2.y = fmaxf(box_coords.col2.y, vertice.y);
//    }

//    // Convert the spatial bounding extremes directly to integer cell index spans
//    float start_cell_x = floorf(box_coords.col1.x / cell_w);
//    float end_cell_x = ceilf(box_coords.col2.x  / cell_w);
//    float start_cell_y = floorf(box_coords.col1.y / cell_h);
//    float end_cell_y = ceilf(box_coords.col2.y / cell_h);

//    // Create a 4-corner bounding rectangle that perfectly encapsulates
//    // every single cell the object is partially or fully occupying.
//    //LArray footprint_vertices = MakeLArray(4, sizeof(Vector2d));

//    // Map the bounding indexes back to absolute spatial coordinates
//    out_vertices[0] = (Vector2d){start_cell_x, start_cell_y}; // Bottom-Left Cell Boundary
//    out_vertices[1] = (Vector2d){end_cell_x, start_cell_y};   // Bottom-Right Cell Boundary
//    out_vertices[2] = (Vector2d){end_cell_x, end_cell_y};     // Top-Right Cell Boundary
//    out_vertices[3] = (Vector2d){start_cell_x, end_cell_y};   // Top-Left Cell Boundary
// }

// bool VectorIsInSpace_2d(Vector2d vector, Space2d *space)
// {
//    // Check if the vector is within the bounds of the coordinate space defined by its origin and the extents of its basis vectors multiplied by their respective steps
//    Matrix2x2 extents = CalcSpaceExtents_2d(space);
//    Vector2d origin = space->local_origin;
//    float min_x = origin.x;
//    float max_x = origin.x + extents.col1.x + extents.col2.x;
//    float min_y = origin.y;
//    float max_y = origin.y + extents.col1.y + extents.col2.y;

//    return (vector.x >= min_x && vector.x < max_x && vector.y >= min_y && vector.y < max_y);
// }

// // This function calculates the spatial extents of a coordinate space based on its origin and the extents of its basis vectors multiplied by their respective steps.
// // It returns a 2x2 matrix where col1 is the vector from the origin to the far corner along the u direction, and col2 is the vector from the origin to the far corner along the v direction.
// Matrix2x2 CalcSpaceExtents_2d(Space2d *space)
// {
//    // Check if the vector is within the bounds of the coordinate space defined by its origin and the extents of its basis vectors multiplied by their respective steps
//    Vector2d u_extent = {space->basis.u.x * space->stepsU, space->basis.u.y * space->stepsU};
//    Vector2d v_extent = {space->basis.v.x * space->stepsV, space->basis.v.y * space->stepsV};

//    return (Matrix2x2){u_extent, v_extent};
// }
