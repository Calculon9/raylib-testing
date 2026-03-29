/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "physics/rectangloid.h"
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

   field.items = items;
   field.shape = object;
   field.lineColour = lineColour;

   int totalWidth = (int)object.width;
   int totalHeight = (int)object.height;

   int unitW = totalWidth / columns;
   int unitH = totalHeight / rows;

   columns = ceil((float)totalWidth / unitW);
   rows = ceil((float)totalHeight / unitH);

   field.gridSpace.rows = rows;
   field.gridSpace.columns = columns;

   // Set the basis vectors for the field;
   // Initialise each basis vector to align with the x and y axes respectively, and have a magnitude equal to the unit width and height respectively, so that we can scale them with a scalar to get the position of any field unit in the field coordinate space
   field.gridSpace.basis.u = (Vector2d){unitW, 0};
   field.gridSpace.basis.v = (Vector2d){0, unitH};

   int totalUnits = rows * columns;

   field.grid = NewDynamicArray(totalUnits, sizeof(Cell)); // AllocateArray((object.height / unitHeight) * (object.width / unitWidth), sizeof(float));

   field = CalculateField(field);

   // printf("Adjusted: Total field units is %f.\n", totalUnits);
   //  The unitWidth and unitHeight may not go evenly into the total width and height
   //  Adjust both so that they do go evenly into the total width and height, and recalculate the number of row and column units accordingly
   //  float unitHeight_calc = object.height / (object.height / unitHeight);
   //  float unitWidth_calc = object.width / (object.width / unitWidth);
   //  float totalUnits_calc = (object.height / unitHeight_calc) * (object.width / unitWidth_calc);
   //  float leftoverHeight_calc = object.height - (int)(object.height / unitHeight) * unitHeight;
   //  float leftoverWidth_calc = object.width - (int)(object.width / unitWidth) * unitWidth;
   //  float rowUnits_calc = object.height / unitHeight;
   //  float columnUnits_calc = object.width / unitWidth;

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

Field CalculateField(Field field)
{
   // Map object positions to the grid for all objects
   // if (field.items == NULL || field.items->coll == NULL || field.items->coll->count <= 0)
   // {
   //    printf("No items are in the provided field. Nothing to calculate.");
   //    return field; // No objects to update
   // }

   // Zero the field unit values before recalculating them based on the current object positions, so that they don't keep increasing indefinitely as the objects move around the field
   memset(field.grid->coll->items, 0, field.grid->coll->elemSize * field.grid->coll->count);

   Vector2d origin = field.shape.object.pos;
   int rows = field.gridSpace.rows;
   int cols = field.gridSpace.columns;

   // 1. Correct the counts
   int numHorizontalLines = rows + 1;
   int numVerticalLines = cols + 1;

   field.gridSpace.lineSegments_u = NewDynamicArray(numHorizontalLines, sizeof(LineSegment2d));
   field.gridSpace.lineSegments_v = NewDynamicArray(numVerticalLines, sizeof(LineSegment2d));

   // 2. Generate Horizontal-ish Lines (spanning the width)
   for (int r = 0; r < numHorizontalLines; r++)
   {
      LineSegment2d segment;
      // Start at origin, step DOWN 'r' times using Basis V
      segment.start.x = origin.x + r * field.gridSpace.basis.v.x;
      segment.start.y = origin.y + r * field.gridSpace.basis.v.y;

      // Extend across the whole width (cols) using Basis U
      segment.end.x = segment.start.x + cols * field.gridSpace.basis.u.x;
      segment.end.y = segment.start.y + cols * field.gridSpace.basis.u.y;

      Array_Push(field.gridSpace.lineSegments_u, &segment);
   }

   // 3. Generate Vertical-ish Lines (spanning the height)
   for (int c = 0; c < numVerticalLines; c++)
   {
      LineSegment2d segment;
      // Start at origin, step RIGHT 'c' times using Basis U
      segment.start.x = origin.x + c * field.gridSpace.basis.u.x;
      segment.start.y = origin.y + c * field.gridSpace.basis.u.y;

      // Extend down the whole height (rows) using Basis V
      segment.end.x = segment.start.x + rows * field.gridSpace.basis.v.x;
      segment.end.y = segment.start.y + rows * field.gridSpace.basis.v.y;

      Array_Push(field.gridSpace.lineSegments_v, &segment);
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
   //       if (cell.x >= 0 && cell.x < field.gridSpace.columns && cell.y >= 0 && cell.y < field.gridSpace.rows)
   //       {
   //          // You are officially inside the grid!
   //          int index = (int)cell.y * field.gridSpace.columns + (int)cell.x; // Convert the 2D position to a linear index
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

Vector2d GetCellFromWorld(Field field, Vector2d objectPos)
{
   Vector2d origin = field.shape.object.pos;
   Vector2d u = field.gridSpace.basis.u;
   Vector2d v = field.gridSpace.basis.v;

   // Get position relative to the grid origin
   float px = objectPos.x - origin.x;
   float py = objectPos.y - origin.y;

   // Calculate the Determinant
   float det = (u.x * v.y) - (u.y * v.x);

   // If determinant is 0, the grid is collapsed (invalid)
   if (fabs(det) < 0.0001f)
      return (Vector2d){-1, -1};

   // Solve for Grid Coordinates (c, r) using the Inverse Matrix logic
   float c = (px * v.y - py * v.x) / det;
   float r = (py * u.x - px * u.y) / det;

   // Use floor() to get the integer index of the cell
   return (Vector2d){floorf(c), floorf(r)};
}

// void Container_Rect_GetCollisionObjects(Rectangloid rect);
//  Rectangloid CreateRectangloid(float height, float width, ColourRgba colour, size_t mass, Vector2d position, Velocity2d velocity, Acceleration2d acceleration)
//  {
//     NewtonObject2d newtOb = CreateNewtonObject2d(mass, position, velocity, acceleration);
//     // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
//     Rectangloid newRect = {0};
//     newRect.object = newtOb;
//     newRect.height = height;
//     newRect.width = width;
//     newRect.colourRgba = colour;

//    return newRect;
//    // Initialize momentum based on mass and velocity
// }

// Rectangloid CreateRectangloid_FromObject(NewtonObject2d newtOb, float height, float width, ColourRgba colour)
// {
//    // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
//    Rectangloid newRect = {0};
//    newRect.object = newtOb;
//    newRect.height = height;
//    newRect.width = width;
//    newRect.colourRgba = colour;

//    return newRect;
// }

// void Rectangloid_GetCollisionObjects(Rectangloid rect)
// {

// }