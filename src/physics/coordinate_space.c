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
void CalculateLineSegmentVectors(CoordinateSpace2d *coordinate_space);
void InitialiseUnitCells(CoordinateSpace2d *coordinate_space);

// Creates a static/immovable rectangloid container with the given rectangloid and items to be contained
CoordinateSpace2d CreateCoordinateSpace(Rectangloid object, Vector2d resolution_ixj, Basis2d basis, ColourRgba lineColour)
{
   CoordinateSpace2d coordinate_space = {0};

   // Needs a grid with resolution according to unit height and width - divide it up and handle the leftover height and width
   // if (unitHeight > object.height || unitWidth > object.width)
   // {
   //    fprintf(stderr, "ERROR: unitHeight and/or unitWidth are greater than the desired height and/or width. Cannot create such a field!");
   //    return field;
   // };

   coordinate_space.object = object;
   coordinate_space.lineColour = lineColour;
   coordinate_space.basis = basis;
   coordinate_space.resolution_ixj = resolution_ixj;

   // Calculate the 'Area' of a single basis tile; this is the determinant of the basis matrix, which gives us the area of the parallelogram formed by the basis vectors, which is the area of each cell in the coordinate space. We can then divide the total area of the field by this cell area to get the total number of cells needed to fill the field.
   float cellArea = fabsf((basis.u.x * basis.v.y) - (basis.u.y * basis.v.x));

   // If cellArea is 0, the basis is invalid (it's a line, not a space)
   if (cellArea < 0.0001f)
   {
      fprintf(stderr, "ERROR: Invalid basis vectors. The area of the basis tile is too small (close to zero). Cannot create coordinate space with these basis vectors!");
   }

   // Total area of the bounding box/object
   float totalArea = resolution_ixj.x * resolution_ixj.y;

   // Total units needed to fill that area
   int totalUnits = (int)ceilf(totalArea / cellArea);

   // int totalWidth = (int)resolution_ixj.x;
   // int totalHeight = (int)resolution_ixj.y;

   // int unitW = totalWidth / columns;
   // int unitH = totalHeight / rows;

   //int basis_u_units = ceil((float)resolution_ixj.x / VectorMagnitude_2d(basis.u));
   //int basis_v_units = ceil((float)resolution_ixj.y / VectorMagnitude_2d(basis.v));
   // basis_v_units = ceil((float)totalHeight / unitH);

   // coordinate_space.rows = rows;
   // coordinate_space.columns = columns;

   // Set the basis vectors for the field;
   // Initialise each basis vector to align with the x and y axes respectively, and have a magnitude equal to the unit width and height respectively, so that we can scale them with a scalar to get the position of any field unit in the field coordinate space
   // coordinate_space.basis.u = (Vector2d){unitW, 0};
   // coordinate_space.basis.v = (Vector2d){0, unitH};

   //int totalUnits = basis_u_units * basis_v_units; // The total number of units in the field is the area of the field divided by the area of each unit, which is equivalent to the determinant of the basis matrix (u.x * v.y - u.y * v.x), but since we are assuming orthogonal basis vectors for now, we can just multiply the magnitudes of the basis vectors together to get the total number of units
   coordinate_space.cells = NewDynamicArray(totalUnits, sizeof(Cell));

   // CalculateLineSegmentVectors(&coordinate_space);
   //InitialiseUnitCells(&coordinate_space);

   // LOG FIELD INFO
   Vector2d basis_u = coordinate_space.basis.u;
   Vector2d basis_v = coordinate_space.basis.v;
   // char text[64]; // Buffer to hold the text
   // snprintf(text, sizeof(text), "FIELD INITIALISED:  Dimensions (%d, %d); Units (%d); Basis -> u = [%d,%d], v = [%d,%d].\n", unitW, unitH, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);
   printf("COORD.SPACE INITIALISED:  Dimensions (W:%d, H:%d); Units (%d); Basis -> u = [%0.2f,%0.2f], v = [%0.2f,%0.2f].\n", resolution_ixj.x, resolution_ixj.y, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);

   return coordinate_space;
}

