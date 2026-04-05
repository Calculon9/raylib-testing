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
#include "physics/field.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------

// Creates a static/immovable rectangloid container with the given rectangloid and items to be contained
Field CreateField(Rectangloid object, int rows, int columns, ColourRgba lineColour, DynamicArray *items)
{
   Field field = {0};

   // Needs a grid with resolution according to unit height and width - divide it up and handle the leftover height and width
   // if (unitHeight > object.height || unitWidth > object.width)
   // {
   //    fprintf(stderr, "ERROR: unitHeight and/or unitWidth are greater than the desired height and/or width. Cannot create such a field!");
   //    return field;
   // };

   field.items = *items;
   field.shape = object;
   field.lineColour = lineColour;

   int totalWidth = (int)object.width;
   int totalHeight = (int)object.height;

   int unitW = totalWidth / columns;
   int unitH = totalHeight / rows;

   columns = ceil((float)totalWidth / unitW);
   rows = ceil((float)totalHeight / unitH);

   field.coordinateSpace.rows = rows;
   field.coordinateSpace.columns = columns;

   // Set the basis vectors for the field;
   // Initialise each basis vector to align with the x and y axes respectively, and have a magnitude equal to the unit width and height respectively, so that we can scale them with a scalar to get the position of any field unit in the field coordinate space
   field.coordinateSpace.basis.u = (Vector2d){unitW, 0};
   field.coordinateSpace.basis.v = (Vector2d){0, unitH};

   int totalUnits = rows * columns;
   field.coordinateSpace.cells = *NewDynamicArray(totalUnits, sizeof(Cell));

   field = CalculateFieldLines(field);
   field = InitialiseFieldCells(field);

   // LOG FIELD INFO
   Vector2d basis_u = field.coordinateSpace.basis.u;
   Vector2d basis_v = field.coordinateSpace.basis.v;
   // char text[64]; // Buffer to hold the text
   // snprintf(text, sizeof(text), "FIELD INITIALISED:  Dimensions (%d, %d); Units (%d); Basis -> u = [%d,%d], v = [%d,%d].\n", unitW, unitH, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);
   printf("FIELD INITIALISED:  Dimensions (W:%d, H:%d); Units (%d); Basis -> u = [%d,%d], v = [%d,%d].\n", object.width, object.height, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);
   // printf("Created field with dimensions (%f, %f) and unit dimensions (%f, %f). Total field units is %d.\n", object.width, object.height, unitW, unitH, totalUnits);
   //  printf("Adjusted: Total field units is %f.\n", totalUnits);
   //   The unitWidth and unitHeight may not go evenly into the total width and height
   //   Adjust both so that they do go evenly into the total width and height, and recalculate the number of row and column units accordingly
   //   float unitHeight_calc = object.height / (object.height / unitHeight);
   //   float unitWidth_calc = object.width / (object.width / unitWidth);
   //   float totalUnits_calc = (object.height / unitHeight_calc) * (object.width / unitWidth_calc);
   //   float leftoverHeight_calc = object.height - (int)(object.height / unitHeight) * unitHeight;
   //   float leftoverWidth_calc = object.width - (int)(object.width / unitWidth) * unitWidth;
   //   float rowUnits_calc = object.height / unitHeight;
   //   float columnUnits_calc = object.width / unitWidth;

   // Adjusted units and dimensions to account for any leftover width and height that doesn't go evenly into the total width and height
   // float rowUnits_adj = rowUnits_calc;
   // float columnUnits_adj = columnUnits_calc;

   // Adjust column and row units
   // float leftoverHeight = leftoverHeight_calc;
   // float leftoverWidth = leftoverWidth_calc;

   // if (leftoverHeight > 0.5 * unitHeight)
   // {
   //    if (leftoverHeight < columnUnits_calc)
   //    {
   //       // Just round up to next integer.
   //       columnUnits_adj = ceil(columnUnits_calc);
   //    }
   //    else
   //    {
   //       // If the leftover height is greater than the number of column units, then add columnUnits in chunks of the unitHeight
   //       float additionalColumnUnits = floor(leftoverHeight / unitHeight);
   //       columnUnits_adj += additionalColumnUnits;
   //       //leftoverHeight -= additionalColumnUnits * unitHeight;
   //    }
   //    printf("Added more column units to account for leftover height. New column units: %f\n", columnUnits_adj);
   // }
   // if (leftoverWidth > 0)
   // {
   //    if (leftoverWidth < rowUnits_calc)
   //    {
   //       // Just add another row unit for the leftover width
   //       rowUnits_adj = ceil(rowUnits_calc);
   //    }
   //    else
   //    {
   //       // If the leftover width is greater than the number of row units, then we need to increase the unit width so that it goes evenly into the total width
   //       float additionalWidthUnits = floor(leftoverWidth / unitWidth);
   //       rowUnits_adj += additionalWidthUnits;
   //       //unitWidth = object.width / (object.width / (unitWidth + (leftoverWidth / rowUnits_calc)));
   //    }
   //    printf("Added more row units to account for leftover width. New row units: %f\n", rowUnits_adj);
   // }

   // TODO: We may also want to adjust the unit height and width to account for the leftover height and width that doesn't go evenly into the total height and width, so that the field units go all the way to the edge of the field container
   // Recalculated column and row unit lengths
   // float unitHeight_adj = object.height / rowUnits_adj;
   // float unitWidth_adj = object.width / columnUnits_adj;

   // Output some debug info to verify calculations are correct
   // printf("Init Unit Width: %f; Init Width Units: %f; Init Calc Width: %f; Actual Width: %f; Leftover Width: %f\n", unitWidth, columnUnits_calc, unitWidth_calc, object.width, leftoverWidth_calc);
   // printf("Init Unit Height: %f; Init Height Units: %f; Init Calc Height: %f; Actual Height: %f; Leftover Height: %f\n", unitHeight, rowUnits_calc, unitHeight_calc, object.height, leftoverHeight_calc);
   // float totalUnitWidth = columnUnits_adj * unitWidth;
   // float totalUnitHeight = rowUnits_adj * unitHeight;
   // float totalUnits = rowUnits_adj * columnUnits_adj;
   // printf("Adj Unit Width: %f; Adj Width Units: %f; Adj Calc Width: %f; Actual Width: %f\n", unitWidth, columnUnits_adj, unitWidth, object.width);
   // printf("Adj Unit Height: %f; Adj Height Units: %f; Adj Calc Height: %f; Actual Height: %f\n", unitHeight, rowUnits_adj, unitHeight, object.height);
   // printf("Adjusted: Field width and height is (%f, %f).\n", totalUnitWidth, totalUnitHeight);

   // Calculate initial field unit values
   // CalculateField_Rect(field);

   // Method 1: Formulate equations of vertical and horizontal lines to create a grid, and use the equations to determine which field units are occupied by rectangloid objects

   // Need a 2D vector to represent the unit/basis vectors that will be scaled with a scalar in order to map to a field position
   // Method 2: Create linear array of field units with the given height and width, and unit height and width
   return field;
}

