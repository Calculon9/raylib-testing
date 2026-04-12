/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "physics/rectangloid.h"
#include "physics/circloid.h"
#include "physics/newton_object.h"
#include "math/coordinate_space.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
// void CalculateLineSegmentVectors(CoordSpace2d *coord_space);
void InitUnitCells(CoordSpace2d *coordinate_space);

// Creates a local coordinate space with physical attributes that can be used to render it to the screen. Basis vectors in most cases (orthogonal dimensions) should be u = {1,0}, v = {0,1}.
CoordSpace2d_Grid NewCoordSpace2d_Grid(Vector2d origin, Vector2d resolution_ixj, Basis2d basis, ColourRgba colour_fill, ColourRgba colour_line)
{
   CoordSpace2d_Grid space_obj = {0};
   space_obj.coord_space = NewCoordSpace2d(origin, resolution_ixj, basis);
   space_obj.colour_line = colour_line;
   space_obj.colour_fill = colour_fill;

   Surface2d surface = CreateSurface_Rectangular(resolution_ixj);

   space_obj.object = CreateNewtonObject2d_Static(origin, surface);
   return space_obj;
}

// Creates a local coordinate space. Basis vectors in most cases (orthogonal dimensions) should be u = {1,0}, v = {0,1}. They can be other values if the space dimensions are not orthogonal.
CoordSpace2d NewCoordSpace2d(Vector2d origin, Vector2d resolution_ixj, Basis2d basis)
{
   CoordSpace2d space = {0};
   space.coords_origin;
   // Needs a grid with resolution according to unit height and width - divide it up and handle the leftover height and width
   // if (unitHeight > object.height || unitWidth > object.width)
   // {
   //    fprintf(stderr, "ERROR: unitHeight and/or unitWidth are greater than the desired height and/or width. Cannot create such a field!");
   //    return field;
   // };
   // Create the underlying object and define its surface

   space.basis = basis;
   space.resolution_ixj = resolution_ixj;
   space.stepsU = ceilf((float)resolution_ixj.x / VectorMagnitude_2d(basis.u));
   space.stepsV = ceilf((float)resolution_ixj.y / VectorMagnitude_2d(basis.v));
   space.unitArea = fabsf((basis.u.x * basis.v.y) - (basis.u.y * basis.v.x)); // Calculate the 'Area' of a single basis tile; this is the determinant of the basis matrix, which gives us the area of the parallelogram formed by the basis vectors, which is the area of each cell in the coordinate space. We can then divide the total area of the field by this cell area to get the total number of cells needed to fill the field.

   // If cellArea is 0, the basis is invalid (it's a line, not a space)
   if (space.unitArea < 0.0001f)
   {
      fprintf(stderr, "ERROR: Invalid basis vectors. The area of the basis tile is too small (close to zero). Cannot create coordinate space with these basis vectors!");
   }

   // Total area and units of the bounding box/object
   float totalArea = resolution_ixj.x * resolution_ixj.y;
   int totalUnits = (int)ceilf(totalArea / space.unitArea);

   space.cells = NewDynamicArray(totalUnits, sizeof(Cell));

   // CalculateLineSegmentVectors(&coordinate_space);
   InitUnitCells(&space);

   // LOG FIELD INFO
   Vector2d basis_u = space.basis.u;
   Vector2d basis_v = space.basis.v;

   // char text[64]; // Buffer to hold the text
   // snprintf(text, sizeof(text), "FIELD INITIALISED:  Dimensions (%d, %d); Units (%d); Basis -> u = [%d,%d], v = [%d,%d].\n", unitW, unitH, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);
   printf("COORD.SPACE INITIALISED:  Dimensions (W:%0.2f, H:%0.2f); Units (%d); Basis -> u = [%0.2f,%0.2f], v = [%0.2f,%0.2f].\n", resolution_ixj.x, resolution_ixj.y, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);

   return space;
}

// Calculate coordinate space lines relative to the space's world position
// void CalculateLineSegmentVectors(CoordSpace2d *coordinate_space)
// {
//    Vector2d space_coords = coordinate_space->object.newtonian_properties.world_position;
//    int rows = coordinate_space->rows;
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

// NEEDS REDOING
void InitUnitCells(CoordSpace2d *space)
{
   Collection *cells = &space->cells->coll;
   size_t cells_capacity = cells->capacity;
   memset(cells->items, 0, cells->elemSize * cells_capacity);

   Vector2d coords_origin = space->coords_origin;
   int stepsU = space->stepsU;
   int stepsV = space->stepsV;

   int count = 0;
   for (int k = 0; k < stepsU * stepsV; k++)
   {
      int i = k / stepsU; // Row index (based on horizontal lines)
      int j = k % stepsU; // Column index (based on vertical lines)
      // Cell *cell = (Cell *)((char *)cells->items + (k * cells->elemSize));
      // Vector2d cell_coords = cell->coords;

      Cell cell;

      // Scale the basis vectors (u,v) and add them to get the displacement from the origin
      Vector2d scaled_u = {j * space->basis.u.x, j * space->basis.u.y};
      Vector2d scaled_v = {i * space->basis.v.x, i * space->basis.v.y};
      Vector2d displacement = {scaled_u.x + scaled_v.x, scaled_u.y + scaled_v.y};

      // Add the displacement vector to the origin to get the coordinates of the cell
      cell.coords.x = coords_origin.x + displacement.x;
      cell.coords.y = coords_origin.y + displacement.y;

      cell.value = 0.0f;  // Initialize the cell value to 0
      cell.occupancy = 0; // Initialize the cell occupancy to 0

      // Write the cell to the array
      Cell *address = (Cell *)((char *)cells->items + (k * cells->elemSize));
      memcpy(address, &cell, cells->elemSize);
      count++;
      // for (int j = 0; j < stepsV; j++)
      // {
      //    // The c and r represent the column and row index of the cell respectively, so we can calculate the position of the cell by scaling the basis vectors by the column and row index and adding it to the origin
      //    int index = (i * stepsV + j); // Convert 2D row and column index to linear index
      //    Cell cell;

      //    // Scale the basis vectors (u,v) and add them to get the displacement from the origin
      //    Vector2d scaled_u = {j * space->basis.u.x, j * space->basis.u.y};
      //    Vector2d scaled_v = {i * space->basis.v.x, i * space->basis.v.y};
      //    Vector2d displacement = {scaled_u.x + scaled_v.x, scaled_u.y + scaled_v.y};

      //    // Add the displacement vector to the origin to get the coordinates of the cell
      //    cell.coords.x = coords.x + displacement.x;
      //    cell.coords.y = coords.y + displacement.y;

      //    cell.value = 0.0f;  // Initialize the cell value to 0
      //    cell.occupancy = 0; // Initialize the cell occupancy to 0

      //    // Write the cell to the array
      //    Cell *address = (Cell *)((char *)cells->items + (index * cells->elemSize));
      //    memcpy(address, &cell, cells->elemSize);
      //    count++;
      //    // printf("Initialised Cell (%d,%d)\n", i + 1, j + 1);
      // }
   }
   cells->count = stepsU * stepsV;
   printf("Initialised %d cells\n", count);
}

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