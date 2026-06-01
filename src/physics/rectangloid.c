/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/

#include <stdio.h>
#include "physics/rectangloid.h"
#include "physics/newton_object.h"
#include "physics/newton_object.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------




//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
// Rectangloid CreateRectangloid(float height, float width, ColourRgba colour, size_t mass, Vector2d origin, Vector2d velocity, Vector2d acceleration)
// {
//    Surface2d surface = {0};
//    surface.surface_vectors = *NewDynamicArray(4, sizeof(Vector2d));

//    NewtonObject2d newtOb = CreateNewtonObject2d(mass, origin, velocity, acceleration, surface);

//    // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
//    Rectangloid newRect = {0};
//    newRect.newtonian_properties = newtOb;
//    newRect.height = height;
//    newRect.width = width;
//    newRect.colourRgba = colour;

//    return newRect;
// }


// Rectangloid CreateRectangloid_Static(float height, float width, ColourRgba colour, Vector2d position)
// {
//    Surface2d surface = {0};
//    surface.surface_vectors = *NewDynamicArray(4, sizeof(Vector2d));

//    NewtonObject2d_Static newtOb = CreateNewtonObject2d_Static(position, surface);
   
//    // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
//    Rectangloid newRect = {0};
//    newRect.newtonian_properties = newtOb;
//    newRect.height = height;
//    newRect.width = width;
//    newRect.colourRgba = colour;

//    return newRect;
// }

Rectangloid CreateRectangloid_FromObject(NewtonObject2d newtOb, float height, float width, ColourRgba colour)
{
   // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
   Rectangloid newRect = {0};
   newRect.newtonian_properties = newtOb;
   newRect.height = height;
   newRect.width = width;
   newRect.colourRgba = colour;

   return newRect;
}

void Rectangloid_GetCollisionObjects(Rectangloid rect)
{
   
}