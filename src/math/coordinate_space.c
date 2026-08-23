/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "physics/newtonoid.h"
#include "math/coordinate_space.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
const CoordinateSpacePreset COORDINATE_SPACE_PRESET_REGULAR = {
   GRID_GEOMETRY_REGULAR,
   {7.0f, 5.0f},
   {{1.0f, 0.0f}, {0.0f, 1.0f}}
};

const CoordinateSpacePreset COORDINATE_SPACE_PRESET_SHEARED_Y = {
   GRID_GEOMETRY_SHEARED_Y,
   {7.0f, 5.0f},
   {{1.0f, 0.0f}, {0.5f, 1.0f}}
};

const CoordinateSpacePreset COORDINATE_SPACE_PRESET_SHEARED_X = {
   GRID_GEOMETRY_SHEARED_X,
   {7.0f, 5.0f},
   {{1.0f, 0.5f}, {0.0f, 1.0f}}
};

const CoordinateSpacePreset COORDINATE_SPACE_PRESET_ISOMETRIC = {
   GRID_GEOMETRY_ISOMETRIC,
   {7.0f, 5.0f},
   {{0.7071f, 0.5f}, {-0.7071f, 0.5f}}
};

const CoordinateSpacePreset COORDINATE_SPACE_PRESET_PERSPECTIVE = {
   GRID_GEOMETRY_PERSPECTIVE,
   {7.0f, 5.0f},
   {{1.0f, 0.0f}, {0.0f, 1.0f}}
};

const CoordinateSpacePreset COORDINATE_SPACE_PRESET_RADIAL = {
   GRID_GEOMETRY_RADIAL,
   {7.0f, 5.0f},
   {{1.0f, 0.0f}, {0.0f, 1.0f}}
};

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
// void CalculateLineSegmentVectors(Space2d *space);
void InitUnitCells(Space2d *coordinate_space);

Frame2d CreateFrame2d(Basis2d basis, Vector2d origin_in_parent, Vector2d local_resolution)
{
    return (Frame2d){
        .basis = basis,
        .origin_in_parent = origin_in_parent,
        .local_min = ZERO_VECTOR_2D,
        .local_max = local_resolution
    };
}

// Create UI coordinate-space geometry without allocating simulation cell data.
UISpace2d NewUISpace2d(Vector2d origin_in_parent, Vector2d local_resolution, Basis2d basis)
{
   UISpace2d space = {0};
   space.frame = CreateFrame2d(basis, origin_in_parent, local_resolution);
   space.columns = (int)fmaxf(1.0f, ceilf(local_resolution.x));
   space.rows = (int)fmaxf(1.0f, ceilf(local_resolution.y));
   return space;
}

// Creates a local coordinate space. Basis vectors in most cases (orthogonal dimensions) should be u = {1,0}, v = {0,1}.
// The origin is in local coordinates of this space, not a parent-space mapped coordinate.
Space2d NewSpace2d(Vector2d origin_in_parent, Vector2d local_resolution, Basis2d basis)
{
   Space2d space = {0};
   int columns = (int)fmaxf(1.0f, ceilf(local_resolution.x));
   int rows = (int)fmaxf(1.0f, ceilf(local_resolution.y));
   Vector2d grid_size = {(float)columns, (float)rows};

   space.frame = CreateFrame2d(basis, origin_in_parent,local_resolution);
   //space.frame.local_min = origin_in_parent;
   //space.frame.local_max = VectorSum_2d(origin_in_parent, grid_size);
   space.columns = columns;
   space.rows = rows;
   space.grid_origin = ZERO_VECTOR_2D;
   space.unitArea = fabsf((basis.u.x * basis.v.y) - (basis.u.y * basis.v.x)); // Calculate the 'Area' of a single basis tile; this is the determinant of the basis matrix, which gives us the area of the parallelogram formed by the basis vectors, which is the area of each cell in the coordinate space. We can then divide the total area of the field by this cell area to get the total number of cells needed to fill the field.

   // If cellArea is 0, the basis is invalid (it's a line, not a space)
   if (space.unitArea < 0.0001f)
   {
      fprintf(stderr, "ERROR: Invalid basis vectors. The area of the basis tile is too small (close to zero). Cannot create coordinate space with these basis vectors!");
   }

   // Total units are deterministic from requested i/j resolution.
   int totalUnits = space.columns * space.rows;
   space.cells = MakeDArray(totalUnits, sizeof(Cell));

   InitUnitCells(&space);

   Vector2d basis_u = space.frame.basis.u;
   Vector2d basis_v = space.frame.basis.v;

   // char text[64]; // Buffer to hold the text
   // snprintf(text, sizeof(text), "FIELD INITIALISED:  Dimensions (%d, %d); Units (%d); Basis -> u = [%d,%d], v = [%d,%d].\n", unitW, unitH, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);
   printf("COORD.SPACE INITIALISED:  Dimensions (W:%d, H:%d); Units (%d); Basis -> u = [%0.2f,%0.2f], v = [%0.2f,%0.2f].\n", columns, rows, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);

   return space;
}