Field CalculateFieldLines(Field field)
{
   Vector2d origin = field.shape.newtonian_properties.world_position;
   int rows = field.coordinateSpace.rows;
   int cols = field.coordinateSpace.columns;

   // 1. Correct the counts
   int numHorizontalLines = rows + 1;
   int numVerticalLines = cols + 1;

   field.coordinateSpace.lineSegments_u = *NewDynamicArray(numHorizontalLines, sizeof(LineSegment2d));
   field.coordinateSpace.lineSegments_v = *NewDynamicArray(numVerticalLines, sizeof(LineSegment2d));

   // 2. Generate Horizontal-ish Lines (spanning the width)
   for (int r = 0; r < numHorizontalLines; r++)
   {
      LineSegment2d segment;
      // Start at origin, step DOWN 'r' times using Basis V
      segment.start.x = origin.x + r * field.coordinateSpace.basis.v.x;
      segment.start.y = origin.y + r * field.coordinateSpace.basis.v.y;

      // Extend across the whole width (cols) using Basis U
      segment.end.x = segment.start.x + cols * field.coordinateSpace.basis.u.x;
      segment.end.y = segment.start.y + cols * field.coordinateSpace.basis.u.y;

      Array_Push(&field.coordinateSpace.lineSegments_u, &segment);
   }

   // 3. Generate Vertical-ish Lines (spanning the height)
   for (int c = 0; c < numVerticalLines; c++)
   {
      LineSegment2d segment;
      // Start at origin, step RIGHT 'c' times using Basis U
      segment.start.x = origin.x + c * field.coordinateSpace.basis.u.x;
      segment.start.y = origin.y + c * field.coordinateSpace.basis.u.y;

      // Extend down the whole height (rows) using Basis V
      segment.end.x = segment.start.x + rows * field.coordinateSpace.basis.v.x;
      segment.end.y = segment.start.y + rows * field.coordinateSpace.basis.v.y;

      Array_Push(&field.coordinateSpace.lineSegments_v, &segment);
   }
   return field;
   // NewtonObject2d *newtObj = Enumerate(field.items->coll);
   // if (newtObj == NULL)
   // {
   //    fprintf(stderr, "Failed to retrieve enumerated Object\n"); // Enumerator failed to retrieve the first item
   // }
   // while (newtObj != NULL)
   // {
   //    if (&newtObj != NULL)
   //    {
   //       // Increment the scalar for the field unit's position the object is currently in
   //       // We will go by its positional center for now
   //       Vector2d pos = newtObj->pos;
   //       int x = (int)pos.x;
   //       int y = (int)pos.y;
   //       Vector2d cell = GetCellFromWorld(field, newtObj->pos);

   //       int index = -1;
   //       if (cell.x >= 0 && cell.x < field.coordinateSpace.columns && cell.y >= 0 && cell.y < field.coordinateSpace.rows)
   //       {
   //          // You are officially inside the grid!
   //          int index = (int)cell.y * field.coordinateSpace.columns + (int)cell.x; // Convert the 2D position to a linear index
   //       }
   //       else
   //       {
   //          // You are outside the grid!
   //          fprintf(stderr, "Object is outside the field grid! Object position: (%f, %f); Cell: (%f, %f)\n", pos.x, pos.y, cell.x, cell.y);
   //          newtObj = Enumerate(field.items->coll);
   //          continue;
   //       }

   //       // int i = (int)cell.x / field.basis_u.x;
   //       // int j = (int)pos.y / field.basis_v.y;
   //       // int i_field = (j * field.columns) + i; // Convert the 2D grid position to a linear index

   //       void *target = (char *)field.grid->coll->items + (index * field.grid->coll->elemSize);
   //       *(float *)target += 1.0;

   //       // Or 1. Get the address
   //       // void *target = (char *)field.grid->coll->items + (i_field * field.grid->coll->elemSize);
   //       // // 2. Tell C it's a double pointer and "dereference" it to set the value
   //       // *(double *)target = 1.0;
   //    }
   //    newtObj = Enumerate(field.items->coll);
   // }
   // ResetEnumerator(field.items->coll); // Reset enumerator once done
   // Enumerate(field.items->coll);
}

