/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/

#include <stdio.h>
#include "physics/rectangloid.h"
#include "physics/newton_object.h"
#include "physics/container_rect.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------




//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------

// Creates a static/immovable rectangloid container with the given rectangloid and items to be contained
// Container_Rect CreateContainer_Rect_Static(float height, float width, ColourRgba colour, Vector2d position, void *items)
// {
//    // Initialise the container and give it the contained items
//    Container_Rect contRect = {0};
//    contRect.items = items;

//    Surface2d surface = {0};
//    surface.surface_vectors = *NewDynamicArray(4, sizeof(Vector2d));

//    // Initialize the static NewtonObject2d and its position here
//    //NewtonObject2d newtObStatic = CreateNewtonObject2d_Static(position);
   
//    // Implement a static NewtonObject2d as a Rectangloid
//    contRect.rect = CreateRectangloid_FromObject(CreateNewtonObject2d_Static(position, surface), height, width, colour);

//    return contRect;
// }

void Container_Rect_GetCollisionObjects(Rectangloid rect) 
{
   
}
// Rectangloid CreateRectangloid(float height, float width, ColourRgba colour, size_t mass, Vector2d position, Velocity2d velocity, Acceleration2d acceleration)
// {
//    NewtonObject2d newtOb = CreateNewtonObject2d(mass, position, velocity, acceleration);
//    // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
//    Rectangloid newRect = {0};
//    newRect.object = newtOb;
//    newRect.height = height;
//    newRect.width = width;
//    newRect.colourRgba = colour;

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