// Creates a local child coordinate space with physical attributes.
// Basis vectors in most cases (orthogonal dimensions) should be u = {1,0}, v = {0,1}.
// The origin is in the child/local frame; parent-space placement is handled externally.
GridSpace2d NewGridSpace2d(Vector2d origin_in_parent, Vector2d local_resolution, Basis2d basis, ColourRgba colour_fill, ColourRgba colour_line)
{
   GridSpace2d space_obj = {0};
   space_obj.space = NewSpace2d(origin_in_parent, local_resolution, basis);
   space_obj.colour_line = colour_line;
   space_obj.colour_fill = colour_fill;

   Vector2d grid_size = {(float)space_obj.space.columns, (float)space_obj.space.rows};
   Surface2d surface = CreateSurface_Rectangular(grid_size, ZERO_VECTOR_2D);

   // Calc anchor_position - this is required when creating new objects
   Vector2d anchor_position = (Vector2d){origin_in_parent.x + (grid_size.x / 2.0f), origin_in_parent.y + (grid_size.y / 2.0f)};
   space_obj.object = CreateNewtonoid2d_Static(anchor_position, surface);
   return space_obj;
}

void InitUnitCells(Space2d *space)
{
   DArray *cells = &(space->cells);
   size_t cells_capacity = cells->capacity;
   MemorySet(cells->items, 0, cells->elem_bytes * cells_capacity);

   // Cell coordinates are stored in this space's local frame, offset by the grid origin.
   Vector2d local_origin = space->grid_origin;
   int columns = space->columns;
   int rows = space->rows;

   int count = 0;
   for (int k = 0; k < columns * rows; k++)
   {
      int i = k / columns; // Row index (based on horizontal lines)
      int j = k % columns; // Column index (based on vertical lines)

      Cell cell = {0}; // Create a new cell and initialize it to zero

      // Scale the basis vectors (u,v) and add them to get the displacement from the origin
      Vector2d scaled_u = {j * space->frame.basis.u.x, j * space->frame.basis.u.y};
      Vector2d scaled_v = {i * space->frame.basis.v.x, i * space->frame.basis.v.y};
      Vector2d displacement = {scaled_u.x + scaled_v.x, scaled_u.y + scaled_v.y};

      // Add the displacement vector to the origin to get the coordinates of the cell
      cell.local_origin.x = local_origin.x + displacement.x;
      cell.local_origin.y = local_origin.y + displacement.y;

      // Write the cell to the array
      Cell *address = (Cell *)((char *)cells->items + (k * cells->elem_bytes));
      MemoryCopy(address, &cell, cells->elem_bytes);
      count++;
   }
   cells->count = columns * rows;
   LOG_INFO("Initialised %d cells\n", count);
}

// Clear transient entity occupancy while preserving each cell's geometry and metadata.
void ResetSpaceCells(Space2d *space)
{
   if (!space || !space->cells.items)
   {
      return;
   }

   Cell *cells = (Cell *)space->cells.items;
   for (size_t cell_index = 0; cell_index < space->cells.count; cell_index++)
   {
      cells[cell_index].occupancy = 0;
      MemorySet(cells[cell_index].object_ids, 0, sizeof(cells[cell_index].object_ids));
   }
}

GridSpace2d NewGridSpace2d_FromPreset(Vector2d origin_in_parent, CoordinateSpacePreset preset, ColourRgba colour_fill, ColourRgba colour_line)
{
   return NewGridSpace2d(origin_in_parent, preset.resolution, preset.basis, colour_fill, colour_line);
}

void RebuildSpaceCells(Space2d *space)
{
   if (!space)
   {
      return;
   }

   space->unitArea = fabsf((space->frame.basis.u.x * space->frame.basis.v.y) -
                           (space->frame.basis.u.y * space->frame.basis.v.x));
   InitUnitCells(space);
}

int GetIndexFromCoords(Space2d *space, Vector2d local_coords)
{
   int i = (int)floorf(local_coords.y - space->grid_origin.y);
   int j = (int)floorf(local_coords.x - space->grid_origin.x);
   int columns = space->columns;
   int rows = space->rows;

   if (i < 0 || j < 0 || i >= rows || j >= columns)
   {
      return -1;
   }

   int cell_index = (i * columns) + j;
   return cell_index;
}

