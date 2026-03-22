/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/

#include <stdio.h>
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
Field_Rect CreateField_Rect(Rectangloid object, float unitHeight, float unitWidth, DynamicArray *items)
{
   Field_Rect field = {0};

   // Needs a grid with resolution according to unit height and width - divide it up and handle the leftover height and width
   if (unitHeight > object.height || unitWidth > object.width)
   {
      fprintf(stderr, "ERROR: unitHeight and/or unitWidth are greater than the desired height and/or width. Cannot create such a field!");
      return field;
   };

   field.items = items;
   field.shape = object;

   // The unitWidth and unitHeight may not go evenly into the total width and height
   // Adjust both so that they do go evenly into the total width and height, and recalculate the number of row and column units accordingly
   unitHeight = object.height / (object.height / unitHeight);
   unitWidth = object.width / (object.width / unitWidth);
   float leftoverHeight = object.height - (int)(object.height / unitHeight) * unitHeight;
   float leftoverWidth = object.width - (int)(object.width / unitWidth) * unitWidth;
   float rowUnits = object.height / unitHeight;
   float columnUnits = object.width / unitWidth;
   if (leftoverHeight > 0)
   {
      if (leftoverHeight < columnUnits)
      {
         // Just add another column unit for the leftover height
      } else
      {
         // If the leftover height is greater than the number of column units, then we need to increase the unit height so that it goes evenly into the total height
         unitHeight = object.height / (object.height / (unitHeight + (leftoverHeight / columnUnits)));
      }
   }
   if (leftoverWidth > 0)
   {
      if (leftoverWidth < rowUnits)
      {
         // Just add another row unit for the leftover width
      } else
      {
         // If the leftover width is greater than the number of row units, then we need to increase the unit width so that it goes evenly into the total width
         unitWidth = object.width / (object.width / (unitWidth + (leftoverWidth / rowUnits)));
      }
   }

   field.row_units = object.width / unitWidth;
   field.column_units = object.height / unitHeight;
   field.unit_vect = (Vector2d){unitWidth, unitHeight};

   // Create the fields grid in linear form rather than a multi-dimensoinal array as it's faster
   field.grid = NewDynamicArray((object.width / unitWidth) * (object.height / unitHeight), sizeof(float)); // AllocateArray((object.height / unitHeight) * (object.width / unitWidth), sizeof(float));

   // Calculate initial field unit values
   CalculateField_Rect(field);

   // Method 1: Formulate equations of vertical and horizontal lines to create a grid, and use the equations to determine which field units are occupied by rectangloid objects

   // Need a 2D vector to represent the unit/basis vectors that will be scaled with a scalar in order to map to a field position
   // Method 2: Create linear array of field units with the given height and width, and unit height and width
   return field;
}

Field_Rect CalculateField_Rect(Field_Rect field)
{
   // Map object positions to the grid for all objects
   if (field.items == NULL || field.items->coll == NULL || field.items->coll->count <= 0)
   {
      printf("No items are in the provided field. Nothing to calculate.");
      return field; // No objects to update
   }
   NewtonObject2d *newtObj = Enumerate(field.items->coll);
   if (newtObj == NULL)
   {
      fprintf(stderr, "Failed to retrieve enumerated Object\n"); // Enumerator failed to retrieve the first item
   }
   while (newtObj != NULL)
   {
      if (&newtObj != NULL)
      {
         // Increment the scalar for the field unit's position the object is currently in
         // We will go by its positional center for now
         Vector2d pos = newtObj->pos;
         int i = (int)pos.x / field.unit_vect.x;
         int j = (int)pos.y / field.unit_vect.y;
         int i_field = (j * field.column_units) + i; // Convert the 2D grid position to a linear index

         void *target = (char *)field.grid->coll->items + (i_field * field.grid->coll->elemSize);
         *(float *)target += 1.0;

         // Or 1. Get the address
         // void *target = (char *)field.grid->coll->items + (i_field * field.grid->coll->elemSize);
         // // 2. Tell C it's a double pointer and "dereference" it to set the value
         // *(double *)target = 1.0;
      }
      newtObj = Enumerate(field.items->coll);
   }
   ResetEnumerator(field.items->coll); // Reset enumerator once done
   Enumerate(field.items->coll);
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