Field InitialiseFieldCells(Field field)
{
   Collection cells = field.coordinateSpace.cells.coll;
   size_t cells_capacity = cells.capacity;
   memset(cells.items, 0, cells.elemSize * cells_capacity);

   Vector2d origin = field.shape.newtonian_properties.world_position;
   int rows = field.coordinateSpace.rows;
   int cols = field.coordinateSpace.columns;

   int count = 0;
   for (int r = 0; r < rows; r++)
   {
      for (int c = 0; c < cols; c++)
      {
         // The c and r represent the column and row index of the cell respectively, so we can calculate the position of the cell by scaling the basis vectors by the column and row index and adding it to the origin
         int index = (r * cols + c); // Convert 2D row and column index to linear index
         Cell cell;

         // Scale the basis vectors (u,v) and add them to get the displacement from the origin
         Vector2d scaled_u = {c * field.coordinateSpace.basis.u.x, c * field.coordinateSpace.basis.u.y};
         Vector2d scaled_v = {r * field.coordinateSpace.basis.v.x, r * field.coordinateSpace.basis.v.y};
         Vector2d displacement = {scaled_u.x + scaled_v.x, scaled_u.y + scaled_v.y};

         // Add the displacement vector to the origin to get the coordinates of the cell
         cell.coordinates.x = origin.x + displacement.x;
         cell.coordinates.y = origin.y + displacement.y;

         cell.value = 0.0f;  // Initialize the cell value to 0
         cell.occupancy = 0; // Initialize the cell occupancy to 0

         // Write the cell to the array
         Cell *address = (Cell *)((char *)cells.items + (index * cells.elemSize));
         memcpy(address, &cell, cells.elemSize);
         count++;
         printf("Initialised Cell (%d,%d)\n", r + 1, c + 1);
      }
   }
   cells.count = rows * cols;
   printf("Initialised %d cells\n", count);
   return field;
}