Cell *GetCellFromCoords(Space2d *space, Vector2d local_coords)
{
   int cell_index = GetIndexFromCoords(space, local_coords);
   if (cell_index < 0 || cell_index >= space->cells.count)
   {
      return NULL;
   }
   Cell *cells = space->cells.items;
   Cell *target_cell = &cells[cell_index];

   return target_cell;
}


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

// This function creates a footprint surface that represents the area of effect of an object based on its surface and the coordinate space's basis vectors.
// It calculates the bounding box of the object's surface in world coordinates, determines which cells in the coordinate space it overlaps with, and then creates a rectangular surface that encompasses all those cells.
// The object offset parameter allows you to specify the world coordinates of the object's center, which is necessary to correctly position the footprint in the coordinate space. Provide a zero vector if the object's surface vertices are already in world coordinates.
void CalcSnappedAABB_Vertices(Vector2d *object_surface_vertices, int object_surface_vertices_count, Vector2d object_offset, Basis2d space_basis, Vector2d out_vertices[4])
{
   // Correctly extract the physical grid cell size from the basis
   float cell_w = sqrtf(space_basis.u.x * space_basis.u.x +
                        space_basis.u.y * space_basis.u.y);
   float cell_h = sqrtf(space_basis.v.x * space_basis.v.x +
                        space_basis.v.y * space_basis.v.y);

   if (object_surface_vertices_count == 0)
   {
      return;
   }

   Matrix2x2 box_coords = {0};

   // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
   box_coords.col1 = VectorSum_2d(object_offset, object_surface_vertices[0]);
   box_coords.col2 = VectorSum_2d(object_offset, object_surface_vertices[0]);
   Vector2d vertice = {0};
   for (size_t i = 1; i < object_surface_vertices_count; i++)
   {
      vertice = VectorSum_2d(object_offset, object_surface_vertices[i]); // Convert vertices to absolute world positions to find the global AABB limits

      box_coords.col1.x = fminf(box_coords.col1.x, vertice.x);
      box_coords.col2.x = fmaxf(box_coords.col2.x, vertice.x);

      box_coords.col1.y = fminf(box_coords.col1.y, vertice.y);
      box_coords.col2.y = fmaxf(box_coords.col2.y, vertice.y);
   }

   // Convert the spatial bounding extremes directly to integer cell index spans
   float start_cell_x = floorf(box_coords.col1.x / cell_w);
   float end_cell_x = ceilf(box_coords.col2.x  / cell_w);
   float start_cell_y = floorf(box_coords.col1.y / cell_h);
   float end_cell_y = ceilf(box_coords.col2.y / cell_h);

   // Create a 4-corner bounding rectangle that perfectly encapsulates
   // every single cell the object is partially or fully occupying.
   //LArray footprint_vertices = MakeLArray(4, sizeof(Vector2d));

   // Map the bounding indexes back to absolute spatial coordinates
   out_vertices[0] = (Vector2d){start_cell_x, start_cell_y}; // Bottom-Left Cell Boundary
   out_vertices[1] = (Vector2d){end_cell_x, start_cell_y};   // Bottom-Right Cell Boundary
   out_vertices[2] = (Vector2d){end_cell_x, end_cell_y};     // Top-Right Cell Boundary
   out_vertices[3] = (Vector2d){start_cell_x, end_cell_y};   // Top-Left Cell Boundary
}

// bool VectorIsInSpace_2d(Vector2d vector, Space2d *space)
// {
//    // Check if the vector is within the bounds of the coordinate space defined by its origin and the extents of its basis vectors multiplied by their respective steps
//    Matrix2x2 extents = CalcSpaceExtents_2d(space);
//    Vector2d origin = space->system.origin_in_parent;
//    float min_x = origin.x;
//    float max_x = origin.x + extents.col1.x + extents.col2.x;
//    float min_y = origin.y;
//    float max_y = origin.y + extents.col1.y + extents.col2.y;

//    return (vector.x >= min_x && vector.x < max_x && vector.y >= min_y && vector.y < max_y);
// }

// This function calculates the spatial extents of a coordinate space based on its origin and the extents of its basis vectors multiplied by their respective steps.
// It returns a 2x2 matrix where col1 is the vector from the origin to the far corner along the u direction, and col2 is the vector from the origin to the far corner along the v direction.
//    int cols = coordinate_space->columns;

