/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/
#include "common/common.h"
#include "world/world.h"
#include "physics/rectangloid.h"
#include "physics/field.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//Physical state variables
float gravity = 9.8f; // Gravitational acceleration (m/s^2)
float field = {0};


//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
World CreateWorld(CoordinateSpace2d world_space, DynamicArray objects, float gravity)
{
   World world = {0};
   world.world_space = world_space;
   world.objects = objects;
   world.gravity = gravity;
   
   return world;
}
//World CalculateFieldLines(Field field);
//World InitialiseFieldCells(Field field);