// Calculate coordinate space lines relative to the space's world position
void CalculateLineSegmentVectors(CoordinateSpace2d *coordinate_space)
{
   Vector2d space_coords = coordinate_space->object.newtonian_properties.world_position;
   int rows = coordinate_space->rows;
   int cols = coordinate_space->columns;

   // 1. Correct the counts
   int numHorizontalLines = rows + 1;
   int numVerticalLines = cols + 1;

   coordinate_space->lineSegments_u = *NewDynamicArray(numHorizontalLines, sizeof(LineSegment2d));
   coordinate_space->lineSegments_v = *NewDynamicArray(numVerticalLines, sizeof(LineSegment2d));

   // 2. Generate Horizontal-ish Lines (spanning the width)
   for (int r = 0; r < numHorizontalLines; r++)
   {
      LineSegment2d segment;
      // Start at origin, step DOWN 'r' times using Basis V
      segment.start.x = space_coords.x + r * coordinate_space->basis.v.x;
      segment.start.y = space_coords.y + r * coordinate_space->basis.v.y;

      // Extend across the whole width (cols) using Basis U
      segment.end.x = segment.start.x + cols * coordinate_space->basis.u.x;
      segment.end.y = segment.start.y + cols * coordinate_space->basis.u.y;

      Array_Push(&coordinate_space->lineSegments_u, &segment);
   }

   // 3. Generate Vertical-ish Lines (spanning the height)
   for (int c = 0; c < numVerticalLines; c++)
   {
      LineSegment2d segment;
      // Start at origin, step RIGHT 'c' times using Basis U
      segment.start.x = space_coords.x + c * coordinate_space->basis.u.x;
      segment.start.y = space_coords.y + c * coordinate_space->basis.u.y;

      // Extend down the whole height (rows) using Basis V
      segment.end.x = segment.start.x + rows * coordinate_space->basis.v.x;
      segment.end.y = segment.start.y + rows * coordinate_space->basis.v.y;

      Array_Push(&coordinate_space->lineSegments_v, &segment);
   }
}

//NEEDS REDOING
void InitialiseUnitCells(CoordinateSpace2d *coordinate_space)
{
   Collection *cells = &coordinate_space->cells->coll;
   size_t cells_capacity = cells->capacity;
   memset(cells->items, 0, cells->elemSize * cells_capacity);

   Vector2d space_coords = coordinate_space->object.newtonian_properties.world_position;
   int rows = coordinate_space->rows;
   int cols = coordinate_space->columns;

   int count = 0;
   for (int r = 0; r < rows; r++)
   {
      for (int c = 0; c < cols; c++)
      {
         // The c and r represent the column and row index of the cell respectively, so we can calculate the position of the cell by scaling the basis vectors by the column and row index and adding it to the origin
         int index = (r * cols + c); // Convert 2D row and column index to linear index
         Cell cell;

         // Scale the basis vectors (u,v) and add them to get the displacement from the origin
         Vector2d scaled_u = {c * coordinate_space->basis.u.x, c * coordinate_space->basis.u.y};
         Vector2d scaled_v = {r * coordinate_space->basis.v.x, r * coordinate_space->basis.v.y};
         Vector2d displacement = {scaled_u.x + scaled_v.x, scaled_u.y + scaled_v.y};

         // Add the displacement vector to the origin to get the coordinates of the cell
         cell.world_coordinates.x = space_coords.x + displacement.x;
         cell.world_coordinates.y = space_coords.y + displacement.y;

         cell.value = 0.0f;  // Initialize the cell value to 0
         cell.occupancy = 0; // Initialize the cell occupancy to 0

         // Write the cell to the array
         Cell *address = (Cell *)((char *)cells->items + (index * cells->elemSize));
         memcpy(address, &cell, cells->elemSize);
         count++;
         printf("Initialised Cell (%d,%d)\n", r + 1, c + 1);
      }
   }
   cells->count = rows * cols;
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