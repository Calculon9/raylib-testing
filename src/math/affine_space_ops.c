// /**********************************************************************************************
//  *
//     INCLUDES/DEFINITIONS
//  *
//  **********************************************************************************************/

// #include <stdio.h>
// #include <string.h>
// #include <math.h>
// #include "math/coordinate_space.h"

// //----------------------------------------------------------------------------------
// // Module Variables Definition (local)
// //----------------------------------------------------------------------------------

// //----------------------------------------------------------------------------------
// // Functions Definition
// //----------------------------------------------------------------------------------
// // void CalculateLineSegmentVectors(CoordSpace2d *coord_space);
// void InitUnitCells(CoordSpace2d *coordinate_space);


// // Creates a local child coordinate space with physical attributes.
// // Basis vectors in most cases (orthogonal dimensions) should be u = {1,0}, v = {0,1}.
// // The origin is in the child/local frame; parent-space placement is handled externally.
// CoordSpace2d_Grid NewCoordSpace2d_Grid(Vector2d origin, Vector2d resolution_ixj, Basis2d basis, ColourRgba colour_fill, ColourRgba colour_line)
// {
//    CoordSpace2d_Grid space_obj = {0};
//    space_obj.coord_space = NewCoordSpace2d(origin, resolution_ixj, basis);
//    space_obj.colour_line = colour_line;
//    space_obj.colour_fill = colour_fill;

//    Surface2d surface = CreateSurface_Rectangular(resolution_ixj, ZERO_VECTOR_2D);

//    // Calc coords_center - this is required when creating new objects
//    Vector2d coords_center = (Vector2d){origin.x + (resolution_ixj.x / 2.0), origin.y + (resolution_ixj.y / 2.0)};
//    space_obj.object = CreateNewtonoid2d_Static(coords_center, surface);
//    return space_obj;
// }

// // Creates a local coordinate space. Basis vectors in most cases (orthogonal dimensions) should be u = {1,0}, v = {0,1}.
// // The origin is in local coordinates of this space, not a parent-space mapped coordinate.
// CoordSpace2d NewCoordSpace2d(Vector2d origin, Vector2d resolution_ixj, Basis2d basis)
// {
//    CoordSpace2d space = {0};
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

// Vector2d CalcCoordSpaceHalfExtent(const CoordSpace2d *space)
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

// Vector2d CalcCoordSpaceOriginFromCenter(const CoordSpace2d *space, Vector2d center)
// {
//    return VectorSum_2d(center, VectorScale_2d(CalcCoordSpaceHalfExtent(space), -1.0f));
// }

// Matrix2x2 CalcCoordSpaceBoundsFromCenter(const CoordSpace2d *space, Vector2d center)
// {
//    Matrix2x2 bounds = {0};
//    if (!space)
//    {
//       return bounds;
//    }

//    Vector2d origin = CalcCoordSpaceOriginFromCenter(space, center);
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

// void InitUnitCells(CoordSpace2d *space)
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

// int GetIndexFromCoords(CoordSpace2d *space, Vector2d space_coords)
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

// Cell *GetCellFromCoords(CoordSpace2d *space, Vector2d coords)
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


// Matrix2x2 CalcSpaceAABB(CoordSpace2d *space)
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
// void CalcSnappedAABB_Vertices(Vector2d *object_surface_vertices, int object_surface_vertices_count, Vector2d object_offset, Basis2d coord_space_basis, Vector2d out_vertices[4])
// {
//    // Correctly extract the physical grid cell size from the basis
//    float cell_w = sqrtf(coord_space_basis.u.x * coord_space_basis.u.x +
//                         coord_space_basis.u.y * coord_space_basis.u.y);
//    float cell_h = sqrtf(coord_space_basis.v.x * coord_space_basis.v.x +
//                         coord_space_basis.v.y * coord_space_basis.v.y);

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

// bool VectorIsInSpace_2d(Vector2d vector, CoordSpace2d *space)
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
// Matrix2x2 CalcSpaceExtents_2d(CoordSpace2d *space)
// {
//    // Check if the vector is within the bounds of the coordinate space defined by its origin and the extents of its basis vectors multiplied by their respective steps
//    Vector2d u_extent = {space->basis.u.x * space->stepsU, space->basis.u.y * space->stepsU};
//    Vector2d v_extent = {space->basis.v.x * space->stepsV, space->basis.v.y * space->stepsV};

//    return (Matrix2x2){u_extent, v_extent};
// }