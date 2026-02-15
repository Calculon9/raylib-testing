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
Container_Rect CreateContainer_Rect(Rectangloid object, void *items)
{

}

void Container_Rect_GetCollisionObjects(Rectangloid rect);
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