// Gets the cell indices (i,j) (zero based) from the provided input coordinates
Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis)
{
   // Get position relative to the grid origin
   float px = input_coordinates.x - origin_coordinates.x;
   float py = input_coordinates.y - origin_coordinates.y;

   // Calculate the Determinant
   float det = (basis.u.x * basis.v.y) - (basis.u.y * basis.v.x);

   // If determinant is 0, the grid is collapsed (invalid)
   if (fabs(det) < 0.0001f) 
   {
      return (Vector2d){-1, -1};
   }

   // Solve for Grid Coordinates (i, j) using the Inverse Matrix logic
   float i = (py * basis.u.x - px * basis.u.y) / det;
   float j = (px * basis.v.y - py * basis.v.x) / det;

   // Use floor() to get the integer index of the cell
   return (Vector2d){floorf(i), floorf(j)};
}

// Update the values of all cells in the field according to object interactions with them
Field UpdateFieldCellValues(Field field)
{
   Collection *cells = &field.coordinateSpace.cells.coll;
   Collection *objects = &field.items.coll;

   // Goal = Adjust cell value if the cell's surface has some overlap with the surface that an object's contact vectors create

   // If there are more cells than objects, iterate through the objects and increment the value for the all cells the object occupies
   for (size_t i = 0; i < objects->count; i++)
   { 
      Circloid *circloid_i = (Circloid *)((char *)objects + (i * objects->elemSize));
      Collection *vertices = &circloid_i->newtonian_properties.surface.surface_vectors.coll;

      DynamicArray *vector_indices = NewDynamicArray(vertices->count, vertices->elemSize);
      // Get the surface vectors of the object and find the corresponding cell indices
      for (size_t i = 0; i < vertices->count; i++)
      {
         // Calculate cell indices
         Vector2d indices = GetCellIndicesFromCoordinates(field.shape.newtonian_properties.world_position, ((Vector2d*)vertices->items)[i], field.coordinateSpace.basis);
         Array_Push(vector_indices, &indices);
      }
      
      
      
   }
   // if (cells->count >= objects->count)
   // {
   // }

   // If there are more objects than cells, iterate through the cells and check for collisions if the cell has occupancy > 1
   // field.

   return field;
}

// Update the values of all cells in the field according to object interactions with them
void UpdateFieldCellValue(Cell *cell)
{
   // Positional
}

// Update the values of all cells in the field according to object interactions with them
void FindCellsWithCollisions(Field *field)
{
   // If there are more objects than cells, iterate through the cells and check for collisions if the cell has occupancy > 1
   // If there are more cells than objects, iterate through the objects and check for collisions with the cell they are in
}

// NOT DONE
Vector2d GetCellCoordinates(Field field, Vector2d objectPos)
{
   Vector2d origin = field.shape.newtonian_properties.world_position;
   Vector2d u = field.coordinateSpace.basis.u;
   Vector2d v = field.coordinateSpace.basis.v;

   // // Get position relative to the grid origin
   // float px = objectPos.x - origin.x;
   // float py = objectPos.y - origin.y;

   // // Calculate the Determinant
   // float det = (u.x * v.y) - (u.y * v.x);

   // // If determinant is 0, the grid is collapsed (invalid)
   // if (fabs(det) < 0.0001f)
   //    return (Vector2d){-1, -1};

   // // Solve for Grid Coordinates (c, r) using the Inverse Matrix logic
   // float c = (px * v.y - py * v.x) / det;
   // float r = (py * u.x - px * u.y) / det;

   // // Use floor() to get the integer index of the cell
   // return (Vector2d){floorf(c), floorf(r)};
}