//    // 1. Correct the counts
//    int numHorizontalLines = rows + 1;
//    int numVerticalLines = cols + 1;

//    coordinate_space->lineSegments_u = *NewDynamicArray(numHorizontalLines, sizeof(LineSegment2d));
//    coordinate_space->lineSegments_v = *NewDynamicArray(numVerticalLines, sizeof(LineSegment2d));

//    // 2. Generate Horizontal-ish Lines (spanning the width)
//    for (int r = 0; r < numHorizontalLines; r++)
//    {
//       LineSegment2d segment;
//       // Start at origin, step DOWN 'r' times using Basis V
//       segment.start.x = space_coords.x + r * coordinate_space->basis.v.x;
//       segment.start.y = space_coords.y + r * coordinate_space->basis.v.y;

//       // Extend across the whole width (cols) using Basis U
//       segment.end.x = segment.start.x + cols * coordinate_space->basis.u.x;
//       segment.end.y = segment.start.y + cols * coordinate_space->basis.u.y;

//       Array_Push(&coordinate_space->lineSegments_u, &segment);
//    }

//    // 3. Generate Vertical-ish Lines (spanning the height)
//    for (int c = 0; c < numVerticalLines; c++)
//    {
//       LineSegment2d segment;
//       // Start at origin, step RIGHT 'c' times using Basis U
//       segment.start.x = space_coords.x + c * coordinate_space->basis.u.x;
//       segment.start.y = space_coords.y + c * coordinate_space->basis.u.y;

//       // Extend down the whole height (rows) using Basis V
//       segment.end.x = segment.start.x + rows * coordinate_space->basis.v.x;
//       segment.end.y = segment.start.y + rows * coordinate_space->basis.v.y;

//       Array_Push(&coordinate_space->lineSegments_v, &segment);
//    }
//}

// Update the values of all cells in the field according to object interactions with them
// void UpdateCellValue(Cell *cell)
// {
//    // Positional
// }

// Update the values of all cells in the field according to object interactions with them
// void FindCellsWithCollisions(Field *field)
// {
//    // If there are more objects than cells, iterate through the cells and check for collisions if the cell has occupancy > 1
//    // If there are more cells than objects, iterate through the objects and check for collisions with the cell they are in
// }

// NOT DONE
// Vector2d GetCellCoordinates(Field field, Vector2d objectPos)
// {
//    Vector2d origin = field.shape.object.position;
//    Vector2d u = field.coordinateSpace.basis.u;
//    Vector2d v = field.coordinateSpace.basis.v;

//    // // Get position relative to the grid origin
//    // float px = objectPos.x - origin.x;
//    // float py = objectPos.y - origin.y;

//    // // Calculate the Determinant
//    // float det = (u.x * v.y) - (u.y * v.x);

//    // // If determinant is 0, the grid is collapsed (invalid)
//    // if (fabs(det) < 0.0001f)
//    //    return (Vector2d){-1, -1};

//    // // Solve for Grid Coordinates (c, r) using the Inverse Matrix logic
//    // float c = (px * v.y - py * v.x) / det;
//    // float r = (py * u.x - px * u.y) / det;

//    // // Use floor() to get the integer index of the cell
//    // return (Vector2d){floorf(c), floorf(r)};
// }

// Update the values of all cells in the field according to object interactions with them
// Field UpdateCellValues(Field field)
// {
//    Collection *cells = &field.coordinateSpace.cells.coll;
//    Collection *objects = &field.items.coll;

//    // Goal = Adjust cell value if the cell's surface has some overlap with the surface that an object's contact vectors create

//    // If there are more cells than objects, iterate through the objects and increment the value for the all cells the object occupies
//    for (size_t i = 0; i < objects->count; i++)
//    {
//       Circloid *circloid_i = (Circloid *)((char *)objects + (i * objects->elemSize));
//       Collection *vertices = &circloid_i->object.surface.surface_vectors.coll;

//       DynamicArray *vector_indices = NewDynamicArray(vertices->count, vertices->elemSize);
//       // Get the surface vectors of the object and find the corresponding cell indices
//       for (size_t i = 0; i < vertices->count; i++)
//       {
//          // Calculate cell indices
//          Vector2d indices = GetCellIndicesFromCoordinates(field.shape.object.position, ((Vector2d*)vertices->items)[i], field.coordinateSpace.basis);
//          Array_Push(vector_indices, &indices);
//       }

//    }
//    // if (cells->count >= objects->count)
//    // {
//    // }

//    // If there are more objects than cells, iterate through the cells and check for collisions if the cell has occupancy > 1
//    // field.

//    return field;